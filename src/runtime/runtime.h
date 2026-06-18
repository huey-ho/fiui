#pragma once

#include "fiui/export.h"
#include "fiui/types.h"

#include "backend/d3d11_backend.h"
#include "diagnostics/diagnostics_internal.h"
#include "event/event_system.h"
#include "image/image_system.h"
#include "menu/menu_system.h"
#include "platform/platform_system.h"
#include "resource/resource_system.h"
#include "scheduler/frame_scheduler.h"
#include "style/style_system.h"
#include "text/text_system.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace fiui {

struct WidgetImpl;
class Window;

struct DirtyThresholds {
    std::uint32_t max_dirty_rects = 16;
    float max_dirty_area_ratio = 0.60f;
};

struct RuntimeContext {
    std::atomic<ObjectId> next_object_id{1};
    std::atomic<std::uint64_t> frame_id{0};
    DirtyThresholds dirty_thresholds;
};

struct ObjectTableRecord {
    ObjectId object_id = 0;
    std::uint32_t generation = 0;
    LifecycleState lifecycle_state = LifecycleState::Destroyed;
    WidgetKind kind = WidgetKind::Widget;
    WidgetImpl* impl = nullptr;
    std::string path;
};

struct ObjectLookupResult {
    bool found = false;
    bool generation_match = false;
    bool alive = false;
    ObjectTableRecord record;
};

struct RuntimeEventRouteProbe {
    bool hit = false;
    bool handled = false;
    ObjectId target_object_id = 0;
    std::uint32_t target_generation = 0;
    std::uint32_t route_count = 0;
    ObjectId focus_target = 0;
    ObjectId capture_target = 0;
    ObjectId hover_target = 0;
};

class FiuiRuntime {
public:
    [[nodiscard]] ObjectId allocate_object_id();
    [[nodiscard]] std::uint64_t next_frame_id();
    [[nodiscard]] std::uint64_t current_frame_id() const;
    [[nodiscard]] const DirtyThresholds& dirty_thresholds() const noexcept;
    [[nodiscard]] EventSystem& event_system() noexcept;
    [[nodiscard]] const EventSystem& event_system() const noexcept;
    [[nodiscard]] StyleSystem& style_system() noexcept;
    [[nodiscard]] const StyleSystem& style_system() const noexcept;
    [[nodiscard]] ResourceSystem& resource_system() noexcept;
    [[nodiscard]] const ResourceSystem& resource_system() const noexcept;
    [[nodiscard]] TextSystem& text_system() noexcept;
    [[nodiscard]] const TextSystem& text_system() const noexcept;
    [[nodiscard]] ImageSystem& image_system() noexcept;
    [[nodiscard]] const ImageSystem& image_system() const noexcept;
    [[nodiscard]] MenuSystem& menu_system() noexcept;
    [[nodiscard]] const MenuSystem& menu_system() const noexcept;
    [[nodiscard]] PlatformSystem& platform_system() noexcept;
    [[nodiscard]] const PlatformSystem& platform_system() const noexcept;
    [[nodiscard]] FrameScheduler& frame_scheduler() noexcept;
    [[nodiscard]] const FrameScheduler& frame_scheduler() const noexcept;
    [[nodiscard]] D3D11Backend& backend() noexcept;
    [[nodiscard]] const D3D11Backend& backend() const noexcept;
    [[nodiscard]] DiagnosticsHub& diagnostics() noexcept;
    [[nodiscard]] const DiagnosticsHub& diagnostics() const noexcept;
    [[nodiscard]] RuntimeContext& context() noexcept;
    [[nodiscard]] const RuntimeContext& context() const noexcept;
    void register_object(WidgetImpl& impl);
    void unregister_object(WidgetImpl& impl);
    [[nodiscard]] ObjectLookupResult lookup_object(ObjectId object_id,
                                                   std::uint32_t generation,
                                                   const char* reason) const;
    [[nodiscard]] std::uint32_t object_table_live_count() const noexcept;

private:
    RuntimeContext context_;
    std::unordered_map<ObjectId, ObjectTableRecord> object_table_;
    EventSystem event_system_;
    StyleSystem style_system_;
    ResourceSystem resource_system_;
    TextSystem text_system_;
    ImageSystem image_system_;
    MenuSystem menu_system_;
    PlatformSystem platform_system_;
    FrameScheduler frame_scheduler_;
    D3D11Backend backend_;
    DiagnosticsHub diagnostics_;
};

