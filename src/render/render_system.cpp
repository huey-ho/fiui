#include "render/render_system.h"

#include "runtime/runtime.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace fiui {
namespace {

float dpi_scale() noexcept
{
    const std::uint32_t dpi = default_runtime().platform_system().state().dpi;
    return std::max(0.25f, static_cast<float>(std::max<std::uint32_t>(1, dpi)) / 96.0f);
}

float scaled(float value) noexcept
{
    return value * dpi_scale();
}

bool is_container_kind(WidgetKind kind) noexcept
{
    switch (kind) {
    case WidgetKind::Widget:
    case WidgetKind::Window:
    case WidgetKind::Button:
    case WidgetKind::CheckBox:
    case WidgetKind::RadioButton:
    case WidgetKind::Switch:
    case WidgetKind::Input:
    case WidgetKind::TextArea:
    case WidgetKind::Progress:
    case WidgetKind::Slider:
    case WidgetKind::Select:
    case WidgetKind::SelectOption:
    case WidgetKind::ListView:
    case WidgetKind::ListItem:
    case WidgetKind::TreeView:
    case WidgetKind::TreeItem:
    case WidgetKind::TableView:
    case WidgetKind::Tabs:
    case WidgetKind::Toolbar:
    case WidgetKind::Column:
    case WidgetKind::Row:
    case WidgetKind::Padding:
    case WidgetKind::Align:
    case WidgetKind::SizedBox:
    case WidgetKind::ScrollView:
    case WidgetKind::Dialog:
    case WidgetKind::MenuBar:
    case WidgetKind::MenuItem:
    case WidgetKind::Separator:
        return true;
    case WidgetKind::Text:
    case WidgetKind::Image:
    case WidgetKind::Spacer:
        return false;
    }
    return false;
}

DisplayStyle base_style(const Theme& theme)
{
    DisplayStyle style;
    style.theme_name = theme.name;
    style.control_state = control_state_name(ControlState::Normal);
    style.fill = theme.colors.surface;
    style.text = theme.colors.text;
    style.border = theme.colors.border;
    style.border_width = 0.0f;
    style.radius = 0.0f;
    style.font_size = scaled(theme.typography.body);
    return style;
}

ControlState control_state_for(const WidgetImpl& impl)
{
    const EventSystem& events = default_runtime().event_system();
    if (events.capture_target() == impl.object.object_id) {
        return ControlState::Pressed;
    }
    if (events.hover_target() == impl.object.object_id) {
        return ControlState::Hover;
    }
    if (events.focus_target() == impl.object.object_id) {
        return ControlState::Focused;
    }
    return ControlState::Normal;
}

Color solid_white() noexcept
{
    return Color{255, 255, 255, 255};
}

bool style_is(const WidgetImpl& impl, const char* name) noexcept
{
    return impl.properties.style_name == name;
}

bool is_disabled(const WidgetImpl& impl) noexcept
{
    return !impl.properties.enabled || (impl.node.kind == WidgetKind::MenuItem &&
                                        !impl.properties.menu_enabled);
}

float text_line_height(const DisplayStyle& style) noexcept
{
    return std::max(style.font_size * 1.35f, 16.0f);
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

bool is_inside_button_content(const WidgetImpl& impl) noexcept
{
    for (const WidgetImpl* node = impl.node.parent; node != nullptr; node = node->node.parent) {
        if (node->node.kind == WidgetKind::Button) {
            return true;
        }
    }
    return false;
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

void apply_button_variant(DisplayStyle& style, const WidgetImpl& impl, const Theme& theme)
{
    if (style_is(impl, "primary")) {
        style.fill = theme.colors.accent;
        style.text = solid_white();
        style.border = theme.colors.accent;
        style.border_width = scaled(1.0f);
        return;
    }
    if (style_is(impl, "danger")) {
        style.fill = theme.colors.error;
        style.text = solid_white();
        style.border = theme.colors.error;
        style.border_width = scaled(1.0f);
        return;
    }
    if (style_is(impl, "subtle")) {
        style.fill = theme.colors.background;
        style.text = theme.colors.muted_text;
        style.border = theme.colors.border;
        style.border_width = 0.0f;
    }
}

void apply_button_overrides(DisplayStyle& style, const WidgetImpl& impl)
{
    const ControlState state = control_state_for(impl);
    if (state == ControlState::Pressed && impl.properties.has_button_pressed_background) {
        style.fill = impl.properties.button_pressed_background;
    } else if (state == ControlState::Hover && impl.properties.has_button_hover_background) {
        style.fill = impl.properties.button_hover_background;
    } else if (impl.properties.has_button_background) {
        style.fill = impl.properties.button_background;
    }
    if (impl.properties.has_button_radius) {
        style.radius = scaled(impl.properties.button_radius);
    }
}

DisplayStyle style_for_rect(const WidgetImpl& impl, const Theme& theme)
{
    DisplayStyle style = base_style(theme);
    switch (impl.node.kind) {
    case WidgetKind::Window:
        style.fill = theme.colors.background;
        break;
    case WidgetKind::Button: {
        const ControlState state = control_state_for(impl);
        const ComponentStateStyle& button =
            default_runtime().style_system().resolve_button_style(theme, state);
        style.control_state = control_state_name(state);
        style.fill = button.background;
        style.text = button.text;
        style.border = button.border;
        style.border_width = scaled(button.border_width);
        style.radius = scaled(button.radius);
        apply_button_variant(style, impl, theme);
        apply_button_overrides(style, impl);
        if (is_disabled(impl)) {
            style.fill = theme.colors.surface;
            style.text = theme.colors.muted_text;
            style.border = theme.colors.border;
            style.fill.a = 220;
        }
        break;
    }
    case WidgetKind::CheckBox:
    case WidgetKind::RadioButton: {
        const ControlState state = control_state_for(impl);
        style.control_state = control_state_name(state);
        style.fill = Color{0, 0, 0, 0};
        style.text = is_disabled(impl) ? theme.colors.muted_text : theme.colors.text;
        style.border = Color{0, 0, 0, 0};
        style.border_width = 0.0f;
        style.radius = scaled(theme.radius.sm);
        break;
    }
    case WidgetKind::Switch: {
        const ControlState state = control_state_for(impl);
        style.control_state = control_state_name(state);
        style.fill = Color{0, 0, 0, 0};
        style.text = is_disabled(impl) ? theme.colors.muted_text : theme.colors.text;
        style.border = Color{0, 0, 0, 0};
        style.border_width = 0.0f;
        style.radius = scaled(theme.radius.md);
        break;
    }
    case WidgetKind::Input:
    case WidgetKind::TextArea:
        style.fill = theme.colors.surface;
        style.border = theme.colors.border;
        style.border_width = scaled(1.0f);
        style.radius = scaled(theme.radius.md);
        break;
    case WidgetKind::Select:
        style.fill = theme.colors.surface;
        style.border = theme.colors.border;
        style.border_width = scaled(1.0f);
        style.radius = scaled(theme.radius.md);
        break;
    case WidgetKind::SelectOption: {
        const ControlState state = control_state_for(impl);
        const bool selected = impl.node.parent != nullptr &&
                              impl.node.parent->node.kind == WidgetKind::Select &&
                              child_index_in_parent(impl) ==
                                  impl.node.parent->properties.selected_index;
        style.control_state = control_state_name(state);
        style.fill = selected ? theme.colors.accent
                              : (state == ControlState::Hover ||
                                         state == ControlState::Focused
                                     ? theme.colors.background
                                     : Color{0, 0, 0, 0});
        style.text = selected ? solid_white() : theme.colors.text;
        style.border = Color{0, 0, 0, 0};
        style.border_width = 0.0f;
        style.radius = selected ? scaled(theme.radius.sm) : 0.0f;
        break;
    }
    case WidgetKind::ListView:
        style.fill = theme.colors.surface;
        style.border = theme.colors.border;
        style.border_width = scaled(1.0f);
        style.radius = scaled(theme.radius.md);
        break;
    case WidgetKind::ListItem: {
        const ControlState state = control_state_for(impl);
        const bool selected = impl.node.parent != nullptr &&
                              impl.node.parent->node.kind == WidgetKind::ListView &&
                              child_index_in_parent(impl) ==
                                  impl.node.parent->properties.selected_index;
        style.control_state = control_state_name(state);
        style.fill = selected ? theme.colors.accent
                              : (state == ControlState::Hover || state == ControlState::Focused
                                     ? theme.colors.background
                                     : Color{0, 0, 0, 0});
        style.text = selected ? solid_white() : theme.colors.text;
        style.border = Color{0, 0, 0, 0};
        style.border_width = 0.0f;
        style.radius = scaled(theme.radius.sm);
        break;
    }
    case WidgetKind::TreeView:
        style.fill = theme.colors.surface;
        style.border = theme.colors.border;
        style.border_width = scaled(1.0f);
        style.radius = scaled(theme.radius.md);
        break;
    case WidgetKind::TreeItem: {
        const ControlState state = control_state_for(impl);
        bool selected = impl.properties.tree_selected;
        for (const WidgetImpl* cursor = impl.node.parent; cursor != nullptr;
             cursor = cursor->node.parent) {
            if (cursor->node.kind == WidgetKind::TreeView) {
                selected = selected || cursor->properties.selected_object_id ==
                                           impl.object.object_id;
                break;
            }
        }
        style.control_state = control_state_name(state);
        style.fill = selected ? theme.colors.accent
                              : (state == ControlState::Hover || state == ControlState::Focused
                                     ? theme.colors.background
                                     : Color{0, 0, 0, 0});
        style.text = selected ? solid_white() : theme.colors.text;
        style.border = Color{0, 0, 0, 0};
        style.border_width = 0.0f;
        style.radius = scaled(theme.radius.sm);
        break;
    }
    case WidgetKind::TableView:
        style.fill = theme.colors.surface;
        style.border = theme.colors.border;
        style.border_width = scaled(1.0f);
        style.radius = scaled(theme.radius.md);
        break;
    case WidgetKind::Tabs:
        style.fill = theme.colors.surface;
        style.border = theme.colors.border;
        style.border_width = scaled(1.0f);
        style.radius = scaled(theme.radius.md);
        break;
    case WidgetKind::Toolbar:
        style.fill = theme.colors.surface;
        style.border = theme.colors.border;
        style.border_width = scaled(1.0f);
        style.radius = scaled(theme.radius.md);
        break;
    case WidgetKind::Progress:
        style.fill = theme.colors.accent;
        style.radius = scaled(theme.radius.sm);
        break;
    case WidgetKind::Slider: {
        const ControlState state = control_state_for(impl);
        style.control_state = control_state_name(state);
        style.fill = Color{0, 0, 0, 0};
        style.border = Color{0, 0, 0, 0};
        style.border_width = 0.0f;
        style.radius = scaled(theme.radius.sm);
        break;
    }
    case WidgetKind::Separator:
        style.fill = theme.colors.border;
        break;
    case WidgetKind::Column:
    case WidgetKind::Row:
    case WidgetKind::Padding:
    case WidgetKind::Align:
    case WidgetKind::SizedBox:
        if (is_inside_button_content(impl)) {
            style.fill = Color{0, 0, 0, 0};
            style.border = Color{0, 0, 0, 0};
            style.border_width = 0.0f;
        } else {
            style.fill = theme.colors.surface;
        }
        break;
    case WidgetKind::SplitView:
        style.fill = theme.colors.surface;
        style.border = theme.colors.border;
        style.border_width = scaled(1.0f);
        style.radius = scaled(theme.radius.md);
        break;
    case WidgetKind::ScrollView:
        style.fill = theme.colors.surface;
        style.radius = scaled(theme.radius.md);
        break;
    case WidgetKind::MenuBar:
        style.fill = theme.colors.surface;
        style.border = Color{0, 0, 0, 0};
        style.border_width = 0.0f;
        style.radius = 0.0f;
        break;
    case WidgetKind::MenuItem: {
        const ControlState state = control_state_for(impl);
        style.control_state = control_state_name(state);
        style.fill = Color{0, 0, 0, 0};
        if (!impl.properties.menu_enabled) {
            style.fill = Color{0, 0, 0, 0};
        } else if (impl.properties.menu_popup_open || state == ControlState::Hover ||
                   state == ControlState::Focused) {
            style.fill = theme.colors.background;
        } else if (state == ControlState::Pressed) {
            style.fill = theme.colors.border;
        }
        style.text = impl.properties.menu_enabled ? theme.colors.text : theme.colors.muted_text;
        style.border = Color{0, 0, 0, 0};
        style.border_width = 0.0f;
        style.radius = 0.0f;
        break;
    }
    case WidgetKind::Widget:
        style.fill = theme.colors.surface;
        break;
    default:
        break;
    }
    return style;
}

DisplayStyle style_for_text(const WidgetImpl& impl, const Theme& theme)
{
    DisplayStyle style = base_style(theme);
    style.fill = Color{0, 0, 0, 0};
    style.text = theme.colors.text;
    style.font_size = scaled(theme.typography.body);
    if (impl.node.kind == WidgetKind::Button) {
        const ControlState state = control_state_for(impl);
        const ComponentStateStyle& button =
            default_runtime().style_system().resolve_button_style(theme, state);
        style.control_state = control_state_name(state);
        style.text = button.text;
        apply_button_variant(style, impl, theme);
        if (is_disabled(impl)) {
            style.text = theme.colors.muted_text;
        }
        const float text_padding =
            impl.properties.has_button_text_padding ? impl.properties.button_text_padding
                                                    : theme.spacing.md;
        style.text_padding_x = scaled(text_padding);
        style.text_padding_y =
            scaled(impl.properties.has_button_text_padding ? text_padding : theme.spacing.sm);
        style.text_align = "center";
        style.paragraph_align = "center";
        style.overflow = "ellipsis";
    } else if (impl.node.kind == WidgetKind::CheckBox ||
               impl.node.kind == WidgetKind::RadioButton) {
        const ControlState state = control_state_for(impl);
        style.control_state = control_state_name(state);
        style.text = theme.colors.text;
        style.text_padding_x = scaled(30.0f);
        style.text_padding_y = scaled(theme.spacing.sm);
        style.text_align = "leading";
        style.paragraph_align = "center";
        style.overflow = "ellipsis";
    } else if (impl.node.kind == WidgetKind::Switch) {
        const ControlState state = control_state_for(impl);
        style.control_state = control_state_name(state);
        style.text = theme.colors.text;
        style.text_padding_x = scaled(56.0f);
        style.text_padding_y = scaled(theme.spacing.sm);
        style.text_align = "leading";
        style.paragraph_align = "center";
        style.overflow = "ellipsis";
    } else if (impl.node.kind == WidgetKind::Select) {
        style.text = impl.properties.has_selected_index ? theme.colors.text : theme.colors.muted_text;
        if (is_disabled(impl)) {
            style.text = theme.colors.muted_text;
        }
        style.text_padding_x = scaled(theme.spacing.md);
        style.text_padding_y = scaled(theme.spacing.sm);
        style.text_align = "leading";
        style.paragraph_align = "center";
        style.overflow = "ellipsis";
    } else if (impl.node.kind == WidgetKind::SelectOption) {
        const ControlState state = control_state_for(impl);
        const bool selected = impl.node.parent != nullptr &&
                              impl.node.parent->node.kind == WidgetKind::Select &&
                              child_index_in_parent(impl) ==
                                  impl.node.parent->properties.selected_index;
        style.control_state = control_state_name(state);
        style.text = selected ? solid_white()
                              : (is_disabled(impl) ? theme.colors.muted_text
                                                   : theme.colors.text);
        style.font_size = scaled(14.0f);
        style.text_padding_x = scaled(10.0f);
        style.text_padding_y = scaled(3.0f);
        style.text_align = "leading";
        style.paragraph_align = "center";
        style.overflow = "ellipsis";
    } else if (impl.node.kind == WidgetKind::ListItem) {
        const ControlState state = control_state_for(impl);
        const bool selected = impl.node.parent != nullptr &&
                              impl.node.parent->node.kind == WidgetKind::ListView &&
                              child_index_in_parent(impl) ==
                                  impl.node.parent->properties.selected_index;
        style.control_state = control_state_name(state);
        style.text = selected ? solid_white()
                              : (is_disabled(impl) ? theme.colors.muted_text
                                                   : theme.colors.text);
        style.font_size = scaled(14.0f);
        style.text_padding_x = scaled(10.0f);
        style.text_padding_y = scaled(3.0f);
        style.text_align = "leading";
        style.paragraph_align = "center";
        style.overflow = "ellipsis";
    } else if (impl.node.kind == WidgetKind::TreeItem) {
        const ControlState state = control_state_for(impl);
        bool selected = impl.properties.tree_selected;
        for (const WidgetImpl* cursor = impl.node.parent; cursor != nullptr;
             cursor = cursor->node.parent) {
            if (cursor->node.kind == WidgetKind::TreeView) {
                selected = selected || cursor->properties.selected_object_id ==
                                           impl.object.object_id;
                break;
            }
        }
        style.control_state = control_state_name(state);
        style.text = selected ? solid_white()
                              : (is_disabled(impl) ? theme.colors.muted_text
                                                   : theme.colors.text);
        style.font_size = scaled(14.0f);
        style.text_padding_x =
            scaled(28.0f + static_cast<float>(impl.properties.tree_depth) * 18.0f);
        style.text_padding_y = scaled(3.0f);
        style.text_align = "leading";
        style.paragraph_align = "center";
        style.overflow = "ellipsis";
    } else if (impl.node.kind == WidgetKind::MenuItem) {
        const ControlState state = control_state_for(impl);
        style.control_state = control_state_name(state);
        style.text = impl.properties.menu_enabled ? theme.colors.text : theme.colors.muted_text;
        style.font_size = scaled(14.0f);
        const bool submenu_item =
            impl.node.parent != nullptr && impl.node.parent->node.kind == WidgetKind::MenuItem;
        style.text_padding_x = scaled(submenu_item ? 12.0f : 8.0f);
        style.text_padding_y = scaled(3.0f);
        style.text_align = submenu_item ? "leading" : "center";
        style.paragraph_align = "center";
        style.overflow = "ellipsis";
    } else if ((impl.node.kind == WidgetKind::Input || impl.node.kind == WidgetKind::TextArea) &&
               impl.properties.text.empty()) {
        style.text = theme.colors.muted_text;
        style.text_padding_x = scaled(theme.spacing.md);
        style.text_padding_y = scaled(theme.spacing.sm);
        style.text_align = "leading";
        style.paragraph_align = impl.node.kind == WidgetKind::TextArea ? "near" : "center";
        style.overflow = "ellipsis";
    } else if (impl.node.kind == WidgetKind::Input || impl.node.kind == WidgetKind::TextArea) {
        style.text_padding_x = scaled(theme.spacing.md);
        style.text_padding_y = scaled(theme.spacing.sm);
        style.text_align = "leading";
        style.paragraph_align = impl.node.kind == WidgetKind::TextArea ? "near" : "center";
        style.overflow = "ellipsis";
    } else if (impl.properties.style_name == "title") {
        style.font_size = scaled(theme.typography.title);
        style.overflow = "ellipsis";
    } else if (impl.properties.style_name == "caption") {
        style.font_size = scaled(theme.typography.caption);
        style.text = theme.colors.muted_text;
        style.overflow = "ellipsis";
    }
    if (impl.node.kind == WidgetKind::Text &&
        (impl.properties.text_multiline || impl.properties.style_name == "multiline")) {
        style.word_wrap = "word";
        style.overflow = "clip";
        style.paragraph_align = "near";
    }
    if (impl.node.kind == WidgetKind::TextArea) {
        style.word_wrap = "word";
        style.overflow = "clip";
        style.paragraph_align = "near";
    }
    return style;
}

DisplayStyle style_for_image(const WidgetImpl& impl, const Theme& theme)
{
    DisplayStyle style = base_style(theme);
    style.fill = Color{255, 255, 255, 255};
    style.border = theme.colors.border;
    style.border_width = 0.0f;
    style.radius = scaled(impl.properties.image_radius);
    if (style.radius <= 0.0f && impl.properties.style_name == "rounded") {
        style.radius = scaled(theme.radius.md);
    }
    return style;
}

DisplayStyle style_for_image_placeholder(const WidgetImpl& impl, const Theme& theme)
{
    DisplayStyle style = base_style(theme);
    style.fill = theme.colors.surface;
    style.border = theme.colors.border;
    style.border_width = scaled(1.0f);
    style.radius = scaled(impl.properties.image_radius);
    if (style.radius <= 0.0f && impl.properties.style_name == "rounded") {
        style.radius = scaled(theme.radius.md);
    }
    return style;
}

Rect popup_bounds_for(const WidgetImpl& impl) noexcept
{
    Rect bounds;
    bool initialized = false;
    for (const WidgetImpl* child : impl.node.children) {
        if (child == nullptr || child->dirty.bounds.width <= 0.0f ||
            child->dirty.bounds.height <= 0.0f) {
            continue;
        }
        if (!initialized) {
            bounds = child->dirty.bounds;
            initialized = true;
            continue;
        }
        const float min_x = std::min(bounds.x, child->dirty.bounds.x);
        const float min_y = std::min(bounds.y, child->dirty.bounds.y);
        const float max_x =
            std::max(bounds.x + bounds.width, child->dirty.bounds.x + child->dirty.bounds.width);
        const float max_y =
            std::max(bounds.y + bounds.height, child->dirty.bounds.y + child->dirty.bounds.height);
        bounds = Rect{min_x, min_y, max_x - min_x, max_y - min_y};
    }
    return bounds;
}

DisplayStyle style_for_menu_popup(const Theme& theme)
{
    DisplayStyle style = base_style(theme);
    style.fill = theme.colors.background;
    if (std::strcmp(theme.name, "modern.dark") == 0) {
        style.fill = Color{34, 38, 45, 255};
    } else {
        style.fill = Color{255, 255, 255, 255};
    }
    style.border = theme.colors.border;
    style.border_width = scaled(1.0f);
    style.radius = 0.0f;
    return style;
}

DisplayStyle style_for_tooltip_surface(const Theme& theme)
{
    DisplayStyle style = base_style(theme);
    if (std::strcmp(theme.name, "modern.dark") == 0) {
        style.fill = Color{18, 22, 28, 255};
        style.text = Color{245, 247, 250, 255};
        style.border = Color{98, 109, 126, 255};
    } else {
        style.fill = Color{255, 255, 255, 255};
        style.text = Color{28, 33, 40, 255};
        style.border = Color{144, 156, 172, 255};
    }
    style.border_width = scaled(1.0f);
    style.radius = scaled(theme.radius.sm);
    return style;
}

DisplayStyle style_for_tooltip_text(const Theme& theme)
{
    DisplayStyle style = style_for_tooltip_surface(theme);
    style.fill = Color{0, 0, 0, 0};
    style.font_size = scaled(theme.typography.caption);
    style.text_padding_x = scaled(theme.spacing.sm);
    style.text_padding_y = scaled(4.0f);
    style.text_align = "leading";
    style.paragraph_align = "center";
    style.overflow = "ellipsis";
    return style;
}

DisplayResource resource_from_record(const ResourceRecord* record)
{
    if (record == nullptr) {
        return DisplayResource{};
    }
    DisplayResource resource;
    resource.id = record->id;
    resource.kind = record->kind;
    resource.cache_state = record->cache_state;
    resource.owner_object_id = record->owner_object_id;
    resource.owner_path = record->owner_path;
    resource.key = record->key;
    resource.text_metrics = record->text_metrics;
    resource.image_metadata = record->image_metadata;
    resource.texture_metadata = record->texture_metadata;
    return resource;
}

std::string text_resource_key(const WidgetImpl& impl, const DisplayStyle& style, std::uint32_t dpi)
{
    return std::string("text:") + widget_kind_name(impl.node.kind) + ":" + impl.properties.text +
           ":" + impl.properties.placeholder + ":" + impl.properties.style_name + ":font=" +
           std::to_string(style.font_size) + ":theme=" + style.theme_name +
           ":align=" + style.text_align + ":paragraph=" + style.paragraph_align +
           ":overflow=" + style.overflow + ":wrap=" + style.word_wrap +
           ":checked=" + (impl.properties.menu_checked ? "true" : "false") +
           ":shortcut=" + impl.properties.menu_shortcut +
           ":dpi=" + std::to_string(dpi);
}

} // namespace

RenderFrameResult RenderSystem::build_frame(const WidgetImpl& root) const
{
    RenderFrameResult result;
    collect_node(root, result, invalid_layer_id);
    collect_open_menu_popups(root, result, invalid_layer_id);
    collect_open_select_popups(root, result, invalid_layer_id);
    collect_open_dialogs(root, result, invalid_layer_id);
    append_hover_tooltip(result);
    build_batches(result);
    return result;
}

void RenderSystem::append_hover_tooltip(RenderFrameResult& result) const
{
    const EventSystem& events = default_runtime().event_system();
    if (events.capture_target() != 0) {
        return;
    }

    const WidgetImpl* target = events.hover_target_node();
    if (target == nullptr || target->object.lifecycle_state == LifecycleState::Destroying ||
        target->object.lifecycle_state == LifecycleState::Destroyed ||
        target->properties.tooltip.empty()) {
        return;
    }

    const Theme& theme = default_runtime().style_system().active_theme();
    const DisplayStyle surface_style = style_for_tooltip_surface(theme);
    const DisplayStyle text_style = style_for_tooltip_text(theme);
    const std::string target_path = widget_path(*target);
    const std::string path = target_path + "/tooltip";
    const std::string& text = target->properties.tooltip;
    const float max_width = scaled(320.0f);
    const float max_height = scaled(48.0f);
    const std::uint32_t dpi = default_runtime().platform_system().state().dpi;
    const TextMetrics metrics = default_runtime().text_system().measure_text(
        text.c_str(), text_style.font_size, max_width, max_height, dpi,
        target->object.object_id, path.c_str(), text_style.word_wrap, text_style.overflow);

    const float padding_x = text_style.text_padding_x;
    const float padding_y = text_style.text_padding_y;
    const float width =
        std::max(scaled(40.0f), std::min(max_width, metrics.width + padding_x * 2.0f));
    const float height =
        std::max(scaled(24.0f), std::min(max_height, metrics.height + padding_y * 2.0f));
    const WidgetImpl* root_node = target;
    while (root_node->node.parent != nullptr) {
        root_node = root_node->node.parent;
    }
    const PlatformState platform = default_runtime().platform_system().state();
    const float root_width = platform.width > 0 ? static_cast<float>(platform.width)
                                                : root_node->dirty.bounds.x +
                                                      root_node->dirty.bounds.width;
    const float root_height = platform.height > 0 ? static_cast<float>(platform.height)
                                                  : root_node->dirty.bounds.y +
                                                        root_node->dirty.bounds.height;
    const float margin = scaled(4.0f);
    const float gap = scaled(8.0f);
    float x = target->dirty.bounds.x;
    float y = target->dirty.bounds.y - height - gap;
    if (y < margin) {
        y = target->dirty.bounds.y + target->dirty.bounds.height + gap;
    }
    if (x + width > root_width - scaled(4.0f)) {
        x = std::max(margin, root_width - width - margin);
    }
    if (y + height > root_height - margin) {
        y = std::max(margin, target->dirty.bounds.y - height - gap);
    }

    const Rect surface_bounds{x, y, width, height};
    const Rect text_bounds{x + padding_x,
                           y + padding_y,
                           std::max(0.0f, width - padding_x * 2.0f),
                           std::max(0.0f, height - padding_y * 2.0f)};
    const ResourceId text_resource_id =
        default_runtime().resource_system().find_or_register_resource(
            ResourceKind::TextLayout, target->object.object_id, target_path.c_str(),
            (std::string("tooltip:") + text + ":font=" +
             std::to_string(text_style.font_size) + ":theme=" + text_style.theme_name +
             ":dpi=" + std::to_string(dpi))
                .c_str(),
            ResourceCacheState::Cached);
    default_runtime().resource_system().update_text_metrics(text_resource_id, metrics);
    const ResourceRecord* text_record = default_runtime().resource_system().find(text_resource_id);
    const DisplayResource resource = resource_from_record(text_record);

    const std::uint32_t rect_command_index =
        add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                           target->object.object_id,
                                           target->node.kind,
                                           path,
                                           surface_bounds,
                                           {},
                                           {},
                                           surface_style,
                                           {}});
    const std::uint32_t rect_layer_id =
        add_layer(result,
                  LayerNode{0,
                            invalid_layer_id,
                            LayerKind::RoundedRect,
                            target->object.object_id,
                            target->object.generation,
                            target->node.kind,
                            path,
                            surface_bounds,
                            surface_bounds,
                            surface_bounds,
                            surface_style,
                            {},
                            {},
                            {},
                            1.0f,
                            surface_style.radius,
                            0.0f,
                            rect_command_index,
                            1,
                            0},
                  invalid_layer_id);

