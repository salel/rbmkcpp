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

using namespace std;

const static float dt = 0.1;
const static float reactor_height = 8;
const static float graphite_width = 0.25;
const static float graphite_holes_diameter = 0.114;
const static float pressure_tube_inner_diameter = 0.08;
const static float pressure_tube_walls_thickness = 0.004;
const static float absorber_length = 5.12;
const static float short_absorber_length = 3.05;
const static float tip_gap = 1.25;
const static float tip_length = 4.5;
const static float rod_insert_speed = 0.4;
const static float fuel_active_length = 6.862;

const static int reactor_width = 56;
const static int axial_sections = reactor_height/graphite_width;

enum class ColumnType {
    None,
    FC_CPS, // Fuel Channel or Control and Protection System channel
    RR, // Reflector column
    RRC // Reflector Coolant Channel
};

struct Rod {
    Rod(int x, int y, float min_pos_z, float max_pos_z, bool direction, bool withdrawn=false): 
        pos_x(x), pos_y(y), 
        min_pos_z(min_pos_z), max_pos_z(max_pos_z),
        direction(direction) {
            pos_z = (!(direction^withdrawn))?max_pos_z:min_pos_z;
    }
    int pos_x;
    int pos_y;
    float pos_z;
    float min_pos_z;
    float max_pos_z;
    bool direction; // 0 for below, 1 for above
};

bool isrod(vector<Rod> &s, int x, int y) {
    for (unsigned i=0;i<s.size();i++) {
        if (s[i].pos_x == x && s[i].pos_y == y) return true;
    }
    return false;
}

ColumnType columns[reactor_width][reactor_width];
double neutron_flux[reactor_width][reactor_width][axial_sections];
vector<Rod> manual_rods;
vector<Rod> automatic_rods;
vector<Rod> short_rods;
vector<Rod> source_rods;
vector<Rod> fuel_rods;

void init_reactor() {
    // layouts

    // 0 = None, 1 = RRC, 2 = RR, 3 = FC_CPS
    const uint32_t layout[] = {
        0x33333333,0x33333333,0x32220000,
        0x33333333,0x33333333,0x22220000,
        0x33333333,0x33333332,0x22210000,
        0x33333333,0x33333322,0x22100000,
        0x33333333,0x33332222,0x21000000,
        0x33333333,0x33222222,0x10000000,
        0x33333332,0x22222211,0,
        0x22222222,0x22221100,0,
        0x22222222,0x22110000,0,
        0x22222111,0x11000000,0,
        0x11111000,0,0,
    };

    // 0 = None, 1 = Manual, 2 = SAR, 3 = automatic, 4 = source
    const uint32_t cps_layout[] = {
        0x00000000,0x10101010,0x00000000,
        0x00000201,0x02010201,0x02000000,
        0x00001010,0x10101010,0x10100000,
        0x00010104,0x01010104,0x01010000,
        0x00101010,0x10101010,0x10101000,
        0x02010201,0x02030201,0x02010200,
        0x00101010,0x10101010,0x10101000,
        0x01040103,0x01040103,0x01040100,
        0x10101010,0x10101010,0x10101010,
        0x02010201,0x02030201,0x02010200,
        0x10101010,0x10101010,0x10101010,
        0x01010304,0x03010304,0x03010100,
        0x10101010,0x10101010,0x10101010,
        0x02010201,0x02030201,0x02010200,
        0x10101010,0x10101010,0x10101010,
        0x01040103,0x01040103,0x01040100,
        0x00101010,0x10101010,0x10101010,
        0x02010201,0x02030201,0x02010200,
        0x00101010,0x10101010,0x10101000,
        0x00010104,0x01010104,0x01010000,
        0x00001010,0x10101010,0x10100000,
        0x00000201,0x02010201,0x02000000,
        0x00000000,0x10101010,0x10000000,
    };


    // Generate graphite stack layout
    for (int i=0;i<reactor_width;i++) {
        for (int j=0;j<reactor_width;j++) {
            auto &c = columns[i][j];
            c = ColumnType::RR;
            int i0 = floor(abs(i-reactor_width/2+0.5));
            int j0 = floor(abs(j-reactor_width/2+0.5));
            if (i0<=16 && j0<=16) {
                c = ColumnType::FC_CPS;
            } else if (i0>19 && j0>19) {
                c = ColumnType::None;
            } else {
                int i1 = min(i0,j0);
                int j1 = max(i0,j0);
                int cell = layout[(j1-17)*3+i1/8];
                int val = (cell >> ((7-(i1%8))*4)) & 0xF;
                if (val == 1) c = ColumnType::RRC;
                else if (val == 2) c = ColumnType::RR;
                else if (val == 3) c = ColumnType::FC_CPS;
                else c = ColumnType::None;
            }
        }
    }

    // Generate CPS layout, scram all control rods and withdraw sources
    for (int i=0;i<23;i++) {
        for (int j=0;j<23;j++) {
            int cell = cps_layout[j*3+i/8];
            int val = (cell >> ((7-(i%8))*4))&0xF;
            int pos_x = i*2+5;
            int pos_y = j*2+5;
            if (val == 1) manual_rods.emplace_back(pos_x, pos_y, -absorber_length, 0, true);
            else if (val == 2) short_rods.emplace_back(pos_x, pos_y, reactor_height-short_absorber_length, reactor_height, false);
            else if (val == 3) automatic_rods.emplace_back(pos_x, pos_y, -absorber_length, 0, true);
            else if (val == 4) source_rods.emplace_back(pos_x, pos_y, -absorber_length, 0, true, true);
        }
    }

    // Generate Fuel layout, withdraw all outside the core
    for (int i=4;i<52;i++) {
        for (int j=4;j<52;j++) {
            if (columns[i][j] == ColumnType::FC_CPS) {
                if (!isrod(manual_rods, i, j) && 
                    !isrod(automatic_rods, i,j) && 
                    !isrod(short_rods, i,j) && 
                    !isrod(source_rods, i,j)) {
                    fuel_rods.emplace_back(i,j, -3*reactor_height, 0, true, true);
                }
            }
        }
    }

    // Initialize neutron flux
    for (int i=0;i<reactor_width;i++) {
        for (int j=0;j<reactor_width;j++) {
            for (int k=0;k<reactor_height;k++) {
                neutron_flux[i][j][k] = 0;
            }
        }
    }
}

