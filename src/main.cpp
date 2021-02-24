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

using namespace std;

const static float dt = 0.1;

void window(int sx, int sy, int x, int y, string title, function<void(WINDOW*)> f) {
    WINDOW* win = newwin(sy, sx, y, x);
    box(win, 0, 0);
    wmove(win, 0, (sx-title.length())/2);
    wprintw(win, title.c_str());
    f(win);
    wrefresh(win);
    delwin(win);
}

vector<string> split(string s, char del) {
    vector<string> ret;
    string c_str = "";
    size_t c = 0;
    while (c < s.length()) {
        if (s[c] != del) {
            c_str += s[c];
        } else {
            if (c_str.size() > 0) {
                ret.push_back(c_str);
            }
            c_str = "";
        }
    }
    return ret;
}

bool sendCommand(Reactor &r, string command) {
    if (command == "exit" || command == "quit") {
        endwin();
        exit(0);
    }
    auto com = split(command, ' ');



    return false;
}

int main() {

    Reactor reactor;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_BLACK, COLOR_GREEN);
    init_pair(3, COLOR_BLACK, COLOR_YELLOW);
    init_pair(4, COLOR_BLACK, COLOR_RED);
    init_pair(5, COLOR_BLACK, COLOR_CYAN);

    string command = "";

    bool command_error = false;

    while (true) {
        // keys
        int ch = getch();
        if (ch == 10 || ch == KEY_ENTER) {
            auto succ = sendCommand(reactor, command);
            if (!succ) {
                command_error = true;
            } else {
                command = "";
            }
        } else if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || 
            ch == ' ' || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
            command_error = false;
            command += (char)ch;
        } else if (ch == KEY_BACKSPACE) {
            command_error = false;
            if (command.size() > 0) command.pop_back();
        }

        // display
        int h,w;
        getmaxyx(stdscr, h, w);
        refresh();

        // overview
        window(50,16,0,0,"Overview", [&](WINDOW *win) {

        });

        window(50,27,0,16,"Rod positions", [&](WINDOW *win) {
            auto print_rod = [&](auto &s) {
                for (auto r : s) {
                    wmove(win, r.pos_y/2, -3+r.pos_x);
                    int ii = (r.pos_z-r.min_pos_z)*100/(r.max_pos_z-r.min_pos_z);
                    if (r.direction) ii = 100-ii;
                    auto txt = (ii==100)?"**":std::string(1,((ii/10)+'0')) + (char)('0'+(ii%10));
                    wprintw(win, txt.c_str());
                }
            };

            wattrset(win, COLOR_PAIR(2));
            print_rod(reactor.manual_rods);
            wattrset(win, COLOR_PAIR(3));
            print_rod(reactor.short_rods);
            wattrset(win, COLOR_PAIR(4));
            print_rod(reactor.automatic_rods);
            wattrset(win, COLOR_PAIR(5));
            print_rod(reactor.source_rods);
        });

        // warnings
        window(40,60,w-40,0, "Alarms", [&](WINDOW *win) {

        });

        // command
        window(40,5,w-40,h-5, "Command", [&](WINDOW *win) {
            wmove(win, 2, 4);
            if (command_error) wattrset(win, COLOR_PAIR(1));
            wprintw(win, command.c_str());
            wattrset(win, A_NORMAL);
        });

        this_thread::sleep_for(chrono::milliseconds(25));
    }
}