#include "scope-ipc.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "portable_atomic.h"

#define SCOPE_TRACK_VOLUME_DEFAULT (-20.0f)

typedef struct {
  int initialized;
#if defined(_WIN32) || defined(_WIN64)
  HANDLE mapping_handle;
#else
  int fd;
#endif
  int sample_rate;
  int scratch_frames;
  size_t mapping_bytes;
  uint64_t generation;
  skred_scope_header_t *header;
  float *frames;
  float *scratch;
  synth_record_bus_t bus;
  atomic_int_t producer_lock;
  atomic_int_t enabled;
  simple_mutex_t lifecycle_mutex;
  char name[SKRED_SCOPE_NAME_MAX];
  float track_volume_db[SKRED_SCOPE_TRACK_COUNT];
  char track_name[SKRED_SCOPE_TRACK_COUNT][SKRED_SCOPE_TRACK_NAME_MAX];
} scope_ipc_t;

static scope_ipc_t scope_ipc = {
#if !defined(_WIN32) && !defined(_WIN64)
  .fd = -1
#endif
};
static uint64_t scope_generation;

static uint64_t scope_atomic_load(const volatile uint64_t *value);
static void scope_atomic_store(volatile uint64_t *value, uint64_t next);
static void scope_ipc_track_metadata_defaults(void);
static void scope_ipc_refresh_track_metadata_locked(void);

static void scope_ipc_stop_locked(void) {
  int expected = 0;
  while (!atomic_compare_exchange_int(&scope_ipc.producer_lock, &expected, 1)) {
    expected = 0;
#if defined(_WIN32) || defined(_WIN64)
    SwitchToThread();
#else
    sched_yield();
#endif
  }
  atomic_store_int(&scope_ipc.enabled, 0);
  if (scope_ipc.header) scope_atomic_store(&scope_ipc.header->active, 0);
  if (scope_ipc.header && scope_ipc.mapping_bytes) {
#if defined(_WIN32) || defined(_WIN64)
    UnmapViewOfFile(scope_ipc.header);
#else
    munmap(scope_ipc.header, scope_ipc.mapping_bytes);
#endif
  }
#if !defined(_WIN32) && !defined(_WIN64)
  if (scope_ipc.fd >= 0) close(scope_ipc.fd);
  if (scope_ipc.name[0]) shm_unlink(scope_ipc.name);
  scope_ipc.fd = -1;
#endif
#if defined(_WIN32) || defined(_WIN64)
  if (scope_ipc.mapping_handle) CloseHandle(scope_ipc.mapping_handle);
  scope_ipc.mapping_handle = NULL;
#endif
  scope_ipc.mapping_bytes = 0;
  scope_ipc.header = NULL;
  scope_ipc.frames = NULL;
  scope_ipc.name[0] = '\0';
  atomic_store_int(&scope_ipc.producer_lock, 0);
}

static uint64_t scope_atomic_load(const volatile uint64_t *value) {
#if defined(_WIN64)
  MemoryBarrier();
  uint64_t result = *value;
  MemoryBarrier();
  return result;
#elif defined(_WIN32)
  return atomic_load_uint64((atomic_uint64_t *)value);
#else
  return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#endif
}

static void scope_atomic_store(volatile uint64_t *value, uint64_t next) {
#if defined(_WIN32) || defined(_WIN64)
  atomic_store_uint64((atomic_uint64_t *)value, next);
#else
  __atomic_store_n(value, next, __ATOMIC_RELEASE);
#endif
}

static int scope_canonical_name(const char *name, char *output,
                                size_t output_size) {
  if (!name || !name[0] || !output || output_size < 2) return -1;
#if defined(_WIN32) || defined(_WIN64)
  while (*name == '/') name++;
  if (!name[0] || strchr(name, '/') || strchr(name, '\\')) return -1;
  int written = snprintf(output, output_size, "Local\\%s", name);
  if (written < 0 || (size_t)written >= output_size) return -1;
#else
  int written = name[0] == '/'
    ? snprintf(output, output_size, "%s", name)
    : snprintf(output, output_size, "/%s", name);
  if (written < 0 || (size_t)written >= output_size) return -1;
  for (int i = 1; output[i]; i++) {
    if (output[i] == '/') return -1;
  }
#endif
  return 0;
}

