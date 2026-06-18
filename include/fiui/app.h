#pragma once

#include "fiui/export.h"
#include "fiui/types.h"
#include "fiui/widget.h"

#include <cstdint>

namespace fiui {

struct FrameReport {
    std::uint64_t frame_id = 0;
    std::uint32_t original_dirty_rects = 0;
    std::uint32_t merged_dirty_rects = 0;
    bool full_repaint = false;
    const char* fallback_reason = "none";
    double layout_ms = 0.0;
    double display_list_ms = 0.0;
};

struct RuntimeSnapshot {
    FrameReport last_frame;
    ObjectId focus_target = 0;
    ObjectId hover_target = 0;
    ObjectId capture_target = 0;
    const char* last_platform_event = "unknown";
    std::uint32_t input_event_count = 0;
    std::uint32_t pointer_route_count = 0;
    std::uint32_t keyboard_route_count = 0;
};

FIUI_API FrameReport render_frame(const Window& window);
FIUI_API RuntimeSnapshot runtime_snapshot();
FIUI_API int run(const Window& window);

} // namespace fiui
