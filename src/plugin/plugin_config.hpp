#pragma once

#include <cstdint>
#include <string>

namespace hyprcapture::plugin_config {

void         registerValues();
std::string  string(const std::string& key);
std::int64_t integer(const std::string& key);
bool         boolean(const std::string& key);

} // namespace hyprcapture::plugin_config
