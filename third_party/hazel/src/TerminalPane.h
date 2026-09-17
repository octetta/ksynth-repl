#pragma once
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl.H>
#include <vector>
#include <string>

class HazelApp;

class TerminalPane : public Fl_Text_Editor {
public:
    TerminalPane(int X, int Y, int W, int H, HazelApp* app);
    ~TerminalPane();

    int handle(int event) override;
    void draw() override;
    
    void printPrompt();
    void appendOutput(const char* text, bool is_error);
    void evaluateCommand();

private:
    HazelApp* app_;
    Fl_Text_Buffer* buf_;
    Fl_Text_Buffer* style_buf_;
    
    std::vector<std::string> history_;
    int history_index_;
    int prompt_pos_;
    
    static void style_update_cb(int pos, int nInserted, int nDeleted, int nRestyled, const char* deletedText, void* cbArg);
};
