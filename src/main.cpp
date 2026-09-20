#include <cstdio>
#include <iostream>
#include <cstring>
#include "hazel/hazel.h"
extern "C" {
#include "ksynth.h"
}
#include <FL/Fl.H>
#include <unistd.h>
#include "scope.h"

uintptr_t ks_handle = 0;
float master_vol_db = 0.0f;
float master_vel_curve[128];
bool vel_curve_initialized = false;

#define NUM_BANKS 128
typedef struct {
    float* buffer;
    int length;
    float base_semis;
    float base_cents;
    float base_gain_db;
    float base_atten;
    float base_vel_sens;
} BankedWave;
BankedWave banks[NUM_BANKS] = {0};


int my_load_cb(hazel_app_t* app, const char* filepath, void* user_data) { 
    size_t f_len = strlen(filepath);
    
    // Normal .ks load
    if (f_len < 3 || strcmp(filepath + f_len - 3, ".ks") != 0) return 0;
    
    FILE* f = fopen(filepath, "r");
    if (!f) return 1;
    
    hazel_clear(app);
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buf = (char*)malloc(size + 1);
    if (buf) {
        fread(buf, 1, size, f);
        buf[size] = '\0';
        hazel_append_block(app, 'A', buf);
        free(buf);
    }
    fclose(f);
    
    hazel_set_filepath(app, filepath);
    hazel_set_dirty(app, 0);
    return 1;
}

int my_save_cb(hazel_app_t* app, const char* filepath, void* user_data) {
    size_t f_len = strlen(filepath);
    if (f_len < 3 || strcmp(filepath + f_len - 3, ".ks") != 0) return 0;
    
    FILE* f = fopen(filepath, "w");
    if (!f) return 1;
    
    const char* text = hazel_get_text(app);
    const char* styles = hazel_get_styles(app);
    
    if (text && styles) {
        int len = strlen(text);
        for (int i = 0; i < len; i++) {
            char s = styles[i];
            if (s == 'A' || s == 'D' || s == 'E') {
                fprintf(f, "%c", text[i]);
            }
        }
    }
    
    fclose(f);
    hazel_set_filepath(app, filepath);
    hazel_set_dirty(app, 0);
    return 1;
}

int my_dir_cb(hazel_app_t* app, const char* dirpath, void* user_data) {
    if (chdir(dirpath) == 0) {
        printf("Changed Working Directory to: %s\n", dirpath);
        return 1;
    }
    return 0;
}



#include "miniaudio.h"
#include "scope-ipc.h"
#include "midi.h"
#include "udp.h"


#define MAX_VOICES 8

typedef struct {
  float* buffer;
  int n;
  double idx;
  double phase_inc;
  float gain;
  float atten;
  int stereo;
  int active;
} Voice;

volatile Voice voices[MAX_VOICES] = {0};

void* global_hazel_ctx = NULL;
void trigger_midi_note(int note, int velocity) {
    if (note < 0 || note >= NUM_BANKS) return;
    if (!banks[note].buffer) return;
    
    // Find free voice
    int v = -1;
    for (int i=0; i<MAX_VOICES; i++) {
        if (!voices[i].active) { v = i; break; }
    }
    if (v == -1) return; // Voice stealing omitted for now
    
    if (voices[v].buffer) free(voices[v].buffer);
    voices[v].buffer = (float*)malloc(banks[note].length * sizeof(float));
    memcpy(voices[v].buffer, banks[note].buffer, banks[note].length * sizeof(float));
    voices[v].n = banks[note].length;
    voices[v].idx = 0;
    
    float semis = banks[note].base_semis;
    float cents = banks[note].base_cents;
    float freq_ratio = powf(2.0f, (semis + cents/100.0f) / 12.0f);
    voices[v].phase_inc = freq_ratio;
    
    float gain = powf(10.0f, banks[note].base_gain_db / 20.0f);
    float vel_sens = banks[note].base_vel_sens;
    float vel_mult = velocity / 127.0f;
    voices[v].gain = gain * (1.0f - vel_sens + vel_sens * vel_mult);
    voices[v].atten = banks[note].base_atten;
    voices[v].stereo = 0;
    voices[v].active = 1;
}


