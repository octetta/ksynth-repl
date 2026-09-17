#include "HazelApp.h"
#include "TerminalPane.h"
#include "Preferences.h"
#include <FL/Fl.H>
#include <FL/Fl_File_Chooser.H>
#include <iostream>
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Item.H>

#include <FL/fl_draw.H>

class MacroBar : public Fl_Group {
    HazelApp* app_;
public:
    MacroBar(int X, int Y, int W, int H, HazelApp* app) : Fl_Group(X, Y, W, H), app_(app) {
        int btn_w = 22;
        int gap = 4;
        int start_x = X + 10;
        
        Fl_Box* lbl = new Fl_Box(start_x, Y + 5, 60, 20, "MACROS:");
        lbl->labelsize(11);
        lbl->labelfont(FL_HELVETICA_BOLD);
        lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        start_x += 60 + gap;
        
        for (int i = 0; i < 26; i++) {
            char name[2] = {(char)('A' + i), '\0'};
            Fl_Button* btn = new Fl_Button(start_x, Y + 5, btn_w, 20);
            btn->copy_label(name);
            btn->labelsize(11);
            btn->box(FL_THIN_UP_BOX);
            btn->color(fl_rgb_color(205, 205, 215));
            btn->selection_color(FL_DARK2);
            btn->callback([](Fl_Widget* w, void* v) {
                HazelApp* app = (HazelApp*)v;
                const char* lbl = w->label();
                char cmd[16];
                snprintf(cmd, sizeof(cmd), "_MC%s", lbl);
                hazel_ctx_t* term_ctx = new hazel_ctx_t();
                memset(term_ctx, 0, sizeof(hazel_ctx_t));
                term_ctx->app = app;
                term_ctx->is_terminal = true;
                term_ctx->at_bottom = true;
                app->evaluateCommand(cmd, term_ctx);
            }, app);
            start_x += btn_w + gap;
        }
        end();
    }
};

class StatusBar : public Fl_Box {
    HazelApp* app_;
public:
    StatusBar(int X, int Y, int W, int H, HazelApp* app) : Fl_Box(X, Y, W, H, ""), app_(app) {
        align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        box(FL_FLAT_BOX);
    }
    
    void drawKeyLeft(int& cur_x, int cur_y, const char* mod, const char* key, const char* label) {
        fl_font(FL_HELVETICA, 12);
        int lbl_w = 0, lbl_h = 0;
        fl_measure(label, lbl_w, lbl_h);
        
        fl_font(FL_HELVETICA_BOLD, 10);
        int mod_w = 0, mod_h = 0;
        fl_measure(mod, mod_w, mod_h);
        mod_w += 10;
        
        int key_w = 0, key_h = 0;
        fl_measure(key, key_w, key_h);
        key_w += 10;
        
        int total_w = mod_w + 2 + key_w + 6 + lbl_w + 12;
        int draw_x = cur_x;
        cur_x += total_w; // Advance rightwards
        
        // Mod (e.g. CTRL)
        fl_color(FL_BLACK);
        fl_rectf(draw_x + 1, cur_y + 5, mod_w - 2, 14); // Rounded effect approximation
        fl_rectf(draw_x, cur_y + 6, mod_w, 12);
        fl_color(FL_WHITE);
        fl_draw(mod, draw_x + 5, cur_y + 16);
        draw_x += mod_w + 2;
        
        // Key
        fl_color(FL_BLACK);
        fl_rectf(draw_x + 1, cur_y + 5, key_w - 2, 14);
        fl_rectf(draw_x, cur_y + 6, key_w, 12);
        fl_color(FL_WHITE);
        fl_draw(key, draw_x + 5, cur_y + 16);
        draw_x += key_w + 6;
        
        // Label
        fl_font(FL_HELVETICA, 12);
        fl_color(FL_DARK3);
        fl_draw(label, draw_x, cur_y + 17);
    }
    
    void draw() override {
        Fl_Box::draw();
        
        int cur_x = x() + 10;
        int cur_y = y();
        
        // Draw the combined status info (filename + cursor + mode) on the BOTTOM line
        fl_font(FL_HELVETICA_BOLD, 12);
        fl_color(FL_DARK3);
        fl_draw(app_->status_info_.c_str(), cur_x, cur_y + 36);

        if (!app_->extra_status_.empty()) {
            int ext_w = 0, ext_h = 0;
            fl_measure(app_->extra_status_.c_str(), ext_w, ext_h);
            fl_draw(app_->extra_status_.c_str(), x() + w() - ext_w - 20, cur_y + 36);
        }
        
        // Draw keys starting from the far left edge!
        int left_x = x() + 10;
        
        #ifdef __APPLE__
        const char* cmd = "CMD";
        #else
        const char* cmd = "CTRL";
        #endif
        
        if (app_->getConfig().parser_mode == 0) {
            drawKeyLeft(left_x, cur_y, cmd, "U", "Mkdn");
            drawKeyLeft(left_x, cur_y, cmd, "Y", "Code");
        }
        drawKeyLeft(left_x, cur_y, cmd, "~", "Terminal");
        drawKeyLeft(left_x, cur_y, cmd, "D", "Delete");
        if (app_->getConfig().parser_mode == 0) {
            drawKeyLeft(left_x, cur_y, "ALT", "RET", "Split");
        }
        drawKeyLeft(left_x, cur_y, cmd, "RET", "Eval");
        drawKeyLeft(left_x, cur_y, cmd, "R", "RunAll");
        drawKeyLeft(left_x, cur_y, cmd, "/", "Help");
    }
};

class Splitter : public Fl_Box {
    HazelApp* app_;
    int drag_start_y_;
    int drag_start_term_h_;
public:
    Splitter(int X, int Y, int W, int H, HazelApp* app) : Fl_Box(X, Y, W, H), app_(app) {
        box(FL_FLAT_BOX);
        color(fl_rgb_color(200, 200, 200));
    }
    int handle(int event) override {
        if (event == FL_ENTER) {
            window()->cursor(FL_CURSOR_NS);
            return 1;
        } else if (event == FL_LEAVE) {
            window()->cursor(FL_CURSOR_DEFAULT);
            return 1;
        } else if (event == FL_PUSH) {
            drag_start_y_ = Fl::event_y();
            drag_start_term_h_ = app_->getTerminalHeight();
            return 1;
        } else if (event == FL_DRAG) {
            int dy = Fl::event_y() - drag_start_y_;
            app_->setTerminalHeight(drag_start_term_h_ - dy);
            return 1;
        } else if (event == FL_RELEASE) {
            window()->cursor(FL_CURSOR_DEFAULT);
            return 1;
        }
        return Fl_Box::handle(event);
    }
};

#include <string.h>




#include <FL/Fl_Help_View.H>

// --- HelpWindow & HTML Documentation ---
// HOW TO TWEAK THE HELP SYSTEM:
// The HelpWindow uses FLTK's Fl_Help_View widget, which acts like a tiny browser.
// It supports basic HTML tags (<b>, <i>, <h1>-<h6>, <p>, <table>, <tr>, <td>, <a>).
// It DOES NOT support modern CSS, <div> styling, <style>, or JavaScript.
// 
// When the user clicks an <a href="help://..."> link, FLTK triggers `my_link_cb`.
// We intercept this click, extract the URI, and ask the host app (via `help_cb_`) 
// to provide a new HTML string. We then replace the widget's contents.
// This allows host apps (like skred-repl) to provide interactive, multi-page help documentation.
class HelpWindow : public Fl_Double_Window {
public:
    
    static const char* my_link_cb(Fl_Widget* w, const char* uri) {
        HelpWindow* hw = (HelpWindow*)w->window();
        if (strncmp(uri, "help://", 7) == 0 && hw->help_cb_) {
            const char* html = hw->help_cb_(uri + 7);
            hw->update_ext_html(html);
            return NULL; // Handled
        }
        return uri;
    }

    const char* (*help_cb_)(const char*);
    std::string base_help_text_;
    Fl_Help_View* disp_;

    void update_ext_html(const char* ext_html) {
        std::string text = base_help_text_;
        if (ext_html) {
            text += ext_html;
        }
        disp_->value(text.c_str());
    }

