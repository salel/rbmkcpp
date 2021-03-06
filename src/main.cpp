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

const int width = 206;
const int height = 65;

int h,w;

void window(int sx, int sy, int x, int y, string title, function<void(WINDOW*)> f) {

    if (w >= width && h >= height) {
        int margin_w = (w-width)/2;
        int margin_h = (h-height)/2;

        if (sy <= 0) sy += height;
        if (sx <= 0) sx += width;

        if (y>=0) y += margin_h;
        else y += height+margin_h-sy;
        if (x>=0) x += margin_w;
        else x += width+margin_w-sx;
    }

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

    string name = com[0];

    if (name == "stop" && com.size() == 1) {
        r.move_rod(0);
        return true;
    }

    if (name == "select") {
        if (com.size() == 2) {
            if (com[1] == "all") {
                r.select_all();
                return true;
            } else if (com[1] == "sources") {
                r.select_sources();
                return true;
            }
        }
        if (com.size() == 3) {
            if (com[1] == "group") {
                stringstream ss(com[2]);
                int g;
                ss >> g;
                if (!ss) return false;
                r.select_group(g);
                return true;
            }
            stringstream ss1(com[1]);
            int x;
            ss1 >> x;
            if (!ss1) return false;
            stringstream ss2(com[2]);
            int y;
            ss2 >> y;
            if (!ss2) return false;
            return r.select_rod(x+3,y+3);
        }
    }
    bool pull = name=="pull";
    bool insert = name=="insert";
    if (pull || insert) {
        float dir = pull?-1:1;
        if (com.size() == 1) {
            r.move_rod(dir*100);
            return true;
        } else if (com.size() == 2) {
            stringstream ss1(com[1]);
            int dp;
            ss1 >> dp;
            if (!ss1) return false;
            r.move_rod(dir*dp*0.01);
            return true;
        } else return false;
    }
    
    if (name == "scram") {
        if (com.size() == 1) {
            r.scram();
            return true;
        } else if (com.size() == 2) {
            if (com[1] == "reset") {
                r.scram_reset();
                return true;
            } else return false;
        } else return false;
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

    auto loop_time = chrono::milliseconds(0);

    while (true) {
        // timer
        auto start = chrono::steady_clock::now();

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
        getmaxyx(stdscr, h, w);

        if (w < width || h < height) {
            window(40,2,0,0,"Please enlarge your terminal", [&](WINDOW *win) {});

        } else {

            // overview
            window(56,16,0,0,"Overview", [&](WINDOW *win) {
                stringstream ss;
                ss << loop_time.count() << "ms";

                mvwprintw(win, 2, 2, ss.str().c_str());
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

                for (int i=4;i<Reactor::reactor_width-4;i++) {
                    for (int j=4;j<Reactor::reactor_width-4;j++) {
                        auto &r = reactor.rods[i][j];
                        if (r.type != Reactor::RodType::Fuel && r.type != Reactor::RodType::None) {
                            int ii = (r.pos_z-r.min_pos_z)*100.0/(r.max_pos_z-r.min_pos_z);
                            if (!r.direction) ii = 100-ii;
                            string txt = (ii==100)?"**":string({(char)(ii/10+'0'),(char)(ii%10+'0')});
                            wattrset(win, COLOR_PAIR(colors[r.type]));
                            if (r.selected && (clock&0x10)) wattron(win, A_STANDOUT);
                            mvwprintw(win, 2+j/2, i, txt.c_str());
                            wattroff(win, A_STANDOUT);
                        }
                    }
                }
            });

            window(56,10,0,46, "Reactivity monitoring", [&](WINDOW *win) {
                mvwprintw(win, 2, 2, "Neutron flux : %g", reactor.get_neutron_flux());
                float period = reactor.get_period();
                stringstream ss;
                if (abs(period)>1000) ss << "***";
                else ss << (int)period << "s";
                mvwprintw(win, 3, 2, "Reactor period : %s", ss.str().c_str());
                mvwprintw(win, 4, 2, "Radial peak : %g", reactor.get_radial_peak());
            });

            // warnings
            window(40,-6,-1,0, "Alarms", [&](WINDOW *win) {

            });

            // command
            window(40,5,-1,-1, "Command", [&](WINDOW *win) {
                if (command_error) wattrset(win, COLOR_PAIR(1));
                mvwprintw(win, 2, 4, command.c_str());
                wattrset(win, A_NORMAL);
            });
        }

        reactor.step(dt);
        auto end = chrono::steady_clock::now();

        loop_time = chrono::duration_cast<chrono::milliseconds>(end-start);

        this_thread::sleep_for(chrono::milliseconds((int)(dt*1000))-loop_time);
        clock++;
    }
}