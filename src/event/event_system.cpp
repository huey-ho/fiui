#include "event/event_system.h"

#include "diagnostics/diagnostics_internal.h"
#include "runtime/runtime.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>

namespace fiui {
namespace {

constexpr std::uint32_t key_modifier_shift = 1u << 16;
constexpr std::uint32_t key_modifier_alt = 1u << 17;
constexpr std::uint32_t key_modifier_ctrl = 1u << 18;
constexpr std::uint32_t key_code_mask = 0xffffu;

float dpi_scale() noexcept
{
    const std::uint32_t dpi = default_runtime().platform_system().state().dpi;
    return std::max(0.25f, static_cast<float>(std::max<std::uint32_t>(1, dpi)) / 96.0f);
}

float scaled(float value) noexcept
{
    return value * dpi_scale();
}

ObjectId target_id(WidgetImpl* target)
{
    return target == nullptr ? 0 : target->object.object_id;
}

bool is_text_editable(const WidgetImpl& target) noexcept
{
    return target.node.kind == WidgetKind::Input || target.node.kind == WidgetKind::TextArea;
}

std::uint32_t target_generation(WidgetImpl* target)
{
    return target == nullptr ? 0 : target->object.generation;
}

bool is_utf8_continuation(unsigned char ch) noexcept
{
    return (ch & 0xc0u) == 0x80u;
}

std::size_t clamp_utf8_boundary(const std::string& text, std::size_t offset) noexcept
{
    offset = std::min(offset, text.size());
    while (offset > 0 && offset < text.size() &&
           is_utf8_continuation(static_cast<unsigned char>(text[offset]))) {
        --offset;
    }
    return offset;
}

std::size_t previous_utf8_boundary(const std::string& text, std::size_t offset) noexcept
{
    offset = clamp_utf8_boundary(text, offset);
    if (offset == 0) {
        return 0;
    }
    --offset;
    while (offset > 0 && is_utf8_continuation(static_cast<unsigned char>(text[offset]))) {
        --offset;
    }
    return offset;
}

std::size_t next_utf8_boundary(const std::string& text, std::size_t offset) noexcept
{
    offset = clamp_utf8_boundary(text, offset);
    if (offset >= text.size()) {
        return text.size();
    }
    ++offset;
    while (offset < text.size() &&
           is_utf8_continuation(static_cast<unsigned char>(text[offset]))) {
        ++offset;
    }
    return offset;
}

std::vector<std::size_t> utf8_boundaries(const std::string& text)
{
    std::vector<std::size_t> boundaries;
    boundaries.push_back(0);
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        cursor = next_utf8_boundary(text, cursor);
        if (boundaries.back() != cursor) {
            boundaries.push_back(cursor);
        }
    }
    return boundaries;
}

bool append_utf8_codepoint(std::string& out, char32_t codepoint)
{
    if (codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
        return false;
    }
    if (codepoint <= 0x7f) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0u | ((codepoint >> 6u) & 0x1fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    } else if (codepoint <= 0xffff) {
        out.push_back(static_cast<char>(0xe0u | ((codepoint >> 12u) & 0x0fu)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    } else {
        out.push_back(static_cast<char>(0xf0u | ((codepoint >> 18u) & 0x07u)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3fu)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    }
    return true;
}

float text_line_height(const WidgetImpl& target) noexcept
{
    (void)target;
    const Theme& theme = default_runtime().style_system().active_theme();
    return std::max(scaled(16.0f), scaled(theme.typography.body) * 1.35f);
}

std::size_t line_start_for(const std::string& text, std::size_t cursor) noexcept
{
    cursor = std::min(cursor, text.size());
    if (cursor > 0 && cursor <= text.size() && text[cursor - 1] == '\n') {
        --cursor;
    }
    const std::size_t found = text.rfind('\n', cursor);
    return found == std::string::npos ? 0 : found + 1;
}

std::size_t line_end_for(const std::string& text, std::size_t cursor) noexcept
{
    cursor = std::min(cursor, text.size());
    const std::size_t found = text.find('\n', cursor);
    return found == std::string::npos ? text.size() : found;
}

std::size_t cursor_line_index(const std::string& text, std::size_t cursor) noexcept
{
    cursor = std::min(cursor, text.size());
    std::size_t line = 0;
    for (std::size_t index = 0; index < cursor; ++index) {
        if (text[index] == '\n') {
            ++line;
        }
    }
    return line;
}

std::size_t line_start_by_index(const std::string& text, std::size_t line_index) noexcept
{
    if (line_index == 0) {
        return 0;
    }
    std::size_t line = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\n') {
            ++line;
            if (line == line_index) {
                return index + 1;
            }
        }
    }
    return text.size();
}

std::size_t move_cursor_vertical(const std::string& text,
                                 std::size_t cursor,
                                 int line_delta) noexcept
{
    cursor = clamp_utf8_boundary(text, cursor);
    const std::size_t current_line = cursor_line_index(text, cursor);
    const std::size_t current_start = line_start_for(text, cursor);
    const std::size_t current_column = cursor - current_start;
    const std::size_t line_count = cursor_line_index(text, text.size()) + 1u;
    const std::size_t target_line =
        line_delta < 0
            ? current_line - std::min<std::size_t>(current_line, static_cast<std::size_t>(-line_delta))
            : std::min<std::size_t>(line_count - 1u,
                                    current_line + static_cast<std::size_t>(line_delta));
    const std::size_t target_start = line_start_by_index(text, target_line);
    const std::size_t target_end = line_end_for(text, target_start);
    return clamp_utf8_boundary(text, std::min(target_end, target_start + current_column));
}

void invoke_text_change_callback(WidgetImpl& target, const char* action)
{
    if (target.properties.text_change_callback == nullptr) {
        return;
    }
    try {
        target.properties.text_change_callback(target.properties.text_change_user_data);
    } catch (...) {
        diagnostics_event_ex("event", "callback_exception", target.object.object_id,
                             target.object.generation, current_frame_id(),
                             widget_path(target).c_str(),
                             action == nullptr ? "text change callback threw" : action);
    }
}

std::string sanitize_utf8_single_line(const char* value, std::uint32_t& skipped)
{
    const char* input = value == nullptr ? "" : value;
    std::string out;
    skipped = 0;
    for (std::size_t index = 0; input[index] != '\0';) {
        const unsigned char ch = static_cast<unsigned char>(input[index]);
        if (ch == '\r' || ch == '\n' || ch == '\t') {
            ++index;
            continue;
        }
        if (ch < 0x20 || ch == 0x7f) {
            ++skipped;
            ++index;
            continue;
        }
        std::size_t length = 0;
        char32_t codepoint = 0;
        if (ch < 0x80) {
            length = 1;
            codepoint = ch;
        } else if ((ch & 0xe0u) == 0xc0u) {
            length = 2;
            codepoint = ch & 0x1fu;
        } else if ((ch & 0xf0u) == 0xe0u) {
            length = 3;
            codepoint = ch & 0x0fu;
        } else if ((ch & 0xf8u) == 0xf0u) {
            length = 4;
            codepoint = ch & 0x07u;
        } else {
            ++skipped;
            ++index;
            continue;
        }
        bool valid = true;
        for (std::size_t part = 1; part < length; ++part) {
            const unsigned char continuation = static_cast<unsigned char>(input[index + part]);
            if (continuation == '\0' || !is_utf8_continuation(continuation)) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6u) | (continuation & 0x3fu);
        }
        if (!valid || (length == 2 && codepoint < 0x80) ||
            (length == 3 && codepoint < 0x800) ||
            (length == 4 && codepoint < 0x10000) || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            ++skipped;
            ++index;
            continue;
        }
        out.append(input + index, length);
        index += length;
    }
    return out;
}

std::string sanitize_utf8_multiline(const char* value, std::uint32_t& skipped)
{
    const char* input = value == nullptr ? "" : value;
    std::string out;
    skipped = 0;
    for (std::size_t index = 0; input[index] != '\0';) {
        const unsigned char ch = static_cast<unsigned char>(input[index]);
        if (ch == '\r') {
            if (input[index + 1] == '\n') {
                ++index;
            }
            out.push_back('\n');
            ++index;
            continue;
        }
        if (ch == '\n') {
            out.push_back('\n');
            ++index;
            continue;
        }
        if (ch == '\t') {
            out.push_back('\t');
            ++index;
            continue;
        }
        if (ch < 0x20 || ch == 0x7f) {
            ++skipped;
            ++index;
            continue;
        }
        std::size_t length = 0;
        char32_t codepoint = 0;
        if (ch < 0x80) {
            length = 1;
            codepoint = ch;
        } else if ((ch & 0xe0u) == 0xc0u) {
            length = 2;
            codepoint = ch & 0x1fu;
        } else if ((ch & 0xf0u) == 0xe0u) {
            length = 3;
            codepoint = ch & 0x0fu;
        } else if ((ch & 0xf8u) == 0xf0u) {
            length = 4;
            codepoint = ch & 0x07u;
        } else {
            ++skipped;
            ++index;
            continue;
        }
        bool valid = true;
        for (std::size_t part = 1; part < length; ++part) {
            const unsigned char continuation = static_cast<unsigned char>(input[index + part]);
            if (continuation == '\0' || !is_utf8_continuation(continuation)) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6u) | (continuation & 0x3fu);
        }
        if (!valid || (length == 2 && codepoint < 0x80) ||
            (length == 3 && codepoint < 0x800) ||
            (length == 4 && codepoint < 0x10000) || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            ++skipped;
            ++index;
            continue;
        }
        out.append(input + index, length);
        index += length;
    }
    return out;
}

void clear_radio_group_selection(WidgetImpl& node, const std::string& group, WidgetImpl* except)
{
    if (node.node.kind == WidgetKind::RadioButton && &node != except &&
        node.properties.radio_group == group && node.properties.checked) {
        node.properties.checked = false;
        mutate_widget(node, DirtyReason::Input | DirtyReason::Paint, "radio_button_unchecked");
    }
    for (WidgetImpl* child : node.node.children) {
        if (child != nullptr) {
            clear_radio_group_selection(*child, group, except);
        }
    }
}

std::uint32_t child_index_in_parent(const WidgetImpl& child) noexcept
{
    if (child.node.parent == nullptr) {
        return 0;
    }
    std::uint32_t index = 0;
    for (const WidgetImpl* sibling : child.node.parent->node.children) {
        if (sibling == &child) {
            return index;
        }
        ++index;
    }
    return 0;
}

std::uint32_t tab_index_from_x(const WidgetImpl& tabs, float x) noexcept
{
    const std::uint32_t count = static_cast<std::uint32_t>(tabs.node.children.size());
    if (count == 0 || tabs.dirty.bounds.width <= 0.0f) {
        return 0;
    }
    const float item_width = std::max(1.0f, tabs.dirty.bounds.width / static_cast<float>(count));
    const float local_x = std::max(0.0f, x - tabs.dirty.bounds.x);
    return std::min<std::uint32_t>(count - 1u, static_cast<std::uint32_t>(local_x / item_width));
}

} // namespace

