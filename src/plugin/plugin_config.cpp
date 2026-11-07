#include "plugin/plugin_config.hpp"

#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/macros.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include <deque>
#include <unordered_map>

extern HANDLE g_pluginHandle;

namespace hyprcapture::plugin_config {
namespace {

std::deque<std::string>                                              g_names;
std::unordered_map<std::string, SP<Config::Values::CStringValue>>    g_strings;
std::unordered_map<std::string, SP<Config::Values::CIntValue>>       g_integers;
std::unordered_map<std::string, SP<Config::Values::CBoolValue>>      g_booleans;

const char* qualifiedName(const char* key) {
    return g_names.emplace_back(std::string("plugin:hyprcapture:") + key).c_str();
}

template <typename Value>
void addValue(std::unordered_map<std::string, SP<Value>>& registry, const char* key, SP<Value> value) {
    if (HyprlandAPI::addConfigValueV2(g_pluginHandle, value))
        registry[key] = value;
    else
        HyprlandAPI::addNotification(g_pluginHandle, std::string("[hyprcapture] failed to register config value ") + key, CHyprColor(1.0, 0.2, 0.2, 1.0), 5000);
}

void addString(const char* key, const char* description, const char* fallback) {
    addValue(g_strings, key, makeShared<Config::Values::CStringValue>(qualifiedName(key), description, fallback));
}

void addInteger(const char* key, const char* description, Config::INTEGER fallback) {
    addValue(g_integers, key, makeShared<Config::Values::CIntValue>(qualifiedName(key), description, fallback));
}

void addBoolean(const char* key, const char* description, bool fallback) {
    addValue(g_booleans, key, makeShared<Config::Values::CBoolValue>(qualifiedName(key), description, fallback));
}

template <typename Value, typename Result>
Result lookup(const std::unordered_map<std::string, SP<Value>>& registry, const std::string& key) {
    const auto it = registry.find(key);
    return it == registry.end() ? Result{} : Result(it->second->value());
}

} // namespace

void registerValues() {
    addString("default_mode", "Default HyprCapture mode", "region");
    addString("fullscreen_scope", "Fullscreen capture scope", "all");
    addString("window_background", "Window capture background mode", "follow-system");
    addString("window_border", "Window capture border policy", "keep");
    addString("window_shadow", "Window capture shadow policy", "keep");
    addBoolean("save", "Save captures to disk", true);
    addBoolean("clipboard", "Copy captures to the clipboard", true);
    addBoolean("show_thumbnail", "Show a result thumbnail after capture", true);
    addBoolean("include_cursor", "Include the cursor in captures", false);
    addBoolean("allow_quick", "Enable the no-confirmation quick capture function", false);
    addBoolean("confirm_before_capture", "Require explicit confirmation after target selection for normal open captures", false);
    addBoolean("fusion_mode", "Fuse region and window interactions in one overlay", false);
    addBoolean("fushion_mode", "Legacy alias for fusion_mode", false);
    addString("save_dir", "Capture output directory", "$XDG_PICTURES_DIR/Screenshots");
    addString("filename_template", "Screenshot filename strftime template", "Screenshot-%Y-%m-%d-%H%M%S.png");
    addString("record_save_dir", "Recording output directory", "$XDG_VIDEOS_DIR/Screenrecords");
    addString("record_filename_template", "Recording filename strftime template", "Recording-%Y-%m-%d-%H%M%S.mp4");
    addString("record_format", "Default recording format", "mp4");
    addString("record_transparent_format", "Default transparent recording container", "webm");
    addInteger("record_fps", "Recording frame rate", 30);
    addString("record_fps_options", "Recording frame rate choices", "15 24 30 60");
    addInteger("record_window_fps_limit", "Compositor window recording FPS cap", 12);
    addInteger("record_window_real_bg_fps_limit", "Real-background window recording FPS cap", 8);
    addString("record_codec", "Default recording codec", "libx264");
    addString("record_transparent_codec", "Default transparent recording codec", "auto");
    addBoolean("record_solid_alpha", "Keep alpha outside follow-system/white/black window recording content when the encoder supports it", false);
    addString("record_preset", "FFmpeg preset", "veryfast");
    addString("record_gsr_flags", "Extra gpu-screen-recorder flags", "");
    addString("record_window_backend", "Window recording backend", "compositor");
    addInteger("record_max_seconds", "Optional automatic recording stop in seconds", 0);
    addInteger("record_countdown_seconds", "Recording start countdown in seconds", 0);
    addInteger("thumbnail_timeout_ms", "Thumbnail auto-close timeout in milliseconds", 5000);
    addString("helper", "Optional helper executable override", "");
    addString("watermark", "Watermark path or built-in name", "");
    addString("watermark_position", "Watermark position", "central");
    addString("watermark_width", "Watermark width", "20%");
    addString("watermark_offset", "Watermark offset", "0 0");
    addBoolean("timing", "Enable HyprCapture timing traces", false);
    addString("timing_file", "Private timing trace output file", "");
}

std::string string(const std::string& key) {
    auto value = lookup<Config::Values::CStringValue, std::string>(g_strings, key);
    return value == STRVAL_EMPTY ? std::string{} : value;
}

std::int64_t integer(const std::string& key) {
    return lookup<Config::Values::CIntValue, std::int64_t>(g_integers, key);
}

bool boolean(const std::string& key) {
    return lookup<Config::Values::CBoolValue, bool>(g_booleans, key);
}

} // namespace hyprcapture::plugin_config
