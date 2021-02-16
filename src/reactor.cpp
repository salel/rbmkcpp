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

    // Generate CPS layout, scram all control rods and withdraw sources
    for (int i=0;i<17;i++) {
        for (int j=0;j<9;j++) {
            int val = (cps_layout[i]>>(24-3*j))&07;
            if (val > 0) {
                int x[] = {11+i*2+j*2, 11+i*2-j*2};
                int y[] = {11+i*2-j*2, 11+i*2+j*2};
                for (int k=0;k<2;k++) {
                    if (val == 1) manual_rods.emplace_back(x[k], y[k], -absorber_length, 0, true);
                    else if (val == 2) short_rods.emplace_back(x[k], y[k], reactor_height-short_absorber_length, reactor_height, false);
                    else if (val == 3) automatic_rods.emplace_back(x[k], y[k], -absorber_length, 0, true);
                    else if (val == 4) source_rods.emplace_back(x[k], y[k], -absorber_length, 0, true, true);
                }
            }
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

int Reactor::indicated_pos(int i) {
    return (i < 4) ? i+51 : ((i>=reactor_width-4)?i+3:i-3);
}

void Reactor::print_layout() {
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