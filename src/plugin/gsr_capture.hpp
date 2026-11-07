#pragma once

#include "shared/protocol.hpp"

#include <span>
#include <string>
#include <vector>

namespace hyprcapture {

std::vector<std::string> gsrCaptureArguments(const Rect& geometry, std::span<const Rect> monitors);

}
