import re
with open("include/hazel/hazel.h", "r") as f:
    text = f.read()

target = """typedef struct hazel_app_t hazel_app_t;"""
replace = """typedef struct hazel_app_t hazel_app_t;

typedef struct {
    int font;
    int font_size;
    unsigned int input_bg;
    unsigned int output_bg;
    unsigned int error_bg;
    unsigned int markdown_bg;
    unsigned int text_fg;
} hazel_config_t;

void hazel_set_config(hazel_app_t* app, const hazel_config_t* config);"""
text = text.replace(target, replace)
with open("include/hazel/hazel.h", "w") as f:
    f.write(text)

with open("src/HazelApp.h", "r") as f:
    text_h = f.read()

target_h = """    static Fl_Text_Display::Style_Table_Entry styletable[];"""
replace_h = """    Fl_Text_Display::Style_Table_Entry styletable_[4];
    hazel_config_t config_;
    void applyConfig();"""
text_h = text_h.replace(target_h, replace_h)

target_h_public = """    void tryQuit();"""
replace_h_public = """    void tryQuit();
    void setConfig(const hazel_config_t* config);"""
text_h = text_h.replace(target_h_public, replace_h_public)
with open("src/HazelApp.h", "w") as f:
    f.write(text_h)

with open("src/HazelApp.cpp", "r") as f:
    text_cpp = f.read()

target_cpp_array = """Fl_Text_Display::Style_Table_Entry HazelApp::styletable[] = {
    { FL_BLACK,      FL_COURIER,        14, Fl_Text_Display::ATTR_BGCOLOR_EXT, FL_WHITE }, // A - Input
    { FL_BLACK,      FL_COURIER,        14, Fl_Text_Display::ATTR_BGCOLOR_EXT, fl_rgb_color(240, 240, 245) }, // B - Output
    { FL_DARK_RED,   FL_COURIER_BOLD,   14, Fl_Text_Display::ATTR_BGCOLOR_EXT, fl_rgb_color(255, 230, 230) }, // C - Error
    { FL_DARK_GREEN, FL_COURIER_ITALIC, 14, Fl_Text_Display::ATTR_BGCOLOR_EXT, fl_rgb_color(240, 255, 240) }  // D - Meta
};"""
replace_cpp_array = ""
text_cpp = text_cpp.replace(target_cpp_array, replace_cpp_array)

target_cpp_ctor = """    buffer_ = new Fl_Text_Buffer();
    style_buffer_ = new Fl_Text_Buffer();
    
    editor_ = new HazelEditor(0, 0, 800, 575, this);
    editor_->buffer(buffer_);
    editor_->box(FL_FLAT_BOX);
    editor_->highlight_data(style_buffer_, styletable, sizeof(styletable)/sizeof(styletable[0]), 'A', 0, 0);"""
replace_cpp_ctor = """    buffer_ = new Fl_Text_Buffer();
    style_buffer_ = new Fl_Text_Buffer();
    
    config_.font = FL_COURIER;
    config_.font_size = 14;
    config_.input_bg = FL_WHITE;
    config_.output_bg = fl_rgb_color(240, 240, 245);
    config_.error_bg = fl_rgb_color(255, 230, 230);
    config_.markdown_bg = fl_rgb_color(240, 255, 240);
    config_.text_fg = FL_BLACK;
    
    applyConfig();
    
    editor_ = new HazelEditor(0, 0, 800, 575, this);
    editor_->buffer(buffer_);
    editor_->box(FL_FLAT_BOX);
    editor_->highlight_data(style_buffer_, styletable_, 4, 'A', 0, 0);"""
text_cpp = text_cpp.replace(target_cpp_ctor, replace_cpp_ctor)

target_cpp_draw1 = """if (p == 'D') fl_color(fl_rgb_color(240, 255, 240));
                else if (p == 'A') fl_color(FL_WHITE);"""
replace_cpp_draw1 = """if (p == 'D') fl_color(app_->config_.markdown_bg);
                else if (p == 'A') fl_color(app_->config_.input_bg);
                else if (p == 'B') fl_color(app_->config_.output_bg);
                else if (p == 'C') fl_color(app_->config_.error_bg);"""
text_cpp = text_cpp.replace(target_cpp_draw1, replace_cpp_draw1)

target_cpp_draw2 = """                if (p == 'D') fl_color(fl_rgb_color(240, 255, 240));
                else if (p == 'B') fl_color(fl_rgb_color(240, 240, 245));
                else if (p == 'C') fl_color(fl_rgb_color(255, 230, 230));"""
replace_cpp_draw2 = """                if (p == 'D') fl_color(app_->config_.markdown_bg);
                else if (p == 'B') fl_color(app_->config_.output_bg);
                else if (p == 'C') fl_color(app_->config_.error_bg);
                else if (p == 'A') fl_color(app_->config_.input_bg);"""
text_cpp = text_cpp.replace(target_cpp_draw2, replace_cpp_draw2)

new_methods = """
void HazelApp::applyConfig() {
    styletable_[0] = { (Fl_Color)config_.text_fg, config_.font, config_.font_size, Fl_Text_Display::ATTR_BGCOLOR_EXT, (Fl_Color)config_.input_bg };
    styletable_[1] = { (Fl_Color)config_.text_fg, config_.font, config_.font_size, Fl_Text_Display::ATTR_BGCOLOR_EXT, (Fl_Color)config_.output_bg };
    styletable_[2] = { FL_DARK_RED, config_.font | FL_BOLD, config_.font_size, Fl_Text_Display::ATTR_BGCOLOR_EXT, (Fl_Color)config_.error_bg };
    styletable_[3] = { FL_DARK_GREEN, config_.font | FL_ITALIC, config_.font_size, Fl_Text_Display::ATTR_BGCOLOR_EXT, (Fl_Color)config_.markdown_bg };
}

void HazelApp::setConfig(const hazel_config_t* config) {
    if (!config) return;
    config_ = *config;
    applyConfig();
    if (editor_) {
        editor_->highlight_data(style_buffer_, styletable_, 4, 'A', 0, 0);
        editor_->textfont(config_.font);
        editor_->textsize(config_.font_size);
        editor_->redraw();
    }
}
"""
text_cpp += new_methods
with open("src/HazelApp.cpp", "w") as f:
    f.write(text_cpp)

with open("src/api.cpp", "r") as f:
    text_api = f.read()

text_api += """
void hazel_set_config(hazel_app_t* app, const hazel_config_t* config) {
    if (app && app->app && config) {
        app->app->setConfig(config);
    }
}
"""
with open("src/api.cpp", "w") as f:
    f.write(text_api)