static size_t scope_mapping_bytes(uint32_t capacity_frames,
                                  uint32_t channels) {
  if (capacity_frames == 0 || channels == 0) return 0;
  if ((size_t)capacity_frames > (SIZE_MAX - sizeof(skred_scope_header_t)) /
      ((size_t)channels * sizeof(float))) {
    return 0;
  }
  return sizeof(skred_scope_header_t) +
    (size_t)capacity_frames * channels * sizeof(float);
}

static void scope_ipc_track_metadata_defaults(void) {
  for (int track = 0; track <= SKRED_SCOPE_TRACK_MAX; track++) {
    scope_ipc.track_volume_db[track] = SCOPE_TRACK_VOLUME_DEFAULT;
    scope_ipc.track_name[track][0] = '\0';
  }
  snprintf(scope_ipc.track_name[0], SKRED_SCOPE_TRACK_NAME_MAX, "master");
}

static void scope_ipc_refresh_track_metadata_locked(void) {
  if (!scope_ipc.header) return;
  scope_ipc.header->track_count = SKRED_SCOPE_TRACK_COUNT;
  scope_ipc.header->track_name_bytes = SKRED_SCOPE_TRACK_NAME_MAX;
  for (int track = 0; track <= SKRED_SCOPE_TRACK_MAX; track++) {
    scope_ipc.header->track_volume_db[track] =
      scope_ipc.track_volume_db[track];
    snprintf(scope_ipc.header->track_name[track],
             SKRED_SCOPE_TRACK_NAME_MAX, "%s",
             scope_ipc.track_name[track]);
  }
}

int scope_ipc_init(int max_block_frames, int sample_rate) {
  if (scope_ipc.initialized) return 0;
  if (max_block_frames < 1 || sample_rate < 1) return -1;
  scope_ipc.scratch = calloc((size_t)max_block_frames * SKRED_SCOPE_CHANNELS,
                             sizeof(float));
  if (!scope_ipc.scratch) return -1;
  scope_ipc.scratch_frames = max_block_frames;
  scope_ipc.sample_rate = sample_rate;
  scope_ipc.bus.frames = scope_ipc.scratch;
  scope_ipc.bus.channels = SKRED_SCOPE_CHANNELS;
  scope_ipc_track_metadata_defaults();
  simple_mutex_init(&scope_ipc.lifecycle_mutex);
  scope_ipc.initialized = 1;
  return 0;
}

int scope_ipc_stop(void) {
  if (!scope_ipc.initialized) return 0;
  simple_mutex_lock(&scope_ipc.lifecycle_mutex);
  scope_ipc_stop_locked();
  simple_mutex_unlock(&scope_ipc.lifecycle_mutex);
  return 0;
}

void scope_ipc_uninit(void) {
  if (!scope_ipc.initialized) return;
  scope_ipc_stop();
  simple_mutex_destroy(&scope_ipc.lifecycle_mutex);
  free(scope_ipc.scratch);
  memset(&scope_ipc, 0, sizeof(scope_ipc));
#if !defined(_WIN32) && !defined(_WIN64)
  scope_ipc.fd = -1;
#endif
}