EventDispatchResult EventSystem::dispatch_click(WidgetImpl& target)
{
    FiuiEvent event;
    event.type = EventType::Click;
    event.target_object_id = target.object.object_id;
    event.target_generation = target.object.generation;
    event.target_path = widget_path(target);
    event.target = &target;
    event.route = build_route(target);
    record_event(event);
    diagnostics_route(event, "route");

    diagnostics_event_ex("event", "dispatch_begin", event.target_object_id,
                         event.target_generation, current_frame_id(), event.target_path.c_str(),
                         event_type_name(event.type));

    EventDispatchResult result;
    MenuSystem& menus = default_runtime().menu_system();
    if (!target.properties.enabled) {
        result.handled = true;
        diagnostics_event_ex("event", "disabled", event.target_object_id,
                             event.target_generation, current_frame_id(), event.target_path.c_str(),
                             "target disabled");
        return result;
    }
    if (target.node.kind == WidgetKind::MenuItem && !target.properties.menu_enabled) {
        result.handled = true;
        diagnostics_event_ex("event", "menu_disabled", event.target_object_id,
                             event.target_generation, current_frame_id(), event.target_path.c_str(),
                             "disabled");
        return result;
    }
    if (target.node.kind == WidgetKind::CheckBox) {
        target.properties.checked = !target.properties.checked;
        mutate_widget(target, DirtyReason::Input | DirtyReason::Paint,
                      target.properties.checked ? "check_box_checked" : "check_box_unchecked");
        diagnostics_event_ex("event", "check_box_toggle", event.target_object_id,
                             event.target_generation, current_frame_id(), event.target_path.c_str(),
                             target.properties.checked ? "checked" : "unchecked");
    }
    if (target.node.kind == WidgetKind::Switch) {
        target.properties.checked = !target.properties.checked;
        mutate_widget(target, DirtyReason::Input | DirtyReason::Paint,
                      target.properties.checked ? "switch_checked" : "switch_unchecked");
        diagnostics_event_ex("event", "switch_toggle", event.target_object_id,
                             event.target_generation, current_frame_id(), event.target_path.c_str(),
                             target.properties.checked ? "checked" : "unchecked");
    }
    if (target.node.kind == WidgetKind::RadioButton) {
        const bool was_checked = target.properties.checked;
        target.properties.checked = true;
        if (!target.properties.radio_group.empty() && target.node.parent != nullptr) {
            clear_radio_group_selection(*target.node.parent, target.properties.radio_group,
                                        &target);
        }
        if (!was_checked || !target.properties.radio_group.empty()) {
            mutate_widget(target, DirtyReason::Input | DirtyReason::Paint,
                          "radio_button_checked");
        }
        diagnostics_event_ex("event", "radio_button_select", event.target_object_id,
                             event.target_generation, current_frame_id(), event.target_path.c_str(),
                             target.properties.radio_group.empty()
                                 ? "default"
                                 : target.properties.radio_group.c_str());
    }
    if (target.node.kind == WidgetKind::Select) {
        target.properties.select_popup_open = !target.properties.select_popup_open;
        mutate_widget(target, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      target.properties.select_popup_open ? "select_open" : "select_close");
        diagnostics_event_ex("event", "select_toggle", event.target_object_id,
                             event.target_generation, current_frame_id(), event.target_path.c_str(),
                             target.properties.select_popup_open ? "open" : "closed");
        result.handled = true;
        return result;
    }
    if (target.node.kind == WidgetKind::SelectOption && target.node.parent != nullptr &&
        target.node.parent->node.kind == WidgetKind::Select) {
        WidgetImpl& select = *target.node.parent;
        const std::uint32_t option_index = child_index_in_parent(target);
        select.properties.selected_index = option_index;
        select.properties.has_selected_index = true;
        select.properties.selected_text_cache = target.properties.text;
        select.properties.select_popup_open = false;
        mutate_widget(select, DirtyReason::Input | DirtyReason::TextChanged |
                                  DirtyReason::Layout | DirtyReason::Paint,
                      "select_option");
        diagnostics_event_ex("event", "select_option", select.object.object_id,
                             select.object.generation, current_frame_id(),
                             widget_path(select).c_str(), target.properties.text.c_str());
        if (select.properties.click_callback != nullptr) {
            try {
                select.properties.click_callback(select.properties.click_user_data);
            } catch (...) {
                result.callback_failed = true;
                diagnostics_event_ex("event", "callback_exception", select.object.object_id,
                                     select.object.generation, current_frame_id(),
                                     widget_path(select).c_str(),
                                     "callback threw during select change");
            }
        }
        result.handled = true;
        return result;
    }
    if (target.node.kind == WidgetKind::ListItem && target.node.parent != nullptr &&
        target.node.parent->node.kind == WidgetKind::ListView) {
        WidgetImpl& list = *target.node.parent;
        const std::uint32_t item_index = child_index_in_parent(target);
        const bool changed = !list.properties.has_selected_index ||
                             list.properties.selected_index != item_index;
        list.properties.selected_index = item_index;
        list.properties.has_selected_index = true;
        list.properties.selected_text_cache = target.properties.text;
        mutate_widget(list, DirtyReason::Input | DirtyReason::TextChanged |
                                DirtyReason::Paint,
                      "list_item");
        diagnostics_event_ex("event", "list_item", list.object.object_id,
                             list.object.generation, current_frame_id(),
                             widget_path(list).c_str(), target.properties.text.c_str());
        if (changed && list.properties.click_callback != nullptr) {
            try {
                list.properties.click_callback(list.properties.click_user_data);
            } catch (...) {
                result.callback_failed = true;
                diagnostics_event_ex("event", "callback_exception", list.object.object_id,
                                     list.object.generation, current_frame_id(),
                                     widget_path(list).c_str(),
                                     "callback threw during list change");
            }
        }
        result.handled = true;
        return result;
    }
    if (target.node.kind == WidgetKind::TreeItem) {
        WidgetImpl* tree = nearest_tree_view(&target);
        if (tree != nullptr && is_tree_toggle_hit(target, hover_x_, hover_y_) &&
            !target.node.children.empty()) {
            target.properties.tree_expanded = !target.properties.tree_expanded;
            mutate_widget(target, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                          target.properties.tree_expanded ? "tree_item_expand"
                                                          : "tree_item_collapse");
            diagnostics_event_ex("event", "tree_toggle", target.object.object_id,
                                 target.object.generation, current_frame_id(),
                                 widget_path(target).c_str(),
                                 target.properties.tree_expanded ? "expanded" : "collapsed");
            result.handled = true;
            return result;
        }
        if (tree != nullptr) {
            const bool changed = tree->properties.selected_object_id != target.object.object_id;
            clear_tree_selection(*tree, &target);
            target.properties.tree_selected = true;
            tree->properties.selected_object_id = target.object.object_id;
            tree->properties.selected_text_cache = target.properties.text;
            mutate_widget(*tree, DirtyReason::Input | DirtyReason::TextChanged |
                                     DirtyReason::Paint,
                          "tree_item_select");
            mutate_widget(target, DirtyReason::Input | DirtyReason::Paint,
                          "tree_item_selected");
            diagnostics_event_ex("event", "tree_item_select", tree->object.object_id,
                                 tree->object.generation, current_frame_id(),
                                 widget_path(*tree).c_str(), target.properties.text.c_str());
            if (changed && tree->properties.click_callback != nullptr) {
                try {
                    tree->properties.click_callback(tree->properties.click_user_data);
                } catch (...) {
                    result.callback_failed = true;
                    diagnostics_event_ex("event", "callback_exception",
                                         tree->object.object_id, tree->object.generation,
                                         current_frame_id(), widget_path(*tree).c_str(),
                                         "callback threw during tree change");
                }
            }
            result.handled = true;
            return result;
        }
    }
    if (target.node.kind == WidgetKind::TableView) {
        if (target.properties.table_resize_suppress_click) {
            target.properties.table_resize_suppress_click = false;
            result.handled = true;
            return result;
        }
        const float header_height =
            scaled(std::max(1.0f, target.properties.table_header_height));
        if (hover_y_ >= target.dirty.bounds.y &&
            hover_y_ <= target.dirty.bounds.y + header_height) {
            const std::uint32_t column = table_column_from_x(target, hover_x_);
            if (column < target.properties.table_columns.size()) {
                const bool ascending =
                    target.properties.table_sort_column == column
                        ? !target.properties.table_sort_ascending
                        : true;
                std::sort(target.properties.table_rows.begin(),
                          target.properties.table_rows.end(),
                          [column, ascending](const std::vector<std::string>& left,
                                              const std::vector<std::string>& right) {
                              const std::string left_value =
                                  column < left.size() ? left[column] : "";
                              const std::string right_value =
                                  column < right.size() ? right[column] : "";
                              return ascending ? left_value < right_value
                                               : right_value < left_value;
                          });
                target.properties.table_sort_column = column;
                target.properties.table_sort_ascending = ascending;
                if (!target.properties.table_rows.empty()) {
                    target.properties.selected_index =
                        std::min<std::uint32_t>(target.properties.selected_index,
                                                static_cast<std::uint32_t>(
                                                    target.properties.table_rows.size() - 1u));
                    target.properties.has_selected_index = true;
                    const std::vector<std::string>& row =
                        target.properties.table_rows[target.properties.selected_index];
                    target.properties.selected_text_cache = row.empty() ? "" : row.front();
                }
                mutate_widget(target, DirtyReason::Input | DirtyReason::TextChanged |
                                          DirtyReason::Paint,
                              ascending ? "table_sort_ascending" : "table_sort_descending");
                diagnostics_event_ex("event", "table_sort", target.object.object_id,
                                     target.object.generation, current_frame_id(),
                                     widget_path(target).c_str(),
                                     ascending ? "ascending" : "descending");
            }
        } else {
            const std::uint32_t row = table_row_from_y(target, hover_y_);
            if (row < target.properties.table_rows.size()) {
                (void)select_table_row(target, row, "table_row_select", true, result);
            }
        }
        result.handled = true;
        return result;
    }
    if (target.node.kind == WidgetKind::Dialog) {
        if (target.properties.dialog_open && target.properties.dialog_backdrop_closes &&
            !target.properties.dialog_suppress_backdrop_click) {
            target.properties.dialog_open = false;
            mutate_widget(target, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                          "dialog_close_backdrop");
            if (target_is_descendant_of(target, focus_target_node_)) {
                (void)set_focus_target(nullptr);
            }
            diagnostics_event_ex("event", "dialog_close", target.object.object_id,
                                 target.object.generation, current_frame_id(),
                                 widget_path(target).c_str(), "backdrop");
            result.handled = true;
        }
        target.properties.dialog_suppress_backdrop_click = false;
        return result;
    }
    if (target.node.kind == WidgetKind::Tabs) {
        const float header_height = scaled(38.0f);
        if (hover_y_ >= target.dirty.bounds.y &&
            hover_y_ <= target.dirty.bounds.y + header_height &&
            !target.node.children.empty()) {
            const std::uint32_t next_index = tab_index_from_x(target, hover_x_);
            if (target.properties.selected_tab_index != next_index) {
                target.properties.selected_tab_index = next_index;
                mutate_widget(target, DirtyReason::Input | DirtyReason::Layout |
                                          DirtyReason::Paint,
                              "tabs_select");
                diagnostics_event_ex("event", "tabs_select", event.target_object_id,
                                     event.target_generation, current_frame_id(),
                                     event.target_path.c_str(), "pointer");
                if (target.properties.click_callback != nullptr) {
                    try {
                        target.properties.click_callback(target.properties.click_user_data);
                    } catch (...) {
                        result.callback_failed = true;
                        diagnostics_event_ex("event", "callback_exception",
                                             event.target_object_id, event.target_generation,
                                             current_frame_id(), event.target_path.c_str(),
                                             "callback threw during tabs change");
                    }
                }
            }
        }
        result.handled = true;
        return result;
    }
    if (menus.has_menu_children(target)) {
        const bool should_open = !target.properties.menu_popup_open;
        if (should_open) {
            (void)menus.open_exclusive(target);
        } else {
            (void)menus.close_all(*menus.root_of(target));
        }
        result.handled = true;
        return result;
    }

    if (target.properties.click_callback == nullptr) {
        result.handled = true;
        if (target.node.kind == WidgetKind::MenuItem) {
            (void)menus.close_all(*menus.root_of(target));
        }
        diagnostics_event_ex("event", "dispatch_end", event.target_object_id,
                             event.target_generation, current_frame_id(), event.target_path.c_str(),
                             "no_callback");
        return result;
    }

    try {
        target.properties.click_callback(target.properties.click_user_data);
        result.handled = true;
        if (target.node.kind == WidgetKind::MenuItem) {
            (void)menus.close_all(*menus.root_of(target));
        }
        diagnostics_event_ex("event", "dispatch_end", event.target_object_id,
                             event.target_generation, current_frame_id(), event.target_path.c_str(),
                             "handled");
    } catch (...) {
        if (target.node.kind == WidgetKind::MenuItem) {
            (void)menus.close_all(*menus.root_of(target));
        }
        result.callback_failed = true;
        diagnostics_event_ex("event", "callback_exception", event.target_object_id,
                             event.target_generation, current_frame_id(), event.target_path.c_str(),
                             "callback threw during event dispatch");
    }

    return result;
}

HitTestResult EventSystem::hit_test(WidgetImpl& root, float x, float y) const
{
    HitTestResult result;
    result.x = x;
    result.y = y;

    WidgetImpl* target = hit_test_open_dialog(root, x, y);
    if (target == nullptr) {
        target = default_runtime().menu_system().hit_test_open_popup(root, x, y);
    }
    if (target == nullptr) {
        target = hit_test_open_select_popup(root, x, y);
    }
    if (target == nullptr) {
        target = hit_test_node(root, x, y);
    }
    if (target == nullptr) {
        diagnostics_event_ex("event", "hit_test_miss", root.object.object_id,
                             root.object.generation, current_frame_id(), widget_path(root).c_str(),
                             "no target");
        return result;
    }

    result.hit = true;
    result.target_object_id = target->object.object_id;
    result.target_generation = target->object.generation;
    result.target_path = widget_path(*target);
    result.target = target;
    diagnostics_event_ex("event", "hit_test", result.target_object_id, result.target_generation,
                         current_frame_id(), result.target_path.c_str(), "hit");
    return result;
}

EventDispatchResult EventSystem::route_pointer_event(WidgetImpl& root,
                                                     EventType type,
                                                     float x,
                                                     float y)
{
    if (type == EventType::PointerMove || type == EventType::PointerDown ||
        type == EventType::PointerUp) {
        hover_x_ = x;
        hover_y_ = y;
    }
    EventDispatchResult result;
    result.hit_test = hit_test(root, x, y);
    if ((type == EventType::PointerMove || type == EventType::PointerUp) &&
        capture_target_node_ != nullptr &&
        (is_text_editable(*capture_target_node_) ||
         capture_target_node_->node.kind == WidgetKind::Dialog ||
         capture_target_node_->node.kind == WidgetKind::ScrollView ||
         capture_target_node_->node.kind == WidgetKind::Slider ||
         capture_target_node_->node.kind == WidgetKind::SplitView ||
         capture_target_node_->node.kind == WidgetKind::TableView)) {
        result.hit_test.hit = true;
        result.hit_test.x = x;
        result.hit_test.y = y;
        result.hit_test.target_object_id = capture_target_node_->object.object_id;
        result.hit_test.target_generation = capture_target_node_->object.generation;
        result.hit_test.target_path = widget_path(*capture_target_node_);
        result.hit_test.target = capture_target_node_;
    }
    if (!result.hit_test.hit) {
        if (type == EventType::PointerDown) {
            (void)default_runtime().menu_system().close_all(root);
            result.target_changed = close_open_select_popups(root) || result.target_changed;
        }
        result.target_changed = apply_pointer_targets(type, nullptr);
        return result;
    }

    WidgetImpl* target = result.hit_test.target;
    MenuSystem& menus = default_runtime().menu_system();
    if (type == EventType::PointerDown &&
        target->node.kind != WidgetKind::Select &&
        target->node.kind != WidgetKind::SelectOption) {
        (void)close_open_select_popups(root);
    }
    if (type == EventType::PointerDown && !menus.target_is_menu_related(target)) {
        (void)menus.close_all(root);
    }
    if (type == EventType::PointerDown) {
        WidgetImpl* scroll_target = scroll_thumb_target(target, x, y);
        if (scroll_target != nullptr) {
            target = scroll_target;
            result.hit_test.target_object_id = target->object.object_id;
            result.hit_test.target_generation = target->object.generation;
            result.hit_test.target_path = widget_path(*target);
            result.hit_test.target = target;
        }
    }
    result.target_changed = apply_pointer_targets(type, target);
    if (type == EventType::PointerMove) {
        result.target_changed = menus.update_on_hover(root, *target) || result.target_changed;
    }

    FiuiEvent event;
    event.type = type;
    event.target_object_id = result.hit_test.target_object_id;
    event.target_generation = result.hit_test.target_generation;
    event.target_path = result.hit_test.target_path;
    event.target = target;
    event.x = x;
    event.y = y;
    event.route = build_route(*target);
    result.route = event.route;
    result.handled = true;

    record_event(event);
    if (type == EventType::PointerDown || type == EventType::PointerMove ||
        type == EventType::PointerUp) {
        result.target_changed = apply_input_pointer_selection(event) || result.target_changed;
        result.target_changed = apply_dialog_drag(event) || result.target_changed;
        result.target_changed = apply_scroll_thumb_drag(event) || result.target_changed;
        result.target_changed = apply_slider_pointer_change(event) || result.target_changed;
        result.target_changed = apply_split_handle_drag(event) || result.target_changed;
        result.target_changed = apply_table_pointer_action(event) || result.target_changed;
    }
    if (type != EventType::PointerMove || result.target_changed) {
        diagnostics_route(event, "route_pointer");
        diagnostics_event_ex("event", "dispatch_end", event.target_object_id,
                             event.target_generation, current_frame_id(),
                             event.target_path.c_str(), event_type_name(type));
    }
    return result;
}

EventDispatchResult EventSystem::route_wheel_event(WidgetImpl& root,
                                                   float x,
                                                   float y,
                                                   float delta)
{
    EventDispatchResult result;
    result.hit_test = hit_test(root, x, y);
    if (!result.hit_test.hit) {
        diagnostics_event_ex("event", "wheel_miss", root.object.object_id, root.object.generation,
                             current_frame_id(), widget_path(root).c_str(), "no target");
        return result;
    }

    FiuiEvent event;
    event.type = EventType::Wheel;
    event.target_object_id = result.hit_test.target_object_id;
    event.target_generation = result.hit_test.target_generation;
    event.target_path = result.hit_test.target_path;
    event.target = result.hit_test.target;
    event.x = x;
    event.y = y;
    event.wheel_delta = delta;
    event.route = build_route(*event.target);
    result.route = event.route;
    result.handled =
        apply_table_wheel(event) || apply_text_area_wheel(event) || apply_scroll_wheel(event);
    record_event(event);
    diagnostics_route(event, "route_wheel");
    return result;
}

std::size_t EventSystem::input_cursor_from_x(const WidgetImpl& target,
                                             float x,
                                             float y,
                                             const char* path) const
{
    const std::string& text = target.properties.text;
    const Theme& theme = default_runtime().style_system().active_theme();
    const float font_size = scaled(theme.typography.body);
    const float left_padding = scaled(theme.spacing.md);
    const float top_padding = scaled(theme.spacing.sm);
    const float local_x = std::max(0.0f, x - target.dirty.bounds.x - left_padding);
    const float local_y =
        std::max(0.0f, y - target.dirty.bounds.y - top_padding + target.properties.scroll_offset_y);
    const float max_width = std::max(1.0f, target.dirty.bounds.width - left_padding * 2.0f);
    const std::uint32_t dpi = default_runtime().platform_system().state().dpi;
    std::size_t line_start = 0;
    std::size_t line_end = text.size();
    if (target.node.kind == WidgetKind::TextArea) {
        const std::size_t line_index =
            static_cast<std::size_t>(std::max(0.0f, local_y) / text_line_height(target));
        line_start = line_start_by_index(text, line_index);
        line_end = line_end_for(text, line_start);
    }

    std::size_t best_cursor = line_start;
    float best_distance = local_x;
    const std::vector<std::size_t> boundaries = utf8_boundaries(text);
    for (std::size_t index : boundaries) {
        if (index < line_start || index > line_end) {
            continue;
        }
        const std::string prefix = text.substr(line_start, index - line_start);
        const TextMetrics metrics = default_runtime().text_system().measure_text(
            prefix.c_str(), font_size, max_width, text_line_height(target), dpi,
            target.object.object_id, path == nullptr ? "" : path, "none", "ellipsis");
        const float distance = std::abs(metrics.width - local_x);
        if (distance <= best_distance) {
            best_distance = distance;
            best_cursor = index;
        }
    }
    return best_cursor;
}