    HelpWindow(const std::string& app_title, const std::string& app_version, bool is_skred_mode, const char* ext_html, const char* (*help_cb)(const char*)) : Fl_Double_Window(600, 700, "Help & About") {
        help_cb_ = help_cb;
        this->color(fl_rgb_color(245, 245, 250));
        
        std::string disp_title = app_title.empty() ? "Hazel Editor" : app_title;
        Fl_Box* title = new Fl_Box(20, 10, 560, 30);
        title->copy_label(disp_title.c_str());
        title->labelsize(20);
        title->labelfont(FL_HELVETICA_BOLD);
        title->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
        
        std::string disp_ver = app_version.empty() ? std::string("Version " HAZEL_VERSION) : (app_version + "<br>(Powered by Hazel " HAZEL_VERSION ")");
        // Convert newlines in app_version to <br> for HTML display
        size_t pos = 0;
        while ((pos = disp_ver.find("\n", pos)) != std::string::npos) {
            disp_ver.replace(pos, 1, "<br>");
            pos += 4;
        }

        // Use Fl_Help_View for the version too to support HTML and prevent clipping
        Fl_Help_View* ver = new Fl_Help_View(20, 45, 560, 60);
        ver->box(FL_FLAT_BOX);
        ver->color(this->color());
        ver->textfont(FL_HELVETICA);
        ver->textsize(12);
        ver->value(("<div align='center'>" + disp_ver + "</div>").c_str());
        
        disp_ = new Fl_Help_View(20, 110, 560, 520);
        disp_->box(FL_FLAT_BOX);
        disp_->color(this->color());
        disp_->textfont(FL_HELVETICA);
        disp_->textsize(13);
        
        base_help_text_ = 
            "<h3 align='center'>Keyboard Shortcuts</h3>"
            "<table width='100%' border='0' cellpadding='4'>"
            "<tr><td align='right' width='45%'><b>Cmd/Ctrl + S</b></td><td>Save File</td></tr>"
            "<tr><td align='right'><b>Cmd/Ctrl + Shift + S</b></td><td>Save As...</td></tr>"
            "<tr><td align='right'><b>Cmd/Ctrl + O</b></td><td>Open File</td></tr>"
            "<tr><td align='right'><b>Cmd/Ctrl + Shift + O</b></td><td>Open Folder (CWD)</td></tr>"
            "<tr><td align='right'><b>Cmd/Ctrl + ,</b></td><td>Preferences</td></tr>"
            "<tr><td align='right'><b>Cmd/Ctrl + /</b></td><td>Help / About</td></tr>"
            "<tr><td colspan='2'> </td></tr>"
            "<tr><td align='right'><b>Cmd/Ctrl + = / - / 0</b></td><td>Zoom In / Out / Reset</td></tr>"
            "<tr><td colspan='2'> </td></tr>"
            "<tr><td align='right'><b>Cmd/Ctrl + Enter</b></td><td>Evaluate Current Block</td></tr>"
            "<tr><td align='right'><b>Cmd/Ctrl + R</b></td><td>Run All Blocks</td></tr>"
            "<tr><td align='right'><b>Cmd/Ctrl + D</b></td><td>Delete Current Block</td></tr>"
            "<tr><td align='right'><b>Cmd/Ctrl + Up / Down</b></td><td>Move Cell</td></tr>"
            "<tr><td align='right'><b>Alt + A..Z</b></td><td>Trigger Macros A-Z</td></tr>"
            "<tr><td colspan='2'> </td></tr>"
            "<tr><td align='right'><b>Ctrl + `</b></td><td>Toggle Terminal Panel</td></tr>"
            "<tr><td align='right'><b>Ctrl + Tab / Down</b></td><td>Focus Terminal</td></tr>"
            "<tr><td align='right'><b>Ctrl + Tab / Up</b></td><td>Focus Editor</td></tr>"
            "</table>";
            
        if (is_skred_mode) {
            base_help_text_ += 
                "<h3 align='center'>Skred-REPL Mode</h3>"
                "<table width='100%' border='0' cellpadding='4'>"
                "<tr><td align='right' width='45%'><b>##</b></td><td>starts a Comment Block</td></tr>"
                "<tr><td align='right'><b>#</b></td><td>continues a Comment Block</td></tr>"
                "<tr><td align='right'><b><i>(normal line)</i></b></td><td>starts a Code Block</td></tr>"
                "<tr><td align='right'><b>Alt + A-Z</b></td><td>Run Macro _MCA to _MCZ</td></tr>"
                "</table>";
        } else {
            base_help_text_ += 
                "<h3 align='center'>Notebook Mode</h3>"
                "<table width='100%' border='0' cellpadding='4'>"
                "<tr><td align='right' width='45%'><b>Cmd/Ctrl + U</b></td><td>Convert to Markdown</td></tr>"
                "<tr><td align='right'><b>Cmd/Ctrl + Y</b></td><td>Convert to Code</td></tr>"
                "<tr><td align='right'><b>Alt + Enter</b></td><td>Split Cell</td></tr>"
                "</table>";
        }
        
        disp_->link(my_link_cb);
        if (help_cb_ && !ext_html) ext_html = help_cb_("");
        update_ext_html(ext_html);

        
        Fl_Button* close_btn = new Fl_Button(260, 650, 80, 30, "Close");
        close_btn->callback([](Fl_Widget*, void* v) {
            ((Fl_Window*)v)->hide();
        }, this);
        
        this->end();
    }
};
static void style_update_cb(int pos, int nInserted, int nDeleted, int nRestyled, const char* deletedText, void* cbArg) {
    HazelApp* app = (HazelApp*)cbArg;
    Fl_Text_Buffer* style_buf = app->getStyleBuffer();
    
    if (nInserted > 0 || nDeleted > 0) {
        app->setDirty(true);
        int current = app->getHighestModifiedPos();
        if (current == -1 || pos < current) {
            app->setHighestModifiedPos(pos);
        }
    }
    
    if (app->getConfig().parser_mode > 0) {
        if (nInserted > 0) {
            std::string styles(nInserted, 'A');
            style_buf->replace(pos, pos + nDeleted, styles.c_str());
        } else if (nDeleted > 0) {
            style_buf->remove(pos, pos + nDeleted);
        }
        
        Fl_Text_Buffer* buffer = app->getBuffer();
        int length = buffer->length();
        char* new_styles = (char*)malloc(length + 1);
        if (!new_styles) return;
        
        bool in_comment = false;
        int i = 0;
        while (i < length) {
            int line_start = buffer->line_start(i);
            int line_end = buffer->line_end(i);
            
            bool starts_with_hash_hash = false;
            bool starts_with_hash = false;
            
            int len = line_end - line_start;
            if (app->getConfig().parser_mode == 2) {
                if (len >= 2 && buffer->char_at(line_start) == '/' && buffer->char_at(line_start + 1) == '/') {
                    starts_with_hash_hash = true;
                } else if (len >= 1 && buffer->char_at(line_start) == '/') {
                    starts_with_hash = true;
                }
            } else {
                if (len >= 2 && buffer->char_at(line_start) == '#' && buffer->char_at(line_start + 1) == '#') {
                    starts_with_hash_hash = true;
                } else if (len >= 1 && buffer->char_at(line_start) == '#') {
                    starts_with_hash = true;
                }
            }
            
            if (starts_with_hash_hash) {
                in_comment = true;
            } else if (!starts_with_hash) {
                in_comment = false;
            }
            
            for (int j = line_start; j <= line_end && j < length; j++) {
                char current_s = style_buf->char_at(j);
                if (current_s == 'B' || current_s == 'C') {
                    new_styles[j] = current_s;
                    while (j < length && (style_buf->char_at(j) == 'B' || style_buf->char_at(j) == 'C')) {
                        new_styles[j] = style_buf->char_at(j);
                        j++;
                    }
                    j--; 
                    in_comment = false;
                } else {
                    new_styles[j] = in_comment ? 'D' : 'A';
                }
            }
            i = line_end + 1;
        }
        new_styles[length] = '\0';
        
        bool changed = false;
        for (int j = 0; j < length; j++) {
            if (style_buf->char_at(j) != new_styles[j]) {
                changed = true;
                break;
            }
        }
        
        if (changed) {
            style_buf->replace(0, length, new_styles);
        }
        free(new_styles);
        return;
    }
    
    if (nInserted > 0) {
        char target_style = 'A';
        char prev = (pos > 0) ? app->getStyleAt(pos - 1) : '\0';
        char curr = app->getStyleAt(pos);
        
        if (app->getPendingStyle() != 0) {
            target_style = app->getPendingStyle();
            app->setPendingStyle(0);
        } else {
            if (curr == 'A' || curr == 'D') target_style = curr;
            else if (prev == 'A' || prev == 'D') target_style = prev;
        }
        
        std::string styles(nInserted, target_style);
        style_buf->replace(pos, pos + nDeleted, styles.c_str());
    } else if (nDeleted > 0) {
        style_buf->remove(pos, pos + nDeleted);
    }
}

HazelEditor::HazelEditor(int x, int y, int w, int h, HazelApp* app) 
    : Fl_Text_Editor(x, y, w, h), app_(app) {
    linenumber_width(40);
    linenumber_fgcolor(FL_DARK3);
    linenumber_bgcolor(FL_LIGHT2);
}

