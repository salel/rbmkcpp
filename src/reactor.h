#pragma once

#include <vector>

class Reactor {
public:
    enum class ColumnType {
        None,
        FC_CPS, // Fuel Channel or Control and Protection System channel
        RR, // Reflector column
        RRC // Reflector Coolant Channel
    };

    struct Rod {
        Rod(int x, int y, float min_pos_z, float max_pos_z, bool direction, bool withdrawn=false): 
            pos_x(x), pos_y(y), 
            min_pos_z(min_pos_z), max_pos_z(max_pos_z),
            direction(direction) {
                pos_z = (!(direction^withdrawn))?max_pos_z:min_pos_z;
        }
        int pos_x;
        int pos_y;
        float pos_z;
        float min_pos_z;
        float max_pos_z;
        bool direction; // 0 for below, 1 for above
    };


private:
    constexpr const static float reactor_height = 8;
    constexpr const static float graphite_width = 0.25;
    constexpr const static float graphite_holes_diameter = 0.114;
    constexpr const static float pressure_tube_inner_diameter = 0.08;
    constexpr const static float pressure_tube_walls_thickness = 0.004;
    constexpr const static float absorber_length = 5.12;
    constexpr const static float short_absorber_length = 3.05;
    constexpr const static float tip_gap = 1.25;
    constexpr const static float tip_length = 4.5;
    constexpr const static float rod_insert_speed = 0.4;
    constexpr const static float fuel_active_length = 6.862;

    constexpr const static int reactor_width = 56;
    constexpr const static int axial_sections = reactor_height/graphite_width;

    int indicated_pos(int i);

    bool isrod(std::vector<Rod> &s, int x, int y) {
        for (unsigned i=0;i<s.size();i++) {
            if (s[i].pos_x == x && s[i].pos_y == y) return true;
        }
        return false;
    }

public:
    
    ColumnType columns[reactor_width][reactor_width];
    double neutron_flux[reactor_width][reactor_width][axial_sections];
    std::vector<Rod> manual_rods;
    std::vector<Rod> automatic_rods;
    std::vector<Rod> short_rods;
    std::vector<Rod> source_rods;
    std::vector<Rod> fuel_rods;

    Reactor();
    void print_layout();
};