    const std::uint32_t text_command_index =
        add_command(result, DisplayCommand{DisplayCommandKind::Text,
                                           target->object.object_id,
                                           target->node.kind,
                                           path + "/text",
                                           text_bounds,
                                           text,
                                           resource.key,
                                           text_style,
                                           resource});
    (void)add_layer(result,
                    LayerNode{0,
                              rect_layer_id,
                              LayerKind::Text,
                              target->object.object_id,
                              target->object.generation,
                              target->node.kind,
                              path + "/text",
                              text_bounds,
                              text_bounds,
                              surface_bounds,
                              text_style,
                              resource,
                              text,
                              resource.key,
                              1.0f,
                              0.0f,
                              0.0f,
                              text_command_index,
                              1,
                              0},
                    rect_layer_id);
}

void RenderSystem::collect_node(const WidgetImpl& impl,
                                RenderFrameResult& result,
                                std::uint32_t parent_layer_id) const
{
    if (!impl.properties.visible) {
        return;
    }
    result.render_tree.nodes.push_back(RenderNode{impl.object.object_id,
                                                  impl.object.generation,
                                                  impl.node.kind,
                                                  widget_path(impl),
                                                  impl.dirty.bounds,
                                                  impl.dirty.paint_bounds,
                                                  impl.dirty.clip_bounds,
                                                  static_cast<std::uint32_t>(
                                                      impl.node.children.size())});

    if (impl.node.kind == WidgetKind::Dialog) {
        return;
    }

    const std::uint32_t node_layer_id =
        append_layers_and_commands(impl, result, parent_layer_id);

    if (impl.node.kind == WidgetKind::TableView) {
        append_table_commands(impl, result, node_layer_id);
    }

    if (impl.node.kind == WidgetKind::SplitView && impl.node.children.size() >= 2 &&
        impl.node.children[0] != nullptr) {
        const Theme& theme = default_runtime().style_system().active_theme();
        const bool horizontal =
            impl.properties.split_orientation == SplitOrientation::Horizontal;
        const float handle_size = scaled(std::max(1.0f, impl.properties.split_handle_size));
        const WidgetImpl* first = impl.node.children[0];
        const Rect handle_bounds =
            horizontal ? Rect{first->dirty.bounds.x + first->dirty.bounds.width,
                              impl.dirty.bounds.y,
                              handle_size,
                              impl.dirty.bounds.height}
                       : Rect{impl.dirty.bounds.x,
                              first->dirty.bounds.y + first->dirty.bounds.height,
                              impl.dirty.bounds.width,
                              handle_size};
        DisplayStyle handle_style = base_style(theme);
        const ControlState state = control_state_for(impl);
        handle_style.control_state = control_state_name(state);
        handle_style.fill = state == ControlState::Hover || state == ControlState::Pressed
                                ? theme.colors.accent
                                : theme.colors.border;
        handle_style.border = Color{0, 0, 0, 0};
        handle_style.border_width = 0.0f;
        handle_style.radius = scaled(2.0f);
        const std::string handle_path = widget_path(impl) + "/split_handle";
        const std::uint32_t handle_command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               handle_path,
                                               handle_bounds,
                                               {},
                                               {},
                                               handle_style,
                                               {}});
        (void)add_layer(result,
                        LayerNode{0,
                                  node_layer_id,
                                  LayerKind::RoundedRect,
                                  impl.object.object_id,
                                  impl.object.generation,
                                  impl.node.kind,
                                  handle_path,
                                  handle_bounds,
                                  handle_bounds,
                                  impl.dirty.clip_bounds,
                                  handle_style,
                                  {},
                                  {},
                                  {},
                                  1.0f,
                                  handle_style.radius,
                                  0.0f,
                                  handle_command_index,
                                  1,
                                  0},
                        node_layer_id);
    }

    const bool menu_popup_children =
        impl.node.kind == WidgetKind::MenuItem && !impl.node.children.empty();
    const bool select_popup_children =
        impl.node.kind == WidgetKind::Select && !impl.node.children.empty();
    const bool collapsed_tree_item =
        impl.node.kind == WidgetKind::TreeItem && !impl.properties.tree_expanded;
    if (!menu_popup_children && !select_popup_children && !collapsed_tree_item) {
        for (const WidgetImpl* child : impl.node.children) {
            if (child != nullptr) {
                collect_node(*child, result, node_layer_id);
            }
        }

    }

    append_node_close_commands(impl, result, node_layer_id);
}

