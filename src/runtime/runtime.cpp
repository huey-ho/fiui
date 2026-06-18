#include "runtime/runtime.h"

#include "core/widget_impl.h"

#include <sstream>

namespace fiui {

ObjectId FiuiRuntime::allocate_object_id()
{
    return context_.next_object_id.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t FiuiRuntime::next_frame_id()
{
    return context_.frame_id.fetch_add(1, std::memory_order_relaxed) + 1;
}

std::uint64_t FiuiRuntime::current_frame_id() const
{
    return context_.frame_id.load(std::memory_order_relaxed);
}

const DirtyThresholds& FiuiRuntime::dirty_thresholds() const noexcept
{
    return context_.dirty_thresholds;
}

EventSystem& FiuiRuntime::event_system() noexcept
{
    return event_system_;
}

const EventSystem& FiuiRuntime::event_system() const noexcept
{
    return event_system_;
}

StyleSystem& FiuiRuntime::style_system() noexcept
{
    return style_system_;
}

const StyleSystem& FiuiRuntime::style_system() const noexcept
{
    return style_system_;
}

ResourceSystem& FiuiRuntime::resource_system() noexcept
{
    return resource_system_;
}

const ResourceSystem& FiuiRuntime::resource_system() const noexcept
{
    return resource_system_;
}

TextSystem& FiuiRuntime::text_system() noexcept
{
    return text_system_;
}

const TextSystem& FiuiRuntime::text_system() const noexcept
{
    return text_system_;
}

ImageSystem& FiuiRuntime::image_system() noexcept
{
    return image_system_;
}

const ImageSystem& FiuiRuntime::image_system() const noexcept
{
    return image_system_;
}

MenuSystem& FiuiRuntime::menu_system() noexcept
{
    return menu_system_;
}

const MenuSystem& FiuiRuntime::menu_system() const noexcept
{
    return menu_system_;
}

PlatformSystem& FiuiRuntime::platform_system() noexcept
{
    return platform_system_;
}

const PlatformSystem& FiuiRuntime::platform_system() const noexcept
{
    return platform_system_;
}

FrameScheduler& FiuiRuntime::frame_scheduler() noexcept
{
    return frame_scheduler_;
}

const FrameScheduler& FiuiRuntime::frame_scheduler() const noexcept
{
    return frame_scheduler_;
}

D3D11Backend& FiuiRuntime::backend() noexcept
{
    return backend_;
}

const D3D11Backend& FiuiRuntime::backend() const noexcept
{
    return backend_;
}

DiagnosticsHub& FiuiRuntime::diagnostics() noexcept
{
    return diagnostics_;
}

const DiagnosticsHub& FiuiRuntime::diagnostics() const noexcept
{
    return diagnostics_;
}

RuntimeContext& FiuiRuntime::context() noexcept
{
    return context_;
}

const RuntimeContext& FiuiRuntime::context() const noexcept
{
    return context_;
}

void FiuiRuntime::register_object(WidgetImpl& impl)
{
    ObjectTableRecord record;
    record.object_id = impl.object.object_id;
    record.generation = impl.object.generation;
    record.lifecycle_state = impl.object.lifecycle_state;
    record.kind = impl.node.kind;
    record.impl = &impl;
    record.path = widget_path(impl);
    object_table_[record.object_id] = record;
    diagnostics_event_ex("object", "object_register", record.object_id, record.generation,
                         current_frame_id(), record.path.c_str(), widget_kind_name(record.kind));
}

void FiuiRuntime::unregister_object(WidgetImpl& impl)
{
    ObjectTableRecord& record = object_table_[impl.object.object_id];
    record.object_id = impl.object.object_id;
    record.generation = impl.object.generation;
    record.lifecycle_state = LifecycleState::Destroyed;
    record.kind = impl.node.kind;
    record.impl = nullptr;
    record.path = widget_path(impl);
    diagnostics_event_ex("object", "object_unregister", record.object_id, record.generation,
                         current_frame_id(), record.path.c_str(), widget_kind_name(record.kind));
}

ObjectLookupResult FiuiRuntime::lookup_object(ObjectId object_id,
                                              std::uint32_t generation,
                                              const char* reason) const
{
    ObjectLookupResult result;
    const auto found = object_table_.find(object_id);
    if (found == object_table_.end()) {
        diagnostics_event_ex("object", "object_lookup_miss", object_id, generation,
                             current_frame_id(), "", reason == nullptr ? "" : reason);
        return result;
    }

    result.found = true;
    result.record = found->second;
    result.generation_match = result.record.generation == generation;
    result.alive = result.generation_match && result.record.impl != nullptr &&
                   result.record.lifecycle_state != LifecycleState::Destroying &&
                   result.record.lifecycle_state != LifecycleState::Destroyed;

    if (!result.generation_match) {
        std::ostringstream detail;
        detail << (reason == nullptr ? "" : reason) << ";expected=" << result.record.generation
               << ";actual=" << generation;
        const std::string text = detail.str();
        diagnostics_event_ex("object", "stale_generation", object_id, generation,
                             current_frame_id(), result.record.path.c_str(), text.c_str());
    } else {
        diagnostics_event_ex("object", result.alive ? "object_lookup" : "object_lookup_dead",
                             object_id, generation, current_frame_id(),
                             result.record.path.c_str(), reason == nullptr ? "" : reason);
    }
    return result;
}

std::uint32_t FiuiRuntime::object_table_live_count() const noexcept
{
    std::uint32_t count = 0;
    for (const auto& entry : object_table_) {
        if (entry.second.impl != nullptr &&
            entry.second.lifecycle_state != LifecycleState::Destroying &&
            entry.second.lifecycle_state != LifecycleState::Destroyed) {
            ++count;
        }
    }
    return count;
}

FiuiRuntime& default_runtime()
{
    static FiuiRuntime runtime;
    return runtime;
}

ObjectId default_runtime_last_event_target_object_id()
{
    const auto& events = default_runtime().event_system().recent_events();
    return events.empty() ? 0 : events.back().target_object_id;
}

void default_runtime_clear_event_targets()
{
    (void)default_runtime().event_system().set_focus_target(nullptr);
    (void)default_runtime().event_system().set_capture_target(nullptr);
    (void)default_runtime().event_system().set_hover_target(nullptr);
}

ObjectId default_runtime_focus_target()
{
    return default_runtime().event_system().focus_target();
}

ObjectId default_runtime_capture_target()
{
    return default_runtime().event_system().capture_target();
}

ObjectId default_runtime_hover_target()
{
    return default_runtime().event_system().hover_target();
}

void default_runtime_set_event_focus_target(WidgetImpl* target)
{
    (void)default_runtime().event_system().set_focus_target(target);
}

bool default_runtime_select_focused_input_all()
{
    return default_runtime().event_system().select_focused_input_all();
}

const char* default_runtime_resolved_theme_name(const char* name)
{
    return default_runtime().style_system().resolve_theme(name).name;
}

const char* default_runtime_active_theme_name()
{
    return default_runtime().style_system().active_theme_name();
}

ResourceId default_runtime_register_test_resource(ResourceKind kind, const char* key)
{
    return default_runtime().resource_system().register_resource(kind, 0, "", key,
                                                                ResourceCacheState::Cached);
}

bool default_runtime_release_resource(ResourceId id)
{
    return default_runtime().resource_system().release_resource(id);
}

std::uint32_t default_runtime_live_resource_count()
{
    return default_runtime().resource_system().live_resource_count();
}

PlatformState default_runtime_platform_state()
{
    return default_runtime().platform_system().state();
}

void default_runtime_record_platform_resize(std::uint32_t width, std::uint32_t height)
{
    default_runtime().platform_system().record_resize(width, height);
}

void default_runtime_record_platform_dpi_changed(std::uint32_t dpi)
{
    default_runtime().platform_system().record_dpi_changed(dpi);
}

void default_runtime_record_platform_input(PlatformEventType type)
{
    default_runtime().platform_system().record_input(type);
}

void default_runtime_bind_platform_root(WidgetImpl& root)
{
    default_runtime().platform_system().bind_root(&root);
}

void default_runtime_bind_platform_window_model(const Window& window)
{
    default_runtime().platform_system().bind_window_model(&window);
}

void default_runtime_record_platform_pointer_event(EventType type, float x, float y)
{
    default_runtime().platform_system().record_pointer_event(type, x, y);
}

void default_runtime_record_platform_keyboard_event(EventType type,
                                                    std::uint32_t key_code,
                                                    char32_t text_codepoint)
{
    (void)default_runtime().platform_system().record_keyboard_event(type, key_code, text_codepoint);
}

void default_runtime_record_platform_focus_lost()
{
    default_runtime().platform_system().record_focus_lost();
}

void default_runtime_set_platform_clipboard_text(const char* text)
{
    default_runtime().platform_system().set_clipboard_text(text);
}

const char* default_runtime_platform_clipboard_text()
{
    return default_runtime().platform_system().clipboard_text();
}

void default_runtime_record_platform_clipboard_copy()
{
    default_runtime().platform_system().record_clipboard_copy();
}

void default_runtime_record_platform_clipboard_paste()
{
    default_runtime().platform_system().record_clipboard_paste();
}

void default_runtime_record_platform_ime_event(PlatformImePhase phase)
{
    default_runtime().platform_system().record_ime_event(phase);
}

void default_runtime_record_platform_timer_tick()
{
    default_runtime().platform_system().record_timer_tick();
}

void default_runtime_record_platform_device_lost(const char* detail)
{
    default_runtime().platform_system().record_device_lost(detail);
}

RuntimeEventRouteProbe default_runtime_route_pointer_event(WidgetImpl& root,
                                                           EventType type,
                                                           float x,
                                                           float y)
{
    const EventDispatchResult result =
        default_runtime().event_system().route_pointer_event(root, type, x, y);
    RuntimeEventRouteProbe probe;
    probe.hit = result.hit_test.hit;
    probe.handled = result.handled;
    probe.target_object_id = result.hit_test.target_object_id;
    probe.target_generation = result.hit_test.target_generation;
    probe.route_count = static_cast<std::uint32_t>(result.route.size());
    probe.focus_target = default_runtime().event_system().focus_target();
    probe.capture_target = default_runtime().event_system().capture_target();
    probe.hover_target = default_runtime().event_system().hover_target();
    return probe;
}

RuntimeEventRouteProbe default_runtime_route_wheel_event(WidgetImpl& root,
                                                         float x,
                                                         float y,
                                                         float delta)
{
    const EventDispatchResult result =
        default_runtime().event_system().route_wheel_event(root, x, y, delta);
    RuntimeEventRouteProbe probe;
    probe.hit = result.hit_test.hit;
    probe.handled = result.handled;
    probe.target_object_id = result.hit_test.target_object_id;
    probe.target_generation = result.hit_test.target_generation;
    probe.route_count = static_cast<std::uint32_t>(result.route.size());
    probe.focus_target = default_runtime().event_system().focus_target();
    probe.capture_target = default_runtime().event_system().capture_target();
    probe.hover_target = default_runtime().event_system().hover_target();
    return probe;
}

FrameSchedulerState default_runtime_frame_scheduler_state()
{
    return default_runtime().frame_scheduler().state();
}

void default_runtime_request_frame(const char* reason, const char* source)
{
    default_runtime().frame_scheduler().request_frame(reason, source, 0, 0,
                                                     default_runtime().current_frame_id(), "");
}

BackendDeviceState default_runtime_backend_state()
{
    return default_runtime().backend().state();
}

void default_runtime_simulate_backend_device_lost(const char* detail)
{
    default_runtime().backend().simulate_device_lost(detail);
}

bool default_runtime_recover_backend_device()
{
    return default_runtime().backend().recover_device();
}

bool default_runtime_bind_backend_window(void* native_handle, std::uint32_t width, std::uint32_t height)
{
    return default_runtime().backend().bind_window(native_handle, width, height);
}

void default_runtime_release_backend_window_resources()
{
    default_runtime().backend().release_window_resources();
}

TextSystemState default_runtime_text_system_state()
{
    return default_runtime().text_system().state();
}

ImageSystemState default_runtime_image_system_state()
{
    return default_runtime().image_system().state();
}

bool default_runtime_lookup_object(ObjectId object_id, std::uint32_t generation)
{
    const ObjectLookupResult result =
        default_runtime().lookup_object(object_id, generation, "test_lookup");
    return result.alive;
}

std::uint32_t default_runtime_object_table_live_count()
{
    return default_runtime().object_table_live_count();
}

} // namespace fiui
