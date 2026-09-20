#pragma once
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Value_Input.H>
#include "hazel/hazel.h"
#include <string>
#include <FL/Fl_Color_Chooser.H>
#include <FL/fl_draw.H>

class ColorButton : public Fl_Button {
public:
    Fl_Color my_color;
    ColorButton(int x, int y, int w, int h, const char* l = 0) : Fl_Button(x, y, w, h, l) {}
    void draw() override {
        Fl_Button::draw();
        fl_color(my_color);
        fl_rectf(x() + 4, y() + 4, w() - 8, h() - 8);
        fl_color(FL_BLACK);
        fl_rect(x() + 4, y() + 4, w() - 8, h() - 8);
    }
};

class PreferencesWindow : public Fl_Double_Window {
    Fl_Hold_Browser* font_browser_;
    Fl_Choice* theme_choice_;
    Fl_Value_Input* size_input_;
    Fl_Value_Input* udp_port_input_;
    Fl_Value_Input* max_voices_input_;
    Fl_Value_Input* events_port_input_;
    Fl_Box* preview_;
    Fl_Button* ok_;
    Fl_Button* cancel_;
    
    Fl_Group* custom_group_;
    ColorButton* btn_fg_;
    ColorButton* btn_bg_;
    ColorButton* btn_out_;
    ColorButton* btn_err_;
    ColorButton* btn_md_;
    ColorButton* btn_err_fg_;
    ColorButton* btn_md_fg_;
    ColorButton* btn_csr_fg_;
    ColorButton* btn_csr_bg_;
    ColorButton* btn_sel_bg_;
    