void cb(ma_device* d, void* o, const void* i, ma_uint32 n) {
  float* out = (float*)o;
  
  for (ma_uint32 j = 0; j < n * 2; j++) {
    out[j] = 0.0f;
  }
  
  for (int v = 0; v < MAX_VOICES; v++) {
    if (!voices[v].active) continue;
    
    float* buf = voices[v].buffer;
    int len = voices[v].n;
    double idx = voices[v].idx;
    double phase_inc = voices[v].phase_inc;
    float gain = voices[v].gain;
    float atten = voices[v].atten;
    int stereo = voices[v].stereo;
    
    if (!buf) {
      voices[v].active = 0;
      continue;
    }
    
    for (ma_uint32 j = 0; j < n; j++) {
      int i0 = (int)idx;
      if (i0 >= len || gain <= 0.0001f) {
        voices[v].active = 0;
        break;
      }
      
      if (stereo) {
          // Nearest neighbor for stereo to keep it simple, or aligned interpolation
          int i0_s = (i0 / 2) * 2;
          if (i0_s + 1 >= len) {
              voices[v].active = 0; break;
          }
          out[j * 2] += buf[i0_s] * gain;
          out[j * 2 + 1] += buf[i0_s + 1] * gain;
          idx += phase_inc * 2.0;
      } else {
          // Linear interpolation for mono
          int i1 = i0 + 1;
          if (i1 >= len) i1 = i0;
          float frac = (float)(idx - i0);
          float sample = buf[i0] + (buf[i1] - buf[i0]) * frac;
          
          out[j * 2] += sample * gain;
          out[j * 2 + 1] += sample * gain;
          idx += phase_inc;
      }
      
      gain *= atten;
    }
    
    voices[v].idx = idx;
    voices[v].gain = gain;
  }
  
  float master_linear = powf(10.0f, master_vol_db / 20.0f);
  for (ma_uint32 j = 0; j < n * 2; j++) {
    out[j] *= master_linear;
  }
  
  if (scope_ipc_active()) {
      synth_record_bus_t* bus = scope_ipc_begin_block(n);
      if (bus) {
          for (ma_uint32 i = 0; i < n; i++) {
              bus->frames[i * bus->channels + 0] = out[i * 2 + 0];
              bus->frames[i * bus->channels + 1] = out[i * 2 + 1];
              for (int c = 2; c < bus->channels; c++) {
                  bus->frames[i * bus->channels + c] = 0.0f;
              }
          }
          scope_ipc_publish(bus->frames, n);
      }
  }
}

ma_device_config cfg;
ma_device dev;

int audio_start(void) {
  cfg = ma_device_config_init(ma_device_type_playback);
  cfg.playback.format = ma_format_f32;
  cfg.playback.channels = 2;
  cfg.sampleRate = 44100;
  cfg.dataCallback = cb;
  if (ma_device_init(NULL, &cfg, &dev) != MA_SUCCESS) return 1;
  ma_device_start(&dev);
  return 0;
}

int audio_end(void) {
  for (int i = 0; i < MAX_VOICES; i++) {
    if (voices[i].buffer) {
      free(voices[i].buffer);
      voices[i].buffer = NULL;
    }
  }
  ma_device_uninit(&dev);
  return 0;
}

static char* get_var(const char *ptr, char *out_name) {
  while (*ptr == ' ') ptr++;
  int i = 0;
  while ((*ptr >= 'A' && *ptr <= 'Z') || (*ptr >= 'a' && *ptr <= 'z') || (*ptr >= '0' && *ptr <= '9') || *ptr == '_') {
      out_name[i++] = *ptr++;
  }
  out_name[i] = '\0';
  return (i > 0) ? out_name : NULL;
}