int HazelEditor::handle(int event) {
    if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE) {
        this->take_focus();
        Fl_Menu_Item rclick_menu[] = {
            { "Copy Cell", 0, 0, 0, 0 },
            { "Cut",   0, 0, 0, 0 },
            { "Copy",  0, 0, 0, 0 },
            { "Paste", 0, 0, 0, 0 },
            { "Delete",0, 0, 0, 0 },
            { 0 }
        };
        const Fl_Menu_Item *m = rclick_menu->popup(Fl::event_x(), Fl::event_y(), 0, 0, 0);
        if (m) {
            if (strcmp(m->label(), "Copy Cell") == 0) {
                int pos = insert_position();
                char style = app_->getStyleAt(pos);
                if (style != 0) {
                    int start = pos;
                    while (start > 0 && app_->getStyleAt(start - 1) == style) start--;
                    int end = pos;
                    while (end < buffer()->length() - 1 && app_->getStyleAt(end + 1) == style) end++;
                    if (end < buffer()->length()) end++;
                    char* text = buffer()->text_range(start, end);
                    Fl::copy(text, strlen(text), 1); // 1 = clipboard
                    free(text);
                }
            } else if (strcmp(m->label(), "Cut") == 0) {
                Fl_Text_Editor::kf_cut(0, this);
            } else if (strcmp(m->label(), "Copy") == 0) {
                Fl_Text_Editor::kf_copy(0, this);
            } else if (strcmp(m->label(), "Paste") == 0) {
                Fl_Text_Editor::kf_paste(0, this);
            } else if (strcmp(m->label(), "Delete") == 0) {
                buffer()->remove_selection();
            }
        }
        return 1;
    }

    if (event == FL_KEYBOARD) {
        if (Fl::event_key() == FL_Up || Fl::event_key() == FL_Down || Fl::event_key() == FL_Left || Fl::event_key() == FL_Right) {
            app_->setPendingStyle(0);
        }
        int key = Fl::event_key();
        
        if (key == 'q' && (Fl::event_state() & FL_COMMAND)) {
            app_->tryQuit();
            return 1;
        } else if (key == 'r' && (Fl::event_state() & FL_COMMAND)) {
            app_->startRunAll();
            return 1;
        } else if (key == 's' && (Fl::event_state() & FL_COMMAND)) {
            if (Fl::event_state() & FL_SHIFT) {
                app_->promptSaveAs();
            } else {
                app_->saveFile();
            }
            return 1;
        } else if (key == 'o' && (Fl::event_state() & FL_COMMAND)) {
            if (Fl::event_state() & FL_SHIFT) {
                app_->openDirectory();
            } else {
                app_->openFile();
            }
            return 1;
        } else if (key == 'd' && (Fl::event_state() & FL_COMMAND)) {
            int pos = insert_position();
            char style = app_->getStyleAt(pos);
            if (style != 0) {
                int start = pos;
                while (start > 0 && app_->getStyleAt(start - 1) == style) start--;
                int end = pos;
                while (end < buffer()->length() - 1 && app_->getStyleAt(end + 1) == style) end++;
                if (end < buffer()->length()) end++;
                
                buffer()->remove_modify_callback(style_update_cb, app_);
                buffer()->remove(start, end);
                app_->getStyleBuffer()->remove(start, end);
                buffer()->add_modify_callback(style_update_cb, app_);
                
                if (start < buffer()->length()) insert_position(start);
                else if (start > 0) insert_position(start - 1);
                show_insert_position();
            }
            return 1;
        }
        
        // Convert to Markdown
        if (app_->getConfig().parser_mode == 0 && key == 'u' && (Fl::event_state() & FL_COMMAND)) {
            int pos = insert_position();
            char style = app_->getStyleAt(pos);
            if (style != 'A' && pos > 0 && app_->getStyleAt(pos - 1) == 'A' && buffer()->char_at(pos - 1) != '\n') {
                pos = pos - 1;
                style = 'A';
            }
            if (buffer()->selected()) {
                int start, end;
                buffer()->selection_position(&start, &end);
                std::string new_styles(end - start, 'D');
                app_->getStyleBuffer()->replace(start, end, new_styles.c_str());
                this->redisplay_range(start, end);
            } else if (style == 'A') {
                int start = pos;
                while (start > 0 && app_->getStyleAt(start - 1) == style) start--;
                int end = pos;
                while (end < buffer()->length() - 1 && app_->getStyleAt(end + 1) == style) end++;
                if (end < buffer()->length()) end++;
                std::string new_styles(end - start, 'D');
                app_->getStyleBuffer()->replace(start, end, new_styles.c_str());
                this->redisplay_range(start, end);
            } else {
                app_->setPendingStyle('D');
                this->redraw();
            }
            return 1;
        }
        
        // Convert to Code
        if (app_->getConfig().parser_mode == 0 && key == 'y' && (Fl::event_state() & FL_COMMAND)) {
            int pos = insert_position();
            char style = app_->getStyleAt(pos);
            if (style != 'D' && pos > 0 && app_->getStyleAt(pos - 1) == 'D' && buffer()->char_at(pos - 1) != '\n') {
                pos = pos - 1;
                style = 'D';
            }
            if (buffer()->selected()) {
                int start, end;
                buffer()->selection_position(&start, &end);
                std::string new_styles(end - start, 'A');
                app_->getStyleBuffer()->replace(start, end, new_styles.c_str());
                this->redisplay_range(start, end);
            } else if (style == 'D') {
                int start = pos;
                while (start > 0 && app_->getStyleAt(start - 1) == style) start--;
                int end = pos;
                while (end < buffer()->length() - 1 && app_->getStyleAt(end + 1) == style) end++;
                if (end < buffer()->length()) end++;
                std::string new_styles(end - start, 'A');
                app_->getStyleBuffer()->replace(start, end, new_styles.c_str());
                this->redisplay_range(start, end);
            } else {
                app_->setPendingStyle('A');
                this->redraw();
            }
            return 1;
        }
        
        // Alt+A to Alt+Z Macros
        if (key >= 'a' && key <= 'z' && (Fl::event_state() & FL_ALT)) {
            char lbl[2] = {(char)('A' + (key - 'a')), '\0'};
            char cmd[16];
            snprintf(cmd, sizeof(cmd), "_MC%s", lbl);
            hazel_ctx_t* term_ctx = new hazel_ctx_t();
            memset(term_ctx, 0, sizeof(hazel_ctx_t));
            term_ctx->app = app_;
            term_ctx->is_terminal = true;
            term_ctx->at_bottom = true;
            app_->evaluateCommand(cmd, term_ctx);
            return 1;
        }

        // Switch Focus (Ctrl+Tab or Ctrl+Down)
        if ((key == FL_Tab && (Fl::event_state() & FL_CTRL)) || (key == FL_Down && (Fl::event_state() & FL_CTRL))) {
            if (app_->getTerminal()->visible()) {
                app_->getTerminal()->take_focus();
            }
            return 1;
        }

        // Split Cell (Alt+Enter)
        if (app_->getConfig().parser_mode == 0 && (key == FL_Enter || key == FL_KP_Enter) && (Fl::event_state() & FL_ALT)) {
            int pos = insert_position();
            char current_style = app_->getStyleAt(pos);
            if (current_style == 'C' || current_style == 'B') return 1; // Don't split output
            
            char new_style = (current_style == 'A') ? 'D' : 'A';
            
            buffer()->remove_modify_callback(style_update_cb, app_);
            buffer()->insert(pos, "\n\n");
            
            std::string s(2, new_style);
            app_->getStyleBuffer()->insert(pos, s.c_str());
            
            buffer()->add_modify_callback(style_update_cb, app_);
            
            insert_position(pos + 1);
            show_insert_position();
            return 1;
        }
        
        // Help / About
        if (key == '/' && (Fl::event_state() & FL_COMMAND)) {
            app_->showHelpWindow();
            return 1;
        }

        // Zoom In/Out
        if ((key == '=' || key == '-' || key == '0') && (Fl::event_state() & FL_COMMAND)) {
            int new_size = app_->getConfig().font_size;
            if (key == '=') new_size += 2;
            else if (key == '-') new_size -= 2;
            else if (key == '0') new_size = 15;
            
            if (new_size < 8) new_size = 8;
            if (new_size > 72) new_size = 72;
            
            hazel_config_t cfg = app_->getConfig();
            cfg.font_size = new_size;
            app_->setConfig(&cfg);
            return 1;
        }

        // Preferences
        if (key == ',' && (Fl::event_state() & FL_COMMAND)) {
            app_->showPreferences();
            return 1;
        }
        
        // Evaluate Block
        if ((key == FL_Enter || key == FL_KP_Enter) && (Fl::event_state() & FL_COMMAND)) {
            app_->evaluateCurrentBlock();
            return 1;
        }
        
        // Prevent modification of output cells (B = Output, C = Error)
        bool is_modifying = (key == FL_BackSpace || key == FL_Delete || key == FL_Enter || key == FL_KP_Enter || (key >= 0x20 && key <= 0xff && !(Fl::event_state() & FL_CTRL) && !(Fl::event_state() & FL_ALT)) || ((key == 'v' || key == 'x') && (Fl::event_state() & FL_COMMAND)));
        if (is_modifying) {
            if (buffer()->selected()) {
                int start, end;
                if (buffer()->selection_position(&start, &end)) {
                    char first_s = app_->getStyleAt(start);
                    for (int i = start; i < end; i++) {
                        char s = app_->getStyleAt(i);
                        if (app_->isOutputStyle(s)) return 1;
                        if (app_->getConfig().parser_mode == 0 && s != first_s) return 1; // Prevent deleting across multiple cell types
                    }
                }
            } else {
                int pos = insert_position();
                if (key == FL_BackSpace) {
                    if (pos > 0) {
                        char s = app_->getStyleAt(pos - 1);
                        if (app_->isOutputStyle(s)) return 1;
                        
                        char curr = app_->getStyleAt(pos);
                        if (app_->getConfig().parser_mode == 0) {
                            if (curr != 0 && s != 0 && curr != s) return 1;
                            
                            if (buffer()->char_at(pos - 1) == '\n' && pos > 1 && pos < buffer()->length()) {
                                if (app_->getStyleAt(pos - 2) != app_->getStyleAt(pos)) return 1;
                            }
                        }
                    }
                } else if (key == FL_Delete) {
                    if (pos < buffer()->length()) {
                        char s = app_->getStyleAt(pos);
                        if (app_->isOutputStyle(s)) return 1;
                        
                        if (app_->getConfig().parser_mode == 0) {
                            if (buffer()->char_at(pos) == '\n' && pos > 0 && pos + 1 < buffer()->length()) {
                                if (app_->getStyleAt(pos - 1) != app_->getStyleAt(pos + 1)) return 1;
                            }
                        }
                    }
                } else {
                    if (pos > 0) {
                        char prev = app_->getStyleAt(pos - 1);
                        char curr = app_->getStyleAt(pos);
                        if (app_->isOutputStyle(prev) && app_->isOutputStyle(curr)) return 1;
                    } else {
                        char curr = app_->getStyleAt(pos);
                        if (app_->isOutputStyle(curr)) return 1;
                    }
                }
            }
        }
        
        // Bash-style keybindings
        if ((key == '`' || key == '~') && (Fl::event_state() & FL_COMMAND)) {
            app_->toggleTerminal();
            return 1;
        }

        if ((Fl::event_state() & FL_COMMAND) && (Fl::event_state() & FL_SHIFT)) {
            if (key == 'c' || key == 'x') {
                int pos = insert_position();
                char style = app_->getStyleAt(pos);
                if (style != 'A' && style != 'D' && pos > 0) {
                    char left = app_->getStyleAt(pos - 1);
                    if (left == 'A' || left == 'D') {
                        pos = pos - 1;
                        style = left;
                    }
                }
                
                int start = pos;
                while (start > 0 && app_->getStyleAt(start - 1) == style) start--;
                int end = pos;
                while (end < buffer()->length() - 1 && app_->getStyleAt(end + 1) == style) end++;
                if (end < buffer()->length()) end++;
                
                if (start < end) {
                    char* t = buffer()->text_range(start, end);
                    app_->cell_clip_text_ = t;
                    app_->cell_clip_style_ = style;
                    free(t);
                    
                    if (key == 'x') {
                        buffer()->remove_modify_callback(style_update_cb, app_);
                        buffer()->remove(start, end);
                        app_->getStyleBuffer()->remove(start, end);
                        buffer()->add_modify_callback(style_update_cb, app_);
                        insert_position(start);
                        show_insert_position();
                    }
                }
                return 1;
            } else if (key == 'v') {
                if (app_->cell_clip_style_ != 0 && !app_->cell_clip_text_.empty()) {
                    int pos = insert_position();
                    buffer()->remove_modify_callback(style_update_cb, app_);
                    
                    // Add newline if we are pasting a block and not at start of line
                    int l_start = buffer()->line_start(pos);
                    std::string payload = app_->cell_clip_text_;
                    if (pos != l_start) {
                        payload = "\n" + payload;
                    }
                    
                    buffer()->insert(pos, payload.c_str());
                    std::string s(payload.length(), app_->cell_clip_style_);
                    app_->getStyleBuffer()->insert(pos, s.c_str());
                    buffer()->add_modify_callback(style_update_cb, app_);
                    insert_position(pos + payload.length());
                    show_insert_position();
                }
                return 1;
            }
        }
        
        if ((Fl::event_state() & FL_COMMAND)) {
            if (key == 'a') {
                insert_position(buffer()->line_start(insert_position()));
                show_insert_position();
                return 1;
            } else if (key == 'e') {
                insert_position(buffer()->line_end(insert_position()));
                show_insert_position();
                return 1;
            } else if (key == 'k') {
                int pos = insert_position();
                int end = buffer()->line_end(pos);
                if (pos == end) buffer()->remove(pos, pos + 1);
                else buffer()->remove(pos, end);
                return 1;
            } else if (key == 'f') {
                if (insert_position() < buffer()->length()) insert_position(insert_position() + 1);
                show_insert_position();
                return 1;
            } else if (key == 'b') {
                if (insert_position() > 0) insert_position(insert_position() - 1);
                show_insert_position();
                return 1;
            } else if (key == 'p') {
                kf_up(0, this);
                return 1;
            } else if (key == 'n') {
                kf_down(0, this);
                return 1;
            }
        }
    }
    
    int r = Fl_Text_Editor::handle(event);
    app_->updateStatusBar();
    return r;
}

