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

    enum class RodType {
        Manual,
        Short,
        Automatic,
        Source,
        Fuel
    };

    struct Rod {
        Rod(int x, int y, RodType type): 
            pos_x(x), pos_y(y) {
            this->type = type;
            if (type == RodType::Manual || type == RodType::Source || type == RodType::Automatic) {
                min_pos_z = -absorber_length;
                max_pos_z = 0;
                direction = true;
            } else if (type == RodType::Short) {
                min_pos_z = reactor_height-short_absorber_length;
                max_pos_z = reactor_height;
                direction = false;
            } else if (type == RodType::Fuel) {
                min_pos_z = 0;
                max_pos_z = 0;
                direction = true;
            }
            pos_z = direction?max_pos_z:min_pos_z;
        }
        int pos_x;
        int pos_y;
        float pos_z;
        float min_pos_z;
        float max_pos_z;
        bool direction; // 0 for below, 1 for above
        RodType type;
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
    constexpr const static float rod_insert_speed = 0.1;
    constexpr const static float rod_scram_speed = 0.4;
    constexpr const static float fuel_active_length = 6.862;

    constexpr const static int reactor_width = 56;
    constexpr const static int axial_sections = reactor_height/graphite_width;

    int indicated_pos(int i);

    Rod* get_rod(int x, int y) {
        for (unsigned i=0;i<rods.size();i++) {
            if (rods[i].pos_x == x && rods[i].pos_y == y) return &rods[i];
        }
        return nullptr;
    }

public:
    void step(float dt);

    bool select_rod(int x, int y);
    Rod *get_selected_rod();
    void move_rod(float dp);

    void scram();
    void scram_reset();

    Rod *selected_rod = nullptr;
    float target_rod_depth = 0;
    bool scrammed = false;
    
    ColumnType columns[reactor_width][reactor_width];
    double neutron_flux[reactor_width][reactor_width][axial_sections];
    std::vector<Rod> rods;

    Reactor();
};