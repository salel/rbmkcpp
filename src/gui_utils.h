#pragma once
#include <ncurses.h>
#include <string>
#include <vector>
#include <functional>

struct gui_ctx {
	int short_color,auto_color,source_color,manual_color;
};

void init_gui(gui_ctx& ctx);

void print_text_center(WINDOW* win, int line, int w, std::string txt);

template <typename T>
void selection(WINDOW* win, std::vector<T> &list, std::function<std::string(T&)> name, int selected, int line, int w) {
	int length = accumulate(list.begin(), list.end(), 2, [&](auto a, auto b){return a+name(b).length()+2;});
    wmove(win, line, (w-length)/2);
    for (size_t i=0;i<list.size(); i++) {
        if (selected == (int)i) wattrset(win, A_STANDOUT);
        else wattrset(win, A_NORMAL);
        wprintw(win, name(list[i]).c_str());
        wattrset(win, A_NORMAL);
        wprintw(win, "   ");
    }
}