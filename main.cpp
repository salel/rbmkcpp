#include <iostream>
#include <map>
#include <functional>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>

using namespace std;

struct Reactor {

    const double dt = 0.001;
    const double reactor_height = 7;
    const double graphite_width = 0.25;
    const double graphite_holes_diameter = 0.114;
    const double pressure_tube_inner_diameter = 0.08;
    const double pressure_tube_walls_thickness = 0.004;
    const double absorber_length = 5.12;
    const double short_absorber_length = 3.05;
    const double tip_gap = 1.25;
    const double tip_length = 4.5;
    const double rod_insert_speed = 0.4;
    const double fuel_active_length = 6.862;
    const double assembly_fuel_mass = 114.7;

    const double ambient_temp = 300;
    const double atm = 101375;

    const static int reactor_width = 48;
    const static int axial_sections = 28;

    enum class ChannelType {
        None, Fuel, Manual, Automatic, Short, Source
    };

    struct Section {
        // Common
        double moderator_temp;
        double coolant_flow;
        double coolant_temp;
        double coolant_pressure;
        // Fuel
        double fuel_volume;
        double burnup;
        double xenon;
        // Absorber
        double absorber;
        double empty;
        double moderator;
    };

    struct Channel {
        // Common
        ChannelType type = ChannelType::None;
        Section sections[axial_sections];
        // Rod
        double rod_insertion = 0;
    };

    Channel channels[reactor_width][reactor_width];

    Reactor() {
        // Generate graphite stack layout (index 0 to 47)
        for (int i=0;i<reactor_width;i++) {
            for (int j=0;j<reactor_width;j++) {
                auto &c = channels[i][j];
                double i0 = i-reactor_width/2+0.5;
                double j0 = j-reactor_width/2+0.5;
                // reactor circle
                if (i0*i0+j0*j0 <= 605) {
                    // no fuel lattice
                    if ((i%4==1 && j%4==1) || (i%4==3 && j%4==3)) {
                        // short rods
                        if (i%8==3 && j%8==3) {
                            c.type = ChannelType::Short;
                            c.rod_insertion = short_absorber_length;
                        // neutron sources
                        } else if ((i%16 == 7 && j%16 == 15) || (i%16 == 15 && j%16 == 7)) {
                            c.type = ChannelType::Source;
                        // automatic rods
                        } else if ((i==23 && (j==11||j==19||j==27||j==35)) || 
                            (j==23 && (i==11||i==19||i==27||i==35)) ||
                            (i%16==15 && j%16==15)) {
                            c.type = ChannelType::Automatic;
                            c.rod_insertion = absorber_length;
                        // manual rods
                        } else {
                            c.type = ChannelType::Manual;
                            c.rod_insertion = absorber_length;
                        }
                    } else {
                        c.type = ChannelType::Fuel;
                    }
                    if (i==reactor_width-1 || j==reactor_width-1) c.type = ChannelType::Fuel;
                }
            }
        }

        // replace too close to the edge with fuel rods
        for (int i=0;i<reactor_width-1;i++) {
            for (int j=0;j<reactor_width-1;j++) {
                auto &c = channels[i][j];
                auto &b = channels[i][j+1];
                auto &r = channels[i+1][j];
                if (c.type != ChannelType::None && (b.type == ChannelType::None || r.type == ChannelType::None))
                    c.type = ChannelType::Fuel;
            }
        }
    }

    void update() {

    }

    void print() {
    }

    void print_layout() {
        auto channel_color = [&](auto c) {
            switch (c.type) {
                case ChannelType::Fuel:return "\033[47m";
                case ChannelType::Source:return "\033[44m";
                case ChannelType::Short:return "\033[43m";
                case ChannelType::Automatic:return "\033[41m";
                case ChannelType::Manual:return "\033[42m";
            }
            return "\033[40m";
        };
        auto channel_to_txt = [&](auto c) -> string {
            double l = 1;
            if (c.type == ChannelType::Short) l = short_absorber_length;
            else if (c.type == ChannelType::Manual || c.type == ChannelType::Automatic)
                l = absorber_length;
            else return "  ";
            int level = (100*c.rod_insertion)/l;
            if (level == 100) return "**";
            else {
                if (level < 10) return string("0")+(char)('0'+level);
                else return string(1,((level/10)+'0')) + (char)('0'+(level%10));
            }
        };

        for (int i=0;i<reactor_width;i++) {
            for (int j=0;j<reactor_width;j++) {
                auto &c = channels[i][j];
                cout << channel_color(c) << channel_to_txt(c);
            }
            cout << "\033[40m" << endl;
        }
    }
};

int main() {
    Reactor r;

    r.print_layout();

    /*
    map<string, function<void(void)>> commands;
    commands["quit"] = [](){exit(0);};
    commands["step"] = [&](){r.update();};
    commands["print"] = [&](){r.print();};
    commands["print layout"] = [&](){r.print_layout();};
    
    while (true) {
        string s;
        getline(cin, s);
        auto a = commands.find(s);
        if (a == commands.end()) {
            cout << "Unrecognized command" << endl;
        } else {
            a->second();
        }
    }
    */
}