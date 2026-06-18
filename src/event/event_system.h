#pragma once

#include "core/widget_impl.h"
#include "fiui/export.h"
#include "fiui/types.h"

#include <string>
#include <vector>

namespace fiui {

enum class EventType {
    Click,
    PointerMove,
    PointerDown,
    PointerUp,
    Wheel,
    KeyDown,
    KeyUp,
    TextInput,
};

struct HitTestResult {
    bool hit = false;
    float x = 0.0f;
    float y = 0.0f;
    float wheel_delta = 0.0f;
    ObjectId target_object_id = 0;
    std::uint32_t target_generation = 0;
    std::string target_path;
    WidgetImpl* target = nullptr;
};

struct EventRouteEntry {
    ObjectId object_id = 0;
    std::uint32_t generation = 0;
    std::string path;
    WidgetKind kind = WidgetKind::Widget;
};

struct FiuiEvent {
    EventType type = EventType::Click;
    ObjectId target_object_id = 0;
    std::uint32_t target_generation = 0;
    std::string target_path;
    WidgetImpl* target = nullptr;
    float x = 0.0f;
    float y = 0.0f;
    float wheel_delta = 0.0f;
    std::uint32_t key_code = 0;
    char32_t text_codepoint = 0;
    std::vector<EventRouteEntry> route;
};

struct EventDispatchResult {
    bool handled = false;
    bool callback_failed = false;
    bool target_changed = false;
    bool menu_shortcut_triggered = false;
    HitTestResult hit_test;
    std::vector<EventRouteEntry> route;
};

class EventSystem {
public:
    [[nodiscard]] EventDispatchResult dispatch_click(WidgetImpl& target);
    [[nodiscard]] HitTestResult hit_test(WidgetImpl& root, float x, float y) const;
    [[nodiscard]] EventDispatchResult route_pointer_event(WidgetImpl& root,
                                                          EventType type,
                                                          float x,
                                                          float y);
    [[nodiscard]] EventDispatchResult route_wheel_event(WidgetImpl& root,
                                                        float x,
                                                        float y,
                                                        float delta);
    [[nodiscard]] EventDispatchResult route_keyboard_event(EventType type,
                                                           std::uint32_t key_code,
                                                           char32_t text_codepoint);
    [[nodiscard]] EventDispatchResult route_text_input_string(const char* text,
                                                              const char* action);
    [[nodiscard]] HitTestResult focused_target();
    [[nodiscard]] bool focused_input_text(std::string& out);
    [[nodiscard]] bool select_focused_input_all();

    [[nodiscard]] bool set_focus_target(WidgetImpl* target);
    [[nodiscard]] bool set_capture_target(WidgetImpl* target);
    [[nodiscard]] bool set_hover_target(WidgetImpl* target);
    void clear_targets_for_destroying(WidgetImpl& target);

