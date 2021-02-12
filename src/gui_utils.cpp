#include "view.h"

using namespace std;

void print_text_center(WINDOW* win, int line, int w, std::string txt) {
	wmove(win, line, (w-txt.length())/2);
    wprintw(win, txt.c_str());
}

void init_gui(gui_ctx &ctx) {

	initscr();
    
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

	start_color();

	ctx.manual_color = COLOR_PAIR(1);
	ctx.short_color = COLOR_PAIR(2);
	ctx.auto_color = COLOR_PAIR(3);
	ctx.source_color = COLOR_PAIR(4);

	init_pair(1, COLOR_BLACK, COLOR_GREEN);
    init_pair(2, COLOR_BLACK, COLOR_YELLOW);
    init_pair(3, COLOR_BLACK, COLOR_RED);
    init_pair(4, COLOR_BLACK, COLOR_CYAN);
}