void RenderSystem::collect_open_menu_popups(const WidgetImpl& impl,
                                            RenderFrameResult& result,
                                            std::uint32_t parent_layer_id) const
{
    if (impl.node.kind == WidgetKind::MenuItem &&
        !default_runtime().menu_system().ancestors_visible(impl)) {
        return;
    }

    if (impl.node.kind == WidgetKind::MenuItem && impl.properties.menu_popup_open &&
        !impl.node.children.empty()) {
        append_menu_popup_surface(impl, result, parent_layer_id);
        for (const WidgetImpl* child : impl.node.children) {
            if (child != nullptr) {
                collect_node(*child, result, parent_layer_id);
            }
        }
    }

    for (const WidgetImpl* child : impl.node.children) {
        if (child != nullptr) {
            collect_open_menu_popups(*child, result, parent_layer_id);
        }
    }
}

void RenderSystem::collect_open_select_popups(const WidgetImpl& impl,
                                              RenderFrameResult& result,
                                              std::uint32_t parent_layer_id) const
{
    if (impl.node.kind == WidgetKind::Select && impl.properties.select_popup_open &&
        !impl.node.children.empty()) {
        append_select_popup_surface(impl, result, parent_layer_id);
        for (const WidgetImpl* child : impl.node.children) {
            if (child != nullptr) {
                collect_node(*child, result, parent_layer_id);
            }
        }
    }

    for (const WidgetImpl* child : impl.node.children) {
        if (child != nullptr) {
            collect_open_select_popups(*child, result, parent_layer_id);
        }
    }
}

void RenderSystem::collect_open_dialogs(const WidgetImpl& impl,
                                        RenderFrameResult& result,
                                        std::uint32_t parent_layer_id) const
{
    if (impl.node.kind == WidgetKind::Dialog && impl.properties.dialog_open &&
        !impl.node.children.empty()) {
        append_dialog_backdrop(impl, result, parent_layer_id);
        for (const WidgetImpl* child : impl.node.children) {
            if (child != nullptr) {
                const Theme& theme = default_runtime().style_system().active_theme();
                DisplayStyle panel_style = base_style(theme);
                panel_style.fill = theme.colors.surface;
                panel_style.border = theme.colors.border;
                panel_style.border_width = scaled(1.0f);
                panel_style.radius = scaled(theme.radius.lg);
                const std::string panel_path = widget_path(impl) + "/panel";
                const std::uint32_t panel_command_index =
                    add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                       impl.object.object_id,
                                                       impl.node.kind,
                                                       panel_path,
                                                       child->dirty.bounds,
                                                       {},
                                                       {},
                                                       panel_style,
                                                       {}});
                (void)add_layer(result,
                                LayerNode{0,
                                          parent_layer_id,
                                          LayerKind::RoundedRect,
                                          impl.object.object_id,
                                          impl.object.generation,
                                          impl.node.kind,
                                          panel_path,
                                          child->dirty.bounds,
                                          child->dirty.bounds,
                                          child->dirty.bounds,
                                          panel_style,
                                          {},
                                          {},
                                          {},
                                          1.0f,
                                          0.0f,
                                          0.0f,
                                          panel_command_index,
                                          1,
                                          0},
                                parent_layer_id);
                (void)add_command(result, DisplayCommand{DisplayCommandKind::RoundedClip,
                                                         impl.object.object_id,
                                                         impl.node.kind,
                                                         panel_path + "/clip",
                                                         child->dirty.bounds,
                                                         {},
                                                         {},
                                                         panel_style,
                                                         {}});
                collect_node(*child, result, parent_layer_id);
                (void)add_command(result, DisplayCommand{DisplayCommandKind::RoundedClipEnd,
                                                         impl.object.object_id,
                                                         impl.node.kind,
                                                         panel_path + "/clip",
                                                         child->dirty.bounds,
                                                         {},
                                                         {},
                                                         panel_style,
                                                         {}});
            }
        }
    }

    for (const WidgetImpl* child : impl.node.children) {
        if (child != nullptr) {
            collect_open_dialogs(*child, result, parent_layer_id);
        }
    }
}

void RenderSystem::append_menu_popup_surface(const WidgetImpl& impl,
                                             RenderFrameResult& result,
                                             std::uint32_t parent_layer_id) const
{
    const Rect bounds = popup_bounds_for(impl);
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return;
    }
    const Theme& theme = default_runtime().style_system().active_theme();
    const DisplayStyle style = style_for_menu_popup(theme);
    const std::string path = widget_path(impl) + "/popup";
    const std::uint32_t command_index =
        add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                           impl.object.object_id,
                                           impl.node.kind,
                                           path,
                                           bounds,
                                           {},
                                           {},
                                           style,
                                           {}});
    (void)add_layer(result,
                    LayerNode{0,
                              parent_layer_id,
                              LayerKind::Rect,
                              impl.object.object_id,
                              impl.object.generation,
                              impl.node.kind,
                              path,
                              bounds,
                              bounds,
                              bounds,
                              style,
                              {},
                              {},
                              {},
                              1.0f,
                              0.0f,
                              0.0f,
                              command_index,
                              1,
                              0},
                    parent_layer_id);
}

void RenderSystem::append_select_popup_surface(const WidgetImpl& impl,
                                               RenderFrameResult& result,
                                               std::uint32_t parent_layer_id) const
{
    const Rect bounds = popup_bounds_for(impl);
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return;
    }
    const Theme& theme = default_runtime().style_system().active_theme();
    const DisplayStyle style = style_for_menu_popup(theme);
    const std::string path = widget_path(impl) + "/popup";
    const std::uint32_t command_index =
        add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                           impl.object.object_id,
                                           impl.node.kind,
                                           path,
                                           bounds,
                                           {},
                                           {},
                                           style,
                                           {}});
    (void)add_layer(result,
                    LayerNode{0,
                              parent_layer_id,
                              LayerKind::Rect,
                              impl.object.object_id,
                              impl.object.generation,
                              impl.node.kind,
                              path,
                              bounds,
                              bounds,
                              bounds,
                              style,
                              {},
                              {},
                              {},
                              1.0f,
                              0.0f,
                              0.0f,
                              command_index,
                              1,
                              0},
                    parent_layer_id);
}

void RenderSystem::append_dialog_backdrop(const WidgetImpl& impl,
                                          RenderFrameResult& result,
                                          std::uint32_t parent_layer_id) const
{
    const Rect bounds = impl.dirty.bounds;
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return;
    }
    const Theme& theme = default_runtime().style_system().active_theme();
    DisplayStyle style = base_style(theme);
    const std::uint8_t alpha =
        static_cast<std::uint8_t>(std::clamp(impl.properties.dialog_backdrop_opacity, 0.0f, 1.0f) *
                                  255.0f);
    style.fill = Color{0, 0, 0, alpha};
    style.border = Color{0, 0, 0, 0};
    style.border_width = 0.0f;
    const std::string path = widget_path(impl) + "/backdrop";
    const std::uint32_t command_index =
        add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                           impl.object.object_id,
                                           impl.node.kind,
                                           path,
                                           bounds,
                                           {},
                                           {},
                                           style,
                                           {}});
    (void)add_layer(result,
                    LayerNode{0,
                              parent_layer_id,
                              LayerKind::Rect,
                              impl.object.object_id,
                              impl.object.generation,
                              impl.node.kind,
                              path,
                              bounds,
                              bounds,
                              bounds,
                              style,
                              {},
                              {},
                              {},
                              1.0f,
                              0.0f,
                              0.0f,
                              command_index,
                              1,
                              0},
                  parent_layer_id);
}