bool EventSystem::apply_input_pointer_selection(FiuiEvent& event)
{
    WidgetImpl* target = event.target;
    if (target == nullptr || !is_text_editable(*target)) {
        return false;
    }
    if (event.type == EventType::PointerMove && !target->properties.text_selection_dragging) {
        return false;
    }

    const std::size_t old_cursor = target->properties.text_cursor;
    const std::size_t old_anchor = target->properties.text_selection_anchor;
    const bool old_dragging = target->properties.text_selection_dragging;
    const std::size_t best_cursor =
        input_cursor_from_x(*target, event.x, event.y, event.target_path.c_str());
    target->properties.text_cursor = best_cursor;
    if (event.type == EventType::PointerDown) {
        target->properties.text_selection_anchor = best_cursor;
        target->properties.text_selection_dragging = true;
    } else if (event.type == EventType::PointerMove &&
               target->properties.text_selection_dragging) {
        target->properties.text_cursor = best_cursor;
    } else if (event.type == EventType::PointerUp) {
        target->properties.text_cursor = best_cursor;
        target->properties.text_selection_dragging = false;
    }

    std::ostringstream detail;
    detail << "cursor=" << target->properties.text_cursor
           << ";anchor=" << target->properties.text_selection_anchor << ";x=" << event.x
           << ";dragging=" << (target->properties.text_selection_dragging ? "true" : "false");
    const std::string detail_text = detail.str();
    diagnostics_event_ex("event", "text_selection_pointer", event.target_object_id,
                         event.target_generation, current_frame_id(), event.target_path.c_str(),
                         detail_text.c_str());
    if (target->properties.text_cursor != old_cursor ||
        target->properties.text_selection_anchor != old_anchor ||
        target->properties.text_selection_dragging != old_dragging) {
        mutate_widget(*target, DirtyReason::Input | DirtyReason::Paint, "text_selection_pointer");
        return true;
    }
    mutate_widget(*target, DirtyReason::Input | DirtyReason::Paint, "text_selection_pointer");
    return false;
}

EventDispatchResult EventSystem::route_keyboard_event(EventType type,
                                                      std::uint32_t key_code,
                                                      char32_t text_codepoint)
{
    EventDispatchResult result;
    MenuSystem& menus = default_runtime().menu_system();
    WidgetImpl* bound_root = default_runtime().platform_system().bound_root();
    if (type == EventType::KeyUp && bound_root != nullptr) {
        WidgetImpl* shortcut_target = menus.shortcut_reveal_target();
        if (shortcut_target != nullptr && menus.clear_shortcut_reveal(*bound_root)) {
            (void)set_hover_target(nullptr);
            result.handled = true;
            result.target_changed = true;
            result.hit_test.hit = true;
            result.hit_test.target_object_id = shortcut_target->object.object_id;
            result.hit_test.target_generation = shortcut_target->object.generation;
            result.hit_test.target_path = widget_path(*shortcut_target);
            result.hit_test.target = shortcut_target;
            diagnostics_event_ex("event", "menu_shortcut_keyup",
                                 shortcut_target->object.object_id,
                                 shortcut_target->object.generation, current_frame_id(),
                                 widget_path(*shortcut_target).c_str(), "closed");
            return result;
        }
    }

    WidgetImpl* target = nullptr;
    if (validate_target(focus_target_node_, focus_target_, focus_generation_, "focus_target")) {
        target = focus_target_node_;
    } else if (type == EventType::KeyDown && is_scroll_keyboard_key(key_code) &&
               validate_target(hover_target_node_, hover_target_, hover_generation_,
                               "hover_target")) {
        target = hover_target_node_;
    } else if (type == EventType::KeyDown &&
               ((key_code & key_code_mask) == 0x1b ||
                default_runtime().menu_system().is_menu_navigation_key(key_code)) &&
               validate_target(hover_target_node_, hover_target_, hover_generation_,
                               "hover_target")) {
        target = hover_target_node_;
    }

    if (target == nullptr && type == EventType::KeyDown && bound_root != nullptr) {
        const std::uint32_t key = key_code & key_code_mask;
        if ((key_code & key_modifier_alt) != 0 || key == 0x1b || menus.is_menu_navigation_key(key)) {
            target = menus.open_top_level_menu_item(*bound_root);
            if (target == nullptr && (key_code & key_modifier_alt) != 0) {
                target = menus.first_top_level_menu_item(*bound_root);
            }
            if (target == nullptr && key == 0x1b) {
                target = bound_root;
            }
        }
    }

    if (target != nullptr && !target->properties.enabled &&
        (type != EventType::KeyDown || (key_code & key_code_mask) != 0x09)) {
        diagnostics_event_ex("event", "keyboard_disabled", target->object.object_id,
                             target->object.generation, current_frame_id(),
                             widget_path(*target).c_str(), event_type_name(type));
        return result;
    }

    if (type == EventType::KeyDown && bound_root != nullptr &&
        (key_code & key_code_mask) == 0x1b) {
        if (WidgetImpl* dialog = open_modal_dialog(*bound_root)) {
            if (dialog->properties.dialog_escape_closes) {
                dialog->properties.dialog_open = false;
                mutate_widget(*dialog, DirtyReason::Input | DirtyReason::Layout |
                                           DirtyReason::Paint,
                              "dialog_close_escape");
                if (target_is_descendant_of(*dialog, focus_target_node_)) {
                    (void)set_focus_target(nullptr);
                }
                result.handled = true;
                result.target_changed = true;
                result.hit_test.hit = true;
                result.hit_test.target_object_id = dialog->object.object_id;
                result.hit_test.target_generation = dialog->object.generation;
                result.hit_test.target_path = widget_path(*dialog);
                result.hit_test.target = dialog;
                diagnostics_event_ex("event", "dialog_close", dialog->object.object_id,
                                     dialog->object.generation, current_frame_id(),
                                     widget_path(*dialog).c_str(), "escape");
                return result;
            }
        }
    }

    if (type == EventType::KeyDown && bound_root != nullptr &&
        (key_code & key_modifier_ctrl) != 0) {
        if (WidgetImpl* shortcut_target = menus.menu_item_for_shortcut(*bound_root, key_code)) {
            (void)menus.reveal_shortcut_target(*bound_root, *shortcut_target);
            (void)set_hover_target(shortcut_target);

            result.handled = true;
            result.target_changed = true;
            result.menu_shortcut_triggered = true;
            result.hit_test.hit = true;
            result.hit_test.target_object_id = shortcut_target->object.object_id;
            result.hit_test.target_generation = shortcut_target->object.generation;
            result.hit_test.target_path = widget_path(*shortcut_target);
            result.hit_test.target = shortcut_target;

            FiuiEvent event;
            event.type = type;
            event.target_object_id = result.hit_test.target_object_id;
            event.target_generation = result.hit_test.target_generation;
            event.target_path = result.hit_test.target_path;
            event.target = shortcut_target;
            event.key_code = key_code;
            event.text_codepoint = text_codepoint;
            event.route = build_route(*shortcut_target);
            result.route = event.route;
            record_event(event);
            diagnostics_route(event, "route_menu_shortcut");

            if (shortcut_target->properties.click_callback != nullptr) {
                try {
                    shortcut_target->properties.click_callback(
                        shortcut_target->properties.click_user_data);
                    diagnostics_event_ex("event", "menu_shortcut_dispatch", 
                                         shortcut_target->object.object_id,
                                         shortcut_target->object.generation, current_frame_id(),
                                         widget_path(*shortcut_target).c_str(), "handled");
                } catch (...) {
                    result.callback_failed = true;
                    diagnostics_event_ex("event", "callback_exception",
                                         shortcut_target->object.object_id,
                                         shortcut_target->object.generation, current_frame_id(),
                                         widget_path(*shortcut_target).c_str(),
                                         "callback threw during menu shortcut dispatch");
                }
            } else {
                diagnostics_event_ex("event", "menu_shortcut_dispatch",
                                     shortcut_target->object.object_id,
                                     shortcut_target->object.generation, current_frame_id(),
                                     widget_path(*shortcut_target).c_str(), "no_callback");
            }
            return result;
        }
    }

    if (type == EventType::KeyDown && bound_root != nullptr &&
        (key_code & key_code_mask) == 0x09) {
        const bool reverse = (key_code & key_modifier_shift) != 0;
        if (move_focus(*bound_root, reverse, result)) {
            return result;
        }
    }

    if (target == nullptr) {
        diagnostics_event_ex("event", "keyboard_miss", 0, 0, current_frame_id(), "",
                             event_type_name(type));
        return result;
    }

    if (type == EventType::KeyDown && (key_code & key_code_mask) == 0x1b) {
        WidgetImpl* root = bound_root != nullptr ? bound_root : menus.root_of(*target);
        const bool closed = menus.close_all(*root);
        if (closed) {
            result.handled = true;
            result.target_changed = true;
            result.hit_test.hit = true;
            result.hit_test.target_object_id = target->object.object_id;
            result.hit_test.target_generation = target->object.generation;
            result.hit_test.target_path = widget_path(*target);
            result.hit_test.target = target;
            diagnostics_event_ex("event", "menu_popup_escape", target->object.object_id,
                                 target->object.generation, current_frame_id(),
                                 widget_path(*target).c_str(), "closed");
            return result;
        }
    }

    if (type == EventType::KeyDown && (key_code & key_modifier_alt) != 0) {
        WidgetImpl* root = bound_root != nullptr ? bound_root : menus.root_of(*target);
        const std::uint32_t key = key_code & key_code_mask;
        WidgetImpl* next_target = nullptr;
        if (key == 0x12) {
            next_target = menus.open_top_level_menu_item(*root);
            if (next_target == nullptr) {
                next_target = menus.first_top_level_menu_item(*root);
            }
        } else {
            next_target = menus.top_level_menu_item_for_mnemonic(*root, key);
        }
        if (next_target != nullptr) {
            (void)menus.open_exclusive(*next_target);
            (void)set_focus_target(next_target);
            (void)set_hover_target(next_target);
            result.handled = true;
            result.target_changed = true;
            result.hit_test.hit = true;
            result.hit_test.target_object_id = next_target->object.object_id;
            result.hit_test.target_generation = next_target->object.generation;
            result.hit_test.target_path = widget_path(*next_target);
            result.hit_test.target = next_target;
            diagnostics_event_ex("event", "menu_alt", next_target->object.object_id,
                                 next_target->object.generation, current_frame_id(),
                                 widget_path(*next_target).c_str(), "activated");
            return result;
        }
    }

    if (type == EventType::KeyDown && menus.is_menu_navigation_key(key_code) &&
        menus.target_is_menu_related(target)) {
        WidgetImpl* root = bound_root != nullptr ? bound_root : menus.root_of(*target);
        WidgetImpl* active_top = menus.open_top_level_menu_item(*root);
        WidgetImpl* target_top = menus.top_level_menu_item_for(target);
        const std::uint32_t key = key_code & key_code_mask;
        WidgetImpl* next_target = nullptr;

        if (key == 0x27 && target != nullptr && !menus.is_top_level_menu_item(*target) &&
            menus.has_menu_children(*target)) {
            (void)menus.open_exclusive(*target);
            next_target = menus.first_menu_child(*target);
        } else if (key == 0x25 && target != nullptr && target->node.parent != nullptr &&
                   target->node.parent->node.kind == WidgetKind::MenuItem &&
                   !menus.is_top_level_menu_item(*target->node.parent)) {
            WidgetImpl* parent = target->node.parent;
            (void)menus.close_all(*parent);
            next_target = parent;
        } else if ((key == 0x25 || key == 0x27) &&
                   (active_top != nullptr || target_top != nullptr)) {
            WidgetImpl* basis = active_top != nullptr ? active_top : target_top;
            next_target = menus.sibling_menu_item(*basis, key == 0x27 ? 1 : -1);
            if (next_target != nullptr) {
                (void)menus.open_exclusive(*next_target);
            }
        } else if (key == 0x28) {
            if (target_top != nullptr && menus.has_menu_children(*target_top)) {
                if (!target_top->properties.menu_popup_open) {
                    (void)menus.open_exclusive(*target_top);
                }
                next_target = menus.first_menu_child(*target_top);
            } else if (target != nullptr && target->node.parent != nullptr &&
                       target->node.parent->node.kind == WidgetKind::MenuItem) {
                next_target = menus.sibling_menu_item(*target, 1);
            }
        } else if (key == 0x26) {
            if (target != nullptr && target->node.parent != nullptr &&
                target->node.parent->node.kind == WidgetKind::MenuItem) {
                next_target = menus.sibling_menu_item(*target, -1);
            } else if (active_top != nullptr) {
                next_target = menus.last_menu_child(*active_top);
            }
        } else if (key == 0x0d) {
            EventDispatchResult click_result = dispatch_click(*target);
            result.handled = click_result.handled;
            result.callback_failed = click_result.callback_failed;
            result.target_changed = true;
        }

        if (next_target != nullptr) {
            (void)set_focus_target(next_target);
            (void)set_hover_target(next_target);
            result.handled = true;
            result.target_changed = true;
            target = next_target;
        }

        if (result.handled || result.callback_failed) {
            result.hit_test.hit = true;
            result.hit_test.target_object_id = target->object.object_id;
            result.hit_test.target_generation = target->object.generation;
            result.hit_test.target_path = widget_path(*target);
            result.hit_test.target = target;
            diagnostics_event_ex("event", "menu_keyboard", target->object.object_id,
                                 target->object.generation, current_frame_id(),
                                 widget_path(*target).c_str(), event_type_name(type));
            return result;
        }
    }

    if (type == EventType::KeyDown &&
        (target->node.kind == WidgetKind::Button || target->node.kind == WidgetKind::CheckBox ||
         target->node.kind == WidgetKind::RadioButton ||
         target->node.kind == WidgetKind::Switch)) {
        const std::uint32_t key = key_code & key_code_mask;
        if (key == 0x0d || key == 0x20) {
            EventDispatchResult click_result = dispatch_click(*target);
            result.handled = click_result.handled;
            result.callback_failed = click_result.callback_failed;
            result.target_changed = true;
            result.hit_test.hit = true;
            result.hit_test.target_object_id = target->object.object_id;
            result.hit_test.target_generation = target->object.generation;
            result.hit_test.target_path = widget_path(*target);
            result.hit_test.target = target;
            result.route = build_route(*target);
            mutate_widget(*target, DirtyReason::Input | DirtyReason::Paint,
                          key == 0x0d ? "keyboard_activate_enter" : "keyboard_activate_space");
            diagnostics_event_ex("event", "keyboard_activate", target->object.object_id,
                                 target->object.generation, current_frame_id(),
                                 widget_path(*target).c_str(),
                                 key == 0x0d ? "enter" : "space");
            return result;
        }
    }

    if (type == EventType::KeyDown && target->node.kind == WidgetKind::Select) {
        const std::uint32_t key = key_code & key_code_mask;
        if (key == 0x0d || key == 0x20) {
            target->properties.select_popup_open = !target->properties.select_popup_open;
            mutate_widget(*target, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                          target->properties.select_popup_open ? "select_open"
                                                               : "select_close");
            diagnostics_event_ex("event", "select_keyboard_toggle", target->object.object_id,
                                 target->object.generation, current_frame_id(),
                                 widget_path(*target).c_str(),
                                 target->properties.select_popup_open ? "open" : "closed");
            result.handled = true;
            result.target_changed = true;
            result.hit_test.hit = true;
            result.hit_test.target_object_id = target->object.object_id;
            result.hit_test.target_generation = target->object.generation;
            result.hit_test.target_path = widget_path(*target);
            result.hit_test.target = target;
            result.route = build_route(*target);
            return result;
        }
        if (key == 0x1b && target->properties.select_popup_open) {
            target->properties.select_popup_open = false;
            mutate_widget(*target, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                          "select_close");
            diagnostics_event_ex("event", "select_keyboard_escape", target->object.object_id,
                                 target->object.generation, current_frame_id(),
                                 widget_path(*target).c_str(), "close");
            result.handled = true;
            result.target_changed = true;
            return result;
        }
        if ((key == 0x28 || key == 0x26 || key == 0x24 || key == 0x23) &&
            target->properties.select_popup_open &&
            !target->node.children.empty()) {
            const std::uint32_t count = static_cast<std::uint32_t>(target->node.children.size());
            if (count > 0) {
                const std::uint32_t current = target->properties.has_selected_index
                                                  ? std::min(target->properties.selected_index,
                                                             count - 1u)
                                                  : 0u;
                std::uint32_t next = current;
                if (key == 0x28) {
                    next = (current + 1u) % count;
                } else if (key == 0x26) {
                    next = (current + count - 1u) % count;
                } else if (key == 0x24) {
                    next = 0;
                } else if (key == 0x23) {
                    next = count - 1u;
                }
                const bool changed =
                    !target->properties.has_selected_index ||
                    target->properties.selected_index != next;
                target->properties.selected_index = next;
                WidgetImpl* option = target->node.children[next];
                target->properties.selected_text_cache =
                    option == nullptr ? "" : option->properties.text;
                target->properties.has_selected_index = true;
                mutate_widget(*target, DirtyReason::Input | DirtyReason::TextChanged |
                                          DirtyReason::Paint,
                              "select_keyboard_nav");
                if (changed && target->properties.click_callback != nullptr) {
                    try {
                        target->properties.click_callback(target->properties.click_user_data);
                    } catch (...) {
                        result.callback_failed = true;
                        diagnostics_event_ex("event", "callback_exception",
                                             target->object.object_id, target->object.generation,
                                             current_frame_id(), widget_path(*target).c_str(),
                                             "callback threw during select keyboard change");
                    }
                }
                diagnostics_event_ex("event", "select_keyboard_nav", target->object.object_id,
                                     target->object.generation, current_frame_id(),
                                     widget_path(*target).c_str(),
                                     option == nullptr ? "" : option->properties.text.c_str());
                result.handled = true;
                result.target_changed = true;
                return result;
            }
        }
    }

    if (type == EventType::KeyDown &&
        (target->node.kind == WidgetKind::ListView ||
         (target->node.kind == WidgetKind::ListItem && target->node.parent != nullptr &&
          target->node.parent->node.kind == WidgetKind::ListView))) {
        WidgetImpl* list =
            target->node.kind == WidgetKind::ListView ? target : target->node.parent;
        const std::uint32_t key = key_code & key_code_mask;
        const std::uint32_t count = static_cast<std::uint32_t>(list->node.children.size());
        if (count > 0) {
            std::uint32_t next = list->properties.has_selected_index
                                     ? std::min(list->properties.selected_index, count - 1u)
                                     : 0u;
            bool handled = true;
            if (key == 0x28) {
                next = (next + 1u) % count;
            } else if (key == 0x26) {
                next = next == 0 ? count - 1u : next - 1u;
            } else if (key == 0x22) {
                const float header_height =
                    scaled(std::max(1.0f, target->properties.table_header_height));
                const float row_height =
                    scaled(std::max(1.0f, target->properties.table_row_height));
                const std::uint32_t page_rows =
                    std::max<std::uint32_t>(1u, static_cast<std::uint32_t>(
                                                   std::max(1.0f,
                                                            target->dirty.bounds.height -
                                                                header_height) /
                                                   row_height));
                next = std::min<std::uint32_t>(count - 1u, next + page_rows);
            } else if (key == 0x21) {
                const float header_height =
                    scaled(std::max(1.0f, target->properties.table_header_height));
                const float row_height =
                    scaled(std::max(1.0f, target->properties.table_row_height));
                const std::uint32_t page_rows =
                    std::max<std::uint32_t>(1u, static_cast<std::uint32_t>(
                                                   std::max(1.0f,
                                                            target->dirty.bounds.height -
                                                                header_height) /
                                                   row_height));
                next = next > page_rows ? next - page_rows : 0u;
            } else if (key == 0x24) {
                next = 0;
            } else if (key == 0x23) {
                next = count - 1u;
            } else if (key == 0x0d || key == 0x20) {
                handled = true;
            } else {
                handled = false;
            }
            if (handled) {
                const bool changed = !list->properties.has_selected_index ||
                                     list->properties.selected_index != next;
                list->properties.selected_index = next;
                list->properties.has_selected_index = true;
                WidgetImpl* item = list->node.children[next];
                list->properties.selected_text_cache =
                    item == nullptr ? "" : item->properties.text;
                mutate_widget(*list, DirtyReason::Input | DirtyReason::TextChanged |
                                         DirtyReason::Paint,
                              "list_keyboard_select");
                if (item != nullptr) {
                    (void)set_focus_target(item);
                    (void)set_hover_target(item);
                }
                if ((changed || key == 0x0d || key == 0x20) &&
                    list->properties.click_callback != nullptr) {
                    try {
                        list->properties.click_callback(list->properties.click_user_data);
                    } catch (...) {
                        result.callback_failed = true;
                        diagnostics_event_ex("event", "callback_exception",
                                             list->object.object_id, list->object.generation,
                                             current_frame_id(), widget_path(*list).c_str(),
                                             "callback threw during list keyboard change");
                    }
                }
                diagnostics_event_ex("event", "list_keyboard", list->object.object_id,
                                     list->object.generation, current_frame_id(),
                                     widget_path(*list).c_str(), event_type_name(type));
                result.handled = true;
                result.target_changed = true;
                result.hit_test.hit = true;
                result.hit_test.target_object_id = list->object.object_id;
                result.hit_test.target_generation = list->object.generation;
                result.hit_test.target_path = widget_path(*list);
                result.hit_test.target = list;
                result.route = build_route(*list);
                return result;
            }
        }
    }

    if (type == EventType::KeyDown && target->node.kind == WidgetKind::TreeItem) {
        const std::uint32_t key = key_code & key_code_mask;
        if ((key == 0x25 || key == 0x27) && !target->node.children.empty()) {
            const bool expand = key == 0x27;
            if (target->properties.tree_expanded != expand) {
                target->properties.tree_expanded = expand;
                mutate_widget(*target, DirtyReason::Input | DirtyReason::Layout |
                                           DirtyReason::Paint,
                              expand ? "tree_keyboard_expand" : "tree_keyboard_collapse");
            }
            result.handled = true;
            result.target_changed = true;
            result.hit_test.hit = true;
            result.hit_test.target_object_id = target->object.object_id;
            result.hit_test.target_generation = target->object.generation;
            result.hit_test.target_path = widget_path(*target);
            result.hit_test.target = target;
            result.route = build_route(*target);
            return result;
        }
        if (key == 0x0d || key == 0x20) {
            EventDispatchResult click_result = dispatch_click(*target);
            result.handled = click_result.handled;
            result.callback_failed = click_result.callback_failed;
            result.target_changed = true;
            result.hit_test.hit = true;
            result.hit_test.target_object_id = target->object.object_id;
            result.hit_test.target_generation = target->object.generation;
            result.hit_test.target_path = widget_path(*target);
            result.hit_test.target = target;
            result.route = build_route(*target);
            return result;
        }
    }

    if (type == EventType::KeyDown && target->node.kind == WidgetKind::TableView) {
        const std::uint32_t key = key_code & key_code_mask;
        const std::uint32_t count =
            static_cast<std::uint32_t>(target->properties.table_rows.size());
        if (count > 0) {
            std::uint32_t next = target->properties.has_selected_index
                                     ? std::min(target->properties.selected_index, count - 1u)
                                     : 0u;
            bool handled = true;
            if (key == 0x28) {
                next = (next + 1u) % count;
            } else if (key == 0x26) {
                next = next == 0 ? count - 1u : next - 1u;
            } else if (key == 0x24) {
                next = 0;
            } else if (key == 0x23) {
                next = count - 1u;
            } else if (key == 0x0d || key == 0x20) {
                handled = true;
            } else {
                handled = false;
            }
            if (handled) {
                (void)select_table_row(*target, next, "table_keyboard_select",
                                       true, result);
                result.handled = true;
                result.target_changed = true;
                result.hit_test.hit = true;
                result.hit_test.target_object_id = target->object.object_id;
                result.hit_test.target_generation = target->object.generation;
                result.hit_test.target_path = widget_path(*target);
                result.hit_test.target = target;
                result.route = build_route(*target);
                return result;
            }
        }
    }

    if (type == EventType::KeyDown && target->node.kind == WidgetKind::Tabs &&
        !target->node.children.empty()) {
        const std::uint32_t key = key_code & key_code_mask;
        const std::uint32_t count = static_cast<std::uint32_t>(target->node.children.size());
        std::uint32_t next = std::min(target->properties.selected_tab_index, count - 1u);
        bool handled = true;
        if (key == 0x25) {
            next = next == 0 ? count - 1u : next - 1u;
        } else if (key == 0x27) {
            next = (next + 1u) % count;
        } else if (key == 0x24) {
            next = 0;
        } else if (key == 0x23) {
            next = count - 1u;
        } else {
            handled = false;
        }
        if (handled) {
            if (target->properties.selected_tab_index != next) {
                target->properties.selected_tab_index = next;
                mutate_widget(*target, DirtyReason::Input | DirtyReason::Layout |
                                          DirtyReason::Paint,
                              "tabs_keyboard_select");
                if (target->properties.click_callback != nullptr) {
                    try {
                        target->properties.click_callback(target->properties.click_user_data);
                    } catch (...) {
                        result.callback_failed = true;
                        diagnostics_event_ex("event", "callback_exception",
                                             target->object.object_id, target->object.generation,
                                             current_frame_id(), widget_path(*target).c_str(),
                                             "callback threw during tabs keyboard change");
                    }
                }
            }
            diagnostics_event_ex("event", "tabs_keyboard", target->object.object_id,
                                 target->object.generation, current_frame_id(),
                                 widget_path(*target).c_str(), event_type_name(type));
            result.handled = true;
            result.target_changed = true;
            result.hit_test.hit = true;
            result.hit_test.target_object_id = target->object.object_id;
            result.hit_test.target_generation = target->object.generation;
            result.hit_test.target_path = widget_path(*target);
            result.hit_test.target = target;
            result.route = build_route(*target);
            return result;
        }
    }

    if (type == EventType::KeyDown && target->node.kind == WidgetKind::Slider) {
        FiuiEvent event;
        event.type = type;
        event.target_object_id = target->object.object_id;
        event.target_generation = target->object.generation;
        event.target_path = widget_path(*target);
        event.target = target;
        event.key_code = key_code;
        event.route = build_route(*target);
        result.route = event.route;
        record_event(event);
        result.handled = apply_slider_keyboard(event);
        if (result.handled) {
            result.target_changed = true;
            result.hit_test.hit = true;
            result.hit_test.target_object_id = target->object.object_id;
            result.hit_test.target_generation = target->object.generation;
            result.hit_test.target_path = widget_path(*target);
            result.hit_test.target = target;
            diagnostics_route(event, "route_keyboard");
            diagnostics_event_ex("event", "slider_keyboard", target->object.object_id,
                                 target->object.generation, current_frame_id(),
                                 widget_path(*target).c_str(), event_type_name(type));
            return result;
        }
    }

    FiuiEvent event;
    event.type = type;
    event.target_object_id = target->object.object_id;
    event.target_generation = target->object.generation;
    event.target_path = widget_path(*target);
    event.target = target;
    event.key_code = key_code;
    event.text_codepoint = text_codepoint;
    event.route = build_route(*target);
    result.route = event.route;
    result.hit_test.hit = true;
    result.hit_test.target_object_id = event.target_object_id;
    result.hit_test.target_generation = event.target_generation;
    result.hit_test.target_path = event.target_path;
    result.hit_test.target = target;

    record_event(event);
    diagnostics_route(event, "route_keyboard");

    const bool edited = apply_text_edit(event);
    const bool scrolled = apply_scroll_keyboard(event);
    result.handled = edited || scrolled || type == EventType::KeyDown || type == EventType::KeyUp;

    std::ostringstream detail;
    detail << event_type_name(type) << ";key=" << key_code
           << ";codepoint=" << static_cast<std::uint32_t>(text_codepoint)
           << ";edited=" << (edited ? "true" : "false")
           << ";scrolled=" << (scrolled ? "true" : "false");
    const std::string text = detail.str();
    diagnostics_event_ex("event", "keyboard_dispatch_end", event.target_object_id,
                         event.target_generation, current_frame_id(), event.target_path.c_str(),
                         text.c_str());
    return result;
}

