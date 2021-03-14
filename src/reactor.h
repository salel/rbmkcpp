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
        Rod(RodType type);
        float pos_z = 0;
        float min_pos_z = 0;
        float max_pos_z = 0;
        float target_z = 0;
        bool direction = true; // 0 for below, 1 for above
        RodType type = RodType::None;
        bool selected = false;
    };

    constexpr const static int reactor_width = 56;
    constexpr const static int axial_sections = 32;

private:
    bool scrammed = false;
    float neutron_flux[reactor_width][reactor_width][axial_sections];
    float total_neutron_flux = 0;
    float previous_flux = 0;
    float axial_peak = 0;
    float radial_peak = 0;
    float period = 0;

    const float telemetry_dt = 0.5;
    float telemetry_time = 0.0;

    std::vector<std::vector<std::pair<int,int>>> groups;

    void unselect_all();

public:
    void step(float dt);

    bool select_rod(int x, int y);
    void select_all();
    void select_group(int g);
    void select_sources();
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