void RenderSystem::append_table_commands(const WidgetImpl& impl,
                                         RenderFrameResult& result,
                                         std::uint32_t parent_layer_id) const
{
    if (impl.dirty.bounds.width <= 0.0f || impl.dirty.bounds.height <= 0.0f) {
        return;
    }
    const Theme& theme = default_runtime().style_system().active_theme();
    const std::string path = widget_path(impl);
    const std::uint32_t column_count =
        static_cast<std::uint32_t>(impl.properties.table_columns.size());
    const std::uint32_t row_count =
        static_cast<std::uint32_t>(impl.properties.table_rows.size());
    if (column_count == 0) {
        return;
    }

    const float header_height =
        scaled(std::max(1.0f, impl.properties.table_header_height));
    const float row_height = scaled(std::max(1.0f, impl.properties.table_row_height));
    float fixed_width = 0.0f;
    std::uint32_t flexible_columns = 0;
    for (const TableColumnData& column : impl.properties.table_columns) {
        if (column.width > 0.0f) {
            fixed_width += scaled(column.width);
        } else {
            ++flexible_columns;
        }
    }
    const float remaining_width = std::max(0.0f, impl.dirty.bounds.width - fixed_width);
    const float flexible_width =
        flexible_columns == 0 ? 0.0f : remaining_width / static_cast<float>(flexible_columns);

    std::vector<float> column_widths;
    column_widths.reserve(column_count);
    for (const TableColumnData& column : impl.properties.table_columns) {
        column_widths.push_back(column.width > 0.0f ? scaled(column.width) : flexible_width);
    }

    DisplayStyle header_style = base_style(theme);
    header_style.fill = theme.colors.background;
    header_style.border = theme.colors.border;
    header_style.border_width = 0.0f;
    header_style.radius = 0.0f;
    const Rect header_bounds{impl.dirty.bounds.x,
                             impl.dirty.bounds.y,
                             impl.dirty.bounds.width,
                             std::min(header_height, impl.dirty.bounds.height)};
    const std::uint32_t header_command_index =
        add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                           impl.object.object_id,
                                           impl.node.kind,
                                           path + "/header",
                                           header_bounds,
                                           {},
                                           {},
                                           header_style,
                                           {}});
    (void)add_layer(result,
                    LayerNode{0,
                              parent_layer_id,
                              LayerKind::Rect,
                              impl.object.object_id,
                              impl.object.generation,
                              impl.node.kind,
                              path + "/header",
                              header_bounds,
                              header_bounds,
                              impl.dirty.clip_bounds,
                              header_style,
                              {},
                              {},
                              {},
                              1.0f,
                              0.0f,
                              0.0f,
                              header_command_index,
                              1,
                              0},
                    parent_layer_id);

    DisplayStyle text_style = base_style(theme);
    text_style.fill = Color{0, 0, 0, 0};
    text_style.text = theme.colors.text;
    text_style.font_size = scaled(13.0f);
    text_style.text_padding_x = scaled(8.0f);
    text_style.text_padding_y = scaled(2.0f);
    text_style.text_align = "leading";
    text_style.paragraph_align = "center";
    text_style.overflow = "ellipsis";
    const std::uint32_t dpi = default_runtime().platform_system().state().dpi;

    auto add_cell_text = [&](const std::string& cell_path,
                             Rect cell_bounds,
                             const std::string& text,
                             DisplayStyle cell_text_style,
                             std::uint32_t cell_parent_layer_id,
                             Rect cell_clip_bounds) {
        const Rect text_bounds{cell_bounds.x + cell_text_style.text_padding_x,
                               cell_bounds.y + cell_text_style.text_padding_y,
                               std::max(0.0f, cell_bounds.width -
                                                   cell_text_style.text_padding_x * 2.0f),
                               std::max(0.0f, cell_bounds.height -
                                                   cell_text_style.text_padding_y * 2.0f)};
        const std::string resource_key =
            std::string("table_cell:") + cell_path + ":" + text + ":font=" +
            std::to_string(cell_text_style.font_size) + ":theme=" +
            cell_text_style.theme_name + ":dpi=" + std::to_string(dpi);
        const ResourceId resource_id =
            default_runtime().resource_system().find_or_register_resource(
                ResourceKind::TextLayout, impl.object.object_id, path.c_str(),
                resource_key.c_str(), ResourceCacheState::Cached);
        const TextMetrics metrics = default_runtime().text_system().measure_text(
            text.c_str(), cell_text_style.font_size, text_bounds.width, text_bounds.height,
            dpi, impl.object.object_id, cell_path.c_str(), cell_text_style.word_wrap,
            cell_text_style.overflow);
        default_runtime().resource_system().update_text_metrics(resource_id, metrics);
        const ResourceRecord* record = default_runtime().resource_system().find(resource_id);
        const DisplayResource resource = resource_from_record(record);
        const std::uint32_t text_command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Text,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               cell_path,
                                               text_bounds,
                                               text,
                                               resource_key,
                                               cell_text_style,
                                               resource});
        (void)add_layer(result,
                        LayerNode{0,
                                  cell_parent_layer_id,
                                  LayerKind::Text,
                                  impl.object.object_id,
                                  impl.object.generation,
                                  impl.node.kind,
                                  cell_path,
                                  text_bounds,
                                  text_bounds,
                                  cell_clip_bounds,
                                  cell_text_style,
                                  resource,
                                  text,
                                  resource_key,
                                  1.0f,
                                  0.0f,
                                  0.0f,
                                  text_command_index,
                                  1,
                                  0},
                        cell_parent_layer_id);
    };

    float cursor_x = impl.dirty.bounds.x;
    for (std::uint32_t column = 0; column < column_count; ++column) {
        const float width = column_widths[column];
        DisplayStyle header_text_style = text_style;
        header_text_style.text = theme.colors.muted_text;
        header_text_style.font_size = scaled(12.0f);
        std::string header_label = impl.properties.table_columns[column].label;
        if (impl.properties.table_sort_column == column) {
            header_label += impl.properties.table_sort_ascending ? " ^" : " v";
            header_text_style.text = theme.colors.text;
        }
        add_cell_text(path + "/header/cell_" + std::to_string(column),
                      Rect{cursor_x, impl.dirty.bounds.y, width, header_height},
                      header_label, header_text_style, parent_layer_id, impl.dirty.clip_bounds);
        if (column + 1u < column_count) {
            DisplayStyle divider_style = base_style(theme);
            const ControlState state = control_state_for(impl);
            divider_style.fill =
                state == ControlState::Hover || state == ControlState::Pressed
                    ? theme.colors.accent
                    : theme.colors.border;
            divider_style.border = Color{0, 0, 0, 0};
            divider_style.border_width = 0.0f;
            divider_style.radius = 0.0f;
            const Rect divider_bounds{cursor_x + width - scaled(0.5f),
                                      impl.dirty.bounds.y + scaled(4.0f),
                                      scaled(1.0f),
                                      std::max(1.0f, header_height - scaled(8.0f))};
            const std::string divider_path =
                path + "/header/divider_" + std::to_string(column);
            const std::uint32_t divider_command_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   divider_path,
                                                   divider_bounds,
                                                   {},
                                                   {},
                                                   divider_style,
                                                   {}});
            (void)add_layer(result,
                            LayerNode{0,
                                      parent_layer_id,
                                      LayerKind::Rect,
                                      impl.object.object_id,
                                      impl.object.generation,
                                      impl.node.kind,
                                      divider_path,
                                      divider_bounds,
                                      divider_bounds,
                                      impl.dirty.clip_bounds,
                                      divider_style,
                                      {},
                                      {},
                                      {},
                                      1.0f,
                                      0.0f,
                                      0.0f,
                                      divider_command_index,
                                      1,
                                      0},
                            parent_layer_id);
        }
        cursor_x += width;
    }

    const Rect body_clip_bounds{impl.dirty.bounds.x,
                                impl.dirty.bounds.y + header_height,
                                impl.dirty.bounds.width,
                                std::max(0.0f, impl.dirty.bounds.height - header_height)};
    DisplayStyle body_clip_style = base_style(theme);
    body_clip_style.fill = Color{0, 0, 0, 0};
    body_clip_style.border = Color{0, 0, 0, 0};
    body_clip_style.border_width = 0.0f;
    body_clip_style.radius = 0.0f;
    const std::string body_clip_path = path + "/body_clip";
    const std::uint32_t body_clip_command_index =
        add_command(result, DisplayCommand{DisplayCommandKind::Clip,
                                           impl.object.object_id,
                                           impl.node.kind,
                                           body_clip_path,
                                           body_clip_bounds,
                                           {},
                                           {},
                                           body_clip_style,
                                           {}});
    const std::uint32_t body_layer_id =
        add_layer(result,
                  LayerNode{0,
                            parent_layer_id,
                            LayerKind::Clip,
                            impl.object.object_id,
                            impl.object.generation,
                            impl.node.kind,
                            body_clip_path,
                            body_clip_bounds,
                            body_clip_bounds,
                            body_clip_bounds,
                            body_clip_style,
                            {},
                            {},
                            {},
                            1.0f,
                            0.0f,
                            0.0f,
                            body_clip_command_index,
                            1,
                            0},
                  parent_layer_id);

    const float viewport_bottom = impl.dirty.bounds.y + impl.dirty.bounds.height;
    for (std::uint32_t row = 0; row < row_count; ++row) {
        const float row_y = impl.dirty.bounds.y + header_height +
                            row_height * static_cast<float>(row) -
                            impl.properties.scroll_offset_y;
        if (row_y + row_height <= impl.dirty.bounds.y + header_height) {
            continue;
        }
        if (row_y >= viewport_bottom) {
            break;
        }
        const Rect row_bounds{impl.dirty.bounds.x,
                              row_y,
                              impl.dirty.bounds.width,
                              std::min(row_height,
                                       viewport_bottom - row_y)};
        DisplayStyle row_style = base_style(theme);
        const bool selected = impl.properties.has_selected_index &&
                              impl.properties.selected_index == row;
        row_style.fill =
            selected ? theme.colors.accent
                     : (row % 2u == 0u ? Color{0, 0, 0, 0} : theme.colors.background);
        if (!selected && row_style.fill.a > 0) {
            row_style.fill.a = 90;
        }
        row_style.border = Color{0, 0, 0, 0};
        row_style.border_width = 0.0f;
        row_style.radius = 0.0f;
        const std::string row_path = path + "/row_" + std::to_string(row);
        const std::uint32_t row_command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               row_path,
                                               row_bounds,
                                               {},
                                               {},
                                               row_style,
                                               {}});
        (void)add_layer(result,
                        LayerNode{0,
                                  body_layer_id,
                                  LayerKind::Rect,
                                  impl.object.object_id,
                                  impl.object.generation,
                                  impl.node.kind,
                                  row_path,
                                  row_bounds,
                                  row_bounds,
                                  body_clip_bounds,
                                  row_style,
                                  {},
                                  {},
                                  {},
                                  1.0f,
                                  0.0f,
                                  0.0f,
                                  row_command_index,
                                  1,
                                  0},
                        body_layer_id);

        cursor_x = impl.dirty.bounds.x;
        for (std::uint32_t column = 0; column < column_count; ++column) {
            DisplayStyle cell_style = text_style;
            if (selected) {
                cell_style.text = solid_white();
            }
            const std::vector<std::string>& table_row = impl.properties.table_rows[row];
            const std::string cell_text =
                column < table_row.size() ? table_row[column] : std::string{};
            add_cell_text(row_path + "/cell_" + std::to_string(column),
                          Rect{cursor_x, row_y, column_widths[column], row_height},
                          cell_text, cell_style, body_layer_id, body_clip_bounds);
            cursor_x += column_widths[column];
        }
    }

    (void)add_command(result, DisplayCommand{DisplayCommandKind::ClipEnd,
                                             impl.object.object_id,
                                             impl.node.kind,
                                             body_clip_path,
                                             body_clip_bounds,
                                             {},
                                             {},
                                             body_clip_style,
                                             {}});

    const float viewport_height = body_clip_bounds.height;
    const float content_height = std::max(0.0f, impl.properties.scroll_content_height);
    if (content_height > viewport_height && viewport_height > 0.0f) {
        DisplayStyle thumb_style = base_style(theme);
        thumb_style.fill = theme.colors.muted_text;
        thumb_style.fill.a = impl.properties.scroll_offset_y > 0.0f ? 112 : 72;
        thumb_style.border = Color{0, 0, 0, 0};
        thumb_style.border_width = 0.0f;
        thumb_style.radius = scaled(2.0f);

        const float thumb_margin = scaled(8.0f);
        const float thumb_width = scaled(3.0f);
        const float track_height = std::max(1.0f, viewport_height - thumb_margin * 2.0f);
        const float thumb_height =
            std::max(scaled(24.0f),
                     track_height * std::min(1.0f, viewport_height / content_height));
        const float max_offset = std::max(1.0f, content_height - viewport_height);
        const float scroll_ratio =
            std::max(0.0f, std::min(1.0f, impl.properties.scroll_offset_y / max_offset));
        const float thumb_travel = std::max(0.0f, track_height - thumb_height);
        const Rect thumb_bounds{impl.dirty.bounds.x + impl.dirty.bounds.width - thumb_margin -
                                    thumb_width,
                                body_clip_bounds.y + thumb_margin + thumb_travel * scroll_ratio,
                                thumb_width,
                                thumb_height};
        const std::string thumb_path = path + "/scrollbar_thumb";
        const std::uint32_t thumb_command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               thumb_path,
                                               thumb_bounds,
                                               {},
                                               {},
                                               thumb_style,
                                               {}});
        (void)add_layer(result,
                        LayerNode{0,
                                  parent_layer_id,
                                  LayerKind::RoundedRect,
                                  impl.object.object_id,
                                  impl.object.generation,
                                  impl.node.kind,
                                  thumb_path,
                                  thumb_bounds,
                                  thumb_bounds,
                                  body_clip_bounds,
                                  thumb_style,
                                  {},
                                  {},
                                  {},
                                  1.0f,
                                  thumb_style.radius,
                                  0.0f,
                                  thumb_command_index,
                                  1,
                                  0},
                        parent_layer_id);
    }
}

void RenderSystem::append_node_close_commands(const WidgetImpl& impl,
                                              RenderFrameResult& result,
                                              std::uint32_t layer_id) const
{
    const Theme& theme = default_runtime().style_system().active_theme();
    const std::string path = widget_path(impl);
    if (impl.node.kind != WidgetKind::ScrollView) {
        if (clips_child_content(impl)) {
            const DisplayStyle clip_style = style_for_rect(impl, theme);
            (void)add_command(result, DisplayCommand{DisplayCommandKind::ClipEnd,
                                                     impl.object.object_id,
                                                     impl.node.kind,
                                                     path + "/overflow_clip",
                                                     impl.dirty.clip_bounds,
                                                     {},
                                                     {},
                                                     clip_style,
                                                     {}});
        }
        if (impl.node.kind == WidgetKind::Window) {
            (void)add_command(result, DisplayCommand{DisplayCommandKind::TransformEnd,
                                                     impl.object.object_id,
                                                     impl.node.kind,
                                                     path,
                                                     impl.dirty.bounds,
                                                     {},
                                                     {},
                                                     base_style(theme),
                                                     {}});
            (void)add_command(result, DisplayCommand{DisplayCommandKind::OpacityEnd,
                                                     impl.object.object_id,
                                                     impl.node.kind,
                                                     path,
                                                     impl.dirty.bounds,
                                                     {},
                                                     {},
                                                     base_style(theme),
                                                     {}});
        }
        if (impl.node.kind == WidgetKind::Image) {
            const DisplayStyle image_style = style_for_image(impl, theme);
            if (image_style.radius > 0.0f) {
                (void)add_command(result, DisplayCommand{DisplayCommandKind::RoundedClipEnd,
                                                         impl.object.object_id,
                                                         impl.node.kind,
                                                         path,
                                                         impl.dirty.bounds,
                                                         {},
                                                         {},
                                                         image_style,
                                                         {}});
            }
        }
        return;
    }

    const DisplayStyle clip_style = style_for_rect(impl, theme);
    const float viewport_height =
        impl.node.kind == WidgetKind::TableView
            ? std::max(0.0f, impl.dirty.bounds.height -
                                 scaled(std::max(1.0f, impl.properties.table_header_height)))
            : std::max(0.0f, impl.dirty.bounds.height);
    const float content_height = std::max(0.0f, impl.properties.scroll_content_height);
    if (content_height > viewport_height && viewport_height > 0.0f) {
        DisplayStyle thumb_style = base_style(theme);
        thumb_style.fill = theme.colors.muted_text;
        thumb_style.fill.a = impl.properties.scroll_offset_y > 0.0f ? 112 : 72;
        thumb_style.border = Color{0, 0, 0, 0};
        thumb_style.border_width = 0.0f;
        thumb_style.radius = scaled(2.0f);

        const float thumb_margin = scaled(8.0f);
        const float thumb_width = scaled(3.0f);
        const float track_y =
            impl.node.kind == WidgetKind::TableView
                ? impl.dirty.bounds.y + scaled(std::max(1.0f, impl.properties.table_header_height))
                : impl.dirty.bounds.y;
        const float track_height = std::max(1.0f, viewport_height - thumb_margin * 2.0f);
        const float thumb_height =
            std::max(scaled(24.0f),
                     track_height * std::min(1.0f, viewport_height / content_height));
        const float max_offset = std::max(1.0f, content_height - viewport_height);
        const float scroll_ratio =
            std::max(0.0f, std::min(1.0f, impl.properties.scroll_offset_y / max_offset));
        const float thumb_travel = std::max(0.0f, track_height - thumb_height);
        const Rect thumb_bounds{impl.dirty.bounds.x + impl.dirty.bounds.width - thumb_margin -
                                    thumb_width,
                                track_y + thumb_margin + thumb_travel * scroll_ratio,
                                thumb_width,
                                thumb_height};
        const std::string thumb_path = path + "/scrollbar_thumb";
        const std::uint32_t thumb_command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               thumb_path,
                                               thumb_bounds,
                                               {},
                                               {},
                                               thumb_style,
                                               {}});
        (void)add_layer(result,
                        LayerNode{0,
                                  layer_id,
                                  LayerKind::RoundedRect,
                                  impl.object.object_id,
                                  impl.object.generation,
                                  impl.node.kind,
                                  thumb_path,
                                  thumb_bounds,
                                  thumb_bounds,
                                  impl.dirty.clip_bounds,
                                  thumb_style,
                                  {},
                                  {},
                                  {},
                                  1.0f,
                                  thumb_style.radius,
                                  0.0f,
                                  thumb_command_index,
                                  1,
                                  0},
                        layer_id);
    }
    const DisplayCommandKind clip_end_kind =
        clip_style.radius > 0.0f ? DisplayCommandKind::RoundedClipEnd
                                 : DisplayCommandKind::ClipEnd;
    (void)add_command(result, DisplayCommand{clip_end_kind,
                                             impl.object.object_id,
                                             impl.node.kind,
                                             path,
                                             impl.dirty.clip_bounds,
                                             {},
                                             {},
                                             clip_style,
                                             {}});
    (void)layer_id;
}

