#pragma once

#include "view.h"

class ViewHelp : public View {
public:
	std::string get_name() { return "Help";}
	std::string get_fancy_name() { return "Welcome page and help";}
	void display(WINDOW* win, int w, Reactor &r, gui_ctx &ctx) {
		print_text_center(win, 5, w, "Welcome!");
	}
	void input(int character, Reactor &r) {

	}
};