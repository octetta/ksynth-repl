#pragma once
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <string>

class SimpleFontPicker : public Fl_Double_Window {
    Fl_Hold_Browser* browser_;
    Fl_Box* preview_;
    Fl_Button* ok_;
    std::string selected_font_;
public:
    SimpleFontPicker() : Fl_Double_Window(400, 300, "Select Font") {
        browser_ = new Fl_Hold_Browser(10, 10, 380, 200);
        preview_ = new Fl_Box(10, 220, 380, 40, "⢀⣴⣾⣿⣿⣷⣦⡀ ⣾⣿ Braille Test");
        preview_->box(FL_DOWN_BOX);
        preview_->color(FL_WHITE);
        ok_ = new Fl_Button(310, 265, 80, 30, "OK");
        
        int num_fonts = Fl::set_fonts("-*");
        for (int i = 0; i < num_fonts; i++) {
            const char* name = Fl::get_font_name((Fl_Font)i);
            if (name) browser_->add(name);
        }
        
        browser_->callback([](Fl_Widget* w, void* v) {
            SimpleFontPicker* self = (SimpleFontPicker*)v;
            int sel = self->browser_->value();
            if (sel > 0) {
                const char* txt = self->browser_->text(sel);
                if (txt) {
                    self->selected_font_ = txt;
                    self->preview_->labelfont(FL_FREE_FONT);
                    Fl::set_font(FL_FREE_FONT, txt);
                    self->preview_->redraw();
                }
            }
        }, this);
        
        ok_->callback([](Fl_Widget*, void* v) {
            ((Fl_Window*)v)->hide();
        }, this);
        end();
    }
    
    std::string getSelectedFont() {
        show();
        while (shown()) Fl::wait();
        return selected_font_;
    }
};
