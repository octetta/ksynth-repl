#include "hazel/hazel.h"
#include "HazelApp.h"
#include "TerminalPane.h"
#include <FL/Fl.H>
#include <string.h>

extern "C" {

void hazel_append_output(hazel_ctx_t* ctx, const char* text, int is_error) {
    if (!ctx || !ctx->app || !text) return;
    
    // We must lock the GUI if called from an async thread
    Fl::lock();
    
    if (ctx->is_terminal) {
        ctx->app->getTerminal()->appendOutput(text, is_error);
    } else {
        ctx->app->appendOutput(ctx->insert_pos, text, is_error);
        ctx->insert_pos += strlen(text);
    }
    
    Fl::unlock();
    Fl::awake();
}

void hazel_finish_eval(hazel_ctx_t* ctx) {
    if (ctx) {
        HazelApp* app = ctx->app;
        Fl::lock();
        app->finishEvaluation(ctx);
        Fl::unlock();
        Fl::awake();
        delete ctx;
    }
}

hazel_app_t* hazel_create(const char* title, hazel_eval_cb_t eval_cb, void* user_data) {
    HazelApp* app = new HazelApp(title, eval_cb, user_data);
    return (hazel_app_t*)app;
}

int hazel_run(hazel_app_t* app) {
    if (!app) return -1;
    return ((HazelApp*)app)->run();
}

void hazel_destroy(hazel_app_t* app) {
    if (app) {
        delete (HazelApp*)app;
    }
}

void hazel_load_file(hazel_app_t* app, const char* filepath) {
    if (app) {
        ((HazelApp*)app)->loadFile(filepath);
    }
}

void hazel_clear(hazel_app_t* app) {
    if (app) ((HazelApp*)app)->clear();
}

void hazel_append_block(hazel_app_t* app, char style, const char* text) {
    if (app) ((HazelApp*)app)->appendBlock(style, text);
}

const char* hazel_get_text(hazel_app_t* app) {
    if (app) return ((HazelApp*)app)->getText();
    return NULL;
}

const char* hazel_get_styles(hazel_app_t* app) {
    if (app) return ((HazelApp*)app)->getStyles();
    return NULL;
}

void hazel_set_filepath(hazel_app_t* app, const char* filepath) {
    if (app) ((HazelApp*)app)->setFilepath(filepath);
}

void hazel_set_dirty(hazel_app_t* app, int dirty) {
    if (app) ((HazelApp*)app)->setDirty(dirty);
}

}

const char* hazel_version() {
    return HAZEL_VERSION;
}

void hazel_set_app_version(hazel_app_t* app, const char* version) {
    if (app) ((HazelApp*)app)->setAppVersion(version);
}

void hazel_set_status(hazel_app_t* app, const char* status) {
    if (app) ((HazelApp*)app)->setExtraStatus(status);
}

void hazel_set_config(hazel_app_t* app, const hazel_config_t* config) {
    if (app && config) {
        ((HazelApp*)app)->setConfig(config);
    }
}

void hazel_get_config(hazel_app_t* app, hazel_config_t* config_out) {
    if (app && config_out) {
        *config_out = ((HazelApp*)app)->getConfig();
    }
}

extern "C" void hazel_load_preferences(hazel_app_t* app) {
    if (app) ((HazelApp*)app)->loadPreferences();
}