FiuiRuntime& default_runtime();

FIUI_API ObjectId default_runtime_last_event_target_object_id();
FIUI_API void default_runtime_clear_event_targets();
FIUI_API ObjectId default_runtime_focus_target();
FIUI_API ObjectId default_runtime_capture_target();
FIUI_API ObjectId default_runtime_hover_target();
FIUI_API void default_runtime_set_event_focus_target(WidgetImpl* target);
FIUI_API bool default_runtime_select_focused_input_all();
FIUI_API const char* default_runtime_resolved_theme_name(const char* name);
FIUI_API const char* default_runtime_active_theme_name();
FIUI_API ResourceId default_runtime_register_test_resource(ResourceKind kind, const char* key);
FIUI_API bool default_runtime_release_resource(ResourceId id);
FIUI_API std::uint32_t default_runtime_live_resource_count();
FIUI_API PlatformState default_runtime_platform_state();
FIUI_API void default_runtime_record_platform_resize(std::uint32_t width, std::uint32_t height);
FIUI_API void default_runtime_record_platform_dpi_changed(std::uint32_t dpi);
FIUI_API void default_runtime_record_platform_input(PlatformEventType type);
FIUI_API void default_runtime_bind_platform_root(WidgetImpl& root);
FIUI_API void default_runtime_bind_platform_window_model(const Window& window);
FIUI_API void default_runtime_record_platform_pointer_event(EventType type, float x, float y);
FIUI_API void default_runtime_record_platform_keyboard_event(EventType type,
                                                            std::uint32_t key_code,
                                                            char32_t text_codepoint);
FIUI_API void default_runtime_record_platform_focus_lost();
FIUI_API void default_runtime_set_platform_clipboard_text(const char* text);
FIUI_API const char* default_runtime_platform_clipboard_text();
FIUI_API void default_runtime_record_platform_clipboard_copy();
FIUI_API void default_runtime_record_platform_clipboard_paste();
FIUI_API void default_runtime_record_platform_ime_event(PlatformImePhase phase);
FIUI_API void default_runtime_record_platform_timer_tick();
FIUI_API void default_runtime_record_platform_device_lost(const char* detail);
FIUI_API RuntimeEventRouteProbe default_runtime_route_pointer_event(WidgetImpl& root,
                                                                    EventType type,
                                                                    float x,
                                                                    float y);
FIUI_API RuntimeEventRouteProbe default_runtime_route_wheel_event(WidgetImpl& root,
                                                                  float x,
                                                                  float y,
                                                                  float delta);
FIUI_API FrameSchedulerState default_runtime_frame_scheduler_state();
FIUI_API void default_runtime_request_frame(const char* reason, const char* source);
FIUI_API BackendDeviceState default_runtime_backend_state();
FIUI_API void default_runtime_simulate_backend_device_lost(const char* detail);
FIUI_API bool default_runtime_recover_backend_device();
FIUI_API bool default_runtime_bind_backend_window(void* native_handle,
                                                 std::uint32_t width,
                                                 std::uint32_t height);
FIUI_API void default_runtime_release_backend_window_resources();
FIUI_API TextSystemState default_runtime_text_system_state();
FIUI_API ImageSystemState default_runtime_image_system_state();
FIUI_API bool default_runtime_lookup_object(ObjectId object_id, std::uint32_t generation);
FIUI_API std::uint32_t default_runtime_object_table_live_count();

} // namespace fiui
