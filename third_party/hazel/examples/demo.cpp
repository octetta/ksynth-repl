#include "hazel/hazel.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <ctype.h>

#include "ueval.h"

static ueval_env env;

void my_eval_callback(const char* input, hazel_ctx_t* ctx, void* user_data) {
    std::string in_str(input);
    
    // Trim
    while (!in_str.empty() && isspace(in_str.back())) in_str.pop_back();
    while (!in_str.empty() && isspace(in_str.front())) in_str.erase(0, 1);
    
    if (in_str.empty()) {
        hazel_finish_eval(ctx);
        return;
    }
    
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", in_str.c_str());
    char* eq = strchr(buf, '=');
    
    if (eq && !strchr(eq + 1, '=')) {
        *eq = '\0';
        char* name = buf;
        char* expr = eq + 1;
        while(isspace(*name)) name++;
        char* end = name + strlen(name) - 1;
        while(end > name && isspace(*end)) *end-- = '\0';
        
        ueval_result r = ueval_eval(&env, expr);
        if (r.status == UEVAL_OK) {
            ueval_bind(&env, name, r.value);
            char out[256];
            snprintf(out, sizeof(out), "%s = %g\n", name, r.value);
            hazel_append_output(ctx, out, 0);
        } else {
            char out[256];
            snprintf(out, sizeof(out), "Error code: %d\n", r.status);
            hazel_append_output(ctx, out, 1);
        }
    } else {
        ueval_result r = ueval_eval(&env, in_str.c_str());
        if (r.status == UEVAL_OK) {
            char out[256];
            snprintf(out, sizeof(out), "%g\n", r.value);
            hazel_append_output(ctx, out, 0);
        } else {
            char out[256];
            snprintf(out, sizeof(out), "Error code: %d\n", r.status);
            hazel_append_output(ctx, out, 1);
        }
    }
    
    hazel_finish_eval(ctx);
}

int main(int argc, char** argv) {
    ueval_init(&env);
    ueval_bind_f1(&env, "sin", sin);
    ueval_bind_f1(&env, "cos", cos);
    ueval_bind_f1(&env, "tan", tan);
    ueval_bind_f1(&env, "sqrt", sqrt);
    ueval_bind(&env, "pi", 3.14159265358979323846);

    hazel_app_t* app = hazel_create("Hazel + ueval Notebook", my_eval_callback, NULL);
    if (argc > 1) {
        hazel_load_file(app, argv[1]);
    }
    int ret = hazel_run(app);
    hazel_destroy(app);
    return ret;
}