HazelApp::HazelApp(const char* title, hazel_eval_cb_t cb, void* user_data) 
    : eval_cb_(cb), user_data_(user_data) {
    app_title_ = title ? title : "Hazel";
    
    is_dirty_ = false;
    prefs_ = new Fl_Preferences(Fl_Preferences::USER, "octetta", "hazel");
    win_ = new HazelWindow(800, 600, title, this);
    buffer_ = new Fl_Text_Buffer();
    style_buffer_ = new Fl_Text_Buffer();
    
    config_.theme = 0;
    config_.font = FL_COURIER;
    config_.font_size = 14;
    config_.input_bg = FL_WHITE;
    config_.output_bg = fl_rgb_color(245, 245, 250);
    config_.error_bg = fl_rgb_color(255, 235, 235);
    config_.markdown_bg = fl_rgb_color(245, 255, 245);
    config_.text_fg = FL_BLACK;
    config_.error_fg = FL_DARK_RED;
    config_.markdown_fg = FL_DARK_GREEN;
    config_.cursor_fg = FL_WHITE;
    config_.cursor_bg = FL_BLACK;
    config_.select_bg = fl_rgb_color(180, 200, 255);
    
    applyConfig();
    
    editor_ = new HazelEditor(0, 0, 800, 575, this);
    editor_->buffer(buffer_);
    editor_->box(FL_FLAT_BOX);
    editor_->cursor_style(Fl_Text_Display::BLOCK_CURSOR);
    editor_->highlight_data(style_buffer_, styletable_, next_style_index_, 'A', 0, 0);
    
    terminal_ = new TerminalPane(0, 400, 800, 175, this);
    terminal_->hide();
    splitter_ = new Splitter(0, 400, 800, 4, this);
    splitter_->hide();
    
    status_bar_ = new StatusBar(0, 555, 800, 45, this);
    status_bar_->color(FL_LIGHT2);
    status_bar_->label("");
    
    macro_bar_ = new MacroBar(0, 545, 800, 30, this);
    macro_bar_->color(FL_LIGHT2);
    macro_bar_->box(FL_FLAT_BOX);
    
    win_->resizable(win_);
    win_->callback([](Fl_Widget*, void* v){ ((HazelApp*)v)->tryQuit(); }, this);
    win_->end();
    

    
    buffer_->add_modify_callback(style_update_cb, this);
    
    editor_->insert_position(buffer_->length());
    highest_modified_pos_ = 0;
    
}

HazelApp::~HazelApp() {
    if (last_text_) free(last_text_);
    if (last_styles_) free(last_styles_);
    delete prefs_;
    delete win_;
    delete buffer_;
    delete style_buffer_;
}

int HazelApp::run() {
    win_->show();
    return Fl::run();
}

void HazelApp::loadFile(const char* filepath) {
    if (config_.on_open) {
        if (config_.on_open((hazel_app_t*)this, filepath, user_data_)) {
            editor_->insert_position(0);
            editor_->show_insert_position();
            return;
        }
    }

    FILE* f = fopen(filepath, "r");
    if (!f) return;
    
    buffer_->remove_modify_callback(style_update_cb, this);
    buffer_->text("");
    style_buffer_->text("");
    
    char line[2048];
    char current_style = 'D'; // Default to Markdown
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "```hazel", 8) == 0) {
            current_style = 'A';
            continue;
        } else if (strncmp(line, "```output", 9) == 0) {
            current_style = 'B';
            continue;
        } else if (strncmp(line, "```", 3) == 0) {
            current_style = 'D';
            continue;
        }
        
        int pos = buffer_->length();
        buffer_->insert(pos, line);
        int len = strlen(line);
        std::string styles(len, current_style);
        style_buffer_->insert(pos, styles.c_str());
    }
    fclose(f);
    buffer_->add_modify_callback(style_update_cb, this);
    
    highest_modified_pos_ = 0;
    current_filepath_ = filepath;
    setDirty(false);
    editor_->insert_position(0);
}

