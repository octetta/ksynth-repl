#ifndef SKRED_SCOPE_IPC_H
#define SKRED_SCOPE_IPC_H

#include <stddef.h>
#include <stdint.h>

#include "synth-types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SKRED_SCOPE_MAGIC UINT32_C(0x534B5343)
#define SKRED_SCOPE_VERSION 2
#define SKRED_SCOPE_NAME_MAX 128
#define SKRED_SCOPE_DEFAULT_NAME "skred-scope"
#define SKRED_SCOPE_DEFAULT_SECONDS 1.0
#define SKRED_SCOPE_TRACK_MAX RECORD_TRACK_MAX
#define SKRED_SCOPE_TRACK_COUNT RECORD_TRACK_COUNT
#define SKRED_SCOPE_CHANNELS RECORD_CHANNELS
#define SKRED_SCOPE_TRACK_NAME_MAX TEXT_MAX
#define SKRED_SCOPE_ALL_CHANNELS ((UINT32_C(1) << SKRED_SCOPE_CHANNELS) - 1)

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t header_bytes;
  uint32_t sample_rate;
  uint32_t channel_count;
  uint32_t capacity_frames;
  uint32_t channel_mask;
  uint32_t track_count;
  uint32_t track_name_bytes;
  uint64_t generation;
  volatile uint64_t sequence;
  volatile uint64_t write_frame;
  volatile uint64_t active;
  float track_volume_db[SKRED_SCOPE_TRACK_COUNT];
  char track_name[SKRED_SCOPE_TRACK_COUNT][SKRED_SCOPE_TRACK_NAME_MAX];
} skred_scope_header_t;

static_assert(sizeof(skred_scope_header_t) == 264,
               "scope IPC header layout changed");
static_assert(offsetof(skred_scope_header_t, sequence) == 48,
               "scope IPC sequence offset changed");

typedef struct {
#if defined(_WIN32) || defined(_WIN64)
  void *mapping_handle;
#else
  int fd;
#endif
  size_t mapping_bytes;
  const skred_scope_header_t *header;
  const float *frames;
  char name[SKRED_SCOPE_NAME_MAX];
} skred_scope_reader_t;

typedef struct {
  int active;
  int sample_rate;
  uint32_t channel_count;
  uint32_t channel_mask;
  uint32_t capacity_frames;
  uint64_t generation;
  uint64_t write_frame;
  char name[SKRED_SCOPE_NAME_MAX];
} skred_scope_status_t;

int scope_ipc_init(int max_block_frames, int sample_rate);
void scope_ipc_uninit(void);
int scope_ipc_start(const char *name, uint32_t channel_mask,
                    double buffer_seconds);
int scope_ipc_stop(void);
int scope_ipc_active(void);
const char *scope_ipc_name(void);
uint32_t scope_ipc_channel_mask(void);
uint32_t scope_ipc_capacity_frames(void);
uint64_t scope_ipc_write_frame(void);
void scope_ipc_status(skred_scope_status_t *status);
int scope_ipc_track_metadata_set(int track, const char *name, float volume_db);
synth_record_bus_t *scope_ipc_begin_block(int frame_count);
void scope_ipc_publish(const float *frames, int frame_count);

int scope_ipc_reader_open(skred_scope_reader_t *reader, const char *name);
void scope_ipc_reader_close(skred_scope_reader_t *reader);
int scope_ipc_reader_latest(const skred_scope_reader_t *reader, float *output,
                            uint32_t requested_frames,
                            uint64_t *first_frame_out);

#ifdef __cplusplus
}
#endif

#endif