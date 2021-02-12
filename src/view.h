#pragma once
#include <string>
#include <ncurses.h>
#include "gui_utils.h"
#include "reactor.h"

class View {
public:
	virtual std::string get_name()=0;
	virtual std::string get_fancy_name()=0;
	virtual void display(WINDOW* win, int w, Reactor &r, gui_ctx &ctx)=0;
	virtual void input(int character, Reactor &r) {};
};