std::uint32_t RenderSystem::append_layers_and_commands(const WidgetImpl& impl,
                                                       RenderFrameResult& result,
                                                       std::uint32_t parent_layer_id) const
{
    if (!impl.properties.visible) {
        return parent_layer_id;
    }
    const Theme& theme = default_runtime().style_system().active_theme();
    const std::string path = widget_path(impl);
    std::uint32_t node_layer_id = parent_layer_id;
    if (clips_child_content(impl)) {
        const DisplayStyle clip_style = style_for_rect(impl, theme);
        const std::uint32_t clip_command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Clip,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               path + "/overflow_clip",
                                               impl.dirty.clip_bounds,
                                               {},
                                               {},
                                               clip_style,
                                               {}});
        node_layer_id =
            add_layer(result,
                      LayerNode{0,
                                node_layer_id,
                                LayerKind::Clip,
                                impl.object.object_id,
                                impl.object.generation,
                                impl.node.kind,
                                path + "/overflow_clip",
                                impl.dirty.clip_bounds,
                                impl.dirty.paint_bounds,
                                impl.dirty.clip_bounds,
                                clip_style,
                                {},
                                {},
                                {},
                                1.0f,
                                0.0f,
                                0.0f,
                                clip_command_index,
                                1,
                                0},
                      node_layer_id);
    }
    if (impl.node.kind == WidgetKind::Window) {
        node_layer_id =
            add_layer(result,
                      LayerNode{0,
                                parent_layer_id,
                                LayerKind::Root,
                                impl.object.object_id,
                                impl.object.generation,
                                impl.node.kind,
                                path,
                                impl.dirty.bounds,
                                impl.dirty.paint_bounds,
                                impl.dirty.clip_bounds,
                                base_style(theme),
                                {},
                                {},
                                {},
                                1.0f,
                                0.0f,
                                0.0f,
                                invalid_display_command_index,
                                0,
                                0},
                      parent_layer_id);
        const std::uint32_t opacity_command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Opacity,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               path,
                                               impl.dirty.bounds,
                                               {},
                                               {},
                                               base_style(theme),
                                               {},
                                               0.0f,
                                               1.0f});
        const std::uint32_t opacity_layer_id =
            add_layer(result,
                      LayerNode{0,
                                node_layer_id,
                                LayerKind::Opacity,
                                impl.object.object_id,
                                impl.object.generation,
                                impl.node.kind,
                                path,
                                impl.dirty.bounds,
                                impl.dirty.paint_bounds,
                                impl.dirty.clip_bounds,
                                base_style(theme),
                                {},
                                {},
                                {},
                                1.0f,
                                0.0f,
                                0.0f,
                                opacity_command_index,
                                1,
                                0},
                      node_layer_id);
        const std::uint32_t transform_command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Transform,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               path,
                                               impl.dirty.bounds,
                                               {},
                                               {},
                                               base_style(theme),
                                               {}});
        node_layer_id =
            add_layer(result,
                      LayerNode{0,
                                opacity_layer_id,
                                LayerKind::Transform,
                                impl.object.object_id,
                                impl.object.generation,
                                impl.node.kind,
                                path,
                                impl.dirty.bounds,
                                impl.dirty.paint_bounds,
                                impl.dirty.clip_bounds,
                                base_style(theme),
                                {},
                                {},
                                {},
                                1.0f,
                                0.0f,
                                0.0f,
                                transform_command_index,
                                1,
                                0},
                      opacity_layer_id);
    }

    if (impl.node.kind == WidgetKind::ScrollView) {
        const DisplayStyle clip_style = style_for_rect(impl, theme);
        const std::uint32_t scroll_layer_id =
            add_layer(result,
                      LayerNode{0,
                                node_layer_id,
                                LayerKind::Scroll,
                                impl.object.object_id,
                                impl.object.generation,
                                impl.node.kind,
                                path,
                                impl.dirty.bounds,
                                impl.dirty.paint_bounds,
                                impl.dirty.clip_bounds,
                                base_style(theme),
                                {},
                                {},
                                {},
                                1.0f,
                                0.0f,
                                0.0f,
                                invalid_display_command_index,
                                0,
                                0},
                      node_layer_id);
        const DisplayCommandKind clip_kind =
            clip_style.radius > 0.0f ? DisplayCommandKind::RoundedClip : DisplayCommandKind::Clip;
        const std::uint32_t command_index =
            add_command(result, DisplayCommand{clip_kind,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               path,
                                               impl.dirty.clip_bounds,
                                               {},
                                               {},
                                               clip_style,
                                               {}});
        const std::uint32_t clip_layer_id =
            add_layer(result,
                      LayerNode{0,
                                scroll_layer_id,
                                LayerKind::Clip,
                                impl.object.object_id,
                                impl.object.generation,
                                impl.node.kind,
                                path,
                                impl.dirty.clip_bounds,
                                impl.dirty.paint_bounds,
                                impl.dirty.clip_bounds,
                                clip_style,
                                {},
                                {},
                                {},
                                1.0f,
                                clip_style.radius,
                                0.0f,
                                command_index,
                                1,
                                0},
                      scroll_layer_id);
        node_layer_id = clip_layer_id;
    }

    if (has_surface_command(impl)) {
        const DisplayStyle rect_style = style_for_rect(impl, theme);
        if (rect_style.radius > 0.0f && impl.node.kind != WidgetKind::Window &&
            (rect_style.fill.a > 0 || rect_style.border_width > 0.0f)) {
            const std::uint32_t shadow_command_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Shadow,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   path,
                                                   impl.dirty.paint_bounds,
                                                   {},
                                                   {},
                                                   rect_style,
                                                   {},
                                                   scaled(theme.shadow.sm_blur)});
            const std::uint32_t shadow_layer_id =
                add_layer(result,
                          LayerNode{0,
                                    node_layer_id,
                                    LayerKind::Shadow,
                                    impl.object.object_id,
                                    impl.object.generation,
                                    impl.node.kind,
                                    path,
                                    impl.dirty.paint_bounds,
                                    impl.dirty.paint_bounds,
                                    impl.dirty.clip_bounds,
                                    rect_style,
                                    {},
                                    {},
                                    {},
                                    1.0f,
                                    rect_style.radius,
                                    scaled(theme.shadow.sm_blur),
                                    shadow_command_index,
                                    1,
                                    0},
                          node_layer_id);
            (void)shadow_layer_id;
        }
        const std::uint32_t command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               path,
                                               impl.dirty.bounds,
                                               {},
                                               {},
                                               rect_style,
                                               {}});
        const LayerKind layer_kind =
            rect_style.radius > 0.0f ? LayerKind::RoundedRect : LayerKind::Rect;
        node_layer_id =
            add_layer(result,
                      LayerNode{0,
                                node_layer_id,
                                layer_kind,
                                impl.object.object_id,
                                impl.object.generation,
                                impl.node.kind,
                                path,
                                impl.dirty.bounds,
                                impl.dirty.paint_bounds,
                                impl.dirty.clip_bounds,
                                rect_style,
                                {},
                                {},
                                {},
                                1.0f,
                                rect_style.radius,
                                0.0f,
                                command_index,
                                1,
                                0},
                      node_layer_id);

        if (impl.node.kind == WidgetKind::Button) {
            const ControlState state = control_state_for(impl);
            std::uint64_t image_resource_id = impl.properties.button_normal_image_resource_id;
            const std::string* image_path = &impl.properties.button_normal_image_path;
            if (state == ControlState::Pressed &&
                impl.properties.button_pressed_image_resource_id != 0) {
                image_resource_id = impl.properties.button_pressed_image_resource_id;
                image_path = &impl.properties.button_pressed_image_path;
            } else if (state == ControlState::Hover &&
                       impl.properties.button_hover_image_resource_id != 0) {
                image_resource_id = impl.properties.button_hover_image_resource_id;
                image_path = &impl.properties.button_hover_image_path;
            }
            if (image_resource_id != 0 && image_path != nullptr && !image_path->empty()) {
                const ImageMetadata metadata = default_runtime().image_system().query_metadata(
                    image_path->c_str(), impl.dirty.bounds.width, impl.dirty.bounds.height,
                    impl.object.object_id, path.c_str());
                default_runtime().resource_system().update_image_metadata(image_resource_id,
                                                                          metadata);
                const ResourceRecord* image_record =
                    default_runtime().resource_system().find(image_resource_id);
                const DisplayResource resource = resource_from_record(image_record);
                const Rect image_bounds = image_bounds_for(impl, resource, impl.dirty.bounds);
                const Rect image_uv = image_uv_for(impl, resource, impl.dirty.bounds);
                DisplayCommand image_command;
                image_command.kind = DisplayCommandKind::Image;
                image_command.object_id = impl.object.object_id;
                image_command.widget_kind = impl.node.kind;
                image_command.path = path + "/state_image";
                image_command.bounds = image_bounds;
                image_command.resource_key = *image_path;
                image_command.style = rect_style;
                image_command.resource = resource;
                image_command.image_fit = impl.properties.image_fit;
                image_command.image_uv = image_uv;
                const std::uint32_t image_command_index =
                    add_command(result, std::move(image_command));
                node_layer_id =
                    add_layer(result,
                              LayerNode{0,
                                        node_layer_id,
                                        LayerKind::Image,
                                        impl.object.object_id,
                                        impl.object.generation,
                                        impl.node.kind,
                                        path + "/state_image",
                                        image_bounds,
                                        impl.dirty.paint_bounds,
                                        impl.dirty.clip_bounds,
                                        rect_style,
                                        resource,
                                        {},
                                        *image_path,
                                        1.0f,
                                        rect_style.radius,
                                        0.0f,
                                        image_command_index,
                                        1,
                                        0},
                              node_layer_id);
            }
        }

        if (impl.node.kind == WidgetKind::Tabs) {
            const float header_height = scaled(38.0f);
            const std::uint32_t tab_count =
                static_cast<std::uint32_t>(impl.properties.tab_labels.size());
            const float tab_width = tab_count == 0 ? impl.dirty.bounds.width
                                                   : impl.dirty.bounds.width /
                                                         static_cast<float>(tab_count);
            DisplayStyle header_style = base_style(theme);
            header_style.fill = theme.colors.surface;
            header_style.border = theme.colors.border;
            header_style.border_width = scaled(1.0f);
            header_style.radius = scaled(theme.radius.md);
            const Rect header_bounds{impl.dirty.bounds.x, impl.dirty.bounds.y,
                                     impl.dirty.bounds.width, header_height};
            const std::string header_path = path + "/header";
            const std::uint32_t header_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   header_path,
                                                   header_bounds,
                                                   {},
                                                   {},
                                                   header_style,
                                                   {}});
            (void)add_layer(result,
                            LayerNode{0,
                                      node_layer_id,
                                      LayerKind::RoundedRect,
                                      impl.object.object_id,
                                      impl.object.generation,
                                      impl.node.kind,
                                      header_path,
                                      header_bounds,
                                      header_bounds,
                                      impl.dirty.clip_bounds,
                                      header_style,
                                      {},
                                      {},
                                      {},
                                      1.0f,
                                      header_style.radius,
                                      0.0f,
                                      header_index,
                                      1,
                                      0},
                            node_layer_id);
            for (std::uint32_t index = 0; index < tab_count; ++index) {
                const bool active = index == impl.properties.selected_tab_index;
                const float tab_x = impl.dirty.bounds.x + tab_width * static_cast<float>(index);
                const Rect tab_bounds{tab_x, impl.dirty.bounds.y, tab_width, header_height};
                DisplayStyle tab_style = base_style(theme);
                tab_style.fill = active ? theme.colors.background : theme.colors.surface;
                tab_style.border = Color{0, 0, 0, 0};
                tab_style.border_width = 0.0f;
                tab_style.text = active ? theme.colors.text : theme.colors.muted_text;
                tab_style.font_size = scaled(theme.typography.body);
                tab_style.text_align = "center";
                tab_style.paragraph_align = "center";
                tab_style.overflow = "ellipsis";
                const std::string tab_path = path + "/tab_" + std::to_string(index);
                const std::uint32_t tab_index =
                    add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                       impl.object.object_id,
                                                       impl.node.kind,
                                                       tab_path,
                                                       tab_bounds,
                                                       {},
                                                       {},
                                                       tab_style,
                                                       {}});
                (void)add_layer(result,
                                LayerNode{0,
                                          node_layer_id,
                                          LayerKind::Rect,
                                          impl.object.object_id,
                                          impl.object.generation,
                                          impl.node.kind,
                                          tab_path,
                                          tab_bounds,
                                          tab_bounds,
                                          impl.dirty.clip_bounds,
                                          tab_style,
                                          {},
                                          {},
                                          {},
                                          1.0f,
                                          0.0f,
                                          0.0f,
                                          tab_index,
                                          1,
                                          0},
                                node_layer_id);
                const Rect label_bounds{tab_bounds.x, tab_bounds.y, tab_bounds.width,
                                        tab_bounds.height};
                const std::uint32_t label_command_index =
                    add_command(result, DisplayCommand{DisplayCommandKind::Text,
                                                       impl.object.object_id,
                                                       impl.node.kind,
                                                       tab_path + "/label",
                                                       label_bounds,
                                                       index < impl.properties.tab_labels.size()
                                                           ? impl.properties.tab_labels[index]
                                                           : "",
                                                       {},
                                                       tab_style,
                                                       {}});
                (void)add_layer(result,
                                LayerNode{0,
                                          node_layer_id,
                                          LayerKind::Text,
                                          impl.object.object_id,
                                          impl.object.generation,
                                          impl.node.kind,
                                          tab_path + "/label",
                                          label_bounds,
                                          label_bounds,
                                          impl.dirty.clip_bounds,
                                          tab_style,
                                          {},
                                          index < impl.properties.tab_labels.size()
                                              ? impl.properties.tab_labels[index]
                                              : "",
                                          {},
                                          1.0f,
                                          0.0f,
                                          0.0f,
                                          label_command_index,
                                          1,
                                          0},
                                node_layer_id);
                if (active) {
                    DisplayStyle underline_style = base_style(theme);
                    underline_style.fill = theme.colors.accent;
                    underline_style.border = Color{0, 0, 0, 0};
                    underline_style.border_width = 0.0f;
                    const Rect underline_bounds{tab_bounds.x, tab_bounds.y + header_height - 2.0f,
                                                tab_bounds.width, 2.0f};
                    const std::uint32_t underline_index =
                        add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                           impl.object.object_id,
                                                           impl.node.kind,
                                                           tab_path + "/underline",
                                                           underline_bounds,
                                                           {},
                                                           {},
                                                           underline_style,
                                                           {}});
                    (void)add_layer(result,
                                    LayerNode{0,
                                              node_layer_id,
                                              LayerKind::Rect,
                                              impl.object.object_id,
                                              impl.object.generation,
                                              impl.node.kind,
                                              tab_path + "/underline",
                                              underline_bounds,
                                              underline_bounds,
                                              impl.dirty.clip_bounds,
                                              underline_style,
                                              {},
                                              {},
                                              {},
                                              1.0f,
                                              0.0f,
                                              0.0f,
                                              underline_index,
                                              1,
                                              0},
                                    node_layer_id);
                }
            }
        }

        if (impl.node.kind == WidgetKind::CheckBox) {
            const ControlState state = control_state_for(impl);
            DisplayStyle box_style = base_style(theme);
            box_style.control_state = control_state_name(state);
            box_style.fill = impl.properties.checked ? theme.colors.accent : theme.colors.surface;
            box_style.border = state == ControlState::Hover || state == ControlState::Focused
                                   ? theme.colors.accent
                                   : theme.colors.border;
            if (is_disabled(impl)) {
                box_style.fill = theme.colors.surface;
                box_style.border = theme.colors.border;
            }
            box_style.border_width = scaled(1.0f);
            box_style.radius = scaled(theme.radius.sm);

            const float box_size = scaled(18.0f);
            const Rect box_bounds{impl.dirty.bounds.x + scaled(6.0f),
                                  impl.dirty.bounds.y +
                                      std::max(0.0f, (impl.dirty.bounds.height - box_size) * 0.5f),
                                  box_size,
                                  box_size};
            const std::string box_path = path + "/box";
            const std::uint32_t box_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   box_path,
                                                   box_bounds,
                                                   {},
                                                   {},
                                                   box_style,
                                                   {}});
            (void)add_layer(result,
                            LayerNode{0,
                                      node_layer_id,
                                      LayerKind::RoundedRect,
                                      impl.object.object_id,
                                      impl.object.generation,
                                      impl.node.kind,
                                      box_path,
                                      box_bounds,
                                      box_bounds,
                                      impl.dirty.clip_bounds,
                                      box_style,
                                      {},
                                      {},
                                      {},
                                      1.0f,
                                      box_style.radius,
                                      0.0f,
                                      box_index,
                                      1,
                                      0},
                            node_layer_id);

            if (impl.properties.checked) {
                DisplayStyle mark_style = base_style(theme);
                mark_style.fill = solid_white();
                mark_style.border = Color{0, 0, 0, 0};
                mark_style.border_width = 0.0f;
                mark_style.radius = scaled(1.0f);
                const Rect mark_bounds{box_bounds.x + scaled(4.0f),
                                       box_bounds.y + scaled(5.0f),
                                       scaled(10.0f),
                                       scaled(8.0f)};
                const std::string mark_path = path + "/mark";
                const std::uint32_t mark_index =
                    add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                       impl.object.object_id,
                                                       impl.node.kind,
                                                       mark_path,
                                                       mark_bounds,
                                                       {},
                                                       {},
                                                       mark_style,
                                                       {}});
                (void)add_layer(result,
                                LayerNode{0,
                                          node_layer_id,
                                          LayerKind::Rect,
                                          impl.object.object_id,
                                          impl.object.generation,
                                          impl.node.kind,
                                          mark_path,
                                          mark_bounds,
                                          mark_bounds,
                                          impl.dirty.clip_bounds,
                                          mark_style,
                                          {},
                                          {},
                                          {},
                                          1.0f,
                                          mark_style.radius,
                                          0.0f,
                                          mark_index,
                                          1,
                                          0},
                                node_layer_id);
            }
        }

        if (impl.node.kind == WidgetKind::RadioButton) {
            const ControlState state = control_state_for(impl);
            DisplayStyle outer_style = base_style(theme);
            outer_style.control_state = control_state_name(state);
            outer_style.fill = theme.colors.surface;
            outer_style.border = state == ControlState::Hover || state == ControlState::Focused
                                     ? theme.colors.accent
                                     : theme.colors.border;
            if (is_disabled(impl)) {
                outer_style.border = theme.colors.border;
            }
            outer_style.border_width = scaled(1.0f);
            const float outer_size = scaled(18.0f);
            outer_style.radius = outer_size * 0.5f;

            const Rect outer_bounds{impl.dirty.bounds.x + scaled(6.0f),
                                    impl.dirty.bounds.y +
                                        std::max(0.0f,
                                                 (impl.dirty.bounds.height - outer_size) * 0.5f),
                                    outer_size,
                                    outer_size};
            const std::string outer_path = path + "/outer";
            const std::uint32_t outer_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   outer_path,
                                                   outer_bounds,
                                                   {},
                                                   {},
                                                   outer_style,
                                                   {}});
            (void)add_layer(result,
                            LayerNode{0,
                                      node_layer_id,
                                      LayerKind::RoundedRect,
                                      impl.object.object_id,
                                      impl.object.generation,
                                      impl.node.kind,
                                      outer_path,
                                      outer_bounds,
                                      outer_bounds,
                                      impl.dirty.clip_bounds,
                                      outer_style,
                                      {},
                                      {},
                                      {},
                                      1.0f,
                                      outer_style.radius,
                                      0.0f,
                                      outer_index,
                                      1,
                                      0},
                            node_layer_id);

            if (impl.properties.checked) {
                DisplayStyle dot_style = base_style(theme);
                dot_style.fill = is_disabled(impl) ? theme.colors.muted_text : theme.colors.accent;
                dot_style.border = Color{0, 0, 0, 0};
                dot_style.border_width = 0.0f;
                const float dot_size = scaled(8.0f);
                dot_style.radius = dot_size * 0.5f;
                const Rect dot_bounds{outer_bounds.x + (outer_bounds.width - dot_size) * 0.5f,
                                      outer_bounds.y + (outer_bounds.height - dot_size) * 0.5f,
                                      dot_size,
                                      dot_size};
                const std::string dot_path = path + "/dot";
                const std::uint32_t dot_index =
                    add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                       impl.object.object_id,
                                                       impl.node.kind,
                                                       dot_path,
                                                       dot_bounds,
                                                       {},
                                                       {},
                                                       dot_style,
                                                       {}});
                (void)add_layer(result,
                                LayerNode{0,
                                          node_layer_id,
                                          LayerKind::RoundedRect,
                                          impl.object.object_id,
                                          impl.object.generation,
                                          impl.node.kind,
                                          dot_path,
                                          dot_bounds,
                                          dot_bounds,
                                          impl.dirty.clip_bounds,
                                          dot_style,
                                          {},
                                          {},
                                          {},
                                          1.0f,
                                          dot_style.radius,
                                          0.0f,
                                          dot_index,
                                          1,
                                          0},
                                node_layer_id);
            }
        }

        if (impl.node.kind == WidgetKind::Switch) {
            const ControlState state = control_state_for(impl);
            const float track_width = scaled(42.0f);
            const float track_height = scaled(22.0f);
            const Rect track_bounds{impl.dirty.bounds.x + scaled(6.0f),
                                    impl.dirty.bounds.y +
                                        std::max(0.0f,
                                                 (impl.dirty.bounds.height - track_height) * 0.5f),
                                    track_width,
                                    track_height};

            DisplayStyle track_style = base_style(theme);
            track_style.control_state = control_state_name(state);
            track_style.fill = impl.properties.checked ? theme.colors.accent : theme.colors.border;
            if (!impl.properties.checked) {
                track_style.fill.a = 180;
            }
            if (is_disabled(impl)) {
                track_style.fill = theme.colors.border;
                track_style.fill.a = 150;
            }
            track_style.border = state == ControlState::Hover || state == ControlState::Focused
                                     ? theme.colors.accent
                                     : Color{0, 0, 0, 0};
            track_style.border_width =
                state == ControlState::Hover || state == ControlState::Focused ? scaled(1.0f)
                                                                                : 0.0f;
            track_style.radius = track_height * 0.5f;
            const std::string track_path = path + "/track";
            const std::uint32_t track_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   track_path,
                                                   track_bounds,
                                                   {},
                                                   {},
                                                   track_style,
                                                   {}});
            (void)add_layer(result,
                            LayerNode{0,
                                      node_layer_id,
                                      LayerKind::RoundedRect,
                                      impl.object.object_id,
                                      impl.object.generation,
                                      impl.node.kind,
                                      track_path,
                                      track_bounds,
                                      track_bounds,
                                      impl.dirty.clip_bounds,
                                      track_style,
                                      {},
                                      {},
                                      {},
                                      1.0f,
                                      track_style.radius,
                                      0.0f,
                                      track_index,
                                      1,
                                      0},
                            node_layer_id);

            const float thumb_size = scaled(18.0f);
            const float thumb_x =
                impl.properties.checked
                    ? track_bounds.x + track_bounds.width - thumb_size - scaled(2.0f)
                    : track_bounds.x + scaled(2.0f);
            const Rect thumb_bounds{thumb_x,
                                    track_bounds.y + (track_bounds.height - thumb_size) * 0.5f,
                                    thumb_size,
                                    thumb_size};
            DisplayStyle thumb_style = base_style(theme);
            thumb_style.control_state = control_state_name(state);
            thumb_style.fill = solid_white();
            thumb_style.border = Color{0, 0, 0, 0};
            thumb_style.border_width = 0.0f;
            thumb_style.radius = thumb_size * 0.5f;
            const std::string thumb_path = path + "/thumb";
            const std::uint32_t thumb_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   thumb_path,
                                                   thumb_bounds,
                                                   {},
                                                   {},
                                                   thumb_style,
                                                   {}});
            (void)add_layer(result,
                            LayerNode{0,
                                      node_layer_id,
                                      LayerKind::RoundedRect,
                                      impl.object.object_id,
                                      impl.object.generation,
                                      impl.node.kind,
                                      thumb_path,
                                      thumb_bounds,
                                      thumb_bounds,
                                      impl.dirty.clip_bounds,
                                      thumb_style,
                                      {},
                                      {},
                                      {},
                                      1.0f,
                                      thumb_style.radius,
                                      0.0f,
                                      thumb_index,
                                      1,
                                      0},
                            node_layer_id);
        }

        if (impl.node.kind == WidgetKind::Slider) {
            const ControlState state = control_state_for(impl);
            const float value = std::max(0.0f, std::min(1.0f, impl.properties.numeric_value));
            const float inset = scaled(10.0f);
            const float track_height = scaled(4.0f);
            const float thumb_size = scaled(16.0f);
            const float track_left = impl.dirty.bounds.x + inset;
            const float track_width = std::max(1.0f, impl.dirty.bounds.width - inset * 2.0f);
            const float track_y =
                impl.dirty.bounds.y + std::max(0.0f, (impl.dirty.bounds.height - track_height) *
                                                        0.5f);
            const Rect track_bounds{track_left, track_y, track_width, track_height};
            const Rect fill_bounds{track_left, track_y, track_width * value, track_height};
            const float thumb_center_x = track_left + track_width * value;
            const Rect thumb_bounds{thumb_center_x - thumb_size * 0.5f,
                                    impl.dirty.bounds.y +
                                        std::max(0.0f, (impl.dirty.bounds.height - thumb_size) *
                                                        0.5f),
                                    thumb_size,
                                    thumb_size};

            DisplayStyle track_style = base_style(theme);
            track_style.control_state = control_state_name(state);
            track_style.fill = theme.colors.border;
            track_style.border = Color{0, 0, 0, 0};
            track_style.border_width = 0.0f;
            track_style.radius = scaled(2.0f);
            const std::string track_path = path + "/track";
            const std::uint32_t track_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   track_path,
                                                   track_bounds,
                                                   {},
                                                   {},
                                                   track_style,
                                                   {}});
            (void)add_layer(result,
                            LayerNode{0,
                                      node_layer_id,
                                      LayerKind::RoundedRect,
                                      impl.object.object_id,
                                      impl.object.generation,
                                      impl.node.kind,
                                      track_path,
                                      track_bounds,
                                      track_bounds,
                                      impl.dirty.clip_bounds,
                                      track_style,
                                      {},
                                      {},
                                      {},
                                      1.0f,
                                      track_style.radius,
                                      0.0f,
                                      track_index,
                                      1,
                                      0},
                            node_layer_id);

            DisplayStyle fill_style = track_style;
            fill_style.fill = theme.colors.accent;
            const std::string fill_path = path + "/fill";
            const std::uint32_t fill_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   fill_path,
                                                   fill_bounds,
                                                   {},
                                                   {},
                                                   fill_style,
                                                   {}});
            (void)add_layer(result,
                            LayerNode{0,
                                      node_layer_id,
                                      LayerKind::RoundedRect,
                                      impl.object.object_id,
                                      impl.object.generation,
                                      impl.node.kind,
                                      fill_path,
                                      fill_bounds,
                                      fill_bounds,
                                      impl.dirty.clip_bounds,
                                      fill_style,
                                      {},
                                      {},
                                      {},
                                      1.0f,
                                      fill_style.radius,
                                      0.0f,
                                      fill_index,
                                      1,
                                      0},
                            node_layer_id);

            DisplayStyle thumb_style = base_style(theme);
            thumb_style.control_state = control_state_name(state);
            thumb_style.fill = theme.colors.accent;
            thumb_style.border = state == ControlState::Hover || state == ControlState::Focused ||
                                         state == ControlState::Pressed
                                     ? solid_white()
                                     : theme.colors.surface;
            thumb_style.border_width = scaled(2.0f);
            thumb_style.radius = thumb_size * 0.5f;
            const std::string thumb_path = path + "/thumb";
            const std::uint32_t thumb_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   thumb_path,
                                                   thumb_bounds,
                                                   {},
                                                   {},
                                                   thumb_style,
                                                   {}});
            (void)add_layer(result,
                            LayerNode{0,
                                      node_layer_id,
                                      LayerKind::RoundedRect,
                                      impl.object.object_id,
                                      impl.object.generation,
                                      impl.node.kind,
                                      thumb_path,
                                      thumb_bounds,
                                      thumb_bounds,
                                      impl.dirty.clip_bounds,
                                      thumb_style,
                                      {},
                                      {},
                                      {},
                                      1.0f,
                                      thumb_style.radius,
                                      0.0f,
                                      thumb_index,
                                      1,
                                      0},
                            node_layer_id);
        }
    }

    if (impl.node.kind == WidgetKind::Image) {
        const DisplayStyle image_style = style_for_image(impl, theme);
        if (impl.properties.resource_path.empty()) {
            const DisplayStyle placeholder_style = style_for_image_placeholder(impl, theme);
            const std::uint32_t placeholder_command_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   path + "/placeholder",
                                                   impl.dirty.bounds,
                                                   {},
                                                   {},
                                                   placeholder_style,
                                                   {}});
            const LayerKind placeholder_layer_kind =
                placeholder_style.radius > 0.0f ? LayerKind::RoundedRect : LayerKind::Rect;
            node_layer_id =
                add_layer(result,
                          LayerNode{0,
                                    node_layer_id,
                                    placeholder_layer_kind,
                                    impl.object.object_id,
                                    impl.object.generation,
                                    impl.node.kind,
                                    path + "/placeholder",
                                    impl.dirty.bounds,
                                    impl.dirty.paint_bounds,
                                    impl.dirty.clip_bounds,
                                    placeholder_style,
                                    {},
                                    {},
                                    {},
                                    1.0f,
                                    placeholder_style.radius,
                                    0.0f,
                                    placeholder_command_index,
                                    1,
                                    0},
                          node_layer_id);
        }
        if (image_style.radius > 0.0f) {
            const std::uint32_t clip_command_index =
                add_command(result, DisplayCommand{DisplayCommandKind::RoundedClip,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   path,
                                                   impl.dirty.bounds,
                                                   {},
                                                   {},
                                                   image_style,
                                                   {}});
            node_layer_id =
                add_layer(result,
                          LayerNode{0,
                                    node_layer_id,
                                    LayerKind::Clip,
                                    impl.object.object_id,
                                    impl.object.generation,
                                    impl.node.kind,
                                    path,
                                    impl.dirty.bounds,
                                    impl.dirty.paint_bounds,
                                    impl.dirty.clip_bounds,
                                    image_style,
                                    {},
                                    {},
                                    {},
                                    1.0f,
                                    image_style.radius,
                                    0.0f,
                                    clip_command_index,
                                    1,
                                    0},
                          node_layer_id);
        }

        if (impl.properties.image_resource_id != 0) {
            const ImageMetadata metadata = default_runtime().image_system().query_metadata(
                impl.properties.resource_path.c_str(), impl.dirty.bounds.width,
                impl.dirty.bounds.height, impl.object.object_id, path.c_str());
            default_runtime().resource_system().update_image_metadata(
                impl.properties.image_resource_id, metadata);
        }
        const ResourceRecord* image_record =
            default_runtime().resource_system().find(impl.properties.image_resource_id);
        const DisplayResource resource = resource_from_record(image_record);
        const Rect image_bounds = image_bounds_for(impl, resource);
        const Rect image_uv = image_uv_for(impl, resource);
        DisplayCommand image_command;
        image_command.kind = DisplayCommandKind::Image;
        image_command.object_id = impl.object.object_id;
        image_command.widget_kind = impl.node.kind;
        image_command.path = path;
        image_command.bounds = image_bounds;
        image_command.resource_key = impl.properties.resource_path;
        image_command.style = image_style;
        image_command.resource = resource;
        image_command.image_fit = impl.properties.image_fit;
        image_command.image_uv = image_uv;
        const std::uint32_t command_index =
            add_command(result, std::move(image_command));
        node_layer_id =
            add_layer(result,
                      LayerNode{0,
                                node_layer_id,
                                LayerKind::Image,
                                impl.object.object_id,
                                impl.object.generation,
                                impl.node.kind,
                                path,
                                image_bounds,
                                impl.dirty.paint_bounds,
                                impl.dirty.clip_bounds,
                                image_style,
                                resource,
                                {},
                                impl.properties.resource_path,
                                1.0f,
                                image_style.radius,
                                0.0f,
                                command_index,
                                1,
                                0},
                      node_layer_id);
    }

    if (has_text_command(impl)) {
        const DisplayStyle text_style = style_for_text(impl, theme);
        const Rect text_bounds = text_bounds_for(impl, text_style);
        const std::uint32_t dpi = default_runtime().platform_system().state().dpi;
        const std::string resource_key = text_resource_key(impl, text_style, dpi);
        const ResourceId text_resource_id =
            default_runtime().resource_system().find_or_register_resource(
                ResourceKind::TextLayout, impl.object.object_id, widget_path(impl).c_str(),
                resource_key.c_str(), ResourceCacheState::Cached);
        const std::string text = text_for(impl);
        const TextMetrics text_metrics = default_runtime().text_system().measure_text(
            text.c_str(), text_style.font_size, text_bounds.width, text_bounds.height,
            dpi, impl.object.object_id, path.c_str(), text_style.word_wrap,
            text_style.overflow);
        default_runtime().resource_system().update_text_metrics(text_resource_id, text_metrics);
        const ResourceRecord* text_record =
            default_runtime().resource_system().find(text_resource_id);
        const DisplayResource resource = resource_from_record(text_record);
        if (has_input_selection(impl)) {
            DisplayStyle selection_style = base_style(theme);
            selection_style.fill = theme.colors.accent;
            selection_style.fill.a = 90;
            selection_style.border = Color{0, 0, 0, 0};
            selection_style.border_width = 0.0f;
            selection_style.radius = scaled(2.0f);
            const Rect selection_bounds = selection_bounds_for(impl, text_style);
            const std::string selection_path = path + "/selection";
            const std::uint32_t selection_command_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Rect,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   selection_path,
                                                   selection_bounds,
                                                   {},
                                                   {},
                                                   selection_style,
                                                   {}});
            node_layer_id =
                add_layer(result,
                          LayerNode{0,
                                    node_layer_id,
                                    LayerKind::Rect,
                                    impl.object.object_id,
                                    impl.object.generation,
                                    impl.node.kind,
                                    selection_path,
                                    selection_bounds,
                                    selection_bounds,
                                    impl.dirty.clip_bounds,
                                    selection_style,
                                    {},
                                    {},
                                    {},
                                    1.0f,
                                    selection_style.radius,
                                    0.0f,
                                    selection_command_index,
                                    1,
                                    0},
                          node_layer_id);
        }
        const std::uint32_t command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Text,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               path,
                                               text_bounds,
                                               text,
                                               resource_key,
                                               text_style,
                                               resource});
        node_layer_id =
            add_layer(result,
                      LayerNode{0,
                                node_layer_id,
                                LayerKind::Text,
                                impl.object.object_id,
                                impl.object.generation,
                                impl.node.kind,
                                path,
                                text_bounds,
                                text_bounds,
                                impl.dirty.clip_bounds,
                                text_style,
                                resource,
                                text,
                                resource_key,
                                1.0f,
                                0.0f,
                                0.0f,
                                command_index,
                                1,
                                0},
                      node_layer_id);

        if (impl.node.kind == WidgetKind::MenuItem && !impl.properties.menu_shortcut.empty()) {
            DisplayStyle shortcut_style = text_style;
            shortcut_style.text_align = "trailing";
            shortcut_style.overflow = "ellipsis";
            const Rect shortcut_bounds = menu_shortcut_bounds_for(impl, shortcut_style);
            const std::string shortcut_path = path + "/shortcut";
            const std::string shortcut_resource_key = resource_key + ":shortcut";
            const ResourceId shortcut_resource_id =
                default_runtime().resource_system().find_or_register_resource(
                    ResourceKind::TextLayout, impl.object.object_id, widget_path(impl).c_str(),
                    shortcut_resource_key.c_str(), ResourceCacheState::Cached);
            const TextMetrics shortcut_metrics = default_runtime().text_system().measure_text(
                impl.properties.menu_shortcut.c_str(), shortcut_style.font_size,
                shortcut_bounds.width, shortcut_bounds.height, dpi, impl.object.object_id,
                shortcut_path.c_str(), shortcut_style.word_wrap, shortcut_style.overflow);
            default_runtime().resource_system().update_text_metrics(shortcut_resource_id,
                                                                    shortcut_metrics);
            const ResourceRecord* shortcut_record =
                default_runtime().resource_system().find(shortcut_resource_id);
            const DisplayResource shortcut_resource = resource_from_record(shortcut_record);
            const std::uint32_t shortcut_command_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Text,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   shortcut_path,
                                                   shortcut_bounds,
                                                   impl.properties.menu_shortcut,
                                                   shortcut_resource_key,
                                                   shortcut_style,
                                                   shortcut_resource});
            node_layer_id =
                add_layer(result,
                          LayerNode{0,
                                    node_layer_id,
                                    LayerKind::Text,
                                    impl.object.object_id,
                                    impl.object.generation,
                                    impl.node.kind,
                                    shortcut_path,
                                    shortcut_bounds,
                                    shortcut_bounds,
                                    impl.dirty.clip_bounds,
                                    shortcut_style,
                                    shortcut_resource,
                                    impl.properties.menu_shortcut,
                                    shortcut_resource_key,
                                    1.0f,
                                    0.0f,
                                    0.0f,
                                    shortcut_command_index,
                                    1,
                                    0},
                          node_layer_id);
        }

        if (has_input_caret(impl)) {
            DisplayStyle caret_style = text_style;
            caret_style.text = theme.colors.accent;
            caret_style.text_align = "center";
            caret_style.paragraph_align = "center";
            caret_style.overflow = "clip";
            const Rect caret_bounds = caret_bounds_for(impl, caret_style);
            const std::string caret_path = path + "/caret";
            const std::uint32_t caret_command_index =
                add_command(result, DisplayCommand{DisplayCommandKind::Text,
                                                   impl.object.object_id,
                                                   impl.node.kind,
                                                   caret_path,
                                                   caret_bounds,
                                                   "|",
                                                   resource_key + ":caret",
                                                   caret_style,
                                                   resource});
            node_layer_id =
                add_layer(result,
                          LayerNode{0,
                                    node_layer_id,
                                    LayerKind::Text,
                                    impl.object.object_id,
                                    impl.object.generation,
                                    impl.node.kind,
                                    caret_path,
                                    caret_bounds,
                                    caret_bounds,
                                    impl.dirty.clip_bounds,
                                    caret_style,
                                    resource,
                                    "|",
                                    resource_key + ":caret",
                                    1.0f,
                                    0.0f,
                                    0.0f,
                                    caret_command_index,
                                    1,
                                    0},
                          node_layer_id);
        }
    }

    if (impl.node.kind == WidgetKind::TreeItem && !impl.node.children.empty()) {
        DisplayStyle toggle_style = style_for_text(impl, theme);
        toggle_style.text_align = "center";
        toggle_style.paragraph_align = "center";
        toggle_style.overflow = "clip";
        toggle_style.font_size = scaled(12.0f);
        const float depth_offset =
            scaled(static_cast<float>(impl.properties.tree_depth) * 18.0f);
        const Rect toggle_bounds{impl.dirty.bounds.x + scaled(6.0f) + depth_offset,
                                 impl.dirty.bounds.y,
                                 scaled(18.0f),
                                 impl.dirty.bounds.height};
        const std::string toggle_text = impl.properties.tree_expanded ? "v" : ">";
        const std::string toggle_path = path + "/tree_toggle";
        const std::uint32_t dpi = default_runtime().platform_system().state().dpi;
        const std::string toggle_resource_key =
            std::string("tree_toggle:") + toggle_text + ":font=" +
            std::to_string(toggle_style.font_size) + ":theme=" + toggle_style.theme_name +
            ":dpi=" + std::to_string(dpi);
        const ResourceId toggle_resource_id =
            default_runtime().resource_system().find_or_register_resource(
                ResourceKind::TextLayout, impl.object.object_id, widget_path(impl).c_str(),
                toggle_resource_key.c_str(), ResourceCacheState::Cached);
        const TextMetrics toggle_metrics = default_runtime().text_system().measure_text(
            toggle_text.c_str(), toggle_style.font_size, toggle_bounds.width,
            toggle_bounds.height, dpi, impl.object.object_id, toggle_path.c_str(),
            toggle_style.word_wrap, toggle_style.overflow);
        default_runtime().resource_system().update_text_metrics(toggle_resource_id,
                                                                toggle_metrics);
        const ResourceRecord* toggle_record =
            default_runtime().resource_system().find(toggle_resource_id);
        const DisplayResource toggle_resource = resource_from_record(toggle_record);
        const std::uint32_t toggle_command_index =
            add_command(result, DisplayCommand{DisplayCommandKind::Text,
                                               impl.object.object_id,
                                               impl.node.kind,
                                               toggle_path,
                                               toggle_bounds,
                                               toggle_text,
                                               toggle_resource_key,
                                               toggle_style,
                                               toggle_resource});
        node_layer_id =
            add_layer(result,
                      LayerNode{0,
                                node_layer_id,
                                LayerKind::Text,
                                impl.object.object_id,
                                impl.object.generation,
                                impl.node.kind,
                                toggle_path,
                                toggle_bounds,
                                toggle_bounds,
                                impl.dirty.clip_bounds,
                                toggle_style,
                                toggle_resource,
                                toggle_text,
                                toggle_resource_key,
                                1.0f,
                                0.0f,
                                0.0f,
                                toggle_command_index,
                                1,
                                0},
                      node_layer_id);
    }

    return node_layer_id;
}

