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

#define NUM_BANKS 16
typedef struct {
    float* buffer;
    int length;
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
            if (s == 'A' || s == 'D') {
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

static char get_var(const char *ptr) {
  while (*ptr == ' ') ptr++;
  char v_name = *ptr;
  if (v_name >= 'A' && v_name <= 'Z') return v_name;
  return '\0';
}


const char* ksynth_help_as_html(const char* ext) {
    return "<h2>KSynth-REPL Help</h2>"
           "<p>KSynth is an array-oriented audio language. Commands are evaluated line by line.</p>"
           "<h3>Slash Commands</h3>"
           "<ul>"
           "<li><b>\\? [var]</b> - View variable waveform graph</li>\n           <li><b>\\p [var]</b> - Play the variable (Mono)</li>"
           "<li><b>\\b [0-15] [var]</b> - Bank a variable into slot 0-15</li>"
           "<li><b>\\pb [0-15] [semi] [cents] [gain] [atten]</b> - Play bank (optional params)</li>"
           "<li><b>\\ps [var]</b> - Play the variable (Stereo)</li>"
           "<li><b>\\l [file]</b> - Load a file (handled by Hazel)</li>"
           "<li><b>\\w [ms]</b> - Wait for N milliseconds</li>"
           "<li><b>\\s [var]</b> - Save to mono WAV (TBD)</li>"
           "<li><b>\\ss [var]</b> - Save to stereo WAV (TBD)</li>"
           "</ul>";
}

void my_eval_engine(const char* input, hazel_ctx_t* ctx, void* user_data) {
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
                float semis = 0.0f, cents = 0.0f, gain = 1.0f, atten = 1.0f;
                int parsed = sscanf(p + 3, "%d %f %f %f %f", &slot, &semis, &cents, &gain, &atten);
                if (parsed >= 1 && slot >= 0 && slot < NUM_BANKS) {
                    if (banks[slot].buffer && banks[slot].length > 0) {
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
                            voices[vslot].gain = gain;
                            voices[vslot].atten = atten;
                            voices[vslot].stereo = 0; // banks are mono for now
                            voices[vslot].active = 1;
                            char msg[128];
                            snprintf(msg, sizeof(msg), "Playing bank %d (semi:%.1f, cents:%.1f, gain:%.2f, atten:%.4f)\n", slot, semis, cents, gain, atten);
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
                char v_name = get_var(arg);
                if (v_name) {
                    int r = ks_ctx_get_var(ks_handle, v_name);
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
                                snprintf(msg, sizeof(msg), "playing %c in slot %d (%s)\n", v_name, slot, is_stereo ? "stereo" : "mono");
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
                char v_name = 0;
                char* arg = p + 2;
                while (*arg == ' ') arg++;
                if (sscanf(arg, "%d %c", &slot, &v_name) == 2 && slot >= 0 && slot < NUM_BANKS) {
                    int r = ks_ctx_get_var(ks_handle, v_name);
                    float* buf = ks_ctx_get_var_buf(ks_handle);
                    if (r > 0 && buf) {
                        if (banks[slot].buffer) free(banks[slot].buffer);
                        banks[slot].buffer = (float*)malloc(r * sizeof(float));
                        memcpy(banks[slot].buffer, buf, r * sizeof(float));
                        banks[slot].length = r;
                        char msg[64];
                        snprintf(msg, sizeof(msg), "Banked %c into slot %d\n", v_name, slot);
                        hazel_append_output(ctx, msg, 0);
                    } else {
                        hazel_append_output(ctx, "Variable empty or invalid\n", 1);
                    }
                } else {
                    hazel_append_output(ctx, "Invalid bank command (use: \\b [0-15] [A-Z])\n", 1);
                }
            } else if (p[1] == '?') {
                char v_name = get_var(p + 2);
                if (v_name) {
                    int r = ks_ctx_get_var(ks_handle, v_name);
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
    const char* file_to_load = nullptr;

    for (int i=1; i<argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'e') events_port = atoi(&argv[i][2]);
            else if (argv[i][1] == 'p') udp_port = atoi(&argv[i][2]);
        } else {
            file_to_load = argv[i];
        }
    }

    // Initialize ksynth context
    ks_handle = ks_ctx_create();

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
    config.cursor_fg = FL_WHITE;
    config.cursor_bg = FL_BLACK;
    config.select_bg = fl_rgb_color(180, 200, 255);
    config.parser_mode = 2; // MODE 2: comments start with /, note blocks with //
    config.udp_port = 60540; // Placeholders
    config.events_port = 60541; // Placeholders
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

    hazel_set_app_version(app, "KSynth-REPL\nEngine: KSynth");
    
    if (file_to_load) {
        hazel_load_file(app, file_to_load);
    }
    
    audio_start();
    hazel_run(app);
    audio_end();
    
    ks_ctx_destroy(ks_handle);
    
    return 0;
}