void HazelApp::openFile() {
    const char* filename = fl_file_chooser("Open Notebook", "Notebook Files (*.{md,sk})\tMarkdown (*.md)\tSkred Script (*.sk)", NULL);
    if (filename) {
        loadFile(filename);
    }
}

void HazelApp::openDirectory() {
    const char* dir = fl_dir_chooser("Open Folder / Set Working Directory", NULL);
    if (dir) {
        if (config_.on_open_dir) {
            config_.on_open_dir((hazel_app_t*)this, dir, user_data_);
        }
    }
}
void HazelApp::saveFileAs(const char* filepath) {
    if (config_.on_save) {
        if (config_.on_save((hazel_app_t*)this, filepath, user_data_)) return;
    }

    std::string path(filepath);
    size_t last_slash = path.find_last_of("/\\");
    size_t last_dot = path.find_last_of(".");
    if (last_dot == std::string::npos || (last_slash != std::string::npos && last_dot < last_slash)) {
        path += ".md";
    }
    
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return;
    
    char current_style = 0;
    int block_start = 0;
    
    auto flush_block = [&](int end_pos) {
        if (block_start >= end_pos) return;
        char* text = buffer_->text_range(block_start, end_pos);
        int len = strlen(text);
        
        if (current_style == 'A') {
            fprintf(f, "```hazel\n%s", text);
            if (len == 0 || text[len-1] != '\n') fprintf(f, "\n");
            fprintf(f, "```\n");
        } else if (isOutputStyle(current_style)) {
            fprintf(f, "```output\n%s", text);
            if (len == 0 || text[len-1] != '\n') fprintf(f, "\n");
            fprintf(f, "```\n");
        } else if (current_style == 'D') {
            fprintf(f, "%s", text);
            if (len == 0 || text[len-1] != '\n') fprintf(f, "\n");
        }
        
        free(text);
    };
    
    for (int i = 0; i < buffer_->length(); i++) {
        char s = style_buffer_->char_at(i);
        if (s == 'C') s = 'B';
        if (s != current_style) {
            flush_block(i);
            current_style = s;
            block_start = i;
        }
    }
    flush_block(buffer_->length());
    fclose(f);
    current_filepath_ = path;
    setDirty(false);
}

void HazelApp::promptSaveAs() {
    const char* filename = fl_file_chooser("Save Notebook As...", "Notebook Files (*.{md,sk})\tMarkdown (*.md)\tSkred Script (*.sk)", NULL);
    if (filename) {
        saveFileAs(filename);
    }
}

void HazelApp::saveFile() {
    if (!current_filepath_.empty()) {
        saveFileAs(current_filepath_.c_str());
    } else {
        promptSaveAs();
    }
}

void HazelApp::evaluateCurrentBlock() {
    if (!eval_cb_) return;
    
    int pos = editor_->insert_position();
    char style = getStyleAt(pos);
    
    if (style != 'A' && style != 'D' && pos > 0) {
        char left = getStyleAt(pos - 1);
        if (left == 'A' || left == 'D') {
            pos = pos - 1;
            style = left;
        }
    }
    
    if (style != 'A' && style != 'D') return; 
    
    int start = pos;
    while (start > 0 && style_buffer_->char_at(start - 1) == style) start--;
    int end = pos;
    while (end < buffer_->length() - 1 && style_buffer_->char_at(end + 1) == style) end++;
    if (end < buffer_->length()) end++;
    
    if (style == 'D') {
        int next_pos = end;
        if (next_pos >= buffer_->length()) {
            buffer_->remove_modify_callback(style_update_cb, this);
            while (end > start && buffer_->char_at(end - 1) == '\n') {
                buffer_->remove(end - 1, end);
                style_buffer_->remove(end - 1, end);
                end--;
            }
            buffer_->insert(end, "\n");
            style_buffer_->insert(end, "A");
            buffer_->add_modify_callback(style_update_cb, this);
            next_pos = end + 1;
        }
        
        editor_->insert_position(next_pos);
        editor_->show_insert_position();
        
        if (run_all_pending_) {
            Fl::add_timeout(0.01, [](void* d) {
                ((HazelApp*)d)->evaluateCurrentBlock();
            }, this);
        }
        return;
    }
    // Check if there is an existing output block immediately following this input block.
    // An output block consists of contiguous 'B' and 'C' characters.
    int output_end = end;
    while (output_end < buffer_->length()) {
        char s = style_buffer_->char_at(output_end);
        if (isOutputStyle(s)) {
            output_end++;
        } else {
            break;
        }
    }
    
    if (output_end > end) {
        buffer_->remove_modify_callback(style_update_cb, this);
        buffer_->remove(end, output_end);
        style_buffer_->remove(end, output_end);
        buffer_->add_modify_callback(style_update_cb, this);
    }
    
    if (highest_modified_pos_ != -1 && highest_modified_pos_ >= start && highest_modified_pos_ <= end) {
        highest_modified_pos_ = end;
    }
    char* input = buffer_->text_range(start, end);
    
    // Force exactly one newline after the input block
    buffer_->remove_modify_callback(style_update_cb, this);
    while (end > start && buffer_->char_at(end - 1) == '\n') {
        buffer_->remove(end - 1, end);
        style_buffer_->remove(end - 1, end);
        end--;
    }
    buffer_->insert(end, "\n");
    style_buffer_->insert(end, "A");
    buffer_->add_modify_callback(style_update_cb, this);
    end++;
    
    bool at_bottom = (end >= buffer_->length());
    
    hazel_ctx_t* ctx = new hazel_ctx_t();
    ctx->app = this;
    ctx->insert_pos = end;
    ctx->at_bottom = at_bottom;
    ctx->is_terminal = false;
    
    eval_cb_(input, ctx, user_data_);
    free(input);
}

// Very basic 256 color map approximation
static unsigned int map_256_to_rgb(int code) {
    if (code < 16) {
        // Standard ANSI 16
        unsigned int colors[16] = {
            0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0,
            0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff
        };
        return colors[code];
    } else if (code < 232) {
        // 6x6x6 color cube
        code -= 16;
        int b = code % 6;
        int g = (code / 6) % 6;
        int r = (code / 36) % 6;
        return fl_rgb_color(r ? r * 40 + 55 : 0, g ? g * 40 + 55 : 0, b ? b * 40 + 55 : 0);
    } else {
        // 24 grayscale
        int gray = (code - 232) * 10 + 8;
        return fl_rgb_color(gray, gray, gray);
    }
}

char HazelApp::getAnsiStyle(unsigned int fg, unsigned int bg, bool is_error) {
    for (int i = 4; i < next_style_index_; i++) {
        if (styletable_[i].color == (Fl_Color)fg && styletable_[i].bgcolor == (Fl_Color)bg) {
            return (char)('A' + i);
        }
    }
    
    if (next_style_index_ >= 250) {
        return is_error ? 'C' : 'B';
    }
    
    int i = next_style_index_++;
    styletable_[i] = { (Fl_Color)fg, config_.font, config_.font_size, Fl_Text_Display::ATTR_BGCOLOR_EXT, (Fl_Color)bg };
    
    editor_->highlight_data(style_buffer_, styletable_, next_style_index_, 'A', 0, 0);
    return (char)('A' + i);
}

void HazelApp::appendOutput(int insert_pos, const char* text, int is_error) {
    if (!text || !buffer_ || !style_buffer_) return;
    
    std::string clean_text;
    std::string clean_styles;
    
    char current_style = is_error ? 'C' : 'B';
    unsigned int current_fg = is_error ? config_.error_fg : config_.text_fg;
    unsigned int current_bg = is_error ? config_.error_bg : config_.output_bg;
    
    const char* src = text;
    while (*src) {
        if (*src == '\x1b' && *(src + 1) == '[') {
            src += 2;
            int codes[16] = {0};
            int num_codes = 0;
            
            while (*src && *src != 'm' && num_codes < 16) {
                if (*src >= '0' && *src <= '9') {
                    codes[num_codes] = codes[num_codes] * 10 + (*src - '0');
                } else if (*src == ';') {
                    num_codes++;
                }
                src++;
            }
            if (*src == 'm') {
                num_codes++;
                src++;
            }
            
            // Apply ANSI codes
            for (int i = 0; i < num_codes; i++) {
                int c = codes[i];
                if (c == 0) {
                    current_fg = config_.text_fg;
                    current_bg = is_error ? config_.error_bg : config_.output_bg;
                    current_style = is_error ? 'C' : 'B';
                } else if (c == 38 && i + 2 < num_codes && codes[i+1] == 5) {
                    current_fg = map_256_to_rgb(codes[i+2]);
                    current_style = getAnsiStyle(current_fg, current_bg, is_error);
                    i += 2;
                } else if (c == 48 && i + 2 < num_codes && codes[i+1] == 5) {
                    current_bg = map_256_to_rgb(codes[i+2]);
                    current_style = getAnsiStyle(current_fg, current_bg, is_error);
                    i += 2;
                } else if (c >= 30 && c <= 37) {
                    current_fg = map_256_to_rgb(c - 30);
                    current_style = getAnsiStyle(current_fg, current_bg, is_error);
                } else if (c >= 40 && c <= 47) {
                    current_bg = map_256_to_rgb(c - 40);
                    current_style = getAnsiStyle(current_fg, current_bg, is_error);
                }
            }
            continue;
        }
        
        clean_text += *src;
        clean_styles += current_style;
        src++;
    }
    
    if (clean_text.empty()) return;
    
    buffer_->remove_modify_callback(style_update_cb, this);
    buffer_->insert(insert_pos, clean_text.c_str());
    style_buffer_->insert(insert_pos, clean_styles.c_str());
    buffer_->add_modify_callback(style_update_cb, this);
    
    // Push the user's cursor to the end of the output so they are ready to type
    if (editor_->insert_position() <= insert_pos) {
        editor_->insert_position(insert_pos + clean_text.length());
        editor_->show_insert_position();
    }
}

