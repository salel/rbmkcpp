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

const float graphite_width = 0.25;
const float reactor_height = Reactor::axial_sections*graphite_width;
const float graphite_holes_diameter = 0.114;
const float pressure_tube_inner_diameter = 0.08;
const float rod_diameter = 0.06;
const float pressure_tube_walls_thickness = 0.004;
const float absorber_length = 5.12;
const float short_absorber_length = 3.05;
const float tip_gap = 1.25;
const float tip_length = 4.5;
const float rod_insert_speed = 0.4;
const float rod_scram_speed = 0.4;
const float source_strength = 1E-10;
const float enrichment = 2E-2;
const float u235_neutrons = 2.43;

// volume occupied by material in section
const float rr_graphite_volume = graphite_width*graphite_width*graphite_width;
const float rrc_coolant_volume = graphite_width*M_PI*pressure_tube_inner_diameter*pressure_tube_inner_diameter/4;
const float graphite_volume = (graphite_width*graphite_width-M_PI*graphite_holes_diameter*graphite_holes_diameter/4)*graphite_width;
const float b4c_volume = graphite_width*M_PI*rod_diameter*rod_diameter/4;
const float u_volume = 3.734E-4;

const float coolant_volume = graphite_width*M_PI*(pressure_tube_inner_diameter*pressure_tube_inner_diameter-rod_diameter*rod_diameter)/4;

// macroscopic cross sections (m-1)
const float graphite_abs_mcs = 2.26E-2;
const float b4c_abs_mcs = 8.43E3;
const float u235_fission_mcs = 1.425E3;
const float u235_abs_mcs = 2.421E2;
const float u238_abs_mcs = 4.89;
const float water_abs_mcs = 1.338;

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

    // Generate groups (outwards to inwards)
    groups.push_back({
        {18,2},{22,2},{26,2},{30,2},{36,4},{38,6},{40,8},{42,10},{44,12},
        {46,18},{46,22},{46,26},{46,30},{46,34},{44,36},{42,38},{40,40},{38,42},{36,44},
        {34,46},{30,46},{26,46},{22,46},{18,46},{12,44},{10,42},{8,40},{6,38},{4,36},
        {2,30},{2,26},{2,22},{2,18},{4,12},{6,10},{8,8},{10,6},{12,4},
    });
    groups.push_back({
        {16,4},{20,4},{24,4},{28,4},{32,4},
        {34,6},{36,8},{38,10},{40,12},{42,14},{44,16},
        {44,20},{44,24},{44,28},{44,32},
        {42,34},{40,36},{38,38},{36,40},{34,42},{32,44},
        {28,44},{24,44},{20,44},{16,44},
        {14,42},{12,40},{10,38},{8,36},{6,34},{4,32},
        {4,28},{4,24},{4,20},{4,16},
        {6,14},{8,12},{10,10},{12,8},{14,6}
    });

    groups.push_back({
        {18,6},{22,6},{26,6},{30,6},
        {34,10},{36,12},{38,14},
        {42,18},{42,22},{42,26},{42,30},
        {38,34},{36,36},{34,38},
        {30,42},{26,42},{22,42},{18,42},
        {14,38},{12,36},{10,34},
        {6,30},{6,26},{6,22},{6,18},
        {10,14},{12,12},{14,10},
    });

    groups.push_back({
        {20,8},{24,8},{28,8},
        {30,10},{32,12},{34,14},{36,16},{38,18},
        {40,20},{40,24},{40,28},
        {38,30},{36,32},{34,34},{32,36},{30,38},
        {28,40},{24,40},{20,40},
        {18,38},{16,36},{14,34},{12,32},{10,30},
        {8,28},{8,24},{8,20},
        {10,18},{12,16},{14,14},{16,12},{18,10},
        {22,10},{26,10},{38,22},{38,26},
        {22,38},{26,38},{10,22},{10,26}
    });

    groups.push_back({
        {20,12},{22,14},{26,14},{28,12},
        {30,14},{34,18},
        {36,20},{34,22},{34,26},{36,28},
        {34,30},{30,34},
        {28,36},{26,34},{22,34},{20,36},
        {18,34},{14,30},
        {12,28},{14,26},{14,22},{12,20},
        {14,18},{18,14}
    });

    groups.push_back({
        {16,20},{18,18},{20,16},
        {28,32},{30,30},{32,28},
        {28,16},{30,18},{32,20},
        {16,28},{18,30},{20,32},
        {18,22},{30,22},{18,26},{30,26},
        {22,18},{22,30},{26,18},{26,30},
    });

    groups.push_back({
        {20,20},{22,22},{24,24},{26,26},{28,28},
        {28,20},{26,22},{22,26},{20,28}
    });

    // source layout

    center_sources = {
        {27,19},
        {27,35},
        {19,27},
        {35,27}
    };


    outer_sources = {
        {19,11},
        {35,11},
        {19,43},
        {35,43},
        {11,19},
        {11,35},
        {43,19},
        {43,35}
    };
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