void RenderSystem::build_batches(RenderFrameResult& result) const
{
    result.batches.clear();
    if (result.display_list.commands.empty()) {
        result.backend.batch_count = 0;
        return;
    }

    BackendBatch current;
    current.command_kind = result.display_list.commands.front().kind;
    current.first_command = 0;
    current.command_count = 0;

    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(result.display_list.commands.size()); ++index) {
        const DisplayCommandKind kind = result.display_list.commands[index].kind;
        if (current.command_count > 0 && kind != current.command_kind) {
            result.batches.push_back(current);
            current.command_kind = kind;
            current.first_command = index;
            current.command_count = 0;
        }
        ++current.command_count;
    }

    if (current.command_count > 0) {
        result.batches.push_back(current);
    }
    result.backend.batch_count = static_cast<std::uint32_t>(result.batches.size());
}

std::uint32_t RenderSystem::add_command(RenderFrameResult& result, DisplayCommand command) const
{
    switch (command.kind) {
    case DisplayCommandKind::Rect:
        ++result.backend.rect_command_count;
        break;
    case DisplayCommandKind::Shadow:
        ++result.backend.shadow_command_count;
        break;
    case DisplayCommandKind::Text:
        ++result.backend.text_command_count;
        break;
    case DisplayCommandKind::Image:
        ++result.backend.image_command_count;
        break;
    case DisplayCommandKind::Clip:
        ++result.backend.clip_command_count;
        break;
    case DisplayCommandKind::ClipEnd:
        ++result.backend.clip_end_command_count;
        break;
    case DisplayCommandKind::RoundedClip:
        ++result.backend.clip_command_count;
        ++result.backend.rounded_clip_command_count;
        break;
    case DisplayCommandKind::RoundedClipEnd:
        ++result.backend.clip_end_command_count;
        ++result.backend.rounded_clip_end_command_count;
        break;
    case DisplayCommandKind::Opacity:
        ++result.backend.opacity_command_count;
        break;
    case DisplayCommandKind::OpacityEnd:
        ++result.backend.opacity_end_command_count;
        break;
    case DisplayCommandKind::Transform:
        ++result.backend.transform_command_count;
        break;
    case DisplayCommandKind::TransformEnd:
        ++result.backend.transform_end_command_count;
        break;
    }
    const std::uint32_t command_index =
        static_cast<std::uint32_t>(result.display_list.commands.size());
    result.display_list.commands.push_back(std::move(command));
    result.backend.command_count =
        static_cast<std::uint32_t>(result.display_list.commands.size());
    return command_index;
}