char HazelApp::getStyleAt(int pos) {
    if (pos >= buffer_->length()) {
        return '\0'; // End of file has no style!
    }
    return style_buffer_->char_at(pos);
}

void HazelApp::startRunAll() {
    if (highest_modified_pos_ == -1) {
        return; // Nothing changed, do not rerun
    }
    int start_pos = highest_modified_pos_;
    
    
    int block_start = start_pos;
    if (block_start < buffer_->length()) {
        char s = getStyleAt(block_start);
        if (s == 'A') {
            while (block_start > 0 && getStyleAt(block_start - 1) == 'A') block_start--;
        } else if (s == 'D') {
            while (block_start > 0 && getStyleAt(block_start - 1) == 'D') block_start--;
        }
    }
    
    editor_->insert_position(block_start);
    if (getStyleAt(block_start) != 'A') {
        int search = block_start;
        while (search < buffer_->length() && getStyleAt(search) != 'A') search++;
        editor_->insert_position(search);
    }
    
    highest_modified_pos_ = -1; // Reset since we are running!
    
    if (getStyleAt(editor_->insert_position()) == 'A') {
        run_all_pending_ = true;
        evaluateCurrentBlock();
    }
}

void HazelApp::finishEvaluation(hazel_ctx_t* ctx) {
    if (ctx->is_terminal) {
        return;
    }
    if (ctx->at_bottom) {
        
        editor_->insert_position(buffer_->length());
        editor_->show_insert_position();
    } else {
        int search = ctx->insert_pos;
        while (search < buffer_->length()) {
            char s = getStyleAt(search);
            if (s == 'A' || s == 'D') {
                editor_->insert_position(search);
                editor_->show_insert_position();
                break;
            }
            search++;
        }
    }

    if (run_all_pending_) {
        int pos = editor_->insert_position();
        char style = getStyleAt(pos);
        
        if (style == 'A' || style == 'D') {
            int end = pos;
            while (end < buffer_->length() && getStyleAt(end) == style) end++;
            
            // A cell is considered empty if it contains just a trailing newline scaffold at the EOF.
            if (style == 'A' && end - pos <= 1 && end >= buffer_->length()) {
                run_all_pending_ = false;
                
                editor_->insert_position(buffer_->length());
            } else {
                Fl::add_timeout(0.01, [](void* d) {
                    ((HazelApp*)d)->evaluateCurrentBlock();
                }, this);
            }
        } else {
            run_all_pending_ = false;
        }
    }
}

void HazelEditor::draw() {
    Fl_Text_Editor::draw();
    
    int margin_x = this->x() + this->linenumber_width();
    int width = this->w() - this->linenumber_width();
    if (mVScrollBar && mVScrollBar->visible()) width -= mVScrollBar->w();
    int height = this->textsize() + 6; 
    
    int y_start = this->y();
    int y_end = this->y() + this->h();
    if (mHScrollBar && mHScrollBar->visible()) y_end -= mHScrollBar->h();
    
    auto getEffectiveStyleAt = [&](int p) -> char {
        if (p < 0) return 'A';
        if (p == buffer()->length() && p > 0 && buffer()->char_at(p - 1) == '\n') {
            return app_->getPendingStyle() ? app_->getPendingStyle() : 'A';
        } else if (p == buffer()->length()) {
            return app_->getPendingStyle() ? app_->getPendingStyle() : app_->getStyleAt(p - 1);
        }
        
        bool is_empty_line = false;
        if (p < buffer()->length() && buffer()->char_at(p) == '\n') {
            if (p == 0 || buffer()->char_at(p - 1) == '\n') is_empty_line = true;
        } else if (p == buffer()->length()) {
            if (p == 0 || buffer()->char_at(p - 1) == '\n') is_empty_line = true;
        }
        if (is_empty_line) {
            if (p == insert_position() && app_->getPendingStyle() != 0) return app_->getPendingStyle();
            char p_curr = app_->getStyleAt(p);
            if (p_curr == 'A' || p_curr == 'D' || p_curr == 'C' || p_curr == 'B') return p_curr;
            char p_prev = (p > 0) ? app_->getStyleAt(p - 1) : '\0';
            if (p_prev == 'A' || p_prev == 'D' || p_prev == 'C' || p_prev == 'B') return p_prev;
            return 'A';
        }
        char s = app_->getStyleAt(p);
        return s == 0 ? 'A' : s;
    };
    
    // Iterate over visible screen space to find empty lines
    int bg_pos = xy_to_position(this->x() + this->linenumber_width(), y_start);
    if (bg_pos >= 0) {
        bg_pos = buffer()->line_start(bg_pos);
        while (bg_pos <= buffer()->length()) {
            int cx, cy;
            if (!position_to_xy(bg_pos, &cx, &cy)) break;
            if (cy > y_end) break;
            
            int line_start = bg_pos;
            int line_end = buffer()->line_end(bg_pos);
            
            if (line_start == line_end) {
                char p = getEffectiveStyleAt(bg_pos);
                if (p == 'D' || app_->isOutputStyle(p)) {
                    if (p == 'D') fl_color(fl_rgb_color(240, 255, 240));
                    else if (p == 'B') fl_color(fl_rgb_color(240, 240, 245));
                    else if (p == 'C') fl_color(fl_rgb_color(255, 230, 230));
                    
                    fl_rectf(margin_x, cy, width, height);
                }
            }
            
            if (bg_pos == buffer()->length()) break;
            bg_pos = line_end + 1;
        }
    }
    
    // Draw Custom Cell Badges in Margin
    int m_width = 40;
    int m_x = this->x();
    fl_color(fl_rgb_color(230, 230, 230)); 
    fl_rectf(m_x, y_start, m_width, y_end - y_start);
    
    auto getBlockType = [&](char s) {
        if (s == 'D') return 1; // Markdown
        if (app_->isOutputStyle(s)) return 2; // Output
        if (s == 'A') return 0; // Code
        return -1;
    };
    
    fl_font(FL_HELVETICA_BOLD, 10);
    fl_color(fl_rgb_color(150, 150, 150));
    
    int badge_pos = xy_to_position(this->x() + m_width, y_start);
    if (badge_pos >= 0) {
        badge_pos = buffer()->line_start(badge_pos);
        int last_cy = y_start;
        while (badge_pos <= buffer()->length()) {
            int cx, cy;
            if (!position_to_xy(badge_pos, &cx, &cy)) {
                cy = last_cy + height;
                cx = margin_x;
            } else {
                last_cy = cy;
            }
            if (cy > y_end) break;
            
            int line_start = badge_pos;
            char curr = getEffectiveStyleAt(line_start);
            char prev = (line_start > 0) ? getEffectiveStyleAt(line_start - 1) : '\0';
            if (prev == 0) prev = 'A';
            
            int curr_type = getBlockType(curr);
            int prev_type = getBlockType(prev);
            
            if (line_start == 0 || curr_type != prev_type) {
                int block_idx = 0;
                int scan_curr = -1;
                for (int i = 0; i <= line_start; i++) {
                    char s = getEffectiveStyleAt(i);
                    int t = getBlockType(s);
                    if (t != scan_curr) {
                        if (t == curr_type) block_idx++;
                        scan_curr = t;
                    }
                }
                
                char badge[16];
                if (curr_type == 0) snprintf(badge, sizeof(badge), "C%d", block_idx);
                else if (curr_type == 1) snprintf(badge, sizeof(badge), (app_->getConfig().parser_mode > 0) ? "N%d" : "M%d", block_idx);
                else snprintf(badge, sizeof(badge), "O%d", block_idx);
                
                if (line_start > 0) {
                    fl_color(fl_color_average(app_->getConfig().text_fg, app_->getConfig().input_bg, 0.2f));
                    int line_w = this->w();
                    if (mVScrollBar && mVScrollBar->visible()) {
                        line_w -= mVScrollBar->w();
                    }
                    fl_line(m_x, cy, this->x() + line_w, cy);
                }
                
                int h_pos = app_->getHighestModifiedPos();
                bool is_stale = false;
                if (h_pos != -1) {
                    int block_end = line_start;
                    while (block_end < buffer()->length() && getEffectiveStyleAt(block_end) == curr) block_end++;
                    if (h_pos <= block_end) {
                        is_stale = true;
                    }
                }
                
                if (is_stale) {
                    fl_font(FL_HELVETICA_BOLD, 10);
                    fl_color(fl_rgb_color(100, 100, 100)); // Slightly darker if stale
                } else {
                    fl_font(FL_HELVETICA, 10);
                    fl_color(fl_rgb_color(150, 150, 150));
                }
                fl_draw(badge, m_x + 4, cy + height - 4);
            }
            
            if (badge_pos == buffer()->length()) break;
            badge_pos = buffer()->line_end(badge_pos) + 1;
        }
    }
    
    // Custom Block Cursor
    if (mCursorOn && Fl::focus() == this) {
        int pos = insert_position();
        int cx, cy;
        if (position_to_xy(pos, &cx, &cy)) {
            char c = (pos < buffer()->length()) ? buffer()->char_at(pos) : '\0';
            
            fl_font(textfont(), textsize());
            int c_width = fl_width("W"); // Monospace uniform block width
            
            fl_color(app_->getConfig().cursor_bg);
            fl_rectf(cx, cy, c_width, mMaxsize);
            
            if (c != '\n' && c != '\0' && c != '\r') {
                fl_color(app_->getConfig().cursor_fg);
                char s[2] = {c, '\0'};
                // Exact FLTK baseline
                fl_draw(s, cx, cy + mMaxsize - fl_descent());
            }
        }
    }
    
    // Draw a subtle border separating margin from content
    fl_color(fl_color_average(app_->getConfig().text_fg, app_->getConfig().input_bg, 0.1f));
    fl_line(m_x + m_width - 1, y_start, m_x + m_width - 1, y_end);
}