    std::string selected_font_;
    hazel_config_t out_cfg_;
    int selected_theme_;
    int selected_size_;
    bool applied_ = false;

public:
    PreferencesWindow(const hazel_config_t& current_cfg) : Fl_Double_Window(420, 535, "Preferences") {
        out_cfg_ = current_cfg;
        const char* current_font_name = Fl::get_font_name(current_cfg.font);
        if (current_font_name) selected_font_ = current_font_name;
        new Fl_Box(10, 10, 400, 20, "Select Font:");
        font_browser_ = new Fl_Hold_Browser(10, 30, 400, 180);
        font_browser_->has_scrollbar(Fl_Browser_::BOTH);
        
        new Fl_Box(10, 220, 100, 25, "Theme:");
        theme_choice_ = new Fl_Choice(110, 220, 300, 25);
        theme_choice_->add("Hazel Light");
        theme_choice_->add("Synth Dark");
        theme_choice_->add("Custom");
        theme_choice_->value(0); // Default to light
        
        new Fl_Box(10, 255, 100, 25, "Size:");
        size_input_ = new Fl_Value_Input(110, 255, 100, 25);
        size_input_->step(1);
        size_input_->bounds(8, 72);
        size_input_->value(current_cfg.font_size);
        

        new Fl_Box(10, 290, 100, 25, "Max Voices:");
        max_voices_input_ = new Fl_Value_Input(110, 290, 80, 25);
        max_voices_input_->step(1);
        max_voices_input_->bounds(1, 128);
        max_voices_input_->value(current_cfg.max_voices > 0 ? current_cfg.max_voices : 8);

        new Fl_Box(10, 320, 100, 25, "UDP Port:");
        udp_port_input_ = new Fl_Value_Input(110, 320, 80, 25);
        udp_port_input_->step(1);
        udp_port_input_->value(current_cfg.udp_port);

        new Fl_Box(200, 320, 100, 25, "Events Port:");
        events_port_input_ = new Fl_Value_Input(310, 320, 80, 25);
        events_port_input_->step(1);
        events_port_input_->value(current_cfg.events_port);
        
        preview_ = new Fl_Box(10, 325, 400, 40, "⢀⣴⣾⣿⣿⣷⣦⡀ ⣾⣿ Braille Test");
        preview_->box(FL_DOWN_BOX);
        preview_->color(current_cfg.input_bg);
        preview_->labelcolor(current_cfg.text_fg);
        preview_->labelsize(current_cfg.font_size);
        
        custom_group_ = new Fl_Group(10, 375, 400, 100);
        Fl_Box* clbl = new Fl_Box(10, 400, 60, 20, "Colors:");
        clbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        // Row 1 (Foregrounds)
        Fl_Box* l1 = new Fl_Box(70, 380, 45, 15, "Text FG"); l1->labelsize(10);
        btn_fg_ = new ColorButton(70, 395, 45, 20); btn_fg_->my_color = current_cfg.text_fg;
        btn_fg_->tooltip("Text Foreground");
        
        Fl_Box* l6 = new Fl_Box(120, 380, 45, 15, "Err FG"); l6->labelsize(10);
        btn_err_fg_ = new ColorButton(120, 395, 45, 20); btn_err_fg_->my_color = current_cfg.error_fg;
        btn_err_fg_->tooltip("Error Text Foreground");
        
        Fl_Box* l7 = new Fl_Box(170, 380, 45, 15, "Note FG"); l7->labelsize(10);
        btn_md_fg_ = new ColorButton(170, 395, 45, 20); btn_md_fg_->my_color = current_cfg.markdown_fg;
        btn_md_fg_->tooltip("Note Text Foreground");

        Fl_Box* l8 = new Fl_Box(220, 380, 45, 15, "Csr FG"); l8->labelsize(10);
        btn_csr_fg_ = new ColorButton(220, 395, 45, 20); btn_csr_fg_->my_color = current_cfg.cursor_fg;
        btn_csr_fg_->tooltip("Cursor Foreground");
        
        // Row 2 (Backgrounds)
        Fl_Box* l2 = new Fl_Box(70, 425, 45, 15, "Code BG"); l2->labelsize(10);
        btn_bg_ = new ColorButton(70, 440, 45, 20); btn_bg_->my_color = current_cfg.input_bg;
        btn_bg_->tooltip("Code Block Background");
        
        Fl_Box* l4 = new Fl_Box(120, 425, 45, 15, "Err BG"); l4->labelsize(10);
        btn_err_ = new ColorButton(120, 440, 45, 20); btn_err_->my_color = current_cfg.error_bg;
        btn_err_->tooltip("Error Block Background");
        
        Fl_Box* l5 = new Fl_Box(170, 425, 45, 15, "Note BG"); l5->labelsize(10);
        btn_md_ = new ColorButton(170, 440, 45, 20); btn_md_->my_color = current_cfg.markdown_bg;
        btn_md_->tooltip("Note Block Background");
        
        Fl_Box* l9 = new Fl_Box(220, 425, 45, 15, "Csr BG"); l9->labelsize(10);
        btn_csr_bg_ = new ColorButton(220, 440, 45, 20); btn_csr_bg_->my_color = current_cfg.cursor_bg;
        btn_csr_bg_->tooltip("Cursor Background");
        
        Fl_Box* l3 = new Fl_Box(270, 425, 45, 15, "Out BG"); l3->labelsize(10);
        btn_out_ = new ColorButton(270, 440, 45, 20); btn_out_->my_color = current_cfg.output_bg;
        btn_out_->tooltip("Output Block Background");

        Fl_Box* l10 = new Fl_Box(320, 425, 45, 15, "Sel BG"); l10->labelsize(10);
        btn_sel_bg_ = new ColorButton(320, 440, 45, 20); btn_sel_bg_->my_color = current_cfg.select_bg;
        btn_sel_bg_->tooltip("Selection Background");
        
        custom_group_->end();
        
        auto color_cb = [](Fl_Widget* w, void* v) {
            ColorButton* btn = (ColorButton*)w;
            PreferencesWindow* self = (PreferencesWindow*)v;
            uchar r, g, b;
            Fl::get_color(btn->my_color, r, g, b);
            double dr = r/255.0, dg = g/255.0, db = b/255.0;
            if (fl_color_chooser("Pick Color", dr, dg, db)) {
                btn->my_color = fl_rgb_color(dr * 255.0, dg * 255.0, db * 255.0);
                btn->redraw();
                if (btn == self->btn_bg_) { self->preview_->color(btn->my_color); self->preview_->redraw(); }
                if (btn == self->btn_fg_) { self->preview_->labelcolor(btn->my_color); self->preview_->redraw(); }
            }
        };
        btn_fg_->callback(color_cb, this);
        btn_err_fg_->callback(color_cb, this);
        btn_md_fg_->callback(color_cb, this);
        btn_bg_->callback(color_cb, this);
        btn_out_->callback(color_cb, this);
        btn_err_->callback(color_cb, this);
        btn_md_->callback(color_cb, this);
        btn_csr_fg_->callback(color_cb, this);
        btn_csr_bg_->callback(color_cb, this);
        btn_sel_bg_->callback(color_cb, this);

        cancel_ = new Fl_Button(240, 490, 80, 30, "Cancel");
        ok_ = new Fl_Button(330, 490, 80, 30, "OK");
        
        int num_fonts = Fl::set_fonts("-*");
        int selected_idx = 1;
        for (int i = 0; i < num_fonts; i++) {
            const char* name = Fl::get_font_name((Fl_Font)i);
            if (name) {
                // Filter out names with non-ASCII characters to prevent tofu boxes
                bool is_ascii = true;
                for (int j = 0; name[j]; j++) {
                    if ((unsigned char)name[j] > 127) {
                        is_ascii = false;
                        break;
                    }
                }
                if (!is_ascii) continue;
                
                std::string lower_name = name;
                for (char& c : lower_name) c = tolower(c);
                if (lower_name.find("mono") == std::string::npos &&
                    lower_name.find("code") == std::string::npos &&
                    lower_name.find("console") == std::string::npos &&
                    lower_name.find("consolas") == std::string::npos &&
                    lower_name.find("courier") == std::string::npos &&
                    lower_name.find("term") == std::string::npos &&
                    lower_name.find("braille") == std::string::npos &&
                    lower_name.find("fixed") == std::string::npos &&
                    lower_name.find("hack") == std::string::npos &&
                    lower_name.find("menlo") == std::string::npos &&
                    lower_name.find("monaco") == std::string::npos &&
                    lower_name.find("inconsolata") == std::string::npos &&
                    lower_name.find("typewriter") == std::string::npos) {
                    continue;
                }

                std::string safe_name = name;
                size_t pos = 0;
                while ((pos = safe_name.find('@', pos)) != std::string::npos) {
                    safe_name.replace(pos, 1, "@@");
                    pos += 2;
                }
                font_browser_->add(safe_name.c_str(), (void*)(uintptr_t)i);
                if (current_font_name && strcmp(name, current_font_name) == 0) {
                    selected_idx = font_browser_->size();
                }
            }
        }
        font_browser_->select(selected_idx);
        if (selected_idx > 0) {
            Fl_Font f = (Fl_Font)(uintptr_t)font_browser_->data(selected_idx);
            preview_->labelfont(f);
        }
        
        theme_choice_->value(current_cfg.theme);
        custom_group_->show();
        
        theme_choice_->callback([](Fl_Widget*, void* v) {
            PreferencesWindow* self = (PreferencesWindow*)v;
            int t = self->theme_choice_->value();
            self->custom_group_->show(); // Always show color pickers
            
            if (t == 2) {
                bool is_light = (self->btn_bg_->my_color == FL_WHITE);
                bool is_dark = (self->btn_bg_->my_color == fl_rgb_color(25, 25, 30));
                if (is_light || is_dark) { // Seed Solarized
                    self->btn_fg_->my_color = fl_rgb_color(131, 148, 150);
                    self->btn_bg_->my_color = fl_rgb_color(0, 43, 54);
                    self->btn_out_->my_color = fl_rgb_color(7, 54, 66);
                    self->btn_err_->my_color = fl_rgb_color(220, 50, 47);
                    self->btn_md_->my_color = fl_rgb_color(7, 54, 66);
                    self->btn_err_fg_->my_color = fl_rgb_color(0, 43, 54);
                    self->btn_md_fg_->my_color = fl_rgb_color(147, 161, 161);
                    self->btn_csr_fg_->my_color = fl_rgb_color(0, 43, 54);
                    self->btn_csr_bg_->my_color = fl_rgb_color(181, 137, 0);
                    self->btn_sel_bg_->my_color = fl_rgb_color(7, 54, 66);
                    
                    self->btn_fg_->redraw(); self->btn_bg_->redraw(); self->btn_out_->redraw();
                    self->btn_err_->redraw(); self->btn_md_->redraw(); self->btn_err_fg_->redraw();
                    self->btn_md_fg_->redraw(); self->btn_csr_fg_->redraw(); self->btn_csr_bg_->redraw(); self->btn_sel_bg_->redraw();
                    self->preview_->color(self->btn_bg_->my_color);
                    self->preview_->labelcolor(self->btn_fg_->my_color);
                    self->preview_->redraw();
                }
            } else {
                if (t == 0) { // Light
                    self->btn_fg_->my_color = FL_BLACK;
                    self->btn_bg_->my_color = FL_WHITE;
                    self->btn_out_->my_color = fl_rgb_color(245, 245, 250);
                    self->btn_err_->my_color = fl_rgb_color(255, 235, 235);
                    self->btn_md_->my_color = fl_rgb_color(245, 255, 245);
                    self->btn_err_fg_->my_color = FL_DARK_RED;
                    self->btn_md_fg_->my_color = FL_DARK_GREEN;
                    self->btn_csr_fg_->my_color = FL_WHITE;
                    self->btn_csr_bg_->my_color = FL_BLACK;
                    self->btn_sel_bg_->my_color = fl_rgb_color(180, 200, 255);
                } else if (t == 1) { // Dark
                    self->btn_fg_->my_color = fl_rgb_color(220, 220, 220);
                    self->btn_bg_->my_color = fl_rgb_color(25, 25, 30);
                    self->btn_out_->my_color = fl_rgb_color(15, 15, 20);
                    self->btn_err_->my_color = fl_rgb_color(40, 10, 10);
                    self->btn_md_->my_color = fl_rgb_color(20, 30, 25);
                    self->btn_err_fg_->my_color = fl_rgb_color(255, 100, 100);
                    self->btn_md_fg_->my_color = fl_rgb_color(100, 255, 100);
                    self->btn_csr_fg_->my_color = fl_rgb_color(25, 25, 30);
                    self->btn_csr_bg_->my_color = fl_rgb_color(220, 220, 220);
                    self->btn_sel_bg_->my_color = fl_rgb_color(60, 60, 80);
                }
                self->btn_fg_->redraw(); self->btn_bg_->redraw(); self->btn_out_->redraw();
                self->btn_err_->redraw(); self->btn_md_->redraw(); self->btn_err_fg_->redraw();
                self->btn_md_fg_->redraw(); self->btn_csr_fg_->redraw(); self->btn_csr_bg_->redraw(); self->btn_sel_bg_->redraw();
                
                self->preview_->color(self->btn_bg_->my_color);
                self->preview_->labelcolor(self->btn_fg_->my_color);
                self->preview_->redraw();
            }
        }, this);
        
        font_browser_->callback([](Fl_Widget*, void* v) {
            PreferencesWindow* self = (PreferencesWindow*)v;
            int sel = self->font_browser_->value();
            if (sel > 0) {
                const char* txt = self->font_browser_->text(sel);
                if (txt) {
                    std::string out_name = txt;
                    size_t pos = 0;
                    while ((pos = out_name.find("@@", pos)) != std::string::npos) {
                        out_name.replace(pos, 2, "@");
                        pos += 1;
                    }
                    self->selected_font_ = out_name;
                    Fl_Font f = (Fl_Font)(uintptr_t)self->font_browser_->data(sel);
                    self->preview_->labelfont(f);
                    self->preview_->redraw();
                }
            }
        }, this);
        
        size_input_->callback([](Fl_Widget*, void* v) {
            PreferencesWindow* self = (PreferencesWindow*)v;
            self->preview_->labelsize((int)self->size_input_->value());
            self->preview_->redraw();
        }, this);
        
        ok_->callback([](Fl_Widget*, void* v) {
            PreferencesWindow* self = (PreferencesWindow*)v;
            self->selected_theme_ = self->theme_choice_->value();
            self->selected_size_ = (int)self->size_input_->value();
            self->out_cfg_.udp_port = (int)self->udp_port_input_->value();
            self->out_cfg_.max_voices = (int)self->max_voices_input_->value();
            self->out_cfg_.events_port = (int)self->events_port_input_->value();
            self->out_cfg_.text_fg = self->btn_fg_->my_color;
            self->out_cfg_.error_fg = self->btn_err_fg_->my_color;
            self->out_cfg_.markdown_fg = self->btn_md_fg_->my_color;
            self->out_cfg_.cursor_fg = self->btn_csr_fg_->my_color;
            self->out_cfg_.cursor_bg = self->btn_csr_bg_->my_color;
            self->out_cfg_.select_bg = self->btn_sel_bg_->my_color;
            self->out_cfg_.input_bg = self->btn_bg_->my_color;
            self->out_cfg_.output_bg = self->btn_out_->my_color;
            self->out_cfg_.error_bg = self->btn_err_->my_color;
            self->out_cfg_.markdown_bg = self->btn_md_->my_color;
            self->applied_ = true;
            self->hide();
        }, this);
        
        cancel_->callback([](Fl_Widget*, void* v) {
            ((Fl_Window*)v)->hide();
        }, this);
        
        end();
    }
    
    bool run(std::string& out_font, int& out_theme, int& out_size, hazel_config_t& out_custom_cfg) {
        applied_ = false;
        show();
        while (shown()) Fl::wait();
        if (applied_) {
            out_font = selected_font_;
            out_theme = selected_theme_;
            out_size = selected_size_;
            out_custom_cfg = out_cfg_;
            return true;
        }
        return false;
    }
};