int scope_ipc_start(const char *name, uint32_t channel_mask,
                    double buffer_seconds) {
  if (!scope_ipc.initialized || !isfinite(buffer_seconds) ||
      buffer_seconds <= 0.0 || channel_mask == 0 ||
      (channel_mask & ~SKRED_SCOPE_ALL_CHANNELS) != 0) {
    return -1;
  }

  char canonical[SKRED_SCOPE_NAME_MAX];
  if (scope_canonical_name(name, canonical, sizeof(canonical)) != 0) return -1;
  double frames_f = buffer_seconds * scope_ipc.sample_rate;
  if (frames_f < 1.0 || frames_f > UINT32_MAX) return -1;
  uint32_t capacity = (uint32_t)frames_f;
  size_t mapping_bytes = scope_mapping_bytes(capacity, SKRED_SCOPE_CHANNELS);
  if (!mapping_bytes) return -1;

  simple_mutex_lock(&scope_ipc.lifecycle_mutex);
  scope_ipc_stop_locked();
#if defined(_WIN32) || defined(_WIN64)
  HANDLE mapping_handle = NULL;
  for (int attempt = 0; attempt < 100; attempt++) {
    SetLastError(ERROR_SUCCESS);
    mapping_handle = CreateFileMappingA(
      INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
      (DWORD)(((uint64_t)mapping_bytes) >> 32),
      (DWORD)((uint64_t)mapping_bytes & UINT32_MAX), canonical);
    if (!mapping_handle) break;
    if (GetLastError() != ERROR_ALREADY_EXISTS) break;
    const skred_scope_header_t *existing = MapViewOfFile(
      mapping_handle, FILE_MAP_READ, 0, 0, sizeof(skred_scope_header_t));
    int reusable = existing &&
      existing->magic == SKRED_SCOPE_MAGIC &&
      existing->version == SKRED_SCOPE_VERSION &&
      scope_mapping_bytes(existing->capacity_frames,
                          existing->channel_count) == mapping_bytes;
    if (existing) UnmapViewOfFile(existing);
    if (reusable) break;
    CloseHandle(mapping_handle);
    mapping_handle = NULL;
    Sleep(10);
  }
  if (!mapping_handle) {
    simple_mutex_unlock(&scope_ipc.lifecycle_mutex);
    return -1;
  }
  void *mapping = MapViewOfFile(mapping_handle, FILE_MAP_ALL_ACCESS,
                                0, 0, mapping_bytes);
  if (!mapping) {
    CloseHandle(mapping_handle);
    simple_mutex_unlock(&scope_ipc.lifecycle_mutex);
    return -1;
  }
#else
  shm_unlink(canonical);
  int fd = shm_open(canonical, O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0) {
    simple_mutex_unlock(&scope_ipc.lifecycle_mutex);
    return -1;
  }
  if (ftruncate(fd, (off_t)mapping_bytes) != 0) {
    close(fd);
    shm_unlink(canonical);
    simple_mutex_unlock(&scope_ipc.lifecycle_mutex);
    return -1;
  }
  void *mapping = mmap(NULL, mapping_bytes, PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, 0);
  if (mapping == MAP_FAILED) {
    close(fd);
    shm_unlink(canonical);
    simple_mutex_unlock(&scope_ipc.lifecycle_mutex);
    return -1;
  }
#endif

  memset(mapping, 0, mapping_bytes);
#if defined(_WIN32) || defined(_WIN64)
  scope_ipc.mapping_handle = mapping_handle;
#else
  scope_ipc.fd = fd;
#endif
  scope_ipc.mapping_bytes = mapping_bytes;
  scope_ipc.header = mapping;
  scope_ipc.frames = (float *)(scope_ipc.header + 1);
  snprintf(scope_ipc.name, sizeof(scope_ipc.name), "%s", canonical);
  scope_ipc.generation = ++scope_generation;

  scope_ipc.header->magic = SKRED_SCOPE_MAGIC;
  scope_ipc.header->version = SKRED_SCOPE_VERSION;
  scope_ipc.header->header_bytes = sizeof(*scope_ipc.header);
  scope_ipc.header->sample_rate = (uint32_t)scope_ipc.sample_rate;
  scope_ipc.header->channel_count = SKRED_SCOPE_CHANNELS;
  scope_ipc.header->capacity_frames = capacity;
  scope_ipc.header->channel_mask = channel_mask;
  scope_ipc.header->generation = scope_ipc.generation;
  scope_ipc_refresh_track_metadata_locked();
  scope_atomic_store(&scope_ipc.header->sequence, 0);
  scope_atomic_store(&scope_ipc.header->write_frame, 0);
  scope_atomic_store(&scope_ipc.header->active, 1);
  atomic_store_int(&scope_ipc.enabled, 1);
  simple_mutex_unlock(&scope_ipc.lifecycle_mutex);
  return 0;
}

int scope_ipc_active(void) {
  return atomic_load_int(&scope_ipc.enabled);
}

const char *scope_ipc_name(void) {
  return scope_ipc.name;
}

uint32_t scope_ipc_channel_mask(void) {
  return scope_ipc.header ? scope_ipc.header->channel_mask : 0;
}

uint32_t scope_ipc_capacity_frames(void) {
  return scope_ipc.header ? scope_ipc.header->capacity_frames : 0;
}

uint64_t scope_ipc_write_frame(void) {
  return scope_ipc.header
    ? scope_atomic_load(&scope_ipc.header->write_frame) : 0;
}

