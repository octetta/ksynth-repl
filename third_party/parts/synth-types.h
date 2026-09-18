#ifndef _SYNTH_TYPES_H_
#define _SYNTH_TYPES_H_

#include "portable_atomic.h"

#define SKRED_DEFAULT_SAMPLE_RATE (44100)
extern int synth_sample_rate;
#define MAIN_SAMPLE_RATE (synth_sample_rate)
#define VOICE_MAX (64)
#define AUDIO_CHANNELS (2)
#define RECORD_TRACK_MAX (4)
#define RECORD_TRACK_COUNT (RECORD_TRACK_MAX + 1)
#define RECORD_CHANNELS (RECORD_TRACK_COUNT * AUDIO_CHANNELS)
#define SYNTH_FRAMES_PER_CALLBACK (128)

#define NEG_60_DB (-60.0f)
#define NEG_60_DB_AS_LINEAR (0.001)
#define SILENT NEG_60_DB


typedef struct {
    int slot;
    const char *name;
    int size;
    float sample_rate;
    int loop_start;
    int loop_end;
    int one_shot;
    const float *data;
} static_wave_meta_t;

typedef enum {
  OSC_PLAYBACK_GENERAL = 0,
  OSC_PLAYBACK_CYCLE_SIMPLE = 1
} osc_playback_class_t;

enum {
  WAVE_TABLE_SINE,     // 0
  WAVE_TABLE_SQR,      // 1
  WAVE_TABLE_SAW_DOWN, // 2
  WAVE_TABLE_SAW_UP,   // 3
  WAVE_TABLE_TRI,      // 4
  WAVE_TABLE_NOISE,    // 5
  WAVE_TABLE_NOISE_ALT,// 6
  WAVE_TABLE_CAP_1, // 7: input pair 1 left
  WAVE_TABLE_CAP_2, // 8: input pair 1 right
  WAVE_TABLE_CAP_3, // 9: input pair 2 left
  WAVE_TABLE_CAP_4, // 10: input pair 2 right
  WAVE_TABLE_CAP_5, // 11: input pair 3 left
  WAVE_TABLE_CAP_6, // 12: input pair 3 right
  WAVE_TABLE_CAP_7, // 13: input pair 4 left
  WAVE_TABLE_CAP_8, // 14: input pair 4 right

  WAVE_TABLE_KRG1 = 15, // was 10
  WAVE_TABLE_KRG2,  // 16
  WAVE_TABLE_KRG3,  // 17
  WAVE_TABLE_KRG4,  // 18
  WAVE_TABLE_KRG5,  // 19
  WAVE_TABLE_KRG6,  // 20
  WAVE_TABLE_KRG7,  // 21
  WAVE_TABLE_KRG8,  // 22
  WAVE_TABLE_KRG9,  // 23
  WAVE_TABLE_KRG10, // 24
  WAVE_TABLE_KRG11, // 25
  WAVE_TABLE_KRG12, // 26
  WAVE_TABLE_KRG13, // 27
  WAVE_TABLE_KRG14, // 28
  WAVE_TABLE_KRG15, // 29
  WAVE_TABLE_KRG16, // 30

  WAVE_TABLE_KRG17, // 31
  WAVE_TABLE_KRG18, // 32
  WAVE_TABLE_KRG19, // 33
  WAVE_TABLE_KRG20, // 34
  WAVE_TABLE_KRG21, // 35
  WAVE_TABLE_KRG22, // 36
  WAVE_TABLE_KRG23, // 37
  WAVE_TABLE_KRG24, // 38
  WAVE_TABLE_KRG25, // 39
  WAVE_TABLE_KRG26, // 40
  WAVE_TABLE_KRG27, // 41
  WAVE_TABLE_KRG28, // 42
  WAVE_TABLE_KRG29, // 43
  WAVE_TABLE_KRG30, // 44
  WAVE_TABLE_KRG31, // 45
  WAVE_TABLE_KRG32, // 46

  WAVE_TABLE_ESQ1_01 = 50,
  WAVE_TABLE_ESQ1_02, WAVE_TABLE_ESQ1_03, WAVE_TABLE_ESQ1_04, WAVE_TABLE_ESQ1_05,
  WAVE_TABLE_ESQ1_06, WAVE_TABLE_ESQ1_07, WAVE_TABLE_ESQ1_08, WAVE_TABLE_ESQ1_09,
  WAVE_TABLE_ESQ1_10, WAVE_TABLE_ESQ1_11, WAVE_TABLE_ESQ1_12, WAVE_TABLE_ESQ1_13,
  WAVE_TABLE_ESQ1_14, WAVE_TABLE_ESQ1_15, WAVE_TABLE_ESQ1_16, WAVE_TABLE_ESQ1_17,
  WAVE_TABLE_ESQ1_18, WAVE_TABLE_ESQ1_19, WAVE_TABLE_ESQ1_20, WAVE_TABLE_ESQ1_21,
  WAVE_TABLE_ESQ1_22, WAVE_TABLE_ESQ1_23, WAVE_TABLE_ESQ1_24, WAVE_TABLE_ESQ1_25,
  WAVE_TABLE_ESQ1_26, WAVE_TABLE_ESQ1_27, WAVE_TABLE_ESQ1_28, WAVE_TABLE_ESQ1_29,
  WAVE_TABLE_ESQ1_30, WAVE_TABLE_ESQ1_31, WAVE_TABLE_ESQ1_32, // 81

