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
        None,
        Manual,
        Short,
        Automatic,
        Source,
        Fuel
    };

    struct Rod {
        Rod() = default;
        Rod(RodType type): type(type) {
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
            target_z = pos_z;
        }
        float pos_z = 0;
        float min_pos_z = 0;
        float max_pos_z = 0;
        float target_z = 0;
        bool direction = true; // 0 for below, 1 for above
        RodType type = RodType::None;
        bool selected = false;
    };

    constexpr const static int reactor_width = 56;

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
    constexpr const static float rod_scram_speed = 0.4;
    constexpr const static float fuel_active_length = 6.862;
    constexpr const static float source_strength = 1E-10;
    constexpr const static float graphite_absorption = 1E-4;
    constexpr const static float boron_absorption = 0.8;
    constexpr const static float water_absorption = 2E-2;
    constexpr const static float enrichment = 2E-2;
    constexpr const static float u235_fission = 583;
    constexpr const static float u235_capture = 99;
    constexpr const static float u238_capture = 2;
    constexpr const static float u235_neutrons = 2.43;


    constexpr const static int axial_sections = reactor_height/graphite_width;

    bool scrammed = false;
    float neutron_flux[reactor_width][reactor_width][axial_sections];
    float total_neutron_flux = 0;
    float axial_peak = 0;
    float radial_peak = 0;
    float period = 0;

    std::vector<std::vector<std::pair<int,int>>> groups;

    void unselect_all();

public:
    void step(float dt);

    bool select_rod(int x, int y);
    void select_all();
    void select_group(int g);
    void move_rod(float dp);

    void scram();
    void scram_reset();

    float get_neutron_flux();
    float get_period();
    float get_radial_peak();
    
    ColumnType columns[reactor_width][reactor_width];
    Rod rods[reactor_width][reactor_width];

    std::vector<std::pair<int,int>> center_sources;
    std::vector<std::pair<int,int>> outer_sources;

    Reactor();
};