void scope_ipc_status(skred_scope_status_t *status) {
  if (!status) return;
  memset(status, 0, sizeof(*status));
  if (!scope_ipc.initialized) return;
  simple_mutex_lock(&scope_ipc.lifecycle_mutex);
  status->active = scope_ipc_active();
  status->sample_rate = scope_ipc.sample_rate;
  status->channel_count = SKRED_SCOPE_CHANNELS;
  if (scope_ipc.header) {
    status->channel_mask = scope_ipc.header->channel_mask;
    status->capacity_frames = scope_ipc.header->capacity_frames;
    status->generation = scope_ipc.header->generation;
    status->write_frame =
      scope_atomic_load(&scope_ipc.header->write_frame);
  }
  snprintf(status->name, sizeof(status->name), "%s", scope_ipc.name);
  simple_mutex_unlock(&scope_ipc.lifecycle_mutex);
}

int scope_ipc_track_metadata_set(int track, const char *name,
                                 float volume_db) {
  if (!scope_ipc.initialized) return -1;
  if (track < 0 || track > SKRED_SCOPE_TRACK_MAX || !name ||
      !isfinite(volume_db)) {
    return -1;
  }
  simple_mutex_lock(&scope_ipc.lifecycle_mutex);
  scope_ipc.track_volume_db[track] = volume_db;
  snprintf(scope_ipc.track_name[track], SKRED_SCOPE_TRACK_NAME_MAX, "%s",
           name);
  scope_ipc_refresh_track_metadata_locked();
  simple_mutex_unlock(&scope_ipc.lifecycle_mutex);
  return 0;
}

synth_record_bus_t *scope_ipc_begin_block(int frame_count) {
  if (frame_count < 1 || frame_count > scope_ipc.scratch_frames) {
    return NULL;
  }
  int expected = 0;
  if (!atomic_compare_exchange_int(&scope_ipc.producer_lock, &expected, 1))
    return NULL;
  if (!scope_ipc_active()) {
    atomic_store_int(&scope_ipc.producer_lock, 0);
    return NULL;
  }
  return &scope_ipc.bus;
}

void scope_ipc_publish(const float *frames, int frame_count) {
  if (!scope_ipc_active() || !frames || frame_count < 1) {
    atomic_store_int(&scope_ipc.producer_lock, 0);
    return;
  }
  uint32_t capacity = scope_ipc.header->capacity_frames;
  uint64_t write_frame = scope_atomic_load(&scope_ipc.header->write_frame);
  uint64_t sequence = scope_atomic_load(&scope_ipc.header->sequence);
  scope_atomic_store(&scope_ipc.header->sequence, sequence + 1);

  uint32_t remaining = (uint32_t)frame_count;
  while (remaining) {
    uint32_t offset = (uint32_t)(write_frame % capacity);
    uint32_t count = capacity - offset;
    if (count > remaining) count = remaining;
    memcpy(scope_ipc.frames + (size_t)offset * SKRED_SCOPE_CHANNELS, frames,
           (size_t)count * SKRED_SCOPE_CHANNELS * sizeof(float));
    frames += (size_t)count * SKRED_SCOPE_CHANNELS;
    write_frame += count;
    remaining -= count;
  }

  scope_atomic_store(&scope_ipc.header->write_frame, write_frame);
  scope_atomic_store(&scope_ipc.header->sequence, sequence + 2);
  atomic_store_int(&scope_ipc.producer_lock, 0);
}

