#include "plugin/gsr_capture.hpp"

#include <cstdlib>
#include <iostream>

using namespace hyprcapture;

void expectArguments(const Rect& geometry, const std::vector<Rect>& monitors, const std::vector<std::string>& expected) {
    const auto actual = gsrCaptureArguments(geometry, monitors);
    if (actual != expected) {
        for (const auto& arg : actual)
            std::cerr << arg << '\n';
        std::exit(1);
    }
}

int main() {
    const std::vector<Rect> monitors{{1920, 0, 1920, 1200}, {3840, 0, 1920, 1200}, {0, 0, 1920, 1200}};
    expectArguments({0, 0, 3344, 1200}, monitors,
        {"-w", "1424x1200+1920+0;x=1920;y=0;width=1424;height=1200;halign=start;valign=start|"
               "1920x1200+0+0;x=0;y=0;width=1920;height=1200;halign=start;valign=start", "-s", "3344x1200"});
    expectArguments({420, 100, 2924, 1000}, monitors,
        {"-w", "1424x1000+1920+100;x=1500;y=0;width=1424;height=1000;halign=start;valign=start|"
               "1500x1000+420+100;x=0;y=0;width=1500;height=1000;halign=start;valign=start", "-s", "2924x1000"});
    expectArguments({2000, 80, 800, 600}, monitors,
        {"-w", "800x600+2000+80;x=0;y=0;width=800;height=600;halign=start;valign=start", "-s", "800x600"});
    expectArguments({-100, -100, 300, 300}, {{-1920, -1200, 1920, 1200}, {0, 0, 1920, 1200}},
        {"-w", "100x100+-100+-100;x=0;y=0;width=100;height=100;halign=start;valign=start|"
               "200x200+0+0;x=100;y=100;width=200;height=200;halign=start;valign=start", "-s", "300x300"});
    expectArguments({10.4, 20.6, 100.2, 200.8}, {{0, 0, 1000, 1000}},
        {"-w", "100x201+10+21;x=0;y=0;width=100;height=201;halign=start;valign=start", "-s", "100x201"});
    expectArguments({}, monitors, {"-w", "screen"});
}