void HazelApp::updateStatusBar(bool force) {
    static int last_pos = -1;
    static int last_length = -1;
    static int last_dirty = -1;
    int pos = editor_->insert_position();
    int length = buffer_->length();
    int dirty = is_dirty_ ? 1 : 0;
    
    if (!force && pos == last_pos && length == last_length && dirty == last_dirty) {
        return; // Skip recalculation if nothing changed
    }
    last_pos = pos;
    last_length = length;
    last_dirty = dirty;
    
    int line = buffer_->count_lines(0, pos) + 1;
    int line_start = buffer_->line_start(pos);
    int col = pos - line_start + 1;
    
    char style = getStyleAt(pos);
    if (pos == buffer_->length() && pos > 0 && buffer_->char_at(pos - 1) == '\n') {
        style = pending_style_ ? pending_style_ : 'A';
    } else if (pos == buffer_->length()) {
        style = pending_style_ ? pending_style_ : getStyleAt(pos - 1);
    }
    
    const char* mode = "Code";
    if (style == 'D') mode = (config_.parser_mode > 0) ? "Note" : "Markdown";
    else if (isOutputStyle(style)) mode = "Output";
    
    char target_block = isOutputStyle(style) ? 'B' : style;
    int block_idx = 0;
    char current_block = '\0';
    for (int i = 0; i <= pos; i++) {
        char s = getStyleAt(i);
        if (i == pos && pos == buffer_->length()) {
             s = target_block;
        }
        if (isOutputStyle(s)) s = 'B';
        if (s != current_block) {
            if (s == target_block) block_idx++;
            current_block = s;
        }
    }
    if (block_idx == 0) block_idx = 1;

    char mode_with_idx[64];
    snprintf(mode_with_idx, sizeof(mode_with_idx), "%s#%d", mode, block_idx);
    
    const char* fname = current_filepath_.empty() ? "Untitled" : current_filepath_.c_str();
    const char* slash = strrchr(fname, '/');
    if (slash) fname = slash + 1;
    
    char status[512];
    snprintf(status, sizeof(status), "Ln %d, Col %d  |  %s", line, col, mode_with_idx);
    
    if (status_info_ != status) {
        status_info_ = status;
        status_bar_->redraw();
    }
    
    char title_buf[1024];
    snprintf(title_buf, sizeof(title_buf), "%s%s - %s", is_dirty_ ? "* " : "", fname, app_title_.c_str());
    if (win_->label() == nullptr || strcmp(win_->label(), title_buf) != 0) {
        win_->copy_label(title_buf);
    }
}

void HazelApp::setDirty(bool dirty) {
    is_dirty_ = dirty;
    updateStatusBar();
}

bool HazelApp::checkSaveBeforeQuit() {
    if (!is_dirty_) return true;
    int r = fl_choice("You have unsaved changes. Quit anyway?", "Cancel", "Quit", 0);
    return r == 1;
}

void HazelApp::tryQuit() {
    if (checkSaveBeforeQuit()) {
        win_->hide();
    }
}

void HazelApp::applyConfig() {
    styletable_[0] = { (Fl_Color)config_.text_fg, config_.font, config_.font_size, Fl_Text_Display::ATTR_BGCOLOR_EXT, (Fl_Color)config_.input_bg };
    styletable_[1] = { (Fl_Color)config_.text_fg, config_.font, config_.font_size, Fl_Text_Display::ATTR_BGCOLOR_EXT, (Fl_Color)config_.output_bg };
    styletable_[2] = { (Fl_Color)config_.error_fg, config_.font, config_.font_size, Fl_Text_Display::ATTR_BGCOLOR_EXT, (Fl_Color)config_.error_bg };
    styletable_[3] = { (Fl_Color)config_.markdown_fg, config_.font, config_.font_size, Fl_Text_Display::ATTR_BGCOLOR_EXT, (Fl_Color)config_.markdown_bg };
}

void HazelApp::setConfig(const hazel_config_t* config) {
    if (!config) return;
    config_ = *config;
    applyConfig();
    
    // Only apply startup text if buffer is completely empty
    if (config_.startup_text && buffer_->length() == 0) {
        buffer_->text(config_.startup_text);
        std::string start_styles(strlen(config_.startup_text), 'D');
        style_buffer_->text(start_styles.c_str());
        buffer_->append("\n");
        style_buffer_->append("A");
    }
    
    if (win_) win_->color(config_.input_bg);
    if (status_bar_) {
        status_bar_->color(FL_LIGHT2);
        status_bar_->labelcolor(FL_BLACK);
    }
    if (macro_bar_) macro_bar_->color(FL_LIGHT2);
    
    if (editor_) {
        editor_->color(config_.input_bg);
        editor_->highlight_data(style_buffer_, styletable_, next_style_index_, 'A', 0, 0);
        editor_->textfont(config_.font);
        editor_->textsize(config_.font_size);
        editor_->cursor_color(config_.cursor_bg);
        editor_->cursor_style(Fl_Text_Display::BLOCK_CURSOR);
        editor_->selection_color(config_.select_bg);
        editor_->hide_cursor();
        editor_->show_cursor();
        editor_->redraw();
    }
    if (terminal_) {
        terminal_->color(config_.input_bg);
        terminal_->textfont(config_.font);
        terminal_->textsize(config_.font_size);
        terminal_->cursor_color(config_.cursor_bg);
        terminal_->selection_color(config_.select_bg);
        terminal_->redraw();
    }
    if (win_) win_->redraw();
}

void HazelApp::clear() {
    buffer_->remove_modify_callback(style_update_cb, this);
    buffer_->text("");
    style_buffer_->text("");
    buffer_->add_modify_callback(style_update_cb, this);
    highest_modified_pos_ = 0;
    setDirty(false);
}

void HazelApp::appendBlock(char style, const char* text) {
    if (!text || strlen(text) == 0) return;
    buffer_->remove_modify_callback(style_update_cb, this);
    int pos = buffer_->length();
    buffer_->insert(pos, text);
    std::string styles(strlen(text), style);
    style_buffer_->insert(pos, styles.c_str());
    buffer_->add_modify_callback(style_update_cb, this);
    
    if (config_.parser_mode > 0) {
        style_update_cb(0, 0, 0, 0, "", this);
    }
}

const char* HazelApp::getText() const {
    if (last_text_) free(last_text_);
    last_text_ = buffer_->text();
    return last_text_;
}

const char* HazelApp::getStyles() const {
    if (last_styles_) free(last_styles_);
    last_styles_ = style_buffer_->text();
    return last_styles_;
}

void HazelApp::setFilepath(const char* path) {
    current_filepath_ = path ? path : "";
    updateStatusBar();
}