int scope_ipc_reader_open(skred_scope_reader_t *reader, const char *name) {
  if (!reader) return -1;
  memset(reader, 0, sizeof(*reader));
#if !defined(_WIN32) && !defined(_WIN64)
  reader->fd = -1;
#endif
  if (scope_canonical_name(name, reader->name, sizeof(reader->name)) != 0)
    return -1;
#if defined(_WIN32) || defined(_WIN64)
  reader->mapping_handle = OpenFileMappingA(FILE_MAP_READ, FALSE, reader->name);
  if (!reader->mapping_handle) return -1;
  const skred_scope_header_t *probe = MapViewOfFile(
    (HANDLE)reader->mapping_handle, FILE_MAP_READ, 0, 0,
    sizeof(skred_scope_header_t));
  if (!probe) {
    scope_ipc_reader_close(reader);
    return -1;
  }
  if (probe->magic != SKRED_SCOPE_MAGIC ||
      probe->version != SKRED_SCOPE_VERSION ||
      probe->header_bytes != sizeof(*probe) ||
      probe->channel_count != SKRED_SCOPE_CHANNELS ||
      probe->track_count != SKRED_SCOPE_TRACK_COUNT ||
      probe->track_name_bytes != SKRED_SCOPE_TRACK_NAME_MAX) {
    UnmapViewOfFile(probe);
    scope_ipc_reader_close(reader);
    return -1;
  }
  reader->mapping_bytes = scope_mapping_bytes(probe->capacity_frames,
                                               probe->channel_count);
  UnmapViewOfFile(probe);
  if (!reader->mapping_bytes) {
    scope_ipc_reader_close(reader);
    return -1;
  }
  reader->header = MapViewOfFile((HANDLE)reader->mapping_handle,
                                 FILE_MAP_READ, 0, 0,
                                 reader->mapping_bytes);
  if (!reader->header) {
    scope_ipc_reader_close(reader);
    return -1;
  }
#else
  reader->fd = shm_open(reader->name, O_RDONLY, 0);
  if (reader->fd < 0) return -1;
  struct stat status;
  if (fstat(reader->fd, &status) != 0 ||
      status.st_size < (off_t)sizeof(skred_scope_header_t)) {
    scope_ipc_reader_close(reader);
    return -1;
  }
  reader->mapping_bytes = (size_t)status.st_size;
  void *mapping = mmap(NULL, reader->mapping_bytes, PROT_READ, MAP_SHARED,
                       reader->fd, 0);
  if (mapping == MAP_FAILED) {
    reader->header = NULL;
    scope_ipc_reader_close(reader);
    return -1;
  }
  reader->header = mapping;
#endif
  if (reader->header->magic != SKRED_SCOPE_MAGIC ||
      reader->header->version != SKRED_SCOPE_VERSION ||
      reader->header->header_bytes != sizeof(*reader->header) ||
      reader->header->channel_count != SKRED_SCOPE_CHANNELS ||
      reader->header->track_count != SKRED_SCOPE_TRACK_COUNT ||
      reader->header->track_name_bytes != SKRED_SCOPE_TRACK_NAME_MAX ||
      scope_mapping_bytes(reader->header->capacity_frames,
                          reader->header->channel_count) !=
        reader->mapping_bytes) {
    scope_ipc_reader_close(reader);
    return -1;
  }
  reader->frames = (const float *)(reader->header + 1);
  return 0;
}

void scope_ipc_reader_close(skred_scope_reader_t *reader) {
  if (!reader) return;
#if defined(_WIN32) || defined(_WIN64)
  if (reader->header) UnmapViewOfFile(reader->header);
  if (reader->mapping_handle) CloseHandle((HANDLE)reader->mapping_handle);
#else
  if (reader->header && reader->mapping_bytes)
    munmap((void *)reader->header, reader->mapping_bytes);
  if (reader->fd >= 0) close(reader->fd);
#endif
  memset(reader, 0, sizeof(*reader));
#if !defined(_WIN32) && !defined(_WIN64)
  reader->fd = -1;
#endif
}

int scope_ipc_reader_latest(const skred_scope_reader_t *reader, float *output,
                            uint32_t requested_frames,
                            uint64_t *first_frame_out) {
  if (!reader || !reader->header || !output || requested_frames == 0)
    return -1;
  uint32_t capacity = reader->header->capacity_frames;
  if (requested_frames > capacity) requested_frames = capacity;

  for (int attempt = 0; attempt < 8; attempt++) {
    uint64_t sequence_before = scope_atomic_load(&reader->header->sequence);
    if (sequence_before & 1) continue;
    uint64_t write_frame = scope_atomic_load(&reader->header->write_frame);
    uint32_t available = write_frame < capacity
      ? (uint32_t)write_frame : capacity;
    uint32_t count = requested_frames < available
      ? requested_frames : available;
    uint64_t first_frame = write_frame - count;
    uint32_t offset = (uint32_t)(first_frame % capacity);
    uint32_t first_count = capacity - offset;
    if (first_count > count) first_count = count;
    memcpy(output, reader->frames + (size_t)offset * SKRED_SCOPE_CHANNELS,
           (size_t)first_count * SKRED_SCOPE_CHANNELS * sizeof(float));
    if (first_count < count) {
      memcpy(output + (size_t)first_count * SKRED_SCOPE_CHANNELS, reader->frames,
             (size_t)(count - first_count) * SKRED_SCOPE_CHANNELS * sizeof(float));
    }
    uint64_t sequence_after = scope_atomic_load(&reader->header->sequence);
    if (sequence_before == sequence_after && !(sequence_after & 1)) {
      if (first_frame_out) *first_frame_out = first_frame;
      return (int)count;
    }
  }
  return 0;
}
