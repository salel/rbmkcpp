#include "reactor.h"

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

using namespace std;


Reactor::Reactor() {
    // layouts

    // 2-bit alignment
    // 0 = None, 1 = RRC, 2 = RR, 3 = FC_CPS
    const uint32_t layout[] = {
        0xFFFFFFFF,0xEA000000,
        0xFFFFFFFF,0xAA000000,
        0xFFFFFFFE,0xA9000000,
        0xFFFFFFFA,0xA4000000,
        0xFFFFFFAA,0x90000000,
        0xFFFFFAAA,0x40000000,
        0xFFFEAAA5,0,
        0xAAAAAA50,0,
        0xAAAAA500,0,
        0xAA955000,0,
        0x55400000,0,
    };

    // 3-bit alignment
    // 0 = None, 1 = Manual, 2 = SAR, 3 = automatic, 4 = source
    const uint32_t cps_layout[] = {
        0112000000,
        0111110000,
        0214121000,
        0111111100,
        0312111210,
        0111311110,
        0214121412,
        0131111111,
        0112131211,
        0131111111,
        0214121412,
        0111311110,
        0312111210,
        0111111100,
        0214121000,
        0111110000,
        0112100000
    };

    // Generate graphite stack layout
    for (int i=0;i<reactor_width;i++) {
        for (int j=0;j<reactor_width;j++) {
            auto &c = columns[i][j];
            c = ColumnType::RR;
            int i0 = floor(abs(i-reactor_width/2+0.5)); // 0-27
            int j0 = floor(abs(j-reactor_width/2+0.5)); // 0-27
            if (i0<=16 && j0<=16) {
                c = ColumnType::FC_CPS;
            } else if (i0>19 && j0>19) {
                c = ColumnType::None;
            } else {
                int i1 = min(i0,j0); // 0-19
                int j1 = max(i0,j0); // 17-27
                int cell = layout[(j1-17)*2+i1/16];
                int val = (cell >> ((15-(i1%16))*2)) & 0x3;
                if (val == 1) c = ColumnType::RRC;
                else if (val == 2) c = ColumnType::RR;
                else if (val == 3) c = ColumnType::FC_CPS;
                else c = ColumnType::None;
            }
        }
    }
    map<int, RodType> rod_decode = {{1, RodType::Manual}, {2, RodType::Short}, {3, RodType::Automatic}, {4, RodType::Source}};

    // Generate CPS layout, scram all control rods and withdraw sources
    for (int i=0;i<17;i++) {
        for (int j=0;j<9;j++) {
            int val = (cps_layout[i]>>(24-3*j))&07;
            if (val > 0) {
                int x[] = {11+i*2+j*2, 11+i*2-j*2};
                int y[] = {11+i*2-j*2, 11+i*2+j*2};
                for (int k=0;k<((j==0)?1:2);k++) {
                    rods.emplace_back(x[k],y[k], rod_decode[val]);
                }
            }
        }
    }

    // Generate Fuel layout, withdraw all outside the core
    for (int i=4;i<52;i++) {
        for (int j=4;j<52;j++) {
            if (columns[i][j] == ColumnType::FC_CPS) {
                if (!get_rod(i,j)) {
                    rods.emplace_back(i,j, RodType::Fuel);
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

int Reactor::indicated_pos(int i) {
    return (i < 4) ? i+51 : ((i>=reactor_width-4)?i+3:i-3);
}

bool Reactor::select_rod(int x, int y) {
    auto r = get_rod(x, y);
    if (r && (r->type == RodType::Manual || r->type == RodType::Short)) {
        selected_rod = r;
        target_rod_depth = r->pos_z;
        return true;
    }
    return false;
}
Reactor::Rod *Reactor::get_selected_rod() {
    return selected_rod;
}

void Reactor::move_rod(float dp) {
    if (selected_rod) {
        target_rod_depth = max(selected_rod->min_pos_z,min(selected_rod->pos_z+(selected_rod->direction?1:-1)*dp, selected_rod->max_pos_z));
    }
}

void Reactor::step(float dt) {
    if (scrammed) {
        target_rod_depth = 100;
        for (auto& r : rods) {
            r.pos_z = max(r.min_pos_z, min(r.pos_z + (r.direction?1:-1)*dt*rod_scram_speed, r.max_pos_z));
        }
    }
    if (selected_rod) {
        if ((selected_rod->pos_z > target_rod_depth)) 
            selected_rod->pos_z = max(target_rod_depth, selected_rod->pos_z - rod_insert_speed*dt);
        else 
            selected_rod->pos_z = min(target_rod_depth, selected_rod->pos_z + rod_insert_speed*dt);
    }
}

void Reactor::scram() {
    scrammed = true;
}

void Reactor::scram_reset() {
    scrammed = false;
}