const char* ksynth_help_as_html(const char* ext) {
    return "<h2>KSynth-REPL Help</h2>"
           "<p>KSynth is an array-oriented audio language. Commands are evaluated line by line.</p>"
           "<h3>Slash Commands</h3>"
           "<ul>"
           "<li><b>\\? [var]</b> - View variable waveform graph</li>\n           <li><b>\\p [var]</b> - Play the variable (Mono)</li>"
           "<li><b>\\b [0-127] [var]</b> - Bank a variable into slot 0-15</li>"
           "<li><b>\\pb [0-15] [semi] [cents] [gain] [atten]</b> - Play bank (optional params)</li>"
           "<li><b>\\ps [var]</b> - Play the variable (Stereo)</li>"
           "<li><b>\\l [file]</b> - Load a file (handled by Hazel)</li>\n           <li><b>\\ra [var] [path]</b> - Read audio file into variable</li>"
           "<li><b>\\w [ms]</b> - Wait for N milliseconds</li>"
           "<li><b>\\s [var]</b> - Save to mono WAV (TBD)</li>"
           "<li><b>\\ss [var]</b> - Save to stereo WAV (TBD)</li>"
           "</ul>";
}

void my_eval_engine(const char* input, hazel_ctx_t* ctx, void* user_data) {
    if (ctx) global_hazel_ctx = (void*)ctx;
    char* text = strdup(input);
    char* line = strtok(text, "\n");
    
    char block_buf[8192];
    block_buf[0] = '\0';
    
    auto flush_block = [&]() {
        if (strlen(block_buf) > 0) {
            int r = ks_ctx_repl(ks_handle, block_buf);
            int r_len = ks_ctx_repl_length(ks_handle);
            const char* out_str = ks_ctx_repl_str(ks_handle);
            
            if (r == 0 && out_str && strlen(out_str) > 0) {
                char msg[1024];
                snprintf(msg, sizeof(msg), "%s\n", out_str);
                hazel_append_output(ctx, msg, 0);
            }
            if (r < 0) {
                const char* err = ks_ctx_get_error(ks_handle);
                char msg[256];
                snprintf(msg, sizeof(msg), "error: %s\n", err ? err : "unknown");
                hazel_append_output(ctx, msg, 1);
            }
            block_buf[0] = '\0';
        }
    };
    
    while (line) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        
        if (p[0] == '\\') {
            flush_block();
            
            if (p[1] == 'p' && p[2] == 'b') {
                int slot = -1;
                float velocity = 127.0f, semis = 0.0f, cents = 0.0f, gain_db = 0.0f, atten = 1.0f, vel_sens = 1.0f;
                int parsed = sscanf(p + 3, "%d %f %f %f %f %f %f", &slot, &velocity, &semis, &cents, &gain_db, &atten, &vel_sens);
                if (parsed >= 1 && slot >= 0 && slot < NUM_BANKS) {
                    if (banks[slot].buffer && banks[slot].length > 0) {
                        if (parsed < 2) velocity = 127.0f;
                        if (parsed < 3) semis = banks[slot].base_semis;
                        if (parsed < 4) cents = banks[slot].base_cents;
                        if (parsed < 5) gain_db = banks[slot].base_gain_db;
                        if (parsed < 6) atten = banks[slot].base_atten;
                        if (parsed < 7) vel_sens = banks[slot].base_vel_sens;
                        
                        int vslot = -1;
                        for (int i = 0; i < MAX_VOICES; i++) {
                            if (!voices[i].active) { vslot = i; break; }
                        }
                        if (vslot != -1) {
                            if (voices[vslot].buffer) free(voices[vslot].buffer);
                            voices[vslot].n = banks[slot].length;
                            voices[vslot].buffer = (float*)malloc(voices[vslot].n * sizeof(float));
                            memcpy(voices[vslot].buffer, banks[slot].buffer, voices[vslot].n * sizeof(float));
                            voices[vslot].idx = 0;
                            voices[vslot].phase_inc = pow(2.0, (semis + cents / 100.0) / 12.0);
                            
                            // Apply velocity sensitivity using the master LUT
                            int int_vel = (int)velocity;
                            if (int_vel < 0) int_vel = 0;
                            if (int_vel > 127) int_vel = 127;
                            
                            float curve_val = master_vel_curve[int_vel];
                            float factor = 1.0f - vel_sens + (vel_sens * curve_val);
                            
                            voices[vslot].gain = powf(10.0f, gain_db / 20.0f) * factor;
                            
                            voices[vslot].atten = atten;
                            voices[vslot].stereo = 0; // banks are mono for now
                            voices[vslot].active = 1;
                            char msg[128];
                            snprintf(msg, sizeof(msg), "Playing bank %d (vel:%.0f, semi:%.1f, gain:%.1fdB, sens:%.2f)\n", slot, velocity, semis, gain_db, vel_sens);
                            hazel_append_output(ctx, msg, 0);
                        } else {
                            hazel_append_output(ctx, "No free voice slots\n", 1);
                        }
                    } else {
                        hazel_append_output(ctx, "Bank empty\n", 1);
                    }
                } else {
                    hazel_append_output(ctx, "Invalid bank slot\n", 1);
                }
            } else if (p[1] == 'r' && p[2] == 'a') {
                char v_name[256];
                char file_path[1024];
                if (sscanf(p + 3, "%255s %1023s", v_name, file_path) == 2) {
                    ma_decoder decoder;
                    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, 44100);
                    if (ma_decoder_init_file(file_path, &config, &decoder) == MA_SUCCESS) {
                        ma_uint64 frames;
                        ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
                        float* buf = (float*)malloc(frames * sizeof(float));
                        if (buf) {
                            ma_uint64 framesRead;
                            ma_decoder_read_pcm_frames(&decoder, buf, frames, &framesRead);
                            if (ks_ctx_set_var_str_f32(ks_handle, v_name, buf, (int)framesRead)) {
                                char msg[256];
                                snprintf(msg, sizeof(msg), "Loaded %llu frames into %s\n", (unsigned long long)framesRead, v_name);
                                hazel_append_output(ctx, msg, 0);
                            }
                            free(buf);
                        }
                        ma_decoder_uninit(&decoder);
                    } else {
                        hazel_append_output(ctx, "Failed to load audio file\n", 1);
                    }
                } else {
                    hazel_append_output(ctx, "Usage: \ra [var_name] [filepath]\n", 1);
                }
            } else if (p[1] == 'p') {
                int is_stereo = 0;
                int is_quiet = 0;
                char* arg = p + 2;
                if (*arg == 's') {
                    is_stereo = 1;
                    arg++;
                } else if (*arg == 'q') {
                    is_quiet = 1;
                    arg++;
                }
                char v_name[256];
                if (get_var(arg, v_name)) {
                    int r = ks_ctx_get_var_str(ks_handle, v_name);
                    float* buf = ks_ctx_get_var_buf(ks_handle);
                    if (r > 0 && buf) {
                        int slot = -1;
                        for (int i = 0; i < MAX_VOICES; i++) {
                            if (!voices[i].active) {
                                slot = i;
                                break;
                            }
                        }
                        if (slot != -1) {
                            if (voices[slot].buffer) free(voices[slot].buffer);
                            voices[slot].n = r;
                            voices[slot].buffer = (float*)malloc(r * sizeof(float));
                            memcpy(voices[slot].buffer, buf, r * sizeof(float));
                            voices[slot].idx = 0;
                            voices[slot].phase_inc = 1.0;
                            voices[slot].gain = 1.0f;
                            voices[slot].atten = 1.0f;
                            voices[slot].stereo = is_stereo;
                            voices[slot].active = 1;
                            
                            if (!is_quiet) {
                                char msg[64];
                                snprintf(msg, sizeof(msg), "playing %s in slot %d (%s)\n", v_name, slot, is_stereo ? "stereo" : "mono");
                                hazel_append_output(ctx, msg, 0);
                            }
                        } else {
                            if (!is_quiet) hazel_append_output(ctx, "No free voice slots\n", 1);
                        }
                    } else {
                        if (!is_quiet) hazel_append_output(ctx, "Variable not found or empty\n", 1);
                    }
                }
            } else if (p[1] == 'b') {
                int slot = -1;
                char v_name[256] = {0};
                float semis = 0.0f, cents = 0.0f, gain_db = 0.0f, atten = 1.0f, vel_sens = 1.0f;
                char* arg = p + 2;
                while (*arg == ' ') arg++;
                int parsed = sscanf(arg, "%d %255s %f %f %f %f %f", &slot, v_name, &semis, &cents, &gain_db, &atten, &vel_sens);
                if (parsed >= 2 && slot >= 0 && slot < NUM_BANKS) {
                    int r = ks_ctx_get_var_str(ks_handle, v_name);
                    float* buf = ks_ctx_get_var_buf(ks_handle);
                    if (r > 0 && buf) {
                        if (banks[slot].buffer) free(banks[slot].buffer);
                        banks[slot].buffer = (float*)malloc(r * sizeof(float));
                        memcpy(banks[slot].buffer, buf, r * sizeof(float));
                        banks[slot].length = r;
                        banks[slot].base_semis = semis;
                        banks[slot].base_cents = cents;
                        banks[slot].base_gain_db = gain_db;
                        banks[slot].base_atten = atten;
                        banks[slot].base_vel_sens = vel_sens;
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Banked %s into slot %d (semi:%.1f, gain:%.1fdB, sens:%.2f)\n", v_name, slot, semis, gain_db, vel_sens);
                        hazel_append_output(ctx, msg, 0);
                    } else {
                        hazel_append_output(ctx, "Variable empty or invalid\n", 1);
                    }
                } else {
                    hazel_append_output(ctx, "Invalid bank command (use: \\b [0-127] [A-Z])\n", 1);
                }
            } else if (p[1] == '?') {
                char v_name[256];
                if (get_var(p + 2, v_name)) {
                    int r = ks_ctx_get_var_str(ks_handle, v_name);
                    float* buf = ks_ctx_get_var_buf(ks_handle);
                    if (r > 0 && buf) {
                        double* dbuf = (double*)malloc(r * sizeof(double));
                        if (dbuf) {
                            for (int i = 0; i < r; i++) dbuf[i] = buf[i];
                            print_scope(ctx, dbuf, r, 128, 64);
                            free(dbuf);
                        }
                    } else {
                        hazel_append_output(ctx, "Variable not found or empty\n", 1);
                    }
                }
            } else if (p[1] == 'm' && p[2] == 'v') {
                float db = 0.0f;
                if (sscanf(p + 3, "%f", &db) == 1) {
                    master_vol_db = db;
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Master volume set to %.1fdB\n", db);
                    hazel_append_output(ctx, msg, 0);
                } else {
                    hazel_append_output(ctx, "Usage: \\mv [dB]\n", 1);
                }
            } else if (p[1] == 'v' && p[2] == 'c') {
                char v_name[256];
                if (get_var(p + 3, v_name)) {
                    int r = ks_ctx_get_var_str(ks_handle, v_name);
                    float* buf = ks_ctx_get_var_buf(ks_handle);
                    if (r >= 128 && buf) {
                        for (int i = 0; i < 128; i++) {
                            master_vel_curve[i] = buf[i];
                        }
                        char msg[64];
                        snprintf(msg, sizeof(msg), "Loaded global velocity curve from %s\n", v_name);
                        hazel_append_output(ctx, msg, 0);
                    } else {
                        hazel_append_output(ctx, "Array must be at least 128 elements long\n", 1);
                    }
                } else {
                    hazel_append_output(ctx, "Usage: \\vc [A-Z]\n", 1);
                }
            } else if (p[1] == 's' && p[2] == 'g') {
                if (scope_ipc_init(4096, 48000) == 0) {
                    if (scope_ipc_start(SKRED_SCOPE_DEFAULT_NAME, SKRED_SCOPE_ALL_CHANNELS, 1.0) == 0) {
                        hazel_append_output(ctx, "Scope IPC Started (ksynth-scope)\n", 0);
                    } else {
                        hazel_append_output(ctx, "Failed to start Scope IPC\n", 1);
                    }
                } else {
                    hazel_append_output(ctx, "Failed to init Scope IPC\n", 1);
                }
            } else if (p[1] == 's' && p[2] == 's') {
                scope_ipc_stop();
                hazel_append_output(ctx, "Scope IPC Stopped\n", 0);
            } else if (p[1] == 's' && p[2] == '?') {
                if (scope_ipc_active()) {
                    hazel_append_output(ctx, "Scope IPC is ACTIVE (ksynth-scope)\n", 0);
                } else {
                    hazel_append_output(ctx, "Scope IPC is INACTIVE\n", 0);
                }
            }
        } else if (p[0] != '/' && p[0] != '\0') {
            if (strlen(block_buf) + strlen(p) + 2 < sizeof(block_buf)) {
                strcat(block_buf, p);
                strcat(block_buf, "\n");
            }
        }
        line = strtok(NULL, "\n");
    }
    
    flush_block();
    free(text);
    hazel_finish_eval(ctx);
}