void Reactor::select_sources() {
    if (scrammed) return;
    unselect_all();
    for (auto r : center_sources) {
        rods[r.first][r.second].selected = true;
    }
    for (auto r : outer_sources) {
        rods[r.first][r.second].selected = true;
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
    const float prompt_gen_time = 0.002;

    // double buffer for diffusion
    float db_neutron_flux[reactor_width][reactor_width][axial_sections];

    for (int it = 0;it<(dt/prompt_gen_time);it++) {
        // sources and sinks
        for (int i = 0; i < reactor_width; ++i) {
            for (int j = 0; j < reactor_width; ++j) {
                auto& r = rods[i][j];
                for (int k=0;k<axial_sections;k++) {
                    const auto n = neutron_flux[i][j][k];
                    float nn = 0;
                    float constant_source = 0;
                    if (columns[i][j] == ColumnType::FC_CPS) {
                        float bound_min_z = k*graphite_width;
                        if (r.type == RodType::Source) {
                            const float source_length = 7;
                            const float source_bound_min = max(0.f, min(r.pos_z-bound_min_z, graphite_width));
                            const float source_bound_max = max(0.f, min(r.pos_z-bound_min_z+source_length, graphite_width));
                            const float source_content = (source_bound_max-source_bound_min)/graphite_width;
                            constant_source = source_content*source_strength;
                        } else if (r.type == RodType::Manual || r.type == RodType::Automatic || r.type == RodType::Short) {
                            const float abs_length = (r.type == RodType::Short)?short_absorber_length:absorber_length;
                            const float boron_bound_min = max(0.f,min(r.pos_z-bound_min_z,graphite_width));
                            const float boron_bound_max = max(0.f,min(r.pos_z+abs_length-bound_min_z,graphite_width));

                            const float boron_content = (boron_bound_max-boron_bound_min)/graphite_width;

                            nn -= boron_content*b4c_volume*b4c_abs_mcs;
                            nn -= (1-boron_content)*b4c_volume*water_abs_mcs;
                        } else if (r.type == RodType::Fuel) {
                            if (k >= 2 && k < reactor_width-2) {
                                const float u235_fission = enrichment*u235_fission_mcs;
                                const float u235_capture = enrichment*u235_abs_mcs;
                                const float u238_capture = (1-enrichment)*u238_abs_mcs;

                                nn += u_volume*(u235_fission*(u235_neutrons-1)-u235_capture-u238_capture);
                            }
                        }
                        nn -= coolant_volume*water_abs_mcs;
                        nn -= graphite_volume*graphite_abs_mcs;
                    } else if (columns[i][j] == ColumnType::RR) {
                        nn -= rr_graphite_volume*graphite_abs_mcs;
                    } else if (columns[i][j] == ColumnType::RRC) {
                        nn -= graphite_volume*graphite_abs_mcs;
                        nn -= rrc_coolant_volume*water_abs_mcs;
                    }
                    db_neutron_flux[i][j][k] = n*(1+max(nn, -1.f))+constant_source;
                }
            }
        }

        // diffuse flux
        const float coef = 1.0/9.0;
        auto &buf_0 = db_neutron_flux;
        for (int i = 0; i < reactor_width; ++i) {
            for (int j = 0; j < reactor_width; ++j) {
                for (int k=0;k<axial_sections;k++) {
                    float n = buf_0[i][j][k];
                    float n1 = k>0?buf_0[i][j][k-1]:0;
                    float n2 = i>0?buf_0[i-1][j][k]:0;
                    float n3 = j>0?buf_0[i][j-1][k]:0;
                    float n4 = (k<axial_sections-1)?buf_0[i][j][k+1]:0;
                    float n5 = (i<reactor_width -1)?buf_0[i+1][j][k]:0;
                    float n6 = (j<reactor_width -1)?buf_0[i][j+1][k]:0;
                    neutron_flux[i][j][k] = n*coef + (n1+n2+n3+n4+n5+n6)*(1-coef)/6;
                }
            }
        }
    }
    // telemetry

    if (telemetry_time >= telemetry_dt) {
        // Neutron total
        total_neutron_flux = 0;

        for (int i = 0; i < reactor_width; ++i) {
            for (int j = 0; j < reactor_width; ++j) {
                if (columns[i][j] == ColumnType::FC_CPS) {
                    for (int k=0;k<axial_sections;k++) {
                        auto &n = neutron_flux[i][j][k];
                        total_neutron_flux += n;
                    }
                }
            }
        }
        // get peaks
        float center_flux = 0;
        float outer_flux = 0;

        for (int k=0;k<axial_sections;k++) {
            for (auto p : center_sources) {
                center_flux += neutron_flux[p.first][p.second][k];
            }
            for (auto p : outer_sources) {
                outer_flux += neutron_flux[p.first][p.second][k];
            }
        }
        radial_peak = (outer_sources.size()*center_flux)/(center_sources.size()*outer_flux);
        // multiplication per dt
        float change = (total_neutron_flux/previous_flux);
        previous_flux = total_neutron_flux;
        // multiplication per second
        float change_s = pow(change, 1/telemetry_time);
        period = 1.0/log(change_s);

        telemetry_time = 0;
    }

    telemetry_time += dt;

}

float Reactor::get_neutron_flux() {
    return total_neutron_flux;
}

float Reactor::get_period() {
    return period;
}

float Reactor::get_radial_peak() {
    return radial_peak;
}

void Reactor::scram() {
    scrammed = true;
}

void Reactor::scram_reset() {
    scrammed = false;
}

Reactor::Rod::Rod(RodType type): type(type) {
    if (type == RodType::Source) {
        min_pos_z = -7;
        max_pos_z = 0.5;
        direction = true;
        pos_z = min_pos_z;
    } else if (type == RodType::Manual || type == RodType::Automatic) {
        min_pos_z = -absorber_length+0.5;
        max_pos_z = 0.5;
        direction = true;
        pos_z = max_pos_z;
    } else if (type == RodType::Short) {
        min_pos_z = reactor_height-short_absorber_length-0.5;
        max_pos_z = reactor_height-0.5;
        direction = false;
        pos_z = min_pos_z;
    } else if (type == RodType::Fuel) {
        min_pos_z = 0;
        max_pos_z = 0;
        direction = true;
        pos_z = 0;
    }
    target_z = pos_z;
}