EventDispatchResult EventSystem::route_text_input_string(const char* text, const char* action)
{
    EventDispatchResult result;
    if (!validate_target(focus_target_node_, focus_target_, focus_generation_,
                         "focus_target")) {
        diagnostics_event_ex("event", "text_input_miss", 0, 0, current_frame_id(), "",
                             action == nullptr ? "text_input_string" : action);
        return result;
    }

    WidgetImpl* target = focus_target_node_;
    FiuiEvent event;
    event.type = EventType::TextInput;
    event.target_object_id = target->object.object_id;
    event.target_generation = target->object.generation;
    event.target_path = widget_path(*target);
    event.target = target;
    event.route = build_route(*target);
    result.route = event.route;
    result.hit_test.hit = true;
    result.hit_test.target_object_id = event.target_object_id;
    result.hit_test.target_generation = event.target_generation;
    result.hit_test.target_path = event.target_path;
    result.hit_test.target = target;

    record_event(event);
    diagnostics_route(event, "route_text_input");

    const char* edit_action = action == nullptr ? "text_input_string" : action;
    result.handled = apply_text_insert(*target, text, edit_action);
    diagnostics_event_ex("event", "text_input_dispatch_end", event.target_object_id,
                         event.target_generation, current_frame_id(), event.target_path.c_str(),
                         edit_action);
    return result;
}

HitTestResult EventSystem::focused_target()
{
    HitTestResult result;
    if (!validate_target(focus_target_node_, focus_target_, focus_generation_,
                         "focus_target")) {
        return result;
    }
    result.hit = true;
    result.target_object_id = focus_target_node_->object.object_id;
    result.target_generation = focus_target_node_->object.generation;
    result.target_path = widget_path(*focus_target_node_);
    result.target = focus_target_node_;
    return result;
}

bool EventSystem::focused_input_text(std::string& out)
{
    if (!validate_target(focus_target_node_, focus_target_, focus_generation_,
                         "focus_target") ||
        !is_text_editable(*focus_target_node_)) {
        return false;
    }
    const std::string& text = focus_target_node_->properties.text;
    const std::size_t anchor =
        std::min<std::size_t>(focus_target_node_->properties.text_selection_anchor, text.size());
    const std::size_t cursor =
        std::min<std::size_t>(focus_target_node_->properties.text_cursor, text.size());
    if (anchor != cursor) {
        const std::size_t begin = std::min(anchor, cursor);
        const std::size_t end = std::max(anchor, cursor);
        out = text.substr(begin, end - begin);
    } else {
        out = text;
    }
    return true;
}

bool EventSystem::select_focused_input_all()
{
    if (!validate_target(focus_target_node_, focus_target_, focus_generation_,
                         "focus_target") ||
        !is_text_editable(*focus_target_node_)) {
        diagnostics_event_ex("event", "select_all_ignored", 0, 0, current_frame_id(), "",
                             "focused input unavailable");
        return false;
    }

    WidgetImpl& target = *focus_target_node_;
    target.properties.text_selection_anchor = 0;
    target.properties.text_cursor = target.properties.text.size();
    target.properties.text_selection_dragging = false;
    diagnostics_event_ex("event", "select_all", target.object.object_id, target.object.generation,
                         current_frame_id(), widget_path(target).c_str(), "focused input");
    mutate_widget(target, DirtyReason::Input | DirtyReason::Paint, "select_all");
    return true;
}

bool EventSystem::set_focus_target(WidgetImpl* target)
{
    return set_target(focus_target_node_, focus_target_, focus_generation_, target,
                      "focus_target");
}

bool EventSystem::set_capture_target(WidgetImpl* target)
{
    return set_target(capture_target_node_, capture_target_, capture_generation_, target,
                      "capture_target");
}

bool EventSystem::set_hover_target(WidgetImpl* target)
{
    return set_target(hover_target_node_, hover_target_, hover_generation_, target,
                      "hover_target");
}

void EventSystem::clear_targets_for_destroying(WidgetImpl& target)
{
    if (focus_target_node_ == &target) {
        (void)set_focus_target(nullptr);
    }
    if (capture_target_node_ == &target) {
        (void)set_capture_target(nullptr);
    }
    if (hover_target_node_ == &target) {
        (void)set_hover_target(nullptr);
    }
}