int main(int argc, char** argv) {
    int udp_port = -1;
    int events_port = -1;
    bool enable_scope = false;
    const char* file_to_load = nullptr;

    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "--scope") == 0 || strcmp(argv[i], "-s") == 0) {
            enable_scope = true;
            continue;
        }
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'e') events_port = atoi(&argv[i][2]);
            else if (argv[i][1] == 'p') udp_port = atoi(&argv[i][2]);
        } else {
            file_to_load = argv[i];
        }
    }

    // Initialize ksynth context
    ks_handle = ks_ctx_create();
    for (int i = 0; i < 128; i++) {
        float norm = i / 127.0f;
        master_vel_curve[i] = norm * norm; // Default audio taper
    }

    Fl::set_font(FL_COURIER, "DejaVu Sans Mono");
    hazel_app_t* app = hazel_create("KSynth", my_eval_engine, nullptr);
    
    hazel_config_t config;
    memset(&config, 0, sizeof(config));
    config.font = FL_COURIER;
    config.font_size = 15;
    config.text_fg = FL_BLACK;
    config.input_bg = FL_WHITE;
    config.output_bg = fl_rgb_color(245, 250, 245);
    config.error_bg = fl_rgb_color(255, 235, 235);
    config.markdown_bg = fl_rgb_color(245, 245, 255);
    config.error_fg = FL_DARK_RED;
    config.markdown_fg = FL_DARK_GREEN;
    config.command_bg = fl_rgb_color(250, 240, 255);
    config.command_fg = fl_rgb_color(90, 0, 150);
    config.cursor_fg = FL_WHITE;
    config.cursor_bg = FL_BLACK;
    config.select_bg = fl_rgb_color(180, 200, 255);
    config.parser_mode = 2; // MODE 2: comments start with /, note blocks with //
    config.udp_port = 60442; // Placeholders
    config.events_port = 60443; // Placeholders
    config.on_open = my_load_cb;
    config.on_save = my_save_cb;
    config.on_open_dir = my_dir_cb;
    config.help_extension_cb = ksynth_help_as_html;
    config.startup_text = "//\nA:!5\n";
    
    hazel_set_config(app, &config);
    hazel_load_preferences(app);
    hazel_get_config(app, &config);
    
    if (udp_port >= 0) config.udp_port = udp_port;
    if (events_port >= 0) config.events_port = events_port;
    hazel_set_config(app, &config);

    char status_str[256];
    snprintf(status_str, sizeof(status_str), "UDP: %d  Evts: %d",
             config.udp_port, config.events_port);
    
    hazel_set_status(app, status_str);

    midi_init();
    udp_server_start(config.udp_port, config.events_port);


    hazel_set_app_version(app, "KSynth-REPL v0.1.0\nEngine: KSynth");
    
    if (file_to_load) {
        hazel_load_file(app, file_to_load);
    }
    
    if (enable_scope) {
        if (scope_ipc_init(4096, 48000) == 0) {
            scope_ipc_start(SKRED_SCOPE_DEFAULT_NAME, SKRED_SCOPE_ALL_CHANNELS, 1.0);
            printf("Scope IPC started as ksynth-scope\n");
        }
    }
    
    audio_start();
    hazel_run(app);
    audio_end();
    
    ks_ctx_destroy(ks_handle);
    
    return 0;
}
