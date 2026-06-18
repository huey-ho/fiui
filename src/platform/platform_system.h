#pragma once

#include "fiui/export.h"
#include "fiui/types.h"

#include <cstdint>
#include <string>

namespace fiui {

enum class EventType;
class Window;
struct EventDispatchResult;
struct WidgetImpl;
struct FrameReport;

enum class PlatformEventType {
    WindowCreated,
    WindowCreateFailed,
    WindowDestroyed,
    Paint,
    Resize,
    DpiChanged,
    Mouse,
    Keyboard,
    Wheel,
    FocusLost,
    Touch,
    Ime,
    Clipboard,
    Timer,
    DeviceLost,
};

enum class PlatformImePhase {
    StartComposition,
    Composition,
    EndComposition,
};

struct PlatformWindowResult {
    bool success = false;
    void* native_handle = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t dpi = 96;
};

struct PlatformState {
    ObjectId window_object_id = 0;
    std::uint32_t window_generation = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t dpi = 96;
    float logical_width = 0.0f;
    float logical_height = 0.0f;
    std::uint32_t resize_count = 0;
    std::uint32_t input_event_count = 0;
    std::uint32_t pointer_route_count = 0;
    std::uint32_t pointer_miss_count = 0;
    std::uint32_t keyboard_event_count = 0;
    std::uint32_t keyboard_route_count = 0;
    std::uint32_t keyboard_miss_count = 0;
    std::uint32_t text_input_count = 0;
    std::uint32_t clipboard_read_count = 0;
    std::uint32_t clipboard_write_count = 0;
    std::uint32_t clipboard_copy_count = 0;
    std::uint32_t clipboard_paste_count = 0;
    std::uint32_t clipboard_failure_count = 0;
    std::uint32_t ime_event_count = 0;
    std::uint32_t ime_start_count = 0;
    std::uint32_t ime_composition_count = 0;
    std::uint32_t ime_end_count = 0;
    std::uint32_t timer_tick_count = 0;
    std::uint32_t device_lost_count = 0;
    PlatformEventType last_event = PlatformEventType::WindowDestroyed;
};

class PlatformSystem {
public:
    [[nodiscard]] FIUI_API PlatformWindowResult create_window(const Window& window,
                                                              std::uint64_t frame_id);
    FIUI_API void show_window(void* native_handle);
    [[nodiscard]] FIUI_API int run_message_loop();
    FIUI_API void record_paint();
    FIUI_API void record_resize(std::uint32_t width, std::uint32_t height);
    FIUI_API void record_dpi_changed(std::uint32_t dpi);
    FIUI_API void record_input(PlatformEventType type);
    FIUI_API void bind_root(WidgetImpl* root);
    FIUI_API void bind_window_model(const Window* window);
    [[nodiscard]] WidgetImpl* bound_root() const noexcept;
    FIUI_API void record_pointer_event(EventType type, float x, float y);
    FIUI_API void record_wheel_event(float x, float y, float delta);
    [[nodiscard]] FIUI_API EventDispatchResult record_keyboard_event(EventType type,
                                                                     std::uint32_t key_code,
                                                                     char32_t text_codepoint);
    FIUI_API void record_focus_lost();
    FIUI_API void set_clipboard_text(const char* text);
    [[nodiscard]] FIUI_API const char* clipboard_text() const noexcept;
    FIUI_API void record_clipboard_copy();
    FIUI_API void record_clipboard_paste();
    FIUI_API void record_ime_event(PlatformImePhase phase);
    FIUI_API void record_timer_tick();
    FIUI_API void record_device_lost(const char* detail);
    [[nodiscard]] FIUI_API const PlatformState& state() const noexcept;

private:
    void record_event(PlatformEventType type, const char* detail);
    void render_bound_window(const char* action,
                             const char* reason,
                             ObjectId object_id,
                             std::uint32_t generation,
                             const char* path,
                             bool invalidate_after_render);
    void request_render_after_input(const char* reason,
                                    ObjectId object_id,
                                    std::uint32_t generation,
                                    const char* path);
    void invalidate_bound_window(const char* action,
                                 const char* reason,
                                 ObjectId object_id,
                                 std::uint32_t generation,
                                 const char* path);

    PlatformState state_;
    void* native_handle_ = nullptr;
    WidgetImpl* root_widget_ = nullptr;
    const Window* window_model_ = nullptr;
    std::string clipboard_text_;
};

FIUI_API const char* platform_event_type_name(PlatformEventType type) noexcept;
FIUI_API const char* platform_ime_phase_name(PlatformImePhase phase) noexcept;

} // namespace fiui
