#include <iostream>
#include <map>
#include <functional>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <memory>

#include <ncurses.h>

#include "reactor.h"
#include "view.h"
#include "view_help.h"
#include "view_reactor.h"
#include "view_exit.h"

using namespace std;

const static float dt = 0.1;

const string overview_title = "Overview";
const string controls_title = "Controls";

enum class Focus {
    Main, Controls
};

int main() {

    Reactor reactor;

    vector<shared_ptr<View>> views;

    views.emplace_back(new ViewHelp());
    views.emplace_back(new ViewReactor());
    views.emplace_back(new ViewExit());

    gui_ctx ctx;
    init_gui(ctx);

    int current_view = 0;
    int selected_view = 0;

    Focus focus = Focus::Main; // 0 = controls, 1 = main

    int overview_height = 8;
    int main_height = 48;
    int controls_height = 10;

    while (true) {

        auto& view = views[current_view];

        // keys
        int ch = getch();
        if (focus == Focus::Main) {
            if (ch == KEY_BACKSPACE) focus = Focus::Controls;
            else {
                view->input(ch, reactor);
            }
        } else if (focus == Focus::Controls) {
            if (ch == KEY_LEFT) {
                selected_view = (selected_view-1+views.size())%views.size();
            } else if (ch == KEY_RIGHT) {
                selected_view = (selected_view+1+views.size())%views.size();
            } else if (ch == 10) {
                current_view = selected_view;
                focus = Focus::Main;
            }
        }
        int h,w;
        getmaxyx(stdscr, h, w);
        refresh();

        // header
        WINDOW* header_win = newwin(overview_height, w, 0, 0);
        box(header_win, 0, 0);
        print_text_center(header_win, 0, w, overview_title);
        wrefresh(header_win);

        // main 
        WINDOW* main_win = newwin(main_height, w, overview_height, 0);
        box(main_win, 0, 0);
        print_text_center(main_win, 0, w, view->get_fancy_name());

        view->display(main_win, w, reactor, ctx);
        wrefresh(main_win);
        
        // controls
        WINDOW* controls_win = newwin(controls_height, w, overview_height+main_height, 0);
        box(controls_win, 0, 0);
        if (focus == Focus::Controls) wattrset(controls_win, A_STANDOUT);
        print_text_center(controls_win, 0, w, controls_title);
        wattrset(controls_win, A_NORMAL);

        // display view choices
        int length = accumulate(views.begin(), views.end(), 2, [](auto a, auto b){return a+b->get_name().length()+2;});
        wmove(controls_win, 3, (w-length)/2);
        for (size_t i=0;i<views.size(); i++) {
            if (selected_view == (int)i) wattrset(controls_win, A_STANDOUT);
            else wattrset(controls_win, A_NORMAL);
            wprintw(controls_win, views[i]->get_name().c_str());
            wattrset(controls_win, A_NORMAL);
            wprintw(controls_win, "   ");
        }

        wrefresh(controls_win);
        this_thread::sleep_for(chrono::milliseconds(25));

        // delete windows
        delwin(header_win);
        delwin(main_win);
        delwin(controls_win);
    }
    endwin();
}