std::uint32_t RenderSystem::add_layer(RenderFrameResult& result,
                                      LayerNode layer,
                                      std::uint32_t parent_layer_id) const
{
    const std::uint32_t layer_id = static_cast<std::uint32_t>(result.layer_tree.nodes.size());
    layer.layer_id = layer_id;
    layer.parent_layer_id = parent_layer_id;
    if (parent_layer_id != invalid_layer_id && parent_layer_id < result.layer_tree.nodes.size()) {
        ++result.layer_tree.nodes[parent_layer_id].child_count;
    }
    result.layer_tree.nodes.push_back(std::move(layer));
    return layer_id;
}

bool RenderSystem::has_surface_command(const WidgetImpl& impl) const
{
    if (impl.node.kind == WidgetKind::Dialog) {
        return false;
    }
    return is_container_kind(impl.node.kind);
}

bool RenderSystem::clips_child_content(const WidgetImpl& impl) const
{
    if (impl.properties.overflow_policy != OverflowPolicy::Clip || impl.node.children.empty()) {
        return false;
    }
    switch (impl.node.kind) {
    case WidgetKind::ListView:
    case WidgetKind::TreeView:
    case WidgetKind::Tabs:
        return true;
    case WidgetKind::Widget:
    case WidgetKind::Column:
    case WidgetKind::Row:
    case WidgetKind::Toolbar:
    case WidgetKind::Padding:
    case WidgetKind::Align:
    case WidgetKind::SizedBox:
    case WidgetKind::MenuBar:
        return impl.properties.overflow_policy_explicit || child_content_overflows(impl);
    case WidgetKind::SplitView:
        return true;
    default:
        return false;
    }
}

