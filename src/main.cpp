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

#include <ncurses.h>

#include "reactor.h"

using namespace std;

const static float dt = 0.1;

const string overview_title = "Overview";
const string controls_title = "Controls";
const string help_text = "Welcome!";
const string exit_text = "Are you sure you want to exit? Press enter to quit, Backspace to return to the controls menu.";

struct View {
    string name;
    string fancy_name;
    function<void(WINDOW*, int)> display;
    function<void(int)> input = [](int c){};
};

enum class Focus {
    Main, Controls
};

int manual_color = COLOR_PAIR(1);
int short_color = COLOR_PAIR(2);
int auto_color = COLOR_PAIR(3);
int source_color = COLOR_PAIR(4);


int main() {

    Reactor reactor;

    vector<View> views;

    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    init_pair(1, COLOR_BLACK, COLOR_GREEN);
    init_pair(2, COLOR_BLACK, COLOR_YELLOW);
    init_pair(3, COLOR_BLACK, COLOR_RED);
    init_pair(4, COLOR_BLACK, COLOR_CYAN);
    

    int current_view = 0;
    int selected_view = 0;

    Focus focus = Focus::Main; // 0 = controls, 1 = main

    int overview_height = 8;
    int main_height = 48;
    int controls_height = 10;

    views.push_back({"Help", "Welcome page & help", [&](WINDOW* main_win, int w) {
        wmove(main_win, 5, (w-help_text.length())/2);
        wprintw(main_win, help_text.c_str());
    }});

    views.push_back({"Reactor", "Reactor controls", [&](WINDOW* main_win, int w) {
        auto print_rod = [&](auto &s) {
            for (auto r : s) {
                wmove(main_win, 2+r.pos_y/2, 2+r.pos_x);
                int ii = (r.pos_z-r.min_pos_z)*100/(r.max_pos_z-r.min_pos_z);
                if (r.direction) ii = 100-ii;
                auto txt = (ii==100)?"**":string(1,((ii/10)+'0')) + (char)('0'+(ii%10));
                wprintw(main_win, txt.c_str());
            }
        };

        wattrset(main_win, manual_color);
        print_rod(reactor.manual_rods);
        wattrset(main_win, short_color);
        print_rod(reactor.short_rods);
        wattrset(main_win, auto_color);
        print_rod(reactor.automatic_rods);
        wattrset(main_win, source_color);
        print_rod(reactor.source_rods);
    }});
    views.push_back({"Pumps", "Pump controls", [](WINDOW* main_win, int w) {}});
    views.push_back({"Turbine", "Turbine controls", [](WINDOW* main_win, int w) {}});
    views.push_back({"Refuel", "Refueling and storage of fuel assemblies", [](WINDOW* main_win, int w) {}});

    views.push_back({"Exit", "Are you sure you want to exit?", [](WINDOW* main_win, int w) {
        wmove(main_win, 5, (w-exit_text.length())/2);
        wprintw(main_win, exit_text.c_str());
    },
    [](int ch) {
        if (ch == 10) {
            endwin();
            exit(0);
        }
    }});

    while (true) {

        View view = views[current_view];

        // keys
        int ch = getch();
        if (focus == Focus::Main) {
            if (ch == KEY_BACKSPACE) focus = Focus::Controls;
            else {
                view.input(ch);
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
        wmove(header_win, 0, (w-overview_title.length())/2);
        //wattrset(header_win, COLOR_PAIR(1));
        wprintw(header_win, overview_title.c_str());
        wrefresh(header_win);

        // main 
        WINDOW* main_win = newwin(main_height, w, overview_height, 0);
        box(main_win, 0, 0);
        auto main_title = view.fancy_name;
        wmove(main_win, 0, (w-main_title.length())/2);
        wprintw(main_win, main_title.c_str());

        view.display(main_win, w);
        wrefresh(main_win);
        
        // controls
        WINDOW* controls_win = newwin(controls_height, w, overview_height+main_height, 0);
        box(controls_win, 0, 0);
        if (focus == Focus::Controls) wattrset(controls_win, A_STANDOUT);
        wmove(controls_win, 0, (w-controls_title.length())/2);
        wprintw(controls_win, controls_title.c_str());
        wattrset(controls_win, A_NORMAL);

        // display view choices
        int length = accumulate(views.begin(), views.end(), 2, [](auto a, auto b){return a+b.name.length()+2;});
        wmove(controls_win, 3, (w-length)/2);
        for (size_t i=0;i<views.size(); i++) {
            if (selected_view == (int)i) wattrset(controls_win, A_STANDOUT);
            else wattrset(controls_win, A_NORMAL);
            wprintw(controls_win, views[i].name.c_str());
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