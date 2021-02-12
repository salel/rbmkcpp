#pragma once

#include "view.h"

class ViewReactor : public View {
public:
	std::string get_name() { return "Reactor";}
	std::string get_fancy_name() { return "Reactor controls";}
	void display(WINDOW* win, int w, Reactor &r, gui_ctx &ctx) {
		auto print_rod = [&](auto &s) {
            for (auto r : s) {
                wmove(win, 2+r.pos_y/2, 2+r.pos_x);
                int ii = (r.pos_z-r.min_pos_z)*100/(r.max_pos_z-r.min_pos_z);
                if (r.direction) ii = 100-ii;
                auto txt = (ii==100)?"**":std::string(1,((ii/10)+'0')) + (char)('0'+(ii%10));
                wprintw(win, txt.c_str());
            }
        };

        wattrset(win, ctx.manual_color);
        print_rod(r.manual_rods);
        wattrset(win, ctx.short_color);
        print_rod(r.short_rods);
        wattrset(win, ctx.auto_color);
        print_rod(r.automatic_rods);
        wattrset(win, ctx.source_color);
        print_rod(r.source_rods);
	}
	void input(int character, Reactor &r) {

	}
};