#pragma once
#include <ncurses.h>
#include <string>

struct gui_ctx {
	int short_color,auto_color,source_color,manual_color;
};

void print_text_center(WINDOW* win, int line, int w, std::string txt);
void init_gui(gui_ctx& ctx);