ObjectId EventSystem::focus_target() const noexcept
{
    return focus_target_;
}

ObjectId EventSystem::capture_target() const noexcept
{
    return capture_target_;
}

ObjectId EventSystem::hover_target() const noexcept
{
    return hover_target_;
}

const WidgetImpl* EventSystem::hover_target_node() const noexcept
{
    return hover_target_node_;
}

float EventSystem::hover_x() const noexcept
{
    return hover_x_;
}

float EventSystem::hover_y() const noexcept
{
    return hover_y_;
}

const std::vector<FiuiEvent>& EventSystem::recent_events() const noexcept
{
    return recent_events_;
}

void EventSystem::record_event(const FiuiEvent& event)
{
    constexpr std::size_t max_recent_events = 64;
    if (recent_events_.size() >= max_recent_events) {
        recent_events_.erase(recent_events_.begin());
    }
    recent_events_.push_back(event);
}

bool EventSystem::contains(const Rect& rect, float x, float y) const noexcept
{
    return x >= rect.x && y >= rect.y && x <= rect.x + rect.width &&
           y <= rect.y + rect.height;
}

WidgetImpl* EventSystem::open_modal_dialog(WidgetImpl& node) const
{
    if (node.node.kind == WidgetKind::Dialog && node.properties.dialog_open &&
        node.properties.dialog_modal &&
        !node.node.children.empty()) {
        return &node;
    }
    for (WidgetImpl* child : node.node.children) {
        if (child == nullptr) {
            continue;
        }
        if (WidgetImpl* found = open_modal_dialog(*child)) {
            return found;
        }
    }
    return nullptr;
}

bool EventSystem::target_is_descendant_of(const WidgetImpl& ancestor,
                                          const WidgetImpl* target) const noexcept
{
    const WidgetImpl* cursor = target;
    while (cursor != nullptr) {
        if (cursor == &ancestor) {
            return true;
        }
        cursor = cursor->node.parent;
    }
    return false;
}

WidgetImpl* EventSystem::hit_test_open_dialog(WidgetImpl& node, float x, float y) const
{
    if (node.node.kind == WidgetKind::Dialog && node.properties.dialog_open &&
        !node.node.children.empty()) {
        if (!contains(node.dirty.bounds, x, y)) {
            return nullptr;
        }
        WidgetImpl* panel = node.node.children.front();
        if (panel != nullptr) {
            WidgetImpl* hit = hit_test_node(*panel, x, y);
            if (hit != nullptr) {
                return hit;
            }
            if (contains(panel->dirty.bounds, x, y)) {
                return panel;
            }
        }
        if (node.properties.dialog_modal) {
            return &node;
        }
    }

    for (auto it = node.node.children.rbegin(); it != node.node.children.rend(); ++it) {
        WidgetImpl* child = *it;
        if (child == nullptr) {
            continue;
        }
        WidgetImpl* hit = hit_test_open_dialog(*child, x, y);
        if (hit != nullptr) {
            return hit;
        }
    }
    return nullptr;
}

WidgetImpl* EventSystem::hit_test_open_select_popup(WidgetImpl& node, float x, float y) const
{
    if (node.node.kind == WidgetKind::Select && node.properties.select_popup_open &&
        !node.node.children.empty()) {
        WidgetImpl* hit = hit_test_node(node, x, y);
        if (hit != nullptr) {
            return hit;
        }
    }

    for (auto it = node.node.children.rbegin(); it != node.node.children.rend(); ++it) {
        WidgetImpl* child = *it;
        if (child == nullptr) {
            continue;
        }
        WidgetImpl* hit = hit_test_open_select_popup(*child, x, y);
        if (hit != nullptr) {
            return hit;
        }
    }
    return nullptr;
}

bool EventSystem::close_open_select_popups(WidgetImpl& node)
{
    bool changed = false;
    if (node.node.kind == WidgetKind::Select && node.properties.select_popup_open) {
        node.properties.select_popup_open = false;
        mutate_widget(node, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      "select_close");
        diagnostics_event_ex("event", "select_close", node.object.object_id,
                             node.object.generation, current_frame_id(),
                             widget_path(node).c_str(), "pointer_down_outside");
        changed = true;
    }
    for (WidgetImpl* child : node.node.children) {
        if (child != nullptr) {
            changed = close_open_select_popups(*child) || changed;
        }
    }
    return changed;
}

WidgetImpl* EventSystem::hit_test_node(WidgetImpl& node, float x, float y) const
{
    if (!node.properties.visible) {
        return nullptr;
    }
    if (node.node.kind == WidgetKind::Dialog) {
        return nullptr;
    }
    const bool menu_popup = node.node.kind == WidgetKind::MenuItem &&
                            node.properties.menu_popup_open &&
                            !node.node.children.empty();
    const bool select_popup = node.node.kind == WidgetKind::Select &&
                              node.properties.select_popup_open &&
                              !node.node.children.empty();
    const bool tree_item_children = node.node.kind == WidgetKind::TreeItem &&
                                    node.properties.tree_expanded &&
                                    !node.node.children.empty();
    const bool inside_node = contains(node.dirty.bounds, x, y);
    if (node.node.kind == WidgetKind::ScrollView && !inside_node) {
        return nullptr;
    }
    const bool clips_children = node.properties.overflow_policy == OverflowPolicy::Clip &&
                                node.node.kind != WidgetKind::ScrollView &&
                                node.node.kind != WidgetKind::MenuItem &&
                                node.node.kind != WidgetKind::Select &&
                                node.node.kind != WidgetKind::TreeItem;
    if (!inside_node && clips_children) {
        return nullptr;
    }
    if (!inside_node && !menu_popup && !select_popup && !tree_item_children &&
        node.properties.overflow_policy != OverflowPolicy::Visible) {
        return nullptr;
    }

    if (inside_node &&
        (node.node.kind == WidgetKind::Button || node.node.kind == WidgetKind::CheckBox ||
         node.node.kind == WidgetKind::RadioButton || node.node.kind == WidgetKind::Switch ||
         node.node.kind == WidgetKind::Slider || node.node.kind == WidgetKind::Select ||
         node.node.kind == WidgetKind::SelectOption ||
         node.node.kind == WidgetKind::ListItem ||
         node.node.kind == WidgetKind::TreeItem ||
         node.node.kind == WidgetKind::TableView ||
         (node.node.kind == WidgetKind::SplitView && is_split_handle_hit(node, x, y)))) {
        return &node;
    }
    if (inside_node && node.node.kind == WidgetKind::Tabs &&
        y <= node.dirty.bounds.y + scaled(38.0f)) {
        return &node;
    }

    if ((node.node.kind != WidgetKind::MenuItem || node.properties.menu_popup_open) &&
        (node.node.kind != WidgetKind::Select || node.properties.select_popup_open)) {
        for (auto it = node.node.children.rbegin(); it != node.node.children.rend(); ++it) {
            WidgetImpl* child = *it;
            if (child == nullptr) {
                continue;
            }
            if (node.node.kind == WidgetKind::ScrollView &&
                !contains(node.dirty.clip_bounds, x, y)) {
                continue;
            }
            WidgetImpl* child_hit = hit_test_node(*child, x, y);
            if (child_hit != nullptr) {
                return child_hit;
            }
        }
    }

    return inside_node ? &node : nullptr;
}

std::vector<EventRouteEntry> EventSystem::build_route(const WidgetImpl& target) const
{
    std::vector<const WidgetImpl*> stack;
    const WidgetImpl* cursor = &target;
    while (cursor != nullptr) {
        stack.push_back(cursor);
        cursor = cursor->node.parent;
    }

    std::vector<EventRouteEntry> route;
    route.reserve(stack.size());
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        const WidgetImpl* node = *it;
        route.push_back(EventRouteEntry{node->object.object_id, node->object.generation,
                                        widget_path(*node), node->node.kind});
    }
    return route;
}

bool EventSystem::apply_pointer_targets(EventType type, WidgetImpl* target)
{
    switch (type) {
    case EventType::PointerMove:
        return set_hover_target(target);
    case EventType::PointerDown:
        return set_focus_target(target) | set_capture_target(target) | set_hover_target(target);
    case EventType::PointerUp:
        return set_hover_target(target) | set_capture_target(nullptr);
    case EventType::Wheel:
    case EventType::Click:
    case EventType::KeyDown:
    case EventType::KeyUp:
    case EventType::TextInput:
        break;
    }
    return false;
}

WidgetImpl* EventSystem::nearest_scroll_view(WidgetImpl* target) const noexcept
{
    WidgetImpl* cursor = target;
    while (cursor != nullptr) {
        if (cursor->node.kind == WidgetKind::ScrollView) {
            return cursor;
        }
        cursor = cursor->node.parent;
    }
    return nullptr;
}

Rect EventSystem::scroll_thumb_bounds(const WidgetImpl& scroll) const noexcept
{
    const bool table = scroll.node.kind == WidgetKind::TableView;
    const float header_height =
        table ? scaled(std::max(1.0f, scroll.properties.table_header_height)) : 0.0f;
    const float viewport_height =
        table ? std::max(0.0f, scroll.dirty.bounds.height - header_height)
              : std::max(0.0f, scroll.dirty.bounds.height);
    const float content_height = std::max(0.0f, scroll.properties.scroll_content_height);
    if (content_height <= viewport_height || viewport_height <= 0.0f) {
        return Rect{};
    }

    const float thumb_margin = scaled(8.0f);
    const float thumb_width = scaled(3.0f);
    const float track_y = scroll.dirty.bounds.y + header_height;
    const float track_height = std::max(1.0f, viewport_height - thumb_margin * 2.0f);
    const float thumb_height =
        std::max(scaled(24.0f),
                 track_height * std::min(1.0f, viewport_height / content_height));
    const float max_offset = std::max(1.0f, content_height - viewport_height);
    const float scroll_ratio =
        std::max(0.0f, std::min(1.0f, scroll.properties.scroll_offset_y / max_offset));
    const float thumb_travel = std::max(0.0f, track_height - thumb_height);
    return Rect{scroll.dirty.bounds.x + scroll.dirty.bounds.width - thumb_margin - thumb_width,
                track_y + thumb_margin + thumb_travel * scroll_ratio,
                thumb_width,
                thumb_height};
}

bool EventSystem::is_scroll_thumb_hit(const WidgetImpl& scroll, float x, float y) const noexcept
{
    Rect hit = scroll_thumb_bounds(scroll);
    if (hit.width <= 0.0f || hit.height <= 0.0f) {
        return false;
    }
    const float center_x = hit.x + hit.width * 0.5f;
    hit.x = center_x - scaled(8.0f);
    hit.width = scaled(16.0f);
    hit.y -= scaled(3.0f);
    hit.height += scaled(6.0f);
    return contains(hit, x, y);
}

WidgetImpl* EventSystem::scroll_thumb_target(WidgetImpl* target, float x, float y) const noexcept
{
    if (target != nullptr && target->node.kind == WidgetKind::TableView &&
        is_scroll_thumb_hit(*target, x, y)) {
        return target;
    }
    WidgetImpl* scroll = nearest_scroll_view(target);
    if (scroll != nullptr && is_scroll_thumb_hit(*scroll, x, y)) {
        return scroll;
    }
    return nullptr;
}

WidgetImpl* EventSystem::nearest_open_dialog(WidgetImpl* target) const noexcept
{
    WidgetImpl* cursor = target;
    while (cursor != nullptr) {
        if (cursor->node.kind == WidgetKind::Dialog && cursor->properties.dialog_open &&
            !cursor->node.children.empty()) {
            return cursor;
        }
        cursor = cursor->node.parent;
    }
    return nullptr;
}

bool EventSystem::dialog_drag_handle_hit(const WidgetImpl& dialog,
                                         const WidgetImpl* target,
                                         float x,
                                         float y) const noexcept
{
    if (dialog.node.kind != WidgetKind::Dialog || !dialog.properties.dialog_open ||
        dialog.node.children.empty() || target == nullptr || target == &dialog) {
        return false;
    }

    const WidgetImpl* panel = dialog.node.children.front();
    if (panel == nullptr || !contains(panel->dirty.bounds, x, y) ||
        !target_is_descendant_of(dialog, target)) {
        return false;
    }

    switch (target->node.kind) {
    case WidgetKind::Button:
    case WidgetKind::CheckBox:
    case WidgetKind::RadioButton:
    case WidgetKind::Switch:
    case WidgetKind::Input:
    case WidgetKind::TextArea:
    case WidgetKind::Slider:
    case WidgetKind::Select:
    case WidgetKind::SelectOption:
    case WidgetKind::ListItem:
    case WidgetKind::TreeItem:
    case WidgetKind::TableView:
    case WidgetKind::Tabs:
    case WidgetKind::MenuItem:
        return false;
    default:
        break;
    }

    const float drag_height = std::min(panel->dirty.bounds.height, scaled(56.0f));
    return y >= panel->dirty.bounds.y && y <= panel->dirty.bounds.y + drag_height;
}

Rect EventSystem::split_handle_bounds(const WidgetImpl& split) const noexcept
{
    if (split.node.kind != WidgetKind::SplitView || split.node.children.size() < 2 ||
        split.node.children[0] == nullptr) {
        return Rect{};
    }
    const WidgetImpl* first = split.node.children[0];
    const float handle_size = scaled(std::max(1.0f, split.properties.split_handle_size));
    if (split.properties.split_orientation == SplitOrientation::Horizontal) {
        return Rect{first->dirty.bounds.x + first->dirty.bounds.width,
                    split.dirty.bounds.y,
                    handle_size,
                    split.dirty.bounds.height};
    }
    return Rect{split.dirty.bounds.x,
                first->dirty.bounds.y + first->dirty.bounds.height,
                split.dirty.bounds.width,
                handle_size};
}

bool EventSystem::is_split_handle_hit(const WidgetImpl& split, float x, float y) const noexcept
{
    Rect hit = split_handle_bounds(split);
    if (hit.width <= 0.0f || hit.height <= 0.0f) {
        return false;
    }
    if (split.properties.split_orientation == SplitOrientation::Horizontal) {
        const float center_x = hit.x + hit.width * 0.5f;
        hit.x = center_x - scaled(6.0f);
        hit.width = scaled(12.0f);
    } else {
        const float center_y = hit.y + hit.height * 0.5f;
        hit.y = center_y - scaled(6.0f);
        hit.height = scaled(12.0f);
    }
    return contains(hit, x, y);
}

bool EventSystem::is_tree_toggle_hit(const WidgetImpl& item, float x, float y) const noexcept
{
    if (item.node.kind != WidgetKind::TreeItem || item.node.children.empty()) {
        return false;
    }
    const float depth_offset =
        scaled(static_cast<float>(item.properties.tree_depth) * 18.0f);
    const Rect hit{item.dirty.bounds.x + scaled(4.0f) + depth_offset,
                   item.dirty.bounds.y,
                   scaled(24.0f),
                   item.dirty.bounds.height};
    return contains(hit, x, y);
}

WidgetImpl* EventSystem::nearest_tree_view(WidgetImpl* target) const noexcept
{
    WidgetImpl* cursor = target;
    while (cursor != nullptr) {
        if (cursor->node.kind == WidgetKind::TreeView) {
            return cursor;
        }
        cursor = cursor->node.parent;
    }
    return nullptr;
}

void EventSystem::clear_tree_selection(WidgetImpl& node, WidgetImpl* except)
{
    if (node.node.kind == WidgetKind::TreeItem && &node != except &&
        node.properties.tree_selected) {
        node.properties.tree_selected = false;
        mutate_widget(node, DirtyReason::Input | DirtyReason::Paint, "tree_item_unselected");
    }
    for (WidgetImpl* child : node.node.children) {
        if (child != nullptr) {
            clear_tree_selection(*child, except);
        }
    }
}