int indicated_pos(int i) {
    return (i < 4) ? i+51 : ((i>=reactor_width-4)?i+3:i-3);
}

void print_layout() {
    auto isrod = [](vector<Rod> &s, int x, int y){
        for (unsigned i=0;i<s.size();i++) {
            if (s[i].pos_x == x && s[i].pos_y == y) return true;
        }
        return false;
    };
    int fc_cps_count = 0;
    int rr_count = 0;
    int rrc_count = 0;
    for (int i=0;i<reactor_width;i++) {
        int ii = indicated_pos(i);
        cout << string(1,((ii/10)+'0')) + (char)('0'+(ii%10));
        for (int j=0;j<reactor_width;j++) {
            auto &c = columns[i][j];
            if (c == ColumnType::FC_CPS) {
                if (isrod(manual_rods, i,j)) cout << "\033[42m  ";
                else if (isrod(short_rods, i,j)) cout << "\033[43m  ";
                else if (isrod(automatic_rods, i,j)) cout << "\033[41m  ";
                else if (isrod(source_rods, i,j)) cout << "\033[44m  ";
                else {
                    fc_cps_count++;
                    cout << "\033[47m  ";
                }
            } else if (c == ColumnType::RR) {
                rr_count++;
                cout << "\033[45m  ";
            } else if (c == ColumnType::RRC) {
                rrc_count++;
                cout << "\033[46m  ";
            } else {
                cout << "\033[40m  ";
            }
        }
        cout << "\033[40m" << endl;
    }
    cout << "  ";
    for (int i=0;i<reactor_width;i++) {
        int ii = indicated_pos(i);
        cout << string(1,((ii/10)+'0')) + (char)('0'+(ii%10));
    }
    cout << endl;
    cout << fc_cps_count << endl;
    cout << rr_count << endl;
    cout << rrc_count << endl;
}

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

    vector<View> views;
    init_reactor();

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

    views.push_back({"Help", "Welcome page & help", [](WINDOW* main_win, int w) {
        wmove(main_win, 5, (w-help_text.length())/2);
        wprintw(main_win, help_text.c_str());
    }});

    views.push_back({"Reactor", "Reactor controls", [](WINDOW* main_win, int w) {
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
        print_rod(manual_rods);
        wattrset(main_win, short_color);
        print_rod(short_rods);
        wattrset(main_win, auto_color);
        print_rod(automatic_rods);
        wattrset(main_win, source_color);
        print_rod(source_rods);
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