#include <algorithm>
#include <chrono>
#include <string>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/rule/Engine.hpp>
#include <hyprland/src/desktop/rule/layerRule/LayerRule.hpp>
#include <hyprland/src/desktop/rule/layerRule/LayerRuleEffectContainer.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "plugin/artifact_capture.hpp"
#include "plugin/plugin_config.hpp"
#include "plugin/recording.hpp"
#include "plugin/session_launcher.hpp"

inline HANDLE g_pluginHandle = nullptr;

namespace {

namespace plugin_config = hyprcapture::plugin_config;

constexpr const char* kOverlayLayerRuleName = "hyprcapture-ui-no-compositor-anim";
constexpr auto        kMinDispatchInterval = std::chrono::milliseconds(750);
std::chrono::steady_clock::time_point g_lastCaptureDispatch {};
std::chrono::steady_clock::time_point g_lastQuickRejectNotification {};

hyprcapture::CaptureDefaults readDefaults() {
    hyprcapture::CaptureDefaults defaults;
    defaults.mode = hyprcapture::parseCaptureMode(plugin_config::string("default_mode"), defaults.mode);
    defaults.fullscreenScope =
        hyprcapture::parseFullscreenScope(plugin_config::string("fullscreen_scope"), defaults.fullscreenScope);
    defaults.windowBackground =
        hyprcapture::parseWindowBackground(plugin_config::string("window_background"), defaults.windowBackground);
    defaults.windowBorder = hyprcapture::parseDecorationPolicy(plugin_config::string("window_border"), defaults.windowBorder);
    defaults.windowShadow = hyprcapture::parseDecorationPolicy(plugin_config::string("window_shadow"), defaults.windowShadow);
    defaults.save = plugin_config::boolean("save");
    defaults.clipboard = plugin_config::boolean("clipboard");
    defaults.showThumbnail = plugin_config::boolean("show_thumbnail");
    defaults.includeCursor = plugin_config::boolean("include_cursor");
    defaults.allowQuick = plugin_config::boolean("allow_quick");
    defaults.confirmBeforeCapture = plugin_config::boolean("confirm_before_capture");
    defaults.fushionMode = plugin_config::boolean("fusion_mode") || plugin_config::boolean("fushion_mode");
    defaults.saveDir = plugin_config::string("save_dir");
    defaults.filenameTemplate = plugin_config::string("filename_template");
    defaults.helper = plugin_config::string("helper");
    defaults.recordSaveDir = plugin_config::string("record_save_dir");
    defaults.recordFilenameTemplate = plugin_config::string("record_filename_template");
    defaults.recordFormat = plugin_config::string("record_format");
    defaults.recordTransparentFormat = plugin_config::string("record_transparent_format");
    defaults.recordCodec = plugin_config::string("record_codec");
    defaults.recordTransparentCodec = plugin_config::string("record_transparent_codec");
    defaults.recordSolidAlpha = plugin_config::boolean("record_solid_alpha");
    defaults.recordPreset = plugin_config::string("record_preset");
    defaults.recordGsrFlags = plugin_config::string("record_gsr_flags");
    defaults.recordWindowBackend =
        hyprcapture::parseRecordWindowBackend(plugin_config::string("record_window_backend"), defaults.recordWindowBackend);
    defaults.recordFps = plugin_config::integer("record_fps");
    defaults.recordFpsOptions = plugin_config::string("record_fps_options");
    defaults.recordWindowFpsLimit = plugin_config::integer("record_window_fps_limit");
    defaults.recordWindowRealBgFpsLimit = plugin_config::integer("record_window_real_bg_fps_limit");
    defaults.recordMaxSeconds = plugin_config::integer("record_max_seconds");
    defaults.recordCountdownSeconds = std::clamp<std::int64_t>(plugin_config::integer("record_countdown_seconds"), 0, 60);
    defaults.thumbnailTimeoutMs = plugin_config::integer("thumbnail_timeout_ms");
    defaults.watermark = plugin_config::string("watermark");
    defaults.watermarkPosition =
        hyprcapture::parseWatermarkPosition(plugin_config::string("watermark_position"), defaults.watermarkPosition);
    defaults.watermarkWidth = plugin_config::string("watermark_width");
    defaults.watermarkOffset = plugin_config::string("watermark_offset");
    return defaults;
}

void installOverlayLayerRule() {
    using namespace Desktop::Rule;

    ruleEngine()->unregisterRule(kOverlayLayerRuleName);

    SP<CLayerRule> rule = makeShared<CLayerRule>(kOverlayLayerRuleName);
    rule->registerMatch(RULE_PROP_NAMESPACE, "^hyprcapture-ui$");
    rule->addEffect(LAYER_RULE_EFFECT_NO_ANIM, "1");
    ruleEngine()->registerRule(SP<IRule>{rule});
}

hyprcapture::LaunchResult failure(std::string error) {
    return {.success = false, .error = std::move(error)};
}

hyprcapture::LaunchResult notifyOnFailure(hyprcapture::LaunchResult result) {
    if (!result.success)
        HyprlandAPI::addNotification(g_pluginHandle, "[hyprcapture] " + result.error, CHyprColor(1.0, 0.2, 0.2, 1.0), 5000);
    return result;
}

hyprcapture::LaunchResult openCapture(const std::string& mode, bool quick, bool record) {
    const auto now = std::chrono::steady_clock::now();
    if (g_lastCaptureDispatch.time_since_epoch().count() != 0 && now - g_lastCaptureDispatch < kMinDispatchInterval) {
        const std::string error = "capture rate-limited";
        HyprlandAPI::addNotification(g_pluginHandle, "[hyprcapture] " + error, CHyprColor(1.0, 0.2, 0.2, 1.0), 2500);
        return failure(error);
    }

    auto defaults = readDefaults();
    if (quick && !defaults.allowQuick) {
        const std::string error = "hl.plugin.hyprcapture.quick is disabled; set plugin.hyprcapture.allow_quick = true to enable no-confirmation capture";
        if (g_lastQuickRejectNotification.time_since_epoch().count() == 0 || now - g_lastQuickRejectNotification >= kMinDispatchInterval) {
            g_lastQuickRejectNotification = now;
            HyprlandAPI::addNotification(g_pluginHandle, "[hyprcapture] " + error, CHyprColor(1.0, 0.2, 0.2, 1.0), 5000);
        }
        return failure(error);
    }

    installOverlayLayerRule();

    const auto requestedMode = hyprcapture::parseCaptureMode(mode, defaults.mode);
    const auto result = notifyOnFailure(hyprcapture::launchHelper(
        {.defaults = defaults, .requestedMode = requestedMode, .quick = quick, .record = record, .recordActive = hyprcapture::isRecordingActive()}));
    if (result.success)
        g_lastCaptureDispatch = now;
    return result;
}

hyprcapture::LaunchResult startRecordingOverlay(const std::string& mode) {
    if (hyprcapture::isRecordingActive())
        return failure("recording already active");
    return openCapture(mode, false, true);
}

template <typename Action>
int luaResult(lua_State* L, Action&& action) {
    bool failed = false;
    {
        const hyprcapture::LaunchResult result = action();
        if (!result.success) {
            lua_pushstring(L, result.error.empty() ? "hyprcapture function failed" : result.error.c_str());
            failed = true;
        }
    }
    return failed ? lua_error(L) : 0;
}

std::string luaOptionalString(lua_State* L, int index) {
    if (lua_gettop(L) < index || lua_isnil(L, index))
        return {};

    return luaL_checkstring(L, index);
}

int luaOpen(lua_State* L) {
    return luaResult(L, [L] { return openCapture(luaOptionalString(L, 1), false, false); });
}

int luaQuick(lua_State* L) {
    return luaResult(L, [L] { return openCapture(luaOptionalString(L, 1), true, false); });
}

int luaRecord(lua_State* L) {
    return luaResult(L, [L] { return startRecordingOverlay(luaOptionalString(L, 1)); });
}

int luaRecordToggle(lua_State* L) {
    return luaResult(L, [L] {
        if (hyprcapture::isRecordingActive())
            return hyprcapture::stopRecording("stopped");
        return startRecordingOverlay(luaOptionalString(L, 1));
    });
}

int luaRecordStop(lua_State* L) {
    return luaResult(L, [] { return hyprcapture::stopRecording("stopped"); });
}

int luaRecordStart(lua_State* L) {
    const char* requestPath = luaL_checkstring(L, 1);
    return luaResult(L, [requestPath] { return notifyOnFailure(hyprcapture::startRecordingFromRequestFile(requestPath)); });
}

int luaWindowCapture(lua_State* L) {
    const char* requestPath = luaL_checkstring(L, 1);
    return luaResult(L, [requestPath] { return notifyOnFailure(hyprcapture::captureWindowArtifactFromRequestFile(requestPath)); });
}

void registerLuaFunctions() {
    const std::pair<const char*, PLUGIN_LUA_FN> functions[] = {
        {"open", luaOpen},
        {"quick", luaQuick},
        {"record", luaRecord},
        {"record_toggle", luaRecordToggle},
        {"record_stop", luaRecordStop},
        {"record_start", luaRecordStart},
        {"window_capture", luaWindowCapture},
    };

    for (const auto& [name, function] : functions) {
        if (!HyprlandAPI::addLuaFunction(g_pluginHandle, "hyprcapture", name, function))
            HyprlandAPI::addNotification(g_pluginHandle, std::string("[hyprcapture] failed to register hl.plugin.hyprcapture.") + name,
                                         CHyprColor(1.0, 0.2, 0.2, 1.0), 5000);
    }
}

} // namespace

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    g_pluginHandle = handle;

    hyprcapture::plugin_config::registerValues();
    registerLuaFunctions();

    if (!HyprlandAPI::reloadConfig())
        HyprlandAPI::addNotification(g_pluginHandle, "[hyprcapture] reloadConfig failed", CHyprColor(1.0, 0.2, 0.2, 1.0), 5000);
    installOverlayLayerRule();

    return {
        .name = "HyprCapture",
        .description = "Hyprland-only screenshot overlay",
        .author = "wilf",
        .version = "0.3.0",
    };
}

APICALL EXPORT void PLUGIN_EXIT() {
    hyprcapture::shutdownRecording();
    hyprcapture::shutdownArtifactCapture();
    Desktop::Rule::ruleEngine()->unregisterRule(kOverlayLayerRuleName);
    g_pluginHandle = nullptr;
}