std::uint32_t EventSystem::table_row_from_y(const WidgetImpl& table, float y) const noexcept
{
    if (table.node.kind != WidgetKind::TableView || table.properties.table_rows.empty()) {
        return 0xffffffffu;
    }
    const float header_height =
        scaled(std::max(1.0f, table.properties.table_header_height));
    const float row_height = scaled(std::max(1.0f, table.properties.table_row_height));
    const float local_y = y - table.dirty.bounds.y - header_height +
                          table.properties.scroll_offset_y;
    if (local_y < 0.0f) {
        return 0xffffffffu;
    }
    const std::uint32_t row = static_cast<std::uint32_t>(local_y / row_height);
    return row < table.properties.table_rows.size() ? row : 0xffffffffu;
}

namespace {

std::vector<float> table_column_widths(const WidgetImpl& table, float scale)
{
    float fixed_width = 0.0f;
    std::uint32_t flexible_columns = 0;
    for (const TableColumnData& column : table.properties.table_columns) {
        if (column.width > 0.0f) {
            fixed_width += column.width * scale;
        } else {
            ++flexible_columns;
        }
    }
    const float remaining_width = std::max(0.0f, table.dirty.bounds.width - fixed_width);
    const float flexible_width =
        flexible_columns == 0 ? 0.0f : remaining_width / static_cast<float>(flexible_columns);
    std::vector<float> widths;
    widths.reserve(table.properties.table_columns.size());
    for (const TableColumnData& column : table.properties.table_columns) {
        widths.push_back(column.width > 0.0f ? column.width * scale : flexible_width);
    }
    return widths;
}

} // namespace

std::uint32_t EventSystem::table_column_from_x(const WidgetImpl& table, float x) const
{
    if (table.node.kind != WidgetKind::TableView || table.properties.table_columns.empty()) {
        return 0xffffffffu;
    }
    const std::vector<float> widths = table_column_widths(table, dpi_scale());
    float cursor_x = table.dirty.bounds.x;
    for (std::uint32_t column = 0; column < widths.size(); ++column) {
        const float next_x = cursor_x + widths[column];
        if (x >= cursor_x && x <= next_x) {
            return column;
        }
        cursor_x = next_x;
    }
    return 0xffffffffu;
}

std::uint32_t EventSystem::table_resize_column_from_x(const WidgetImpl& table, float x) const
{
    if (table.node.kind != WidgetKind::TableView || table.properties.table_columns.size() < 2) {
        return 0xffffffffu;
    }
    const std::vector<float> widths = table_column_widths(table, dpi_scale());
    float cursor_x = table.dirty.bounds.x;
    const float slop = scaled(5.0f);
    for (std::uint32_t column = 0; column + 1u < widths.size(); ++column) {
        cursor_x += widths[column];
        if (std::abs(x - cursor_x) <= slop) {
            return column;
        }
    }
    return 0xffffffffu;
}

bool EventSystem::select_table_row(WidgetImpl& table,
                                   std::uint32_t row,
                                   const char* action,
                                   bool invoke_callback,
                                   EventDispatchResult& result)
{
    if (table.node.kind != WidgetKind::TableView || row >= table.properties.table_rows.size()) {
        return false;
    }
    const bool changed = !table.properties.has_selected_index ||
                         table.properties.selected_index != row;
    table.properties.selected_index = row;
    table.properties.has_selected_index = true;
    const std::vector<std::string>& table_row = table.properties.table_rows[row];
    table.properties.selected_text_cache = table_row.empty() ? "" : table_row.front();
    const bool scrolled = ensure_table_row_visible(table, row);
    mutate_widget(table, DirtyReason::Input | DirtyReason::TextChanged | DirtyReason::Paint,
                  action == nullptr ? "table_select" : action);
    diagnostics_event_ex("event", action == nullptr ? "table_select" : action,
                         table.object.object_id, table.object.generation, current_frame_id(),
                         widget_path(table).c_str(), table.properties.selected_text_cache.c_str());
    if ((changed || invoke_callback) && table.properties.click_callback != nullptr) {
        try {
            table.properties.click_callback(table.properties.click_user_data);
        } catch (...) {
            result.callback_failed = true;
            diagnostics_event_ex("event", "callback_exception", table.object.object_id,
                                 table.object.generation, current_frame_id(),
                                 widget_path(table).c_str(),
                                 "callback threw during table change");
        }
    }
    result.handled = true;
    result.target_changed = changed || scrolled;
    return changed || scrolled;
}

bool EventSystem::ensure_table_row_visible(WidgetImpl& table, std::uint32_t row)
{
    if (table.node.kind != WidgetKind::TableView || row >= table.properties.table_rows.size()) {
        return false;
    }
    const float header_height =
        scaled(std::max(1.0f, table.properties.table_header_height));
    const float row_height = scaled(std::max(1.0f, table.properties.table_row_height));
    const float viewport_height = std::max(0.0f, table.dirty.bounds.height - header_height);
    if (viewport_height <= 0.0f) {
        return false;
    }
    const float row_top = row_height * static_cast<float>(row);
    const float row_bottom = row_top + row_height;
    float next_offset = table.properties.scroll_offset_y;
    if (row_top < next_offset) {
        next_offset = row_top;
    } else if (row_bottom > next_offset + viewport_height) {
        next_offset = row_bottom - viewport_height;
    }
    const float max_offset = std::max(0.0f, table.properties.scroll_content_height -
                                                 viewport_height);
    const float old_offset = table.properties.scroll_offset_y;
    table.properties.scroll_offset_y = std::min(std::max(0.0f, next_offset), max_offset);
    return table.properties.scroll_offset_y != old_offset;
}

bool EventSystem::apply_table_wheel(FiuiEvent& event)
{
    WidgetImpl* table = event.target;
    if (table == nullptr || table->node.kind != WidgetKind::TableView) {
        return false;
    }
    const float header_height =
        scaled(std::max(1.0f, table->properties.table_header_height));
    const float viewport_height =
        std::max(0.0f, table->dirty.bounds.height - header_height);
    const float content_height = std::max(0.0f, table->properties.scroll_content_height);
    if (content_height <= viewport_height) {
        diagnostics_event_ex("event", "table_wheel_ignored", table->object.object_id,
                             table->object.generation, current_frame_id(),
                             widget_path(*table).c_str(), "content fits");
        return false;
    }
    const float old_offset = table->properties.scroll_offset_y;
    const float step = scaled(std::max(1.0f, table->properties.table_row_height)) * 3.0f;
    const float direction = event.wheel_delta > 0.0f ? -1.0f : 1.0f;
    const float max_offset = std::max(0.0f, content_height - viewport_height);
    table->properties.scroll_offset_y =
        std::min(std::max(0.0f, old_offset + direction * step), max_offset);
    if (table->properties.scroll_offset_y != old_offset) {
        std::ostringstream detail;
        detail << "offset=" << table->properties.scroll_offset_y << ";old=" << old_offset
               << ";max=" << max_offset << ";delta=" << event.wheel_delta;
        const std::string detail_text = detail.str();
        diagnostics_event_ex("event", "table_wheel", table->object.object_id,
                             table->object.generation, current_frame_id(),
                             widget_path(*table).c_str(), detail_text.c_str());
        mutate_widget(*table, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      "table_wheel");
        return true;
    }
    return false;
}

bool EventSystem::apply_table_pointer_action(FiuiEvent& event)
{
    WidgetImpl* table = event.target;
    if (table == nullptr || table->node.kind != WidgetKind::TableView) {
        return false;
    }
    const float header_height =
        scaled(std::max(1.0f, table->properties.table_header_height));
    if (event.type == EventType::PointerDown &&
        event.y >= table->dirty.bounds.y &&
        event.y <= table->dirty.bounds.y + header_height) {
        const std::uint32_t resize_column = table_resize_column_from_x(*table, event.x);
        if (resize_column < table->properties.table_columns.size()) {
            table->properties.table_column_resizing = true;
            table->properties.table_resize_column = resize_column;
            table->properties.table_resize_start_x = event.x;
            table->properties.table_resize_start_width =
                table->properties.table_columns[resize_column].width;
            if (table->properties.table_resize_start_width <= 0.0f) {
                const std::vector<float> widths = table_column_widths(*table, dpi_scale());
                table->properties.table_resize_start_width =
                    resize_column < widths.size() ? widths[resize_column] / dpi_scale() : 80.0f;
            }
            diagnostics_event_ex("event", "table_column_resize_begin",
                                 table->object.object_id, table->object.generation,
                                 current_frame_id(), widget_path(*table).c_str(),
                                 "header divider");
            mutate_widget(*table, DirtyReason::Input | DirtyReason::Paint,
                          "table_column_resize_begin");
            return true;
        }
    }

    if (event.type == EventType::PointerMove && table->properties.table_column_resizing) {
        const std::uint32_t column = table->properties.table_resize_column;
        if (column < table->properties.table_columns.size()) {
            const float delta = (event.x - table->properties.table_resize_start_x) / dpi_scale();
            const float next_width =
                std::max(44.0f, table->properties.table_resize_start_width + delta);
            if (table->properties.table_columns[column].width != next_width) {
                table->properties.table_columns[column].width = next_width;
                std::ostringstream detail;
                detail << "column=" << column << ";width=" << next_width;
                const std::string detail_text = detail.str();
                diagnostics_event_ex("event", "table_column_resize",
                                     table->object.object_id, table->object.generation,
                                     current_frame_id(), widget_path(*table).c_str(),
                                     detail_text.c_str());
                mutate_widget(*table, DirtyReason::Input | DirtyReason::Layout |
                                          DirtyReason::Paint,
                              "table_column_resize");
                return true;
            }
        }
        return false;
    }

    if (event.type == EventType::PointerUp && table->properties.table_column_resizing) {
        table->properties.table_column_resizing = false;
        table->properties.table_resize_suppress_click = true;
        table->properties.table_resize_column = 0xffffffffu;
        diagnostics_event_ex("event", "table_column_resize_end",
                             table->object.object_id, table->object.generation,
                             current_frame_id(), widget_path(*table).c_str(), "release");
        mutate_widget(*table, DirtyReason::Input | DirtyReason::Paint,
                      "table_column_resize_end");
        return true;
    }
    return false;
}

bool EventSystem::is_scroll_keyboard_key(std::uint32_t key_code) const noexcept
{
    switch (key_code & key_code_mask) {
    case 0x21: // PageUp
    case 0x22: // PageDown
    case 0x23: // End
    case 0x24: // Home
        return true;
    default:
        return false;
    }
}

bool EventSystem::set_scroll_offset(WidgetImpl& scroll,
                                    float offset,
                                    const char* action,
                                    const char* detail)
{
    const float viewport_height = std::max(0.0f, scroll.dirty.bounds.height);
    const float max_offset =
        std::max(0.0f, scroll.properties.scroll_content_height - viewport_height);
    const float old_offset = scroll.properties.scroll_offset_y;
    scroll.properties.scroll_offset_y = std::min(std::max(0.0f, offset), max_offset);

    std::ostringstream full_detail;
    full_detail << "offset=" << scroll.properties.scroll_offset_y << ";old=" << old_offset
                << ";max=" << max_offset;
    if (detail != nullptr && detail[0] != '\0') {
        full_detail << ";" << detail;
    }
    const std::string detail_text = full_detail.str();
    diagnostics_event_ex("event", action == nullptr ? "scroll_offset" : action,
                         scroll.object.object_id, scroll.object.generation, current_frame_id(),
                         widget_path(scroll).c_str(), detail_text.c_str());
    if (scroll.properties.scroll_offset_y != old_offset) {
        mutate_widget(scroll, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      action == nullptr ? "scroll_offset" : action);
        return true;
    }
    return false;
}

bool EventSystem::apply_dialog_drag(FiuiEvent& event)
{
    WidgetImpl* dialog =
        event.target != nullptr && event.target->node.kind == WidgetKind::Dialog
            ? event.target
            : nearest_open_dialog(event.target);
    if (dialog == nullptr) {
        return false;
    }

    if (event.type == EventType::PointerDown) {
        if (!dialog_drag_handle_hit(*dialog, event.target, event.x, event.y)) {
            dialog->properties.dialog_suppress_backdrop_click = false;
            return false;
        }
        WidgetImpl* panel = dialog->node.children.empty() ? nullptr : dialog->node.children.front();
        if (panel != nullptr) {
            const float centered_x =
                dialog->dirty.bounds.x +
                std::max(0.0f, (dialog->dirty.bounds.width - panel->dirty.bounds.width) * 0.5f);
            const float centered_y =
                dialog->dirty.bounds.y +
                std::max(0.0f, (dialog->dirty.bounds.height - panel->dirty.bounds.height) * 0.5f);
            dialog->properties.dialog_offset_x = panel->dirty.bounds.x - centered_x;
            dialog->properties.dialog_offset_y = panel->dirty.bounds.y - centered_y;
        }
        dialog->properties.dialog_dragging = true;
        dialog->properties.dialog_suppress_backdrop_click = false;
        dialog->properties.dialog_drag_start_x = event.x;
        dialog->properties.dialog_drag_start_y = event.y;
        dialog->properties.dialog_drag_origin_offset_x = dialog->properties.dialog_offset_x;
        dialog->properties.dialog_drag_origin_offset_y = dialog->properties.dialog_offset_y;
        (void)set_capture_target(dialog);
        diagnostics_event_ex("event", "dialog_drag_begin", dialog->object.object_id,
                             dialog->object.generation, current_frame_id(),
                             widget_path(*dialog).c_str(), "panel header capture");
        mutate_widget(*dialog, DirtyReason::Input | DirtyReason::Paint, "dialog_drag_begin");
        return true;
    }

    if (!dialog->properties.dialog_dragging) {
        return false;
    }

    if (event.type == EventType::PointerMove) {
        const float old_x = dialog->properties.dialog_offset_x;
        const float old_y = dialog->properties.dialog_offset_y;
        float next_offset_x =
            dialog->properties.dialog_drag_origin_offset_x +
            (event.x - dialog->properties.dialog_drag_start_x);
        float next_offset_y =
            dialog->properties.dialog_drag_origin_offset_y +
            (event.y - dialog->properties.dialog_drag_start_y);
        WidgetImpl* panel = dialog->node.children.empty() ? nullptr : dialog->node.children.front();
        if (panel != nullptr) {
            const float centered_x =
                dialog->dirty.bounds.x +
                std::max(0.0f, (dialog->dirty.bounds.width - panel->dirty.bounds.width) * 0.5f);
            const float centered_y =
                dialog->dirty.bounds.y +
                std::max(0.0f, (dialog->dirty.bounds.height - panel->dirty.bounds.height) * 0.5f);
            const float min_offset_x = dialog->dirty.bounds.x - centered_x;
            const float max_offset_x =
                dialog->dirty.bounds.x +
                std::max(0.0f, dialog->dirty.bounds.width - panel->dirty.bounds.width) -
                centered_x;
            const float min_offset_y = dialog->dirty.bounds.y - centered_y;
            const float max_offset_y =
                dialog->dirty.bounds.y +
                std::max(0.0f, dialog->dirty.bounds.height - panel->dirty.bounds.height) -
                centered_y;
            next_offset_x = std::min(std::max(min_offset_x, next_offset_x), max_offset_x);
            next_offset_y = std::min(std::max(min_offset_y, next_offset_y), max_offset_y);
        }
        dialog->properties.dialog_offset_x = next_offset_x;
        dialog->properties.dialog_offset_y = next_offset_y;
        const bool changed = dialog->properties.dialog_offset_x != old_x ||
                             dialog->properties.dialog_offset_y != old_y;
        if (changed) {
            std::ostringstream detail;
            detail << "offset_x=" << dialog->properties.dialog_offset_x
                   << ";offset_y=" << dialog->properties.dialog_offset_y;
            const std::string detail_text = detail.str();
            diagnostics_event_ex("event", "dialog_drag", dialog->object.object_id,
                                 dialog->object.generation, current_frame_id(),
                                 widget_path(*dialog).c_str(), detail_text.c_str());
            mutate_widget(*dialog, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                          "dialog_drag");
        }
        return changed;
    }

    if (event.type == EventType::PointerUp) {
        dialog->properties.dialog_dragging = false;
        dialog->properties.dialog_suppress_backdrop_click = true;
        diagnostics_event_ex("event", "dialog_drag_end", dialog->object.object_id,
                             dialog->object.generation, current_frame_id(),
                             widget_path(*dialog).c_str(), "panel header release");
        mutate_widget(*dialog, DirtyReason::Input | DirtyReason::Paint, "dialog_drag_end");
        return true;
    }

    return false;
}