  WAVE_TABLE_DRUM_01 = 82,
  WAVE_TABLE_DRUM_02, WAVE_TABLE_DRUM_03, WAVE_TABLE_DRUM_04, WAVE_TABLE_DRUM_05,
  WAVE_TABLE_DRUM_06, WAVE_TABLE_DRUM_07, WAVE_TABLE_DRUM_08, WAVE_TABLE_DRUM_09,
  WAVE_TABLE_DRUM_10, WAVE_TABLE_DRUM_11, WAVE_TABLE_DRUM_12, WAVE_TABLE_DRUM_13,
  WAVE_TABLE_DRUM_14, WAVE_TABLE_DRUM_15, WAVE_TABLE_DRUM_16, WAVE_TABLE_DRUM_17,
  WAVE_TABLE_DRUM_18, // 99
  WAVE_TABLE_COSINE = 47, // 47
  WAVE_TABLE_CENTERED_PULSE = 48,
  WAVE_TABLE_KRG2_CENTERED = 49,

  EW_00 = 50,
  EW_01,
  EW_02,
  EW_03,
  EW_04,
  EW_05,
  EW_99 = 50+99,

  EXT_SAMPLE_000 = 300,        // was 200
  EXT_SAMPLE_999 = 300 + 999,  // 999
  WAVE_TABLE_MAX
};

#define WAVE_TABLE_CAPTURE_FIRST WAVE_TABLE_CAP_1
#define WAVE_TABLE_CAPTURE_LAST WAVE_TABLE_CAP_8
#define WAVE_TABLE_CAPTURE_CHANNELS 8
#define WAVE_TABLE_CAP_LEFT WAVE_TABLE_CAP_1
#define WAVE_TABLE_CAP_RIGHT WAVE_TABLE_CAP_2

enum {
  FILTER_LOWPASS = 1,
  FILTER_HIGHPASS = 2,
  FILTER_BANDPASS = 3,
  FILTER_NOTCH = 4,
  FILTER_ALL_PASS = 5,
};

// Low-pass filter state structure
typedef struct {
  float x1, x2;  // Input delay line
  float y1, y2;  // Output delay line
  float b0, b1, b2;  // Feedforward coefficients
  float a1, a2;      // Feedback coefficients
    
  // Parameter tracking for coefficient updates
  float last_freq;
  float last_resonance;
  int last_mode;
  float drive;  // cached per-character soft-clip amount; 0 = bypass (character 0)
} mmf_t;

#include <stdint.h> // for uint64_t

typedef struct {
    float a;
    float d;
    float s;
    float r;
    float attack_time;    // attack duration in samples
    float decay_time;     // decay duration in samples
    float sustain_level;     // 0 to 1
    float release_time;   // release duration in samples
    uint64_t sample_start;   // sample count when note is triggered
    uint64_t sample_release; // release sample; UINT64_MAX while held
    int is_active;            // envelope state
    float velocity; // multiply envelope by this value
    float amplitude_at_release;
    float current_amplitude;
    float amplitude_at_trigger; // The "floor" for the current attack

    // --- MULTISTAGE ENVELOPE EXTENSIONS ---
    uint8_t mode;             // 0 = ADSR, 1 = MULTISTAGE
    uint8_t num_stages;       // How many pairs [time, level] were passed
    uint8_t sustain_stage;    // The stage index that holds until release (-1 if none)
    uint8_t current_stage;    // State machine tracker
    float times[16];          // Segment durations in samples (converted from seconds)
    float levels[16];         // Target amplitude levels
    uint64_t stage_start_sample; // Sample count when current stage began
    float stage_start_level;  // Amplitude at the start of current stage
} envelope_t;

#define WAVE_NAME_MAX (16+1)
typedef char wave_name_t[WAVE_NAME_MAX];
#define TEXT_MAX (32+1)
typedef char text_t[TEXT_MAX];

enum {
  SAMPLE_STATE_IDLE = 0,
  SAMPLE_STATE_ARMED,
  SAMPLE_STATE_RECORDING,
  SAMPLE_STATE_COMPLETE
};

enum {
  SAMPLE_SOURCE_DRY = 0,
  SAMPLE_SOURCE_VOICE,
  SAMPLE_SOURCE_MASTER
};

typedef struct {
  int capacity; /* frames; storage is allocated for up to stereo */
  atomic_int_t state;
  int source;
  int source_voice;
  atomic_int_t frames; /* frames remaining while recording */
  int channels;
  int ptr;      /* frames written */
  int len;      /* completed frames */
  float *where;
  int offset;
  int trim;
} synth_sample_t;

typedef struct {
  float *frames;
  int channels;
} synth_record_bus_t;

#endif
