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

    // 0 = None, 1 = RRC, 2 = RR, 3 = FC_CPS
    const uint32_t layout[] = {
        0x33333333,0x33333333,0x32220000,
        0x33333333,0x33333333,0x22220000,
        0x33333333,0x33333332,0x22210000,
        0x33333333,0x33333322,0x22100000,
        0x33333333,0x33332222,0x21000000,
        0x33333333,0x33222222,0x10000000,
        0x33333332,0x22222211,0,
        0x22222222,0x22221100,0,
        0x22222222,0x22110000,0,
        0x22222111,0x11000000,0,
        0x11111000,0,0,
    };

    // 0 = None, 1 = Manual, 2 = SAR, 3 = automatic, 4 = source
    const uint32_t cps_layout[] = {
        0x00000000,0x10101010,0x00000000,
        0x00000201,0x02010201,0x02000000,
        0x00001010,0x10101010,0x10100000,
        0x00010104,0x01010104,0x01010000,
        0x00101010,0x10101010,0x10101000,
        0x02010201,0x02030201,0x02010200,
        0x00101010,0x10101010,0x10101000,
        0x01040103,0x01040103,0x01040100,
        0x10101010,0x10101010,0x10101010,
        0x02010201,0x02030201,0x02010200,
        0x10101010,0x10101010,0x10101010,
        0x01010304,0x03010304,0x03010100,
        0x10101010,0x10101010,0x10101010,
        0x02010201,0x02030201,0x02010200,
        0x10101010,0x10101010,0x10101010,
        0x01040103,0x01040103,0x01040100,
        0x00101010,0x10101010,0x10101010,
        0x02010201,0x02030201,0x02010200,
        0x00101010,0x10101010,0x10101000,
        0x00010104,0x01010104,0x01010000,
        0x00001010,0x10101010,0x10100000,
        0x00000201,0x02010201,0x02000000,
        0x00000000,0x10101010,0x10000000,
    };


    // Generate graphite stack layout
    for (int i=0;i<reactor_width;i++) {
        for (int j=0;j<reactor_width;j++) {
            auto &c = columns[i][j];
            c = ColumnType::RR;
            int i0 = floor(abs(i-reactor_width/2+0.5));
            int j0 = floor(abs(j-reactor_width/2+0.5));
            if (i0<=16 && j0<=16) {
                c = ColumnType::FC_CPS;
            } else if (i0>19 && j0>19) {
                c = ColumnType::None;
            } else {
                int i1 = min(i0,j0);
                int j1 = max(i0,j0);
                int cell = layout[(j1-17)*3+i1/8];
                int val = (cell >> ((7-(i1%8))*4)) & 0xF;
                if (val == 1) c = ColumnType::RRC;
                else if (val == 2) c = ColumnType::RR;
                else if (val == 3) c = ColumnType::FC_CPS;
                else c = ColumnType::None;
            }
        }
    }

    // Generate CPS layout, scram all control rods and withdraw sources
    for (int i=0;i<23;i++) {
        for (int j=0;j<23;j++) {
            int cell = cps_layout[j*3+i/8];
            int val = (cell >> ((7-(i%8))*4))&0xF;
            int pos_x = i*2+5;
            int pos_y = j*2+5;
            if (val == 1) manual_rods.emplace_back(pos_x, pos_y, -absorber_length, 0, true);
            else if (val == 2) short_rods.emplace_back(pos_x, pos_y, reactor_height-short_absorber_length, reactor_height, false);
            else if (val == 3) automatic_rods.emplace_back(pos_x, pos_y, -absorber_length, 0, true);
            else if (val == 4) source_rods.emplace_back(pos_x, pos_y, -absorber_length, 0, true, true);
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