bool EventSystem::apply_scroll_wheel(FiuiEvent& event)
{
    WidgetImpl* scroll = nearest_scroll_view(event.target);
    if (scroll == nullptr) {
        diagnostics_event_ex("event", "wheel_ignored", event.target_object_id,
                             event.target_generation, current_frame_id(),
                             event.target_path.c_str(), "no scroll ancestor");
        return false;
    }
    const float viewport_height = std::max(0.0f, scroll->dirty.bounds.height);
    const float max_offset =
        std::max(0.0f, scroll->properties.scroll_content_height - viewport_height);
    const float step = scaled(48.0f);
    const float old_offset = scroll->properties.scroll_offset_y;
    const float direction = event.wheel_delta > 0.0f ? -1.0f : 1.0f;
    std::ostringstream detail;
    detail << "delta=" << event.wheel_delta;
    const std::string detail_text = detail.str();
    (void)viewport_height;
    (void)max_offset;
    return set_scroll_offset(*scroll, old_offset + direction * step, "scroll_wheel",
                             detail_text.c_str());
}

bool EventSystem::set_text_area_scroll_offset(WidgetImpl& target,
                                              float offset,
                                              const char* action)
{
    if (target.node.kind != WidgetKind::TextArea) {
        return false;
    }
    const float viewport_height = std::max(0.0f, target.dirty.bounds.height);
    const float max_offset =
        std::max(0.0f, target.properties.scroll_content_height - viewport_height);
    const float old_offset = target.properties.scroll_offset_y;
    target.properties.scroll_offset_y = std::min(std::max(0.0f, offset), max_offset);
    if (target.properties.scroll_offset_y == old_offset) {
        return false;
    }
    std::ostringstream detail;
    detail << "offset=" << target.properties.scroll_offset_y << ";old=" << old_offset
           << ";max=" << max_offset;
    const std::string detail_text = detail.str();
    diagnostics_event_ex("event", action == nullptr ? "text_area_scroll" : action,
                         target.object.object_id, target.object.generation,
                         current_frame_id(), widget_path(target).c_str(), detail_text.c_str());
    mutate_widget(target, DirtyReason::Input | DirtyReason::Paint,
                  action == nullptr ? "text_area_scroll" : action);
    return true;
}

bool EventSystem::apply_text_area_wheel(FiuiEvent& event)
{
    WidgetImpl* target = event.target;
    while (target != nullptr && target->node.kind != WidgetKind::TextArea) {
        target = target->node.parent;
    }
    if (target == nullptr) {
        return false;
    }
    const float direction = event.wheel_delta > 0.0f ? -1.0f : 1.0f;
    return set_text_area_scroll_offset(*target,
                                       target->properties.scroll_offset_y +
                                           direction * text_line_height(*target) * 3.0f,
                                       "text_area_wheel");
}

bool EventSystem::ensure_text_area_cursor_visible(WidgetImpl& target)
{
    if (target.node.kind != WidgetKind::TextArea) {
        return false;
    }
    const float line_height = text_line_height(target);
    const std::size_t line_count =
        1u + static_cast<std::size_t>(
                 std::count(target.properties.text.begin(), target.properties.text.end(), '\n'));
    target.properties.scroll_content_height = line_height * static_cast<float>(line_count);
    const std::size_t line = cursor_line_index(target.properties.text, target.properties.text_cursor);
    const float caret_top = static_cast<float>(line) * line_height;
    const float caret_bottom = caret_top + line_height;
    const float viewport_height = std::max(1.0f, target.dirty.bounds.height);
    float next_offset = target.properties.scroll_offset_y;
    if (caret_top < next_offset) {
        next_offset = caret_top;
    } else if (caret_bottom > next_offset + viewport_height) {
        next_offset = caret_bottom - viewport_height;
    }
    return set_text_area_scroll_offset(target, next_offset, "text_area_caret_scroll");
}

bool EventSystem::apply_scroll_keyboard(FiuiEvent& event)
{
    if (event.type != EventType::KeyDown || !is_scroll_keyboard_key(event.key_code)) {
        return false;
    }
    const std::uint32_t key_code = event.key_code & key_code_mask;
    if (event.target != nullptr && is_text_editable(*event.target) &&
        (key_code == 0x23 || key_code == 0x24)) {
        return false;
    }

    WidgetImpl* scroll = nearest_scroll_view(event.target);
    if (scroll == nullptr) {
        diagnostics_event_ex("event", "scroll_keyboard_ignored", event.target_object_id,
                             event.target_generation, current_frame_id(),
                             event.target_path.c_str(), "no scroll ancestor");
        return false;
    }

    const float viewport_height = std::max(0.0f, scroll->dirty.bounds.height);
    const float max_offset =
        std::max(0.0f, scroll->properties.scroll_content_height - viewport_height);
    float next_offset = scroll->properties.scroll_offset_y;
    const float page_step = std::max(1.0f, viewport_height * 0.85f);
    const char* key_name = "unknown";
    switch (key_code) {
    case 0x21: // PageUp
        next_offset -= page_step;
        key_name = "page_up";
        break;
    case 0x22: // PageDown
        next_offset += page_step;
        key_name = "page_down";
        break;
    case 0x23: // End
        next_offset = max_offset;
        key_name = "end";
        break;
    case 0x24: // Home
        next_offset = 0.0f;
        key_name = "home";
        break;
    default:
        break;
    }

    std::ostringstream detail;
    detail << "key=" << key_name << ";page_step=" << page_step;
    const std::string detail_text = detail.str();
    return set_scroll_offset(*scroll, next_offset, "scroll_keyboard", detail_text.c_str());
}

bool EventSystem::apply_scroll_thumb_drag(FiuiEvent& event)
{
    WidgetImpl* scroll = event.target;
    if (scroll == nullptr ||
        (scroll->node.kind != WidgetKind::ScrollView && scroll->node.kind != WidgetKind::TableView)) {
        return false;
    }

    const float header_height =
        scroll->node.kind == WidgetKind::TableView
            ? scaled(std::max(1.0f, scroll->properties.table_header_height))
            : 0.0f;
    const float viewport_height =
        scroll->node.kind == WidgetKind::TableView
            ? std::max(0.0f, scroll->dirty.bounds.height - header_height)
            : std::max(0.0f, scroll->dirty.bounds.height);
    const float content_height = std::max(0.0f, scroll->properties.scroll_content_height);
    const float max_offset = std::max(0.0f, content_height - viewport_height);
    if (max_offset <= 0.0f) {
        scroll->properties.scroll_thumb_dragging = false;
        return false;
    }

    bool changed = false;
    const float old_offset = scroll->properties.scroll_offset_y;
    if (event.type == EventType::PointerDown && is_scroll_thumb_hit(*scroll, event.x, event.y)) {
        scroll->properties.scroll_thumb_dragging = true;
        scroll->properties.scroll_drag_start_y = event.y;
        scroll->properties.scroll_drag_start_offset_y = scroll->properties.scroll_offset_y;
        diagnostics_event_ex("event", "scroll_thumb_drag_begin", scroll->object.object_id,
                             scroll->object.generation, current_frame_id(),
                             widget_path(*scroll).c_str(), "thumb capture");
        mutate_widget(*scroll, DirtyReason::Input | DirtyReason::Paint, "scroll_thumb_drag_begin");
        return true;
    }

    if (event.type == EventType::PointerMove && scroll->properties.scroll_thumb_dragging) {
        const Rect thumb = scroll_thumb_bounds(*scroll);
        const float track_height = std::max(1.0f, viewport_height - scaled(16.0f));
        const float thumb_travel = std::max(1.0f, track_height - thumb.height);
        const float drag_delta = event.y - scroll->properties.scroll_drag_start_y;
        const float offset_delta = (drag_delta / thumb_travel) * max_offset;
        scroll->properties.scroll_offset_y =
            std::min(std::max(0.0f,
                              scroll->properties.scroll_drag_start_offset_y + offset_delta),
                     max_offset);
        changed = scroll->properties.scroll_offset_y != old_offset;
        if (changed) {
            std::ostringstream detail;
            detail << "offset=" << scroll->properties.scroll_offset_y << ";old=" << old_offset
                   << ";max=" << max_offset << ";drag_delta=" << drag_delta;
            const std::string detail_text = detail.str();
            diagnostics_event_ex("event", "scroll_thumb_drag", scroll->object.object_id,
                                 scroll->object.generation, current_frame_id(),
                                 widget_path(*scroll).c_str(), detail_text.c_str());
            mutate_widget(*scroll, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                          "scroll_thumb_drag");
        }
        return changed;
    }

    if (event.type == EventType::PointerUp && scroll->properties.scroll_thumb_dragging) {
        scroll->properties.scroll_thumb_dragging = false;
        diagnostics_event_ex("event", "scroll_thumb_drag_end", scroll->object.object_id,
                             scroll->object.generation, current_frame_id(),
                             widget_path(*scroll).c_str(), "thumb release");
        mutate_widget(*scroll, DirtyReason::Input | DirtyReason::Paint, "scroll_thumb_drag_end");
        return true;
    }

    return false;
}

bool EventSystem::apply_split_handle_drag(FiuiEvent& event)
{
    WidgetImpl* split = event.target;
    if (split == nullptr || split->node.kind != WidgetKind::SplitView ||
        split->node.children.size() < 2) {
        return false;
    }

    const bool horizontal =
        split->properties.split_orientation == SplitOrientation::Horizontal;
    const float axis_size =
        horizontal ? std::max(1.0f, split->dirty.bounds.width)
                   : std::max(1.0f, split->dirty.bounds.height);
    const float handle_size = scaled(std::max(1.0f, split->properties.split_handle_size));
    const float available = std::max(1.0f, axis_size - handle_size);
    const float min_pane =
        std::min(available * 0.45f, scaled(std::max(0.0f, split->properties.split_min_pane_size)));
    const float min_ratio = min_pane / available;
    const float max_ratio = 1.0f - min_ratio;

    if (event.type == EventType::PointerDown && is_split_handle_hit(*split, event.x, event.y)) {
        split->properties.split_handle_dragging = true;
        split->properties.split_drag_start_position = horizontal ? event.x : event.y;
        split->properties.split_drag_start_ratio = split->properties.split_ratio;
        diagnostics_event_ex("event", "split_drag_begin", split->object.object_id,
                             split->object.generation, current_frame_id(),
                             widget_path(*split).c_str(), "handle capture");
        mutate_widget(*split, DirtyReason::Input | DirtyReason::Paint, "split_drag_begin");
        return true;
    }

    if (event.type == EventType::PointerMove && split->properties.split_handle_dragging) {
        const float position = horizontal ? event.x : event.y;
        const float delta = position - split->properties.split_drag_start_position;
        const float old_ratio = split->properties.split_ratio;
        split->properties.split_ratio =
            std::min(max_ratio,
                     std::max(min_ratio,
                              split->properties.split_drag_start_ratio + delta / available));
        if (split->properties.split_ratio != old_ratio) {
            std::ostringstream detail;
            detail << "ratio=" << split->properties.split_ratio << ";old=" << old_ratio
                   << ";delta=" << delta;
            const std::string detail_text = detail.str();
            diagnostics_event_ex("event", "split_drag", split->object.object_id,
                                 split->object.generation, current_frame_id(),
                                 widget_path(*split).c_str(), detail_text.c_str());
            mutate_widget(*split, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                          "split_drag");
            return true;
        }
        return false;
    }

    if (event.type == EventType::PointerUp && split->properties.split_handle_dragging) {
        split->properties.split_handle_dragging = false;
        diagnostics_event_ex("event", "split_drag_end", split->object.object_id,
                             split->object.generation, current_frame_id(),
                             widget_path(*split).c_str(), "handle release");
        mutate_widget(*split, DirtyReason::Input | DirtyReason::Paint, "split_drag_end");
        return true;
    }

    return false;
}

bool EventSystem::set_slider_value(WidgetImpl& slider,
                                   float value,
                                   const char* action,
                                   const char* detail)
{
    if (slider.node.kind != WidgetKind::Slider) {
        return false;
    }

    const float old_value = slider.properties.numeric_value;
    slider.properties.numeric_value = std::max(0.0f, std::min(1.0f, value));
    std::ostringstream full_detail;
    full_detail << "value=" << slider.properties.numeric_value << ";old=" << old_value;
    if (detail != nullptr && detail[0] != '\0') {
        full_detail << ";" << detail;
    }
    const std::string detail_text = full_detail.str();
    diagnostics_event_ex("event", action == nullptr ? "slider_value" : action,
                         slider.object.object_id, slider.object.generation, current_frame_id(),
                         widget_path(slider).c_str(), detail_text.c_str());

    if (slider.properties.numeric_value == old_value) {
        return false;
    }

    mutate_widget(slider, DirtyReason::Input | DirtyReason::Paint,
                  action == nullptr ? "slider_value" : action);
    if (slider.properties.click_callback != nullptr) {
        try {
            slider.properties.click_callback(slider.properties.click_user_data);
        } catch (...) {
            diagnostics_event_ex("event", "callback_exception", slider.object.object_id,
                                 slider.object.generation, current_frame_id(),
                                 widget_path(slider).c_str(),
                                 "callback threw during slider change");
        }
    }
    return true;
}

bool EventSystem::apply_slider_pointer_change(FiuiEvent& event)
{
    WidgetImpl* slider = event.target;
    if (slider == nullptr || slider->node.kind != WidgetKind::Slider) {
        return false;
    }
    if (event.type != EventType::PointerDown && event.type != EventType::PointerMove) {
        return false;
    }

    const float inset = scaled(10.0f);
    const float track_left = slider->dirty.bounds.x + inset;
    const float track_width = std::max(1.0f, slider->dirty.bounds.width - inset * 2.0f);
    const float value = (event.x - track_left) / track_width;
    std::ostringstream detail;
    detail << "x=" << event.x << ";track_left=" << track_left << ";track_width=" << track_width;
    const std::string detail_text = detail.str();
    return set_slider_value(*slider, value,
                            event.type == EventType::PointerDown ? "slider_pointer_down"
                                                                 : "slider_drag",
                            detail_text.c_str());
}

bool EventSystem::apply_slider_keyboard(FiuiEvent& event)
{
    WidgetImpl* slider = event.target;
    if (slider == nullptr || slider->node.kind != WidgetKind::Slider ||
        event.type != EventType::KeyDown) {
        return false;
    }
    const std::uint32_t key_code = event.key_code & key_code_mask;
    float next_value = slider->properties.numeric_value;
    const char* action = nullptr;
    switch (key_code) {
    case 0x25: // Left
    case 0x28: // Down
        next_value -= 0.05f;
        action = "slider_keyboard_decrease";
        break;
    case 0x27: // Right
    case 0x26: // Up
        next_value += 0.05f;
        action = "slider_keyboard_increase";
        break;
    case 0x24: // Home
        next_value = 0.0f;
        action = "slider_keyboard_home";
        break;
    case 0x23: // End
        next_value = 1.0f;
        action = "slider_keyboard_end";
        break;
    default:
        return false;
    }
    return set_slider_value(*slider, next_value, action, nullptr);
}

