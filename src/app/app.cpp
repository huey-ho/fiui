#include "fiui/app.h"

#include "diagnostics/diagnostics_internal.h"
#include "platform/platform_system.h"
#include "runtime/runtime.h"

namespace fiui {

int run(const Window& window)
{
#if defined(_WIN32)
    PlatformSystem& platform = default_runtime().platform_system();
    const PlatformWindowResult window_result =
        platform.create_window(window, default_runtime().current_frame_id());
    if (!window_result.success) {
        diagnostics_event_ex("app", "window_create_failed", window.object_id(),
                             window.generation(), default_runtime().current_frame_id(), window.path(),
                             "platform window creation failed");
        return 1;
    }

    const FrameReport report = render_frame(window);
    diagnostics_event_ex("app", "run", window.object_id(), window.generation(), report.frame_id,
                         window.path(), report.fallback_reason);
    platform.show_window(window_result.native_handle);
    return platform.run_message_loop();
#else
    const FrameReport report = render_frame(window);
    diagnostics_event_ex("app", "run", window.object_id(), window.generation(), report.frame_id,
                         window.path(), report.fallback_reason);
    return 0;
#endif
}

} // namespace fiui
