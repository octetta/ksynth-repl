#pragma once
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Preferences.H>
#include <string>
#include <vector>
#include <mutex>
#include "hazel/hazel.h"



class HazelEditor : public Fl_Text_Editor {
public:
    HazelEditor(int x, int y, int w, int h, class HazelApp* app);
    int handle(int event) override;
    void draw() override;
private:
    HazelApp* app_;
};

class HazelApp;

class HazelWindow : public Fl_Double_Window {
public:
    HazelWindow(int W, int H, const char* title, HazelApp* app) 
        : Fl_Double_Window(W, H, title), app_(app) {}
    void resize(int X, int Y, int W, int H) override;
private:
    HazelApp* app_;
};

class HazelApp {
    friend class TerminalPane;
    friend class StatusBar;
public:
    HazelApp(const char* title, hazel_eval_cb_t cb, void* user_data);
    ~HazelApp();

    int run();
    
    void loadPreferences();
    void showPreferences();
    void showHelpWindow();
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void savePreferences(const std::string& font, int theme, int size);
    
    void openFile();
    void openDirectory();
    void saveFile();
    void promptSaveAs();
    void toggleTerminal();
    int getTerminalHeight() const { return terminal_height_; }
    void setTerminalHeight(int h);
    void layoutWidgets(int W, int H);
    class TerminalPane* getTerminal() { return terminal_; }
    void evaluateCommand(const char* cmd, hazel_ctx_t* ctx);
    void startRunAll();
    void finishEvaluation(hazel_ctx_t* ctx);
    bool run_all_pending_ = false;
    std::string status_info_;
    std::string extra_status_;
    void setExtraStatus(const char* s) { 
        extra_status_ = s ? s : ""; 
        status_info_ = ""; // force redraw
        updateStatusBar(true); 
    }
    int highest_modified_pos_ = 0;
    char pending_style_ = 0;
    std::string cell_clip_text_;
    char cell_clip_style_ = 0;
    int getHighestModifiedPos() const { return highest_modified_pos_; }
    void setHighestModifiedPos(int pos) { highest_modified_pos_ = pos; }
    void setPendingStyle(char s) { pending_style_ = s; }
    char getPendingStyle() const { return pending_style_; }
    bool isOutputStyle(char s) const {
        return s != 'A' && s != 'D' && s != 0;
    }
    void loadFile(const char* filepath);
    void saveFileAs(const char* filepath);

    void clear();
    void appendBlock(char style, const char* text);
    const char* getText() const;
    const char* getStyles() const;
    void setFilepath(const char* path);
    void setAppVersion(const char* version) { app_version_ = version ? version : ""; }

    void updateStatusBar(bool force = false);
    bool checkSaveBeforeQuit();
    void setDirty(bool dirty);
    bool isDirty() const { return is_dirty_; }
    void tryQuit();
    void setConfig(const hazel_config_t* config);
    const hazel_config_t& getConfig() const { return config_; }
    
    void evaluateCurrentBlock();
    char getStyleAt(int pos);
    void appendOutput(int insert_pos, const char* text, int is_error);
    
    
    
    Fl_Text_Buffer* getBuffer() { return buffer_; }
    Fl_Text_Buffer* getStyleBuffer() { return style_buffer_; }
    HazelEditor* getEditor() { return editor_; }

private:
    HazelWindow* win_;
    HazelEditor* editor_;
    Fl_Text_Buffer* buffer_;
    Fl_Text_Buffer* style_buffer_;
    class StatusBar* status_bar_;
    class MacroBar* macro_bar_;
    class TerminalPane* terminal_;
    class Splitter* splitter_;
    int terminal_height_ = 0;
    std::string current_filepath_;
    std::string app_title_;
    std::string app_version_;
    mutable char* last_text_ = nullptr;
    mutable char* last_styles_ = nullptr;
    bool is_dirty_;
    
    Fl_Preferences* prefs_;
    
    hazel_eval_cb_t eval_cb_;
    void* user_data_;
    
    
    Fl_Text_Display::Style_Table_Entry styletable_[256];
    int next_style_index_ = 4;
    char getAnsiStyle(unsigned int fg, unsigned int bg, bool is_error);
    hazel_config_t config_;
    void applyConfig();
};

struct hazel_ctx_t {
    HazelApp* app;
    int insert_pos;
    std::mutex mtx;
    bool at_bottom;
    bool is_terminal;
};