    [[nodiscard]] ObjectId focus_target() const noexcept;
    [[nodiscard]] ObjectId capture_target() const noexcept;
    [[nodiscard]] ObjectId hover_target() const noexcept;
    [[nodiscard]] const WidgetImpl* hover_target_node() const noexcept;
    [[nodiscard]] float hover_x() const noexcept;
    [[nodiscard]] float hover_y() const noexcept;
    [[nodiscard]] const std::vector<FiuiEvent>& recent_events() const noexcept;

private:
    void record_event(const FiuiEvent& event);
    [[nodiscard]] bool contains(const Rect& rect, float x, float y) const noexcept;
    [[nodiscard]] WidgetImpl* open_modal_dialog(WidgetImpl& node) const;
    [[nodiscard]] bool target_is_descendant_of(const WidgetImpl& ancestor,
                                               const WidgetImpl* target) const noexcept;
    [[nodiscard]] WidgetImpl* hit_test_open_dialog(WidgetImpl& node, float x, float y) const;
    [[nodiscard]] WidgetImpl* hit_test_open_select_popup(WidgetImpl& node, float x, float y) const;
    [[nodiscard]] bool close_open_select_popups(WidgetImpl& node);
    [[nodiscard]] WidgetImpl* hit_test_node(WidgetImpl& node, float x, float y) const;
    [[nodiscard]] std::vector<EventRouteEntry> build_route(const WidgetImpl& target) const;
    [[nodiscard]] bool apply_pointer_targets(EventType type, WidgetImpl* target);
    [[nodiscard]] bool apply_scroll_wheel(FiuiEvent& event);
    [[nodiscard]] bool apply_scroll_keyboard(FiuiEvent& event);
    [[nodiscard]] bool apply_text_area_wheel(FiuiEvent& event);
    [[nodiscard]] bool set_text_area_scroll_offset(WidgetImpl& target,
                                                   float offset,
                                                   const char* action);
    [[nodiscard]] bool ensure_text_area_cursor_visible(WidgetImpl& target);
    [[nodiscard]] bool apply_scroll_thumb_drag(FiuiEvent& event);
    [[nodiscard]] bool apply_dialog_drag(FiuiEvent& event);
    [[nodiscard]] WidgetImpl* nearest_open_dialog(WidgetImpl* target) const noexcept;
    [[nodiscard]] bool dialog_drag_handle_hit(const WidgetImpl& dialog,
                                              const WidgetImpl* target,
                                              float x,
                                              float y) const noexcept;
    [[nodiscard]] bool apply_split_handle_drag(FiuiEvent& event);
    [[nodiscard]] bool apply_slider_pointer_change(FiuiEvent& event);
    [[nodiscard]] bool apply_slider_keyboard(FiuiEvent& event);
    [[nodiscard]] bool set_slider_value(WidgetImpl& slider,
                                        float value,
                                        const char* action,
                                        const char* detail);
    [[nodiscard]] bool is_scroll_keyboard_key(std::uint32_t key_code) const noexcept;
    [[nodiscard]] bool set_scroll_offset(WidgetImpl& scroll,
                                         float offset,
                                         const char* action,
                                         const char* detail);
    [[nodiscard]] WidgetImpl* scroll_thumb_target(WidgetImpl* target, float x, float y) const noexcept;
    [[nodiscard]] bool is_scroll_thumb_hit(const WidgetImpl& scroll, float x, float y) const noexcept;
    [[nodiscard]] Rect scroll_thumb_bounds(const WidgetImpl& scroll) const noexcept;
    [[nodiscard]] bool is_split_handle_hit(const WidgetImpl& split, float x, float y) const noexcept;
    [[nodiscard]] Rect split_handle_bounds(const WidgetImpl& split) const noexcept;
    [[nodiscard]] bool is_tree_toggle_hit(const WidgetImpl& item, float x, float y) const noexcept;
    [[nodiscard]] WidgetImpl* nearest_tree_view(WidgetImpl* target) const noexcept;
    void clear_tree_selection(WidgetImpl& node, WidgetImpl* except);
    [[nodiscard]] std::uint32_t table_row_from_y(const WidgetImpl& table, float y) const noexcept;
    [[nodiscard]] std::uint32_t table_column_from_x(const WidgetImpl& table, float x) const;
    [[nodiscard]] std::uint32_t table_resize_column_from_x(const WidgetImpl& table, float x) const;
    [[nodiscard]] bool select_table_row(WidgetImpl& table,
                                        std::uint32_t row,
                                        const char* action,
                                        bool invoke_callback,
                                        EventDispatchResult& result);
    [[nodiscard]] bool apply_table_pointer_action(FiuiEvent& event);
    [[nodiscard]] bool apply_table_wheel(FiuiEvent& event);
    [[nodiscard]] bool ensure_table_row_visible(WidgetImpl& table, std::uint32_t row);
    [[nodiscard]] WidgetImpl* nearest_scroll_view(WidgetImpl* target) const noexcept;
    [[nodiscard]] bool apply_input_pointer_selection(FiuiEvent& event);
    [[nodiscard]] std::size_t input_cursor_from_x(const WidgetImpl& target,
                                                  float x,
                                                  float y,
                                                  const char* path) const;
    [[nodiscard]] bool apply_text_edit(FiuiEvent& event);
    [[nodiscard]] bool apply_text_insert(WidgetImpl& target,
                                         const char* text,
                                         const char* action);
    [[nodiscard]] bool is_focusable(const WidgetImpl& target) const noexcept;
    void collect_focusable_nodes(WidgetImpl& node, std::vector<WidgetImpl*>& out) const;
    [[nodiscard]] WidgetImpl* next_focus_target(WidgetImpl& root, bool reverse);
    [[nodiscard]] bool move_focus(WidgetImpl& root, bool reverse, EventDispatchResult& result);
    void diagnostics_route(const FiuiEvent& event, const char* action) const;
    [[nodiscard]] bool set_target(WidgetImpl*& target_slot,
                                  ObjectId& id_slot,
                                  std::uint32_t& generation_slot,
                                  WidgetImpl* target,
                                  const char* action);
    [[nodiscard]] bool validate_target(WidgetImpl*& target_slot,
                                       ObjectId& id_slot,
                                       std::uint32_t& generation_slot,
                                       const char* slot_name);
    void invalidate_target_change(WidgetImpl* old_target, WidgetImpl* new_target, const char* action);

    ObjectId focus_target_ = 0;
    ObjectId capture_target_ = 0;
    ObjectId hover_target_ = 0;
    std::uint32_t focus_generation_ = 0;
    std::uint32_t capture_generation_ = 0;
    std::uint32_t hover_generation_ = 0;
    WidgetImpl* focus_target_node_ = nullptr;
    WidgetImpl* capture_target_node_ = nullptr;
    WidgetImpl* hover_target_node_ = nullptr;
    float hover_x_ = 0.0f;
    float hover_y_ = 0.0f;
    std::vector<FiuiEvent> recent_events_;
};

FIUI_API const char* event_type_name(EventType type) noexcept;

} // namespace fiui
