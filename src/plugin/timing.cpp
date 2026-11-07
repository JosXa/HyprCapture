#include "plugin/timing.hpp"

#include "plugin/plugin_config.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace hyprcapture {
namespace {

std::mutex g_timingMutex;
std::atomic<bool> g_cachedTimingEnabled = false;
std::atomic<long long> g_nextTimingRefreshNs = 0;

bool runtimeRootMatches(const std::filesystem::path& root, const std::filesystem::path& path) {
    std::error_code ec;
    const auto      canonicalRoot = std::filesystem::weakly_canonical(root, ec);
    if (ec)
        return false;
    const auto canonicalPath = std::filesystem::weakly_canonical(path, ec);
    if (ec)
        return false;

    struct stat st {};
    const auto  rootNative = canonicalRoot.string();
    if (stat(rootNative.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 0777) != 0700)
        return false;

    const auto rootString = canonicalRoot.native();
    const auto pathString = canonicalPath.native();
    return pathString == rootString || pathString.starts_with(rootString + "/");
}

bool privateTimingPath(const std::filesystem::path& path) {
    if (path.empty() || !path.is_absolute())
        return false;

    const auto rootName = "hyprcapture-" + std::to_string(static_cast<unsigned long long>(geteuid()));
    return runtimeRootMatches(std::filesystem::path{"/dev/shm"} / rootName, path) ||
        runtimeRootMatches(std::filesystem::temp_directory_path() / rootName, path);
}

std::string timingFilePath() {
    if (const auto configured = plugin_config::string("timing_file"); !configured.empty())
        return configured;
    if (const char* env = std::getenv("HYPRCAPTURE_TIMING_FILE"); env && *env)
        return env;
    return {};
}

} // namespace

bool timingEnabled() {
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now < g_nextTimingRefreshNs.load(std::memory_order_relaxed))
        return g_cachedTimingEnabled.load(std::memory_order_relaxed);

    std::lock_guard lock(g_timingMutex);
    if (now < g_nextTimingRefreshNs.load(std::memory_order_relaxed))
        return g_cachedTimingEnabled.load(std::memory_order_relaxed);

    const bool enabled = plugin_config::boolean("timing") || std::getenv("HYPRCAPTURE_TIMING") || std::getenv("HYPRCAPTURE_TIMING_FILE");
    g_cachedTimingEnabled.store(enabled, std::memory_order_relaxed);
    g_nextTimingRefreshNs.store(now + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(250)).count(), std::memory_order_relaxed);
    return enabled;
}

void traceTiming(std::string_view event, long long elapsedUs) {
    if (!timingEnabled())
        return;

    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::string line = "time_ms=" + std::to_string(ms) + " event=" + std::string(event);
    if (elapsedUs >= 0)
        line += " elapsed_us=" + std::to_string(elapsedUs);
    line += "\n";

    std::lock_guard lock(g_timingMutex);
    const auto      path = timingFilePath();
    if (!path.empty() && privateTimingPath(path)) {
        std::ofstream out(path, std::ios::app);
        if (out) {
            out << line;
            return;
        }
    }

    std::cerr << "[hyprcapture-timing] " << line;
}

ScopedTiming::ScopedTiming(std::string_view event) : m_event(event), m_started(std::chrono::steady_clock::now()), m_enabled(timingEnabled()) {}

ScopedTiming::~ScopedTiming() {
    if (!m_enabled)
        return;

    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_started).count();
    traceTiming(m_event, elapsed);
}

} // namespace hyprcapture
