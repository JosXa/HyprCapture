#include "plugin/gsr_capture.hpp"

#include <algorithm>
#include <cmath>

namespace hyprcapture {

std::vector<std::string> gsrCaptureArguments(const Rect& geometry, std::span<const Rect> monitors) {
    if (geometry.width <= 0.0 || geometry.height <= 0.0)
        return {"-w", "screen"};

    const int x = std::lround(geometry.x);
    const int y = std::lround(geometry.y);
    const int width = std::max(1L, std::lround(geometry.width));
    const int height = std::max(1L, std::lround(geometry.height));
    std::string sources;
    for (const auto& monitor : monitors) {
        const int left = std::max(x, static_cast<int>(std::lround(monitor.x)));
        const int top = std::max(y, static_cast<int>(std::lround(monitor.y)));
        const int right = std::min(x + width, static_cast<int>(std::lround(monitor.x + monitor.width)));
        const int bottom = std::min(y + height, static_cast<int>(std::lround(monitor.y + monitor.height)));
        if (right <= left || bottom <= top)
            continue;

        const auto sliceWidth = std::to_string(right - left);
        const auto sliceHeight = std::to_string(bottom - top);
        if (!sources.empty())
            sources += '|';
        sources += sliceWidth + "x" + sliceHeight + "+" + std::to_string(left) + "+" + std::to_string(top) +
            ";x=" + std::to_string(left - x) + ";y=" + std::to_string(top - y) +
            ";width=" + sliceWidth + ";height=" + sliceHeight + ";halign=start;valign=start";
    }
    return {"-w", sources, "-s", std::to_string(width) + "x" + std::to_string(height)};
}

}