bool RenderSystem::child_content_overflows(const WidgetImpl& impl) const
{
    constexpr float epsilon = 0.5f;
    const Rect clip = impl.dirty.clip_bounds;
    if (clip.width <= 0.0f || clip.height <= 0.0f) {
        return false;
    }
    const float clip_right = clip.x + clip.width;
    const float clip_bottom = clip.y + clip.height;
    for (const WidgetImpl* child : impl.node.children) {
        if (child == nullptr) {
            continue;
        }
        const Rect bounds = child->dirty.bounds;
        if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
            continue;
        }
        if (bounds.x < clip.x - epsilon || bounds.y < clip.y - epsilon ||
            bounds.x + bounds.width > clip_right + epsilon ||
            bounds.y + bounds.height > clip_bottom + epsilon) {
            return true;
        }
    }
    return false;
}

bool RenderSystem::has_text_command(const WidgetImpl& impl) const
{
    switch (impl.node.kind) {
    case WidgetKind::Text:
    case WidgetKind::Button:
        if (!impl.node.children.empty()) {
            return false;
        }
        return !impl.properties.text.empty();
    case WidgetKind::CheckBox:
    case WidgetKind::RadioButton:
    case WidgetKind::Switch:
    case WidgetKind::SelectOption:
    case WidgetKind::ListItem:
    case WidgetKind::TreeItem:
        return !impl.properties.text.empty();
    case WidgetKind::Select:
        return !impl.properties.selected_text_cache.empty() || !impl.properties.placeholder.empty();
    case WidgetKind::MenuItem:
        return !impl.properties.text.empty();
    case WidgetKind::Input:
    case WidgetKind::TextArea:
        return !impl.properties.text.empty() || !impl.properties.placeholder.empty();
    default:
        return false;
    }
}

bool RenderSystem::has_input_caret(const WidgetImpl& impl) const
{
    const PlatformState platform = default_runtime().platform_system().state();
    return (impl.node.kind == WidgetKind::Input || impl.node.kind == WidgetKind::TextArea) &&
           default_runtime().event_system().focus_target() == impl.object.object_id &&
           platform.timer_tick_count % 2 == 0;
}

bool RenderSystem::has_input_selection(const WidgetImpl& impl) const
{
    if (impl.node.kind != WidgetKind::Input && impl.node.kind != WidgetKind::TextArea) {
        return false;
    }
    const std::size_t text_size = impl.properties.text.size();
    const std::size_t anchor =
        std::min<std::size_t>(impl.properties.text_selection_anchor, text_size);
    const std::size_t cursor = std::min<std::size_t>(impl.properties.text_cursor, text_size);
    return anchor != cursor;
}

Rect RenderSystem::text_bounds_for(const WidgetImpl& impl, const DisplayStyle& style) const
{
    Rect bounds = impl.dirty.bounds;
    const float inset_x = std::min(style.text_padding_x, bounds.width * 0.45f);
    const float inset_y = std::min(style.text_padding_y, bounds.height * 0.45f);
    bounds.x += inset_x;
    bounds.y += inset_y;
    bounds.width = std::max(0.0f, bounds.width - inset_x * 2.0f);
    bounds.height = std::max(0.0f, bounds.height - inset_y * 2.0f);
    if (impl.node.kind == WidgetKind::TextArea) {
        bounds.y -= impl.properties.scroll_offset_y;
    }
    if (impl.node.kind == WidgetKind::MenuItem && !impl.properties.menu_shortcut.empty() &&
        impl.node.parent != nullptr && impl.node.parent->node.kind == WidgetKind::MenuItem) {
        bounds.width = std::max(0.0f, bounds.width - 72.0f);
    }
    return bounds;
}

Rect RenderSystem::menu_shortcut_bounds_for(const WidgetImpl& impl,
                                            const DisplayStyle& style) const
{
    Rect bounds = impl.dirty.bounds;
    const float inset_x = std::min(style.text_padding_x, bounds.width * 0.45f);
    const float inset_y = std::min(style.text_padding_y, bounds.height * 0.45f);
    const float width = std::min(96.0f, std::max(0.0f, bounds.width - inset_x * 2.0f));
    bounds.x = bounds.x + bounds.width - inset_x - width;
    bounds.y += inset_y;
    bounds.width = width;
    bounds.height = std::max(0.0f, bounds.height - inset_y * 2.0f);
    return bounds;
}

float RenderSystem::text_advance_for(const WidgetImpl& impl,
                                     const DisplayStyle& style,
                                     std::size_t cursor) const
{
    const Rect text_bounds = text_bounds_for(impl, style);
    const std::string& input_text = impl.properties.text;
    const std::size_t clamped_cursor = std::min<std::size_t>(cursor, input_text.size());
    const std::size_t line_start =
        impl.node.kind == WidgetKind::TextArea ? line_start_for(input_text, clamped_cursor) : 0;
    const std::string prefix = input_text.substr(line_start, clamped_cursor - line_start);
    if (!prefix.empty()) {
        const std::uint32_t dpi = default_runtime().platform_system().state().dpi;
        const TextMetrics metrics = default_runtime().text_system().measure_text(
            prefix.c_str(), style.font_size, text_bounds.width, text_bounds.height, dpi,
            impl.object.object_id, widget_path(impl).c_str(), style.word_wrap, style.overflow);
        if (metrics.valid) {
            return metrics.width;
        }
    }
    return 0.0f;
}

Rect RenderSystem::caret_bounds_for(const WidgetImpl& impl, const DisplayStyle& style) const
{
    const Rect text_bounds = text_bounds_for(impl, style);
    const float measured_advance = text_advance_for(impl, style, impl.properties.text_cursor);
    const float caret_x = std::min(text_bounds.x + text_bounds.width - scaled(2.0f),
                                   text_bounds.x + measured_advance);
    const float caret_width = std::max(scaled(1.0f), style.font_size * 0.12f);
    if (impl.node.kind == WidgetKind::TextArea) {
        const float line_height = text_line_height(style);
        const float caret_y = text_bounds.y +
                              static_cast<float>(cursor_line_index(impl.properties.text,
                                                                   impl.properties.text_cursor)) *
                                  line_height;
        return Rect{caret_x, caret_y, caret_width, line_height};
    }
    return Rect{caret_x, text_bounds.y, caret_width, text_bounds.height};
}

Rect RenderSystem::selection_bounds_for(const WidgetImpl& impl, const DisplayStyle& style) const
{
    const Rect text_bounds = text_bounds_for(impl, style);
    const std::size_t text_size = impl.properties.text.size();
    const std::size_t anchor =
        std::min<std::size_t>(impl.properties.text_selection_anchor, text_size);
    const std::size_t cursor = std::min<std::size_t>(impl.properties.text_cursor, text_size);
    const std::size_t begin = std::min(anchor, cursor);
    const std::size_t end = std::max(anchor, cursor);
    if (impl.node.kind == WidgetKind::TextArea) {
        const float line_height = text_line_height(style);
        const std::size_t begin_line = cursor_line_index(impl.properties.text, begin);
        const std::size_t end_line = cursor_line_index(impl.properties.text, end);
        const float top = text_bounds.y + static_cast<float>(begin_line) * line_height;
        const float bottom = text_bounds.y + static_cast<float>(end_line + 1u) * line_height;
        return Rect{text_bounds.x, top, text_bounds.width, std::max(1.0f, bottom - top)};
    }
    const float begin_x = text_bounds.x + text_advance_for(impl, style, begin);
    const float end_x = text_bounds.x + text_advance_for(impl, style, end);
    const float left = std::min(begin_x, end_x);
    const float right = std::min(text_bounds.x + text_bounds.width, std::max(begin_x, end_x));
    return Rect{left, text_bounds.y, std::max(1.0f, right - left), text_bounds.height};
}

Rect RenderSystem::image_bounds_for(const WidgetImpl& impl, const DisplayResource& resource) const
{
    return image_bounds_for(impl, resource, impl.dirty.bounds);
}

Rect RenderSystem::image_bounds_for(const WidgetImpl& impl,
                                    const DisplayResource& resource,
                                    Rect bounds) const
{
    if (impl.properties.image_fit != ImageFit::Contain ||
        !resource.image_metadata.valid || resource.image_metadata.width == 0 ||
        resource.image_metadata.height == 0 || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return bounds;
    }

    const float source_width = static_cast<float>(resource.image_metadata.width);
    const float source_height = static_cast<float>(resource.image_metadata.height);
    const float scale = std::min(bounds.width / source_width, bounds.height / source_height);
    const float fitted_width = source_width * scale;
    const float fitted_height = source_height * scale;
    return Rect{bounds.x + (bounds.width - fitted_width) * 0.5f,
                bounds.y + (bounds.height - fitted_height) * 0.5f,
                fitted_width,
                fitted_height};
}

Rect RenderSystem::image_uv_for(const WidgetImpl& impl, const DisplayResource& resource) const
{
    return image_uv_for(impl, resource, impl.dirty.bounds);
}

Rect RenderSystem::image_uv_for(const WidgetImpl& impl,
                                const DisplayResource& resource,
                                Rect bounds) const
{
    const Rect full_uv{0.0f, 0.0f, 1.0f, 1.0f};
    if (impl.properties.image_fit != ImageFit::Cover || !resource.image_metadata.valid ||
        resource.image_metadata.width == 0 || resource.image_metadata.height == 0 ||
        bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return full_uv;
    }

    const float source_aspect =
        static_cast<float>(resource.image_metadata.width) /
        static_cast<float>(resource.image_metadata.height);
    const float destination_aspect = bounds.width / bounds.height;
    if (source_aspect > destination_aspect) {
        const float uv_width = std::max(0.0f, std::min(1.0f, destination_aspect / source_aspect));
        return Rect{(1.0f - uv_width) * 0.5f, 0.0f, uv_width, 1.0f};
    }

    const float uv_height = std::max(0.0f, std::min(1.0f, source_aspect / destination_aspect));
    return Rect{0.0f, (1.0f - uv_height) * 0.5f, 1.0f, uv_height};
}

std::string RenderSystem::text_for(const WidgetImpl& impl) const
{
    if ((impl.node.kind == WidgetKind::Input || impl.node.kind == WidgetKind::TextArea) &&
        impl.properties.text.empty()) {
        return impl.properties.placeholder;
    }
    if (impl.node.kind == WidgetKind::Select) {
        if (!impl.properties.selected_text_cache.empty()) {
            return impl.properties.selected_text_cache;
        }
        return impl.properties.placeholder;
    }
    if (impl.node.kind == WidgetKind::MenuItem) {
        std::string text = impl.properties.menu_checked ? "[x] " : "";
        text += impl.properties.text;
        return text;
    }
    return impl.properties.text;
}

const char* display_command_kind_name(DisplayCommandKind kind) noexcept
{
    switch (kind) {
    case DisplayCommandKind::Rect:
        return "rect";
    case DisplayCommandKind::Shadow:
        return "shadow";
    case DisplayCommandKind::Text:
        return "text";
    case DisplayCommandKind::Image:
        return "image";
    case DisplayCommandKind::Clip:
        return "clip";
    case DisplayCommandKind::ClipEnd:
        return "clip_end";
    case DisplayCommandKind::RoundedClip:
        return "rounded_clip";
    case DisplayCommandKind::RoundedClipEnd:
        return "rounded_clip_end";
    case DisplayCommandKind::Opacity:
        return "opacity";
    case DisplayCommandKind::OpacityEnd:
        return "opacity_end";
    case DisplayCommandKind::Transform:
        return "transform";
    case DisplayCommandKind::TransformEnd:
        return "transform_end";
    }
    return "unknown";
}

const char* layer_kind_name(LayerKind kind) noexcept
{
    switch (kind) {
    case LayerKind::Root:
        return "root";
    case LayerKind::Rect:
        return "rect";
    case LayerKind::RoundedRect:
        return "rounded_rect";
    case LayerKind::Text:
        return "text";
    case LayerKind::Image:
        return "image";
    case LayerKind::Clip:
        return "clip";
    case LayerKind::Scroll:
        return "scroll";
    case LayerKind::Opacity:
        return "opacity";
    case LayerKind::Transform:
        return "transform";
    case LayerKind::Shadow:
        return "shadow";
    }
    return "unknown";
}

const char* image_fit_name(ImageFit fit) noexcept
{
    switch (fit) {
    case ImageFit::Stretch:
        return "stretch";
    case ImageFit::Contain:
        return "contain";
    case ImageFit::Cover:
        return "cover";
    }
    return "unknown";
}

} // namespace fiui
