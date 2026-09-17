#include <cstdio>
#include <iostream>
#include <cstring>
#include "hazel/hazel.h"
#include "ksynth.h"
#include <FL/Fl.H>
#include <unistd.h>

uintptr_t ks_handle = 0;

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

void my_eval_engine(const char* input, hazel_ctx_t* ctx, void* user_data) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", input);
    
    int r = ks_ctx_repl(ks_handle, buf);
    
    const char* out_str = ks_ctx_repl_str(ks_handle);
    if (out_str && strlen(out_str) > 0) {
        hazel_append_output(ctx, out_str, 0);
    }
    
    if (r < 0) {
        const char* err = ks_ctx_get_error(ks_handle);
        char msg[256];
        snprintf(msg, sizeof(msg), "error: %s\n", err ? err : "unknown");
        hazel_append_output(ctx, msg, 1);
    }
    
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
    config.startup_text = "//\n1000 0.5 T\n";
    
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
    
    hazel_run(app);
    
    ks_ctx_destroy(ks_handle);
    
    return 0;
}
