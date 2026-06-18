#pragma once

#include "fiui/types.h"
#include "fiui/widget.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fiui {

enum class LayoutSizeMode {
    Auto,
    Fixed,
    Fill,
    Flex,
};

struct ObjectHeader {
    std::atomic<std::uint32_t> ref_count{1};
    ObjectId object_id = 0;
    std::uint32_t generation = 1;
    LifecycleState lifecycle_state = LifecycleState::Created;
};

struct NodeData {
    WidgetKind kind = WidgetKind::Widget;
    WidgetImpl* parent = nullptr;
    std::vector<WidgetImpl*> children;
    std::string debug_id;
    std::string fallback_id;
    std::string cached_path;
};

struct TableColumnData {
    std::string label;
    float width = 0.0f;
};

struct WidgetProperties {
    std::string text;
    std::string style_name;
    std::string placeholder;
    std::string resource_path;
    std::string tooltip;
    std::string button_normal_image_path;
    std::string button_hover_image_path;
    std::string button_pressed_image_path;
    std::uint64_t image_resource_id = 0;
    std::uint64_t button_normal_image_resource_id = 0;
    std::uint64_t button_hover_image_resource_id = 0;
    std::uint64_t button_pressed_image_resource_id = 0;
    float requested_width = 0.0f;
    float requested_height = 0.0f;
    LayoutSizeMode width_mode = LayoutSizeMode::Auto;
    LayoutSizeMode height_mode = LayoutSizeMode::Auto;
    OverflowPolicy overflow_policy = OverflowPolicy::Clip;
    bool overflow_policy_explicit = false;
    float flex_grow = 1.0f;
    float padding = 0.0f;
    float gap = 0.0f;
    float numeric_value = 0.0f;
    bool checked = false;
    std::string radio_group;
    bool enabled = true;
    bool visible = true;
    Color button_background;
    Color button_hover_background;
    Color button_pressed_background;
    float button_text_padding = 0.0f;
    float button_radius = 0.0f;
    bool has_button_background = false;
    bool has_button_hover_background = false;
    bool has_button_pressed_background = false;
    bool has_button_text_padding = false;
    bool has_button_radius = false;
    std::size_t text_cursor = 0;
    std::size_t text_selection_anchor = 0;
    bool text_selection_dragging = false;
    bool text_multiline = false;
    float scroll_offset_y = 0.0f;
    float scroll_content_height = 0.0f;
    bool scroll_thumb_dragging = false;
    float scroll_drag_start_y = 0.0f;
    float scroll_drag_start_offset_y = 0.0f;
    bool dialog_open = false;
    bool dialog_modal = true;
    bool dialog_backdrop_closes = true;
    bool dialog_escape_closes = true;
    float dialog_backdrop_opacity = 0.32f;
    bool dialog_dragging = false;
    bool dialog_suppress_backdrop_click = false;
    float dialog_drag_start_x = 0.0f;
    float dialog_drag_start_y = 0.0f;
    float dialog_drag_origin_offset_x = 0.0f;
    float dialog_drag_origin_offset_y = 0.0f;
    float dialog_offset_x = 0.0f;
    float dialog_offset_y = 0.0f;
    SplitOrientation split_orientation = SplitOrientation::Horizontal;
    float split_ratio = 0.5f;
    float split_min_pane_size = 80.0f;
    float split_handle_size = 6.0f;
    bool split_handle_dragging = false;
    float split_drag_start_position = 0.0f;
    float split_drag_start_ratio = 0.5f;
    ImageFit image_fit = ImageFit::Stretch;
    float image_radius = 0.0f;
    bool menu_popup_open = false;
    bool menu_enabled = true;
    bool menu_checked = false;
    std::string menu_shortcut;
    std::uint32_t selected_index = 0;
    bool has_selected_index = false;
    ObjectId selected_object_id = 0;
    bool tree_expanded = true;
    bool tree_selected = false;
    std::uint32_t tree_depth = 0;
    std::vector<TableColumnData> table_columns;
    std::vector<std::vector<std::string>> table_rows;
    float table_header_height = 30.0f;
    float table_row_height = 28.0f;
    std::uint32_t table_sort_column = 0xffffffffu;
    bool table_sort_ascending = true;
    bool table_column_resizing = false;
    bool table_resize_suppress_click = false;
    std::uint32_t table_resize_column = 0xffffffffu;
    float table_resize_start_x = 0.0f;
    float table_resize_start_width = 0.0f;
    bool select_popup_open = false;
    std::string selected_text_cache;
    std::vector<std::string> tab_labels;
    std::uint32_t selected_tab_index = 0;
    EventCallback click_callback = nullptr;
    void* click_user_data = nullptr;
    EventCallback text_change_callback = nullptr;
    void* text_change_user_data = nullptr;
};

struct DirtyState {
    Rect bounds;
    Rect paint_bounds;
    Rect clip_bounds;
    DirtyReason reason = DirtyReason::None;
    std::uint64_t last_mutation_frame = 0;
};

struct WidgetImpl {
    ObjectHeader object;
    NodeData node;
    WidgetProperties properties;
    DirtyState dirty;
};

WidgetImpl* make_widget_impl(WidgetKind kind, const char* initial_text);
void retain_widget_impl(WidgetImpl* impl) noexcept;
void release_widget_impl(WidgetImpl* impl) noexcept;
void mutate_widget(WidgetImpl& impl, DirtyReason reason, const char* action);
bool attach_child(WidgetImpl& parent, WidgetImpl& child);
bool detach_widget(WidgetImpl& child);
const char* widget_kind_name(WidgetKind kind) noexcept;
const char* lifecycle_state_name(LifecycleState state) noexcept;
const char* dirty_reason_name(DirtyReason reason) noexcept;
std::string widget_path(const WidgetImpl& impl);
std::uint64_t next_frame_id();
std::uint64_t current_frame_id();

} // namespace fiui
