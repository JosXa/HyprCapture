#pragma once

#include "shared/config.hpp"

#include <string>
#include <sys/types.h>

namespace hyprcapture {

struct LaunchRequest {
    CaptureDefaults defaults;
    CaptureMode     requestedMode = CaptureMode::Region;
    bool            quick = false;
    bool            record = false;
    bool            recordActive = false;
};

struct LaunchResult {
    bool        success = false;
    std::string error;
};

LaunchResult launchHelper(const LaunchRequest& request);
LaunchResult launchRecordingResultHelper(const CaptureDefaults& defaults, const std::string& outputPath);
LaunchResult launchRecordingOutlineHelper(const CaptureDefaults& defaults, const std::string& geometry, pid_t& pid);
LaunchResult launchRecordingTranscodeHelper(const CaptureDefaults& defaults,
                                            const std::string&     inputPath,
                                            const std::string&     outputPath,
                                            bool                  preserveAlpha,
                                            int                   durationMs);

} // namespace hyprcapture
