#pragma once

#include "fiui/export.h"
#include "fiui/types.h"

namespace fiui {

struct DiagnosticsConfig {
    DebugMode mode = DebugMode::Basic;
    const char* output_directory = ".";
    bool echo_to_debugger = false;
};

FIUI_API void configure_diagnostics(const DiagnosticsConfig& config);
FIUI_API DebugMode diagnostics_mode();
FIUI_API void flush_diagnostics();

} // namespace fiui
