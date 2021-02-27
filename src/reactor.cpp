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
                for (int k=0;k<2;k++) {
                    rods[x[k]][y[k]] = Rod(rod_decode[val]);
                }
            }
        }
    }

    // Generate Fuel layout, withdraw all outside the core
    for (int i=4;i<reactor_width-4;i++) {
        for (int j=4;j<reactor_width-4;j++) {
            if (columns[i][j] == ColumnType::FC_CPS &&
                rods[i][j].type == RodType::None)
                    rods[i][j] = Rod(RodType::Fuel);
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

    // Generate groups
    groups.push_back({
        {18,2},{22,2},{26,2},{30,2},{36,4},{38,6},{40,8},{42,10},{44,12},
        {46,18},{46,22},{46,26},{46,30},{46,34},{44,36},{42,38},{40,40},{38,42},{36,44},
        {34,46},{30,46},{26,46},{22,46},{18,46},{12,44},{10,42},{8,40},{6,38},{4,36},
        {2,30},{2,26},{2,22},{2,18},{4,12},{6,10},{8,8},{10,6},{12,4}
    });
}

bool Reactor::select_rod(int x, int y) {
    if (scrammed) return true;
    if (x < 0 || x >= reactor_width || y < 0 || y>= reactor_width) return false;
    unselect_all();
    auto& r = rods[x][y];
    if (r.type == RodType::Manual || r.type == RodType::Short) {
        r.target_z = r.pos_z;
        r.selected = true;
        return true;
    }
    return false;
}

void Reactor::select_all() {
    if (scrammed) return;
    unselect_all();
    for (int i=0;i<reactor_width;i++) {
        for (int j=0;j<reactor_width;j++) {
            if (rods[i][j].type == RodType::Manual || rods[i][j].type == RodType::Short) rods[i][j].selected = true;
        }
    }
}

void Reactor::select_group(int g) {
    if (scrammed) return;
    if (g < 1 || g> (int)groups.size()) return;
    unselect_all();
    for (auto r : groups[g-1]) {
        rods[r.first+3][r.second+3].selected = true;
    }
}

void Reactor::unselect_all() {
    for (int i=0;i<reactor_width;i++) {
        for (int j=0;j<reactor_width;j++) {
            rods[i][j].selected = false;
            if (rods[i][j].type != RodType::Automatic) rods[i][j].target_z = rods[i][j].pos_z;
        }
    }
}

void Reactor::move_rod(float dp) {
    for (int i=0;i<reactor_width;i++) {
        for (int j=0;j<reactor_width;j++) {
            auto &r = rods[i][j];
            if (r.selected) r.target_z = max(r.min_pos_z,min(r.pos_z+(r.direction?1:-1)*dp, r.max_pos_z));
        }
    }
}

void Reactor::step(float dt) {
    // Scram movement
    if (scrammed) {
        unselect_all();
        for (int i=0;i<reactor_width;i++) {
            for (int j=0;j<reactor_width;j++) {
                auto &r = rods[i][j];
                if (r.type == RodType::Manual || r.type == RodType::Automatic) {
                    r.target_z = r.max_pos_z;
                    r.pos_z = max(r.min_pos_z, min(r.pos_z + dt*rod_scram_speed, r.max_pos_z));
                } else {
                    // Stop all else movements
                    r.target_z = r.pos_z;
                }
            }
        }
    }
    // Rod movement
    for (int i=0;i<reactor_width;i++) {
        for (int j=0;j<reactor_width;j++) {
            auto &r = rods[i][j];
            if (r.pos_z > r.target_z)
                r.pos_z = max(r.target_z, r.pos_z - rod_insert_speed*dt);
            else 
                r.pos_z = min(r.target_z, r.pos_z + rod_insert_speed*dt);
        }
    }
    // Neutron flux computation

    // double buffered neutron flux for diffusion
    float db_neutron_flux[reactor_width][reactor_width][axial_sections];
    auto buf_0 = neutron_flux;
    auto buf_1 = db_neutron_flux;
    for (int l = 0; l < 9; l++) {

        // sources and sinks
        for (int i = 0; i < reactor_width; ++i) {
            for (int j = 0; j < reactor_width; ++j) {
                auto& r = rods[i][j];
                for (int k=0;k<axial_sections;k++) {
                    auto &n = buf_0[i][j][k];
                    float bound_min_z = k*graphite_width;
                    if (r.type == RodType::Source) {
                        n+=source_strength;
                    } else if (r.type == RodType::Manual || r.type == RodType::Automatic || r.type == RodType::Short) {
                        float abs_length = (r.type == RodType::Short)?short_absorber_length:absorber_length;
                        float boron_bound_min = max(0.f,min(r.pos_z-bound_min_z,graphite_width));
                        float boron_bound_max = max(0.f,min(r.pos_z+abs_length-bound_min_z,graphite_width));

                        float boron_content = (boron_bound_max-boron_bound_min)/graphite_width;

                        n*=(1-boron_absorption*boron_content-water_absorption*(1-boron_content));
                    } else if (r.type == RodType::Fuel) {
                        float fuel_content = 0.001;
                        float u235_content = fuel_content*enrichment;
                        float u238_content = fuel_content*(1-enrichment);
                        n *= (1+u235_content*(u235_fission*(u235_neutrons-1) - u235_capture) - u238_content*u238_capture);
                    }
                    n*=(1-graphite_absorption);
                }
            }
        }

        // diffuse flux
        for (int i = 0; i < reactor_width; ++i) {
            for (int j = 0; j < reactor_width; ++j) {
                for (int k=0;k<axial_sections;k++) {
                    auto &n = buf_0[i][j][k];
                    float n1 = k>0?buf_0[i][j][k-1]:0;
                    float n2 = i>0?buf_0[i-1][j][k]:0;
                    float n3 = j>0?buf_0[i][j-1][k]:0;
                    float n4 = (k<axial_sections-1)?buf_0[i][j][k+1]:0;
                    float n5 = (i<reactor_width -1)?buf_0[i+1][j][k]:0;
                    float n6 = (j<reactor_width -1)?buf_0[i][j+1][k]:0;
                    buf_1[i][j][k] = (2*n+n1+n2+n3+n4+n5+n6)*0.125;
                }
            }
        }
        // swap buffers
        swap(buf_0, buf_1);
    }

    // Neutron total
    float previous_flux = total_neutron_flux;
    total_neutron_flux = 0;
    for (int i = 0; i < reactor_width; ++i) {
        for (int j = 0; j < reactor_width; ++j) {
            for (int k=0;k<axial_sections;k++) {
                total_neutron_flux += neutron_flux[i][j][k];
            }
        }
    }

    // multiplication per dt
    float change = (total_neutron_flux/previous_flux);
    // multiplication per second
    float change_s = pow(change, 1/dt);
    period = 1.0/log(change_s);
}

float Reactor::get_neutron_flux() {
    return total_neutron_flux;
}

float Reactor::get_period() {
    return period;
}

void Reactor::scram() {
    scrammed = true;
}

void Reactor::scram_reset() {
    scrammed = false;
}