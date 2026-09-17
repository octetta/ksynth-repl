import re
with open("src/api.cpp", "r") as f:
    text = f.read()

target = """void hazel_set_config(hazel_app_t* app, const hazel_config_t* config) {
    if (app && app->app && config) {
        app->app->setConfig(config);
    }
}"""
replace = """void hazel_set_config(hazel_app_t* app, const hazel_config_t* config) {
    if (app && config) {
        ((HazelApp*)app)->setConfig(config);
    }
}"""
text = text.replace(target, replace)
with open("src/api.cpp", "w") as f:
    f.write(text)
