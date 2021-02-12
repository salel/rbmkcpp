#pragma once

#include "view.h"

class ViewExit : public View {
public:
	std::string get_name() { return "Exit";}
	std::string get_fancy_name() { return "Are you sure you want to exit?";}
	void display(WINDOW* win, int w, Reactor &r, gui_ctx &ctx) {
		print_text_center(win, 5, w, "Are you sure you want to exit? Press enter to quit, Backspace to return to the controls menu.");
	}
	void input(int ch, Reactor &r) {
		if (ch == 10) {
            endwin();
            exit(0);
        }
	}
};