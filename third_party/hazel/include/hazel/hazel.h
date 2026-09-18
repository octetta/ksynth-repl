#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define HAZEL_VERSION "0.1.2"
#define HAZEL_VERSION_MAJOR 0
#define HAZEL_VERSION_MINOR 1
#define HAZEL_VERSION_PATCH 2

const char* hazel_version();

typedef struct hazel_app_t hazel_app_t;

typedef struct {
    int font;
    int font_size;
    int theme; // 0=Light, 1=Dark, 2=Custom
    unsigned int input_bg;
    unsigned int output_bg;
    unsigned int error_bg;
    unsigned int markdown_bg;
    unsigned int text_fg;
    unsigned int error_fg;
    unsigned int markdown_fg;
    unsigned int cursor_fg;
    unsigned int cursor_bg;
    unsigned int select_bg;
    int parser_mode; // 0 = default, 1 = skred-repl contiguous mode
    int udp_port;
    int events_port;
    int (*on_open)(hazel_app_t* app, const char* filepath, void* user_data);
    int (*on_save)(hazel_app_t* app, const char* filepath, void* user_data);
    int (*on_open_dir)(hazel_app_t* app, const char* dirpath, void* user_data);
    const char* startup_text;
    const char* help_extension_html;
    const char* (*help_extension_cb)(const char* path);
    unsigned int command_bg;
    unsigned int command_fg;
} hazel_config_t;

void hazel_set_config(hazel_app_t* app, const hazel_config_t* config);
void hazel_get_config(hazel_app_t* app, hazel_config_t* config_out);
void hazel_set_app_version(hazel_app_t* app, const char* version);
void hazel_set_status(hazel_app_t* app, const char* status);
void hazel_load_preferences(hazel_app_t* app);

// Context passed to the evaluation callback to append output asynchronously
typedef struct hazel_ctx_t hazel_ctx_t;

// The user's evaluation function.
// It receives the 'input' text that the user pressed Ctrl+Enter on.
// It can call hazel_append_output() asynchronously or synchronously.
// It must call hazel_finish_eval() when done.
typedef void (*hazel_eval_cb_t)(const char* input, hazel_ctx_t* ctx, void* user_data);

// Appends output to the notebook. Can be called from any thread!
void hazel_append_output(hazel_ctx_t* ctx, const char* text, int is_error);

// Signals that evaluation is complete. Can be called from any thread!
void hazel_finish_eval(hazel_ctx_t* ctx);

// Create and run the notebook
hazel_app_t* hazel_create(const char* title, hazel_eval_cb_t eval_cb, void* user_data);
int hazel_run(hazel_app_t* app);
void hazel_destroy(hazel_app_t* app);
void hazel_load_file(hazel_app_t* app, const char* filepath);

void hazel_clear(hazel_app_t* app);
void hazel_append_block(hazel_app_t* app, char style, const char* text);
const char* hazel_get_text(hazel_app_t* app);
const char* hazel_get_styles(hazel_app_t* app);
void hazel_set_filepath(hazel_app_t* app, const char* filepath);
void hazel_set_dirty(hazel_app_t* app, int dirty);

#ifdef __cplusplus
}
#endif
