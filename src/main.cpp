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
#include <sstream>

#include <ncurses.h>

#include "reactor.h"

using namespace std;

const static float dt = 0.025;

void window(int sx, int sy, int x, int y, string title, function<void(WINDOW*)> f) {
    WINDOW* win = newwin(sy, sx, y, x);
    box(win, 0, 0);
    mvwprintw(win, 0, (sx-title.length())/2, title.c_str());
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
        c++;
    }
    if (c_str != "") ret.push_back(c_str);
    return ret;
}

bool sendCommand(Reactor &r, string command) {
    if (command == "exit" || command == "quit") {
        endwin();
        exit(0);
    }
    auto com = split(command, ' ');

    if (com[0] == "select" && com.size() == 3) {
        stringstream ss1(com[1]);
        int x;
        ss1 >> x;
        if (!ss1) return false;
        stringstream ss2(com[2]);
        int y;
        ss2 >> y;
        if (!ss2) return false;
        return r.select_rod(x+3,y+3);
    } else if (com[0] == "pull") {
        if (com.size() == 1) {
            r.move_rod(-10);
            return true;
        } else if (com.size() == 2) {
            stringstream ss1(com[1]);
            int dp;
            ss1 >> dp;
            if (!ss1) return false;
            r.move_rod(-dp*0.01);
            return true;
        }
    } else if (com[0] == "insert") {
        if (com.size() == 1) {
            r.move_rod(10);
            return true;
        } else if (com.size() == 2) {
            stringstream ss1(com[1]);
            int dp;
            ss1 >> dp;
            if (!ss1) return false;
            r.move_rod(dp*0.01);
            return true;
        }
    }

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

    uint clock = 0;

    while (true) {
        // keys
        int ch = getch();
        if (ch == 10 || ch == KEY_ENTER) {
            if (command != "") {
                auto succ = sendCommand(reactor, command);
                if (!succ) {
                    command_error = true;
                } else {
                    command = "";
                }
            }
        } else if (ch>= ' ' && ch <= '~') {
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
        window(56,16,0,0,"Overview", [&](WINDOW *win) {
            
        });

        window(56,30,0,16,"Rod positions", [&](WINDOW *win) {
            for (int i=0;i<23;i++) {
                int num = i*2+2;
                string format = {(char)(num/10+'0'), (char)(num%10+'0')};
                mvwprintw(win, i+4, 2, format.c_str());
                mvwprintw(win, i+4, 52, format.c_str());
                mvwprintw(win, 2, i*2+5, format.c_str());
                mvwprintw(win, 28, i*2+5, format.c_str());
            }

            map<Reactor::RodType, int> colors = {
                {Reactor::RodType::Manual, 2},
                {Reactor::RodType::Short, 3},
                {Reactor::RodType::Automatic, 4},
                {Reactor::RodType::Source, 5}
            };

            for (auto r : reactor.rods) {
                if (r.type != Reactor::RodType::Fuel) {
                    int ii = (r.pos_z-r.min_pos_z)*100.0/(r.max_pos_z-r.min_pos_z);
                    if (!r.direction) ii = 100-ii;
                    string txt = (ii==100)?"**":string({(char)(ii/10+'0'),(char)(ii%10+'0')});
                    wattrset(win, COLOR_PAIR(colors[r.type]));
                    auto sr = reactor.get_selected_rod();
                    if (sr && r.pos_y == sr->pos_y && r.pos_x == sr->pos_x && (clock&0x10)) wattron(win, A_STANDOUT);
                    mvwprintw(win, 2+r.pos_y/2, r.pos_x, txt.c_str());
                    wattroff(win, A_STANDOUT);
                }
            }
        });

        // warnings
        window(40,60,w-40,0, "Alarms", [&](WINDOW *win) {

        });

        // command
        window(40,5,w-40,h-5, "Command", [&](WINDOW *win) {
            if (command_error) wattrset(win, COLOR_PAIR(1));
            mvwprintw(win, 2, 4, command.c_str());
            wattrset(win, A_NORMAL);
        });

        reactor.step(dt);
        this_thread::sleep_for(chrono::milliseconds((int)(dt*1000)));
        clock++;
    }
}