bool EventSystem::apply_text_edit(FiuiEvent& event)
{
    WidgetImpl* target = event.target;
    if (target == nullptr || !is_text_editable(*target)) {
        diagnostics_event_ex("event", "text_edit_ignored", event.target_object_id,
                             event.target_generation, current_frame_id(),
                             event.target_path.c_str(), "target is not input");
        return false;
    }

    std::string& text = target->properties.text;
    std::size_t& cursor = target->properties.text_cursor;
    cursor = clamp_utf8_boundary(text, cursor);
    std::size_t& anchor = target->properties.text_selection_anchor;
    anchor = clamp_utf8_boundary(text, anchor);
    const std::string old_text = text;
    const std::size_t old_cursor = cursor;
    const std::size_t old_anchor = anchor;
    const bool has_selection = anchor != cursor;
    const std::size_t selection_begin = std::min(anchor, cursor);
    const std::size_t selection_end = std::max(anchor, cursor);

    bool edited = false;
    const char* action = "text_edit";
    if (event.type == EventType::TextInput) {
        if (event.text_codepoint >= 0x20 && event.text_codepoint != 0x7f) {
            std::string inserted;
            if (append_utf8_codepoint(inserted, event.text_codepoint)) {
                return apply_text_insert(*target, inserted.c_str(), "text_insert");
            } else {
                diagnostics_event_ex("event", "text_edit_ignored", event.target_object_id,
                                     event.target_generation, current_frame_id(),
                                     event.target_path.c_str(), "invalid_unicode_codepoint");
            }
        }
    } else if (event.type == EventType::KeyDown) {
        const std::uint32_t key_code = event.key_code & key_code_mask;
        const bool extend_selection = (event.key_code & key_modifier_shift) != 0;
        switch (key_code) {
        case 0x0d: // Enter
            if (target->node.kind == WidgetKind::TextArea) {
                return apply_text_insert(*target, "\n", "text_newline");
            }
            break;
        case 0x08: // Backspace
            if (has_selection) {
                text.erase(selection_begin, selection_end - selection_begin);
                cursor = selection_begin;
                anchor = cursor;
                edited = true;
                action = "text_selection_backspace";
            } else if (cursor > 0 && !text.empty()) {
                const std::size_t previous = previous_utf8_boundary(text, cursor);
                text.erase(previous, cursor - previous);
                cursor = previous;
                anchor = cursor;
                edited = true;
                action = "text_backspace";
            }
            break;
        case 0x2e: // Delete
            if (has_selection) {
                text.erase(selection_begin, selection_end - selection_begin);
                cursor = selection_begin;
                anchor = cursor;
                edited = true;
                action = "text_selection_delete";
            } else if (cursor < text.size()) {
                const std::size_t next = next_utf8_boundary(text, cursor);
                text.erase(cursor, next - cursor);
                anchor = cursor;
                edited = true;
                action = "text_delete";
            }
            break;
        case 0x25: // Left
            if (!extend_selection && has_selection) {
                cursor = selection_begin;
            } else if (cursor > 0) {
                cursor = previous_utf8_boundary(text, cursor);
            }
            if (!extend_selection) {
                anchor = cursor;
            }
            target->properties.text_selection_dragging = false;
            action = extend_selection ? "text_selection_left" : "text_cursor_left";
            break;
        case 0x26: // Up
            if (target->node.kind == WidgetKind::TextArea) {
                cursor = move_cursor_vertical(text, cursor, -1);
                if (!extend_selection) {
                    anchor = cursor;
                }
                target->properties.text_selection_dragging = false;
                action = extend_selection ? "text_selection_up" : "text_cursor_up";
            }
            break;
        case 0x27: // Right
            if (!extend_selection && has_selection) {
                cursor = selection_end;
            } else if (cursor < text.size()) {
                cursor = next_utf8_boundary(text, cursor);
            }
            if (!extend_selection) {
                anchor = cursor;
            }
            target->properties.text_selection_dragging = false;
            action = extend_selection ? "text_selection_right" : "text_cursor_right";
            break;
        case 0x28: // Down
            if (target->node.kind == WidgetKind::TextArea) {
                cursor = move_cursor_vertical(text, cursor, 1);
                if (!extend_selection) {
                    anchor = cursor;
                }
                target->properties.text_selection_dragging = false;
                action = extend_selection ? "text_selection_down" : "text_cursor_down";
            }
            break;
        case 0x24: // Home
            cursor = target->node.kind == WidgetKind::TextArea ? line_start_for(text, cursor) : 0;
            if (!extend_selection) {
                anchor = cursor;
            }
            target->properties.text_selection_dragging = false;
            action = extend_selection ? "text_selection_home" : "text_cursor_home";
            break;
        case 0x23: // End
            cursor =
                target->node.kind == WidgetKind::TextArea ? line_end_for(text, cursor) : text.size();
            if (!extend_selection) {
                anchor = cursor;
            }
            target->properties.text_selection_dragging = false;
            action = extend_selection ? "text_selection_end" : "text_cursor_end";
            break;
        case 0x21: // PageUp
        case 0x22: // PageDown
            if (target->node.kind == WidgetKind::TextArea) {
                const int direction = key_code == 0x21 ? -1 : 1;
                const float line_height = text_line_height(*target);
                const int visible_lines = static_cast<int>(
                    std::max(1.0f, target->dirty.bounds.height / std::max(1.0f, line_height)));
                cursor = move_cursor_vertical(text, cursor, direction * visible_lines);
                if (!extend_selection) {
                    anchor = cursor;
                }
                target->properties.text_selection_dragging = false;
                (void)set_text_area_scroll_offset(
                    *target,
                    target->properties.scroll_offset_y +
                        static_cast<float>(direction) * target->dirty.bounds.height * 0.85f,
                    key_code == 0x21 ? "text_area_page_up" : "text_area_page_down");
                action = key_code == 0x21 ? "text_cursor_page_up" : "text_cursor_page_down";
            }
            break;
        default:
            break;
        }
    }

    if (target->node.kind == WidgetKind::TextArea) {
        (void)ensure_text_area_cursor_visible(*target);
    }
    std::ostringstream detail;
    detail << action << ";cursor=" << cursor << ";length=" << text.size();
    const std::string detail_text = detail.str();
    diagnostics_event_ex("event", action, event.target_object_id, event.target_generation,
                         current_frame_id(), event.target_path.c_str(), detail_text.c_str());

    const bool text_changed = text != old_text;
    const bool caret_changed = cursor != old_cursor || anchor != old_anchor;
    if (text_changed || caret_changed || edited) {
        mutate_widget(*target,
                      text_changed ? (DirtyReason::Input | DirtyReason::TextChanged |
                                      DirtyReason::Layout | DirtyReason::Paint)
                                   : (DirtyReason::Input | DirtyReason::Paint),
                      action);
        if (text_changed) {
            invoke_text_change_callback(*target, action);
        }
    }
    return text_changed || caret_changed || edited;
}

bool EventSystem::apply_text_insert(WidgetImpl& target, const char* value, const char* action)
{
    if (!is_text_editable(target)) {
        diagnostics_event_ex("event", "text_edit_ignored", target.object.object_id,
                             target.object.generation, current_frame_id(),
                             widget_path(target).c_str(), "target is not input");
        return false;
    }
    const char* input = value == nullptr ? "" : value;
    std::string& text = target.properties.text;
    std::size_t& cursor = target.properties.text_cursor;
    cursor = clamp_utf8_boundary(text, cursor);
    std::size_t& anchor = target.properties.text_selection_anchor;
    anchor = clamp_utf8_boundary(text, anchor);

    std::uint32_t skipped = 0;
    const std::string sanitized = target.node.kind == WidgetKind::TextArea
                                      ? sanitize_utf8_multiline(input, skipped)
                                      : sanitize_utf8_single_line(input, skipped);

    if (sanitized.empty()) {
        diagnostics_event_ex("event", "text_edit_ignored", target.object.object_id,
                             target.object.generation, current_frame_id(),
                             widget_path(target).c_str(),
                             skipped > 0 ? "invalid_text_input" : "empty_text_input");
        return false;
    }

    if (anchor != cursor) {
        const std::size_t begin = std::min(anchor, cursor);
        const std::size_t end = std::max(anchor, cursor);
        text.erase(begin, end - begin);
        cursor = begin;
        anchor = begin;
    }
    text.insert(cursor, sanitized);
    cursor += sanitized.size();
    anchor = cursor;
    std::ostringstream detail;
    detail << (action == nullptr ? "text_insert" : action) << ";cursor=" << cursor
           << ";length=" << text.size() << ";inserted=" << sanitized.size()
           << ";skipped=" << skipped;
    const std::string detail_text = detail.str();
    diagnostics_event_ex("event", action == nullptr ? "text_insert" : action,
                         target.object.object_id, target.object.generation,
                         current_frame_id(), widget_path(target).c_str(), detail_text.c_str());
    mutate_widget(target, DirtyReason::Input | DirtyReason::TextChanged |
                              DirtyReason::Layout | DirtyReason::Paint,
                  action == nullptr ? "text_insert" : action);
    if (target.node.kind == WidgetKind::TextArea) {
        (void)ensure_text_area_cursor_visible(target);
    }
    invoke_text_change_callback(target, action == nullptr ? "text_insert" : action);
    return true;
}

bool EventSystem::is_focusable(const WidgetImpl& target) const noexcept
{
    if (target.object.lifecycle_state == LifecycleState::Destroying ||
        target.object.lifecycle_state == LifecycleState::Destroyed ||
        !target.properties.enabled || !target.properties.visible) {
        return false;
    }
    switch (target.node.kind) {
    case WidgetKind::Button:
    case WidgetKind::CheckBox:
    case WidgetKind::RadioButton:
    case WidgetKind::Switch:
    case WidgetKind::Input:
    case WidgetKind::TextArea:
    case WidgetKind::Slider:
    case WidgetKind::Select:
    case WidgetKind::ListView:
    case WidgetKind::ListItem:
    case WidgetKind::TreeView:
    case WidgetKind::TreeItem:
    case WidgetKind::TableView:
    case WidgetKind::Tabs:
    case WidgetKind::ScrollView:
        return true;
    case WidgetKind::MenuItem:
        return target.properties.menu_enabled;
    default:
        return false;
    }
}

void EventSystem::collect_focusable_nodes(WidgetImpl& node, std::vector<WidgetImpl*>& out) const
{
    if (!node.properties.visible) {
        return;
    }
    if (is_focusable(node)) {
        out.push_back(&node);
    }
    if (node.node.kind == WidgetKind::MenuItem && !node.properties.menu_popup_open) {
        return;
    }
    if (node.node.kind == WidgetKind::TreeItem && !node.properties.tree_expanded) {
        return;
    }
    for (WidgetImpl* child : node.node.children) {
        if (child != nullptr) {
            collect_focusable_nodes(*child, out);
        }
    }
}

WidgetImpl* EventSystem::next_focus_target(WidgetImpl& root, bool reverse)
{
    std::vector<WidgetImpl*> nodes;
    WidgetImpl* focus_root = open_modal_dialog(root);
    collect_focusable_nodes(focus_root == nullptr ? root : *focus_root, nodes);
    if (nodes.empty()) {
        return nullptr;
    }

    WidgetImpl* current = nullptr;
    if (validate_target(focus_target_node_, focus_target_, focus_generation_, "focus_target")) {
        current = focus_target_node_;
    }

    auto it = std::find(nodes.begin(), nodes.end(), current);
    if (it == nodes.end()) {
        return reverse ? nodes.back() : nodes.front();
    }
    if (reverse) {
        return it == nodes.begin() ? nodes.back() : *(it - 1);
    }
    ++it;
    return it == nodes.end() ? nodes.front() : *it;
}

bool EventSystem::move_focus(WidgetImpl& root, bool reverse, EventDispatchResult& result)
{
    WidgetImpl* previous_target = nullptr;
    if (validate_target(focus_target_node_, focus_target_, focus_generation_, "focus_target")) {
        previous_target = focus_target_node_;
    }

    WidgetImpl* target = next_focus_target(root, reverse);
    if (target == nullptr) {
        diagnostics_event_ex("event", "focus_traversal_miss", root.object.object_id,
                             root.object.generation, current_frame_id(), widget_path(root).c_str(),
                             reverse ? "reverse" : "forward");
        return false;
    }

    bool closed_select_popup = false;
    if (previous_target != nullptr && previous_target != target &&
        previous_target->node.kind == WidgetKind::Select &&
        previous_target->properties.select_popup_open) {
        previous_target->properties.select_popup_open = false;
        mutate_widget(*previous_target, DirtyReason::Input | DirtyReason::Layout |
                                            DirtyReason::Paint,
                      "select_close_focus_traversal");
        diagnostics_event_ex("event", "select_close", previous_target->object.object_id,
                             previous_target->object.generation, current_frame_id(),
                             widget_path(*previous_target).c_str(), "focus_traversal");
        closed_select_popup = true;
    }

    const bool changed = set_focus_target(target);
    (void)set_hover_target(target);
    result.handled = true;
    result.target_changed = changed || closed_select_popup;
    result.hit_test.hit = true;
    result.hit_test.target_object_id = target->object.object_id;
    result.hit_test.target_generation = target->object.generation;
    result.hit_test.target_path = widget_path(*target);
    result.hit_test.target = target;
    result.route = build_route(*target);

    FiuiEvent event;
    event.type = EventType::KeyDown;
    event.target_object_id = result.hit_test.target_object_id;
    event.target_generation = result.hit_test.target_generation;
    event.target_path = result.hit_test.target_path;
    event.target = target;
    event.key_code = reverse ? (key_modifier_shift | 0x09) : 0x09;
    event.route = result.route;
    record_event(event);
    diagnostics_route(event, "route_focus_traversal");
    diagnostics_event_ex("event", "focus_traversal", target->object.object_id,
                         target->object.generation, current_frame_id(),
                         widget_path(*target).c_str(), reverse ? "reverse" : "forward");
    return true;
}

void EventSystem::diagnostics_route(const FiuiEvent& event, const char* action) const
{
    std::ostringstream route;
    route << event_type_name(event.type) << " route:";
    for (std::size_t index = 0; index < event.route.size(); ++index) {
        if (index > 0) {
            route << ">";
        }
        route << event.route[index].path;
    }
    diagnostics_event_ex("event", action, event.target_object_id, event.target_generation,
                         current_frame_id(), event.target_path.c_str(), route.str().c_str());
}

bool EventSystem::set_target(WidgetImpl*& target_slot,
                             ObjectId& id_slot,
                             std::uint32_t& generation_slot,
                             WidgetImpl* target,
                             const char* action)
{
    WidgetImpl* old_target = target_slot;
    if (old_target == target) {
        return false;
    }

    target_slot = target;
    id_slot = target_id(target);
    generation_slot = target_generation(target);
    invalidate_target_change(old_target, target, action);
    diagnostics_event_ex("event", action, id_slot, target_generation(target), current_frame_id(),
                         target == nullptr ? "" : widget_path(*target).c_str(),
                         target == nullptr ? "clear" : "set");
    return true;
}

bool EventSystem::validate_target(WidgetImpl*& target_slot,
                                  ObjectId& id_slot,
                                  std::uint32_t& generation_slot,
                                  const char* slot_name)
{
    if (id_slot == 0 || target_slot == nullptr) {
        target_slot = nullptr;
        id_slot = 0;
        generation_slot = 0;
        return false;
    }

    const ObjectLookupResult lookup =
        default_runtime().lookup_object(id_slot, generation_slot, slot_name);
    if (!lookup.alive || lookup.record.impl != target_slot) {
        diagnostics_event_ex("event", "target_invalidated", id_slot, generation_slot,
                             current_frame_id(),
                             lookup.record.path.empty() ? "" : lookup.record.path.c_str(),
                             slot_name == nullptr ? "" : slot_name);
        target_slot = nullptr;
        id_slot = 0;
        generation_slot = 0;
        return false;
    }

    target_slot = lookup.record.impl;
    return true;
}

void EventSystem::invalidate_target_change(WidgetImpl* old_target,
                                           WidgetImpl* new_target,
                                           const char* action)
{
    const DirtyReason input_paint = DirtyReason::Input | DirtyReason::Paint;
    if (old_target != nullptr) {
        mutate_widget(*old_target, input_paint, action);
    }
    if (new_target != nullptr && new_target != old_target) {
        mutate_widget(*new_target, input_paint, action);
    }
}

const char* event_type_name(EventType type) noexcept
{
    switch (type) {
    case EventType::Click:
        return "click";
    case EventType::PointerMove:
        return "pointer_move";
    case EventType::PointerDown:
        return "pointer_down";
    case EventType::PointerUp:
        return "pointer_up";
    case EventType::Wheel:
        return "wheel";
    case EventType::KeyDown:
        return "key_down";
    case EventType::KeyUp:
        return "key_up";
    case EventType::TextInput:
        return "text_input";
    }
    return "unknown";
}

} // namespace fiui