void HazelWindow::resize(int X, int Y, int W, int H) {
    Fl_Double_Window::resize(X, Y, W, H);
    if (app_) app_->layoutWidgets(W, H);
}

void HazelApp::setTerminalHeight(int h) {
    int max_h = win_->h() - status_bar_->h() - 100;
    if (h > max_h) h = max_h;
    if (h < 50) h = 50;
    terminal_height_ = h;
    layoutWidgets(win_->w(), win_->h());
    win_->redraw();
}

void HazelApp::layoutWidgets(int W, int H) {
    int status_h = 45;
    int macro_h = 30;
    status_bar_->resize(0, H - status_h, W, status_h);
    macro_bar_->resize(0, H - status_h - macro_h, W, macro_h);
    
    int bottom_h = status_h + macro_h;
    
    if (terminal_->visible()) {
        if (terminal_height_ == 0) terminal_height_ = 175;
        int term_h = terminal_height_;
        editor_->resize(0, 0, W, H - bottom_h - term_h - 4);
        splitter_->resize(0, H - bottom_h - term_h - 4, W, 4);
        terminal_->resize(0, H - bottom_h - term_h, W, term_h);
    } else {
        editor_->resize(0, 0, W, H - bottom_h);
    }
}

void HazelApp::toggleTerminal() {
    if (terminal_->visible()) {
        if (Fl::focus() == terminal_) {
            terminal_->hide();
            splitter_->hide();
            editor_->take_focus();
        } else {
            terminal_->take_focus();
        }
    } else {
        terminal_->show();
        splitter_->show();
        terminal_->take_focus();
    }
    layoutWidgets(win_->w(), win_->h());
    win_->redraw();
}

void HazelApp::evaluateCommand(const char* cmd, hazel_ctx_t* ctx) {
    if (eval_cb_) eval_cb_(cmd, ctx, user_data_);
}

#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define OS_MKDIR(path) _mkdir(path)
#else
#define OS_MKDIR(path) mkdir(path, 0755)
#endif

static std::string getPrefsPath() {
    const char* home = getenv("HOME");
    if (!home) return "hazel_prefs.cfg";
    std::string dir = std::string(home) + "/.config";
    OS_MKDIR(dir.c_str());
    dir += "/hazel";
    OS_MKDIR(dir.c_str());
    return dir + "/prefs.cfg";
}

void HazelApp::savePreferences(const std::string& font_name, int theme, int size) {
    std::ofstream out(getPrefsPath());
    if (out.is_open()) {
        out << font_name << "\n";
        out << theme << "\n";
        out << size << "\n";
        out << config_.text_fg << " " << config_.input_bg << " " << config_.output_bg << " " << config_.error_bg << " " << config_.markdown_bg << " " << config_.error_fg << " " << config_.markdown_fg << " " << config_.cursor_fg << " " << config_.cursor_bg << " " << config_.select_bg << "\n";
        out << config_.udp_port << " " << config_.events_port << "\n";
    }
}

void HazelApp::loadPreferences() {
    std::ifstream in(getPrefsPath());
    if (in.is_open()) {
        std::string font_name;
        int theme = 0;
        int size = 15;
        std::getline(in, font_name);
        in >> theme;
        if (in >> size) {
            config_.font_size = size;
        }
        Fl_Color fg, bg, out_bg, err_bg, md_bg, err_f = FL_DARK_RED, md_f = FL_DARK_GREEN, csr_f = FL_BLACK, csr_b = FL_BLACK;
        bool has_colors = false;
        if (in >> fg >> bg >> out_bg >> err_bg >> md_bg) {
            has_colors = true;
            if (!(in >> err_f >> md_f)) {
                err_f = FL_DARK_RED;
                md_f = FL_DARK_GREEN;
            }
            if (!(in >> csr_f >> csr_b)) {
                csr_f = fg;
                csr_b = fg;
            }
            Fl_Color sel_b = FL_SELECTION_COLOR;
            if (!(in >> sel_b)) sel_b = fl_rgb_color(180, 200, 255);
            config_.select_bg = sel_b; // Temporary save to config_ so it propagates
            
            int udp = 60440, evt = 60441;
            if (in >> udp >> evt) {
                config_.udp_port = udp;
                config_.events_port = evt;
            } else {
                config_.udp_port = 60440;
                config_.events_port = 60441;
            }
        } else {
            config_.select_bg = fl_rgb_color(180, 200, 255);
            config_.udp_port = 60440;
            config_.events_port = 60441;
        }
        
        hazel_config_t cfg = config_;
        
        if (!font_name.empty()) {
            int num_fonts = Fl::set_fonts("-*");
            for (int i = 0; i < num_fonts; i++) {
                const char* name = Fl::get_font_name((Fl_Font)i);
                if (name && font_name == name) {
                    cfg.font = (Fl_Font)i;
                    break;
                }
            }
        }
        
        cfg.theme = theme;
        if (has_colors) { // Always respect saved colors for any theme!
            cfg.text_fg = fg;
            cfg.input_bg = bg;
            cfg.output_bg = out_bg;
            cfg.error_bg = err_bg;
            cfg.markdown_bg = md_bg;
            cfg.error_fg = err_f;
            cfg.markdown_fg = md_f;
            cfg.cursor_fg = csr_f;
            cfg.cursor_bg = csr_b;
            cfg.select_bg = config_.select_bg; // Use saved
        } else {
            if (theme == 1) { // Dark Theme fallback
                cfg.text_fg = fl_rgb_color(220, 220, 220);
                cfg.input_bg = fl_rgb_color(25, 25, 30);
                cfg.output_bg = fl_rgb_color(15, 15, 20);
                cfg.error_bg = fl_rgb_color(40, 10, 10);
                cfg.markdown_bg = fl_rgb_color(20, 30, 25);
                cfg.error_fg = fl_rgb_color(255, 100, 100);
                cfg.markdown_fg = fl_rgb_color(100, 255, 100);
                cfg.cursor_fg = fl_rgb_color(25, 25, 30);
                cfg.cursor_bg = fl_rgb_color(220, 220, 220);
                cfg.select_bg = fl_rgb_color(60, 60, 80);
            } else if (theme == 2) { // Custom Theme fallback (Solarized)
                cfg.text_fg = fl_rgb_color(131, 148, 150);
                cfg.input_bg = fl_rgb_color(0, 43, 54);
                cfg.output_bg = fl_rgb_color(7, 54, 66);
                cfg.error_bg = fl_rgb_color(220, 50, 47);
                cfg.markdown_bg = fl_rgb_color(7, 54, 66);
                cfg.error_fg = fl_rgb_color(0, 43, 54);
                cfg.markdown_fg = fl_rgb_color(147, 161, 161);
                cfg.cursor_fg = fl_rgb_color(0, 43, 54);
                cfg.cursor_bg = fl_rgb_color(181, 137, 0);
                cfg.select_bg = fl_rgb_color(7, 54, 66);
            }
        }
        
        setConfig(&cfg);
    }
}

void HazelApp::showHelpWindow() {
    HelpWindow* hw = new HelpWindow(app_title_, app_version_, config_.parser_mode > 0, config_.help_extension_html, config_.help_extension_cb);
    hw->callback([](Fl_Widget* w, void*) {
        w->hide();
        Fl::delete_widget(w);
    });
    hw->show();
}

void HazelApp::showPreferences() {
    PreferencesWindow prefs(getConfig());
    std::string font;
    int theme = 0;
    int size = 15;
    hazel_config_t custom_colors;
    if (prefs.run(font, theme, size, custom_colors)) {
        hazel_config_t cfg = getConfig();
        cfg.font_size = size;
        if (!font.empty()) {
            int num_fonts = Fl::set_fonts("-*");
            for (int i = 0; i < num_fonts; i++) {
                const char* name = Fl::get_font_name((Fl_Font)i);
                if (name && font == name) {
                    cfg.font = (Fl_Font)i;
                    break;
                }
            }
        }
        cfg.theme = theme;
        cfg.text_fg = custom_colors.text_fg;
        cfg.input_bg = custom_colors.input_bg;
        cfg.output_bg = custom_colors.output_bg;
        cfg.error_bg = custom_colors.error_bg;
        cfg.markdown_bg = custom_colors.markdown_bg;
        cfg.error_fg = custom_colors.error_fg;
        cfg.markdown_fg = custom_colors.markdown_fg;
        cfg.cursor_fg = custom_colors.cursor_fg;
        cfg.cursor_bg = custom_colors.cursor_bg;
        cfg.select_bg = custom_colors.select_bg;
        setConfig(&cfg);
        savePreferences(font, theme, size);
    }
}

void HazelApp::zoomIn() {
    config_.font_size += 2;
    setConfig(&config_);
}

void HazelApp::zoomOut() {
    if (config_.font_size > 8) config_.font_size -= 2;
    setConfig(&config_);
}

void HazelApp::zoomReset() {
    config_.font_size = 15;
    setConfig(&config_);
}
