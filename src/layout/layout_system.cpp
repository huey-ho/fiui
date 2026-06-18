#include "layout/layout_system.h"

#include "runtime/runtime.h"

#include <algorithm>

namespace fiui {

namespace {

void clear_layout_bounds_recursive(WidgetImpl& impl) noexcept;
bool is_visible(const WidgetImpl* impl) noexcept;
std::uint32_t visible_child_count(const WidgetImpl& impl) noexcept;

} // namespace

LayoutResult LayoutSystem::arrange(WidgetImpl& root, const LayoutConstraints& constraints)
{
    LayoutResult result;
    const float width = root.dirty.bounds.width > 0.0f ? root.dirty.bounds.width
                                                       : constraints.max_width;
    const float height = root.dirty.bounds.height > 0.0f ? root.dirty.bounds.height
                                                         : constraints.max_height;
    const Rect root_bounds{0.0f, 0.0f, width, height};
    arrange_node(root, 0.0f, 0.0f, width, height, root_bounds, result);
    return result;
}

float LayoutSystem::default_height(const WidgetImpl& impl) const
{
    if (!impl.properties.visible) {
        return 0.0f;
    }
    if (is_fixed_height(impl)) {
        return requested_height(impl);
    }
    switch (impl.node.kind) {
    case WidgetKind::Text:
        if (impl.properties.style_name == "title") {
            return scaled(38.0f);
        }
        if (impl.properties.style_name == "caption") {
            return scaled(24.0f);
        }
        return scaled(28.0f);
    case WidgetKind::Button:
    case WidgetKind::CheckBox:
    case WidgetKind::RadioButton:
    case WidgetKind::Switch:
    case WidgetKind::Input:
    case WidgetKind::Select:
    case WidgetKind::Tabs:
        return scaled(40.0f);
    case WidgetKind::TextArea:
        return scaled(112.0f);
    case WidgetKind::ListView:
        return scaled(120.0f);
    case WidgetKind::ListItem:
        return scaled(30.0f);
    case WidgetKind::TreeView:
        return scaled(160.0f);
    case WidgetKind::TreeItem:
        return scaled(28.0f);
    case WidgetKind::TableView:
        return scaled(180.0f);
    case WidgetKind::Toolbar:
        return scaled(44.0f);
    case WidgetKind::SelectOption:
        return scaled(28.0f);
    case WidgetKind::MenuBar:
        return scaled(26.0f);
    case WidgetKind::MenuItem:
        return scaled(24.0f);
    case WidgetKind::Image:
        return scaled(120.0f);
    case WidgetKind::Progress:
        return scaled(12.0f);
    case WidgetKind::Slider:
        return scaled(32.0f);
    case WidgetKind::Dialog:
        return 0.0f;
    case WidgetKind::SplitView:
        return scaled(180.0f);
    case WidgetKind::Separator:
        return scaled(1.0f);
    case WidgetKind::Spacer:
        return scaled(8.0f);
    default:
        return scaled(32.0f);
    }
}

float LayoutSystem::default_width(const WidgetImpl& impl, float parent_width) const
{
    if (!impl.properties.visible) {
        return 0.0f;
    }
    return is_fixed_width(impl) ? requested_width(impl) : parent_width;
}

Rect LayoutSystem::inflate(Rect rect, float amount) const
{
    rect.x -= amount;
    rect.y -= amount;
    rect.width += amount * 2.0f;
    rect.height += amount * 2.0f;
    return rect;
}

float LayoutSystem::preferred_tree_item_height(const WidgetImpl& item) const
{
    float height = default_height(item);
    if (item.properties.tree_expanded) {
        for (const WidgetImpl* child : item.node.children) {
            if (is_visible(child) && child->node.kind == WidgetKind::TreeItem) {
                height += preferred_tree_item_height(*child);
            }
        }
    }
    return height;
}

float LayoutSystem::arrange_tree_item(WidgetImpl& item,
                                      float x,
                                      float y,
                                      float width,
                                      const Rect& root_bounds,
                                      LayoutResult& result,
                                      std::uint32_t depth)
{
    item.properties.tree_depth = depth;
    const float row_height = default_height(item);
    arrange_node(item, x, y, width, row_height, root_bounds, result);
    float cursor_y = y + row_height;
    if (!item.properties.tree_expanded) {
        for (WidgetImpl* child : item.node.children) {
            if (child != nullptr) {
                clear_layout_bounds_recursive(*child);
            }
        }
        return row_height;
    }
    for (WidgetImpl* child : item.node.children) {
        if (is_visible(child) && child->node.kind == WidgetKind::TreeItem) {
            cursor_y += arrange_tree_item(*child, x, cursor_y, width, root_bounds, result,
                                          depth + 1u);
        } else if (child != nullptr) {
            clear_layout_bounds_recursive(*child);
        }
    }
    return cursor_y - y;
}

bool LayoutSystem::is_container(const WidgetImpl& impl) const noexcept
{
    if (!impl.properties.visible) {
        return false;
    }
    switch (impl.node.kind) {
    case WidgetKind::Widget:
    case WidgetKind::Window:
    case WidgetKind::Column:
    case WidgetKind::Row:
    case WidgetKind::Toolbar:
    case WidgetKind::ListView:
    case WidgetKind::TreeView:
    case WidgetKind::Padding:
    case WidgetKind::Align:
    case WidgetKind::SizedBox:
    case WidgetKind::ScrollView:
    case WidgetKind::Dialog:
    case WidgetKind::SplitView:
    case WidgetKind::MenuBar:
    case WidgetKind::Button:
        return !impl.node.children.empty();
    default:
        return false;
    }
}

namespace {

float menu_item_popup_width(const WidgetImpl& impl) noexcept
{
    float width = 160.0f;
    for (const WidgetImpl* child : impl.node.children) {
        if (child != nullptr) {
            if (child->node.kind == WidgetKind::Separator) {
                continue;
            }
            const std::size_t text_size = child->properties.text.size() +
                                          child->properties.menu_shortcut.size() +
                                          (child->properties.menu_checked ? 4u : 0u);
            width = std::max(width, 36.0f + static_cast<float>(text_size) * 7.0f);
        }
    }
    return width;
}

float estimated_text_width(const std::string& text) noexcept
{
    return static_cast<float>(text.size()) * 8.5f;
}

Rect union_rect(Rect left, Rect right) noexcept
{
    if (left.width <= 0.0f || left.height <= 0.0f) {
        return right;
    }
    if (right.width <= 0.0f || right.height <= 0.0f) {
        return left;
    }
    const float min_x = std::min(left.x, right.x);
    const float min_y = std::min(left.y, right.y);
    const float max_x = std::max(left.x + left.width, right.x + right.width);
    const float max_y = std::max(left.y + left.height, right.y + right.height);
    return Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

void clear_layout_bounds_recursive(WidgetImpl& impl) noexcept
{
    impl.dirty.bounds = Rect{};
    impl.dirty.paint_bounds = Rect{};
    impl.dirty.clip_bounds = Rect{};
    for (WidgetImpl* child : impl.node.children) {
        if (child != nullptr) {
            clear_layout_bounds_recursive(*child);
        }
    }
}

bool is_visible(const WidgetImpl* impl) noexcept
{
    return impl != nullptr && impl->properties.visible;
}

std::uint32_t visible_child_count(const WidgetImpl& impl) noexcept
{
    std::uint32_t count = 0;
    for (const WidgetImpl* child : impl.node.children) {
        if (is_visible(child)) {
            ++count;
        }
    }
    return count;
}

} // namespace

bool LayoutSystem::fills_single_child(const WidgetImpl& impl) const noexcept
{
    if (impl.node.children.size() != 1) {
        return false;
    }
    switch (impl.node.kind) {
    case WidgetKind::Window:
    case WidgetKind::Padding:
    case WidgetKind::Align:
    case WidgetKind::SizedBox:
    case WidgetKind::Button:
        return true;
    default:
        return false;
    }
}

bool LayoutSystem::is_flexible_height(const WidgetImpl& impl) const noexcept
{
    return impl.properties.height_mode == LayoutSizeMode::Flex ||
           impl.properties.height_mode == LayoutSizeMode::Fill;
}

bool LayoutSystem::is_fixed_width(const WidgetImpl& impl) const noexcept
{
    return impl.properties.width_mode == LayoutSizeMode::Fixed &&
           impl.properties.requested_width > 0.0f;
}

bool LayoutSystem::is_fixed_height(const WidgetImpl& impl) const noexcept
{
    return impl.properties.height_mode == LayoutSizeMode::Fixed &&
           impl.properties.requested_height > 0.0f;
}

bool LayoutSystem::is_row_flexible_width(const WidgetImpl& impl) const noexcept
{
    return impl.properties.width_mode == LayoutSizeMode::Flex ||
           impl.properties.width_mode == LayoutSizeMode::Fill;
}

float LayoutSystem::flex_weight(const WidgetImpl& impl) const noexcept
{
    return impl.properties.flex_grow > 0.0f ? impl.properties.flex_grow : 1.0f;
}

float LayoutSystem::dpi_scale() const noexcept
{
    const std::uint32_t dpi = default_runtime().platform_system().state().dpi;
    return std::max(0.25f, static_cast<float>(std::max<std::uint32_t>(1, dpi)) / 96.0f);
}

float LayoutSystem::scaled(float value) const noexcept
{
    return value * dpi_scale();
}

float LayoutSystem::requested_width(const WidgetImpl& impl) const noexcept
{
    return scaled(impl.properties.requested_width);
}

float LayoutSystem::requested_height(const WidgetImpl& impl) const noexcept
{
    return scaled(impl.properties.requested_height);
}

float LayoutSystem::padding(const WidgetImpl& impl) const noexcept
{
    return scaled(impl.properties.padding);
}

float LayoutSystem::gap(const WidgetImpl& impl) const noexcept
{
    return scaled(impl.properties.gap);
}

float LayoutSystem::button_content_padding(const WidgetImpl& impl) const noexcept
{
    return scaled(impl.properties.has_button_text_padding ? impl.properties.button_text_padding
                                                          : 12.0f);
}

float LayoutSystem::preferred_height(const WidgetImpl& impl) const
{
    if (!impl.properties.visible) {
        return 0.0f;
    }
    if (impl.node.kind == WidgetKind::Dialog) {
        return 0.0f;
    }
    if (is_fixed_height(impl)) {
        return requested_height(impl);
    }
    if (impl.node.kind == WidgetKind::TableView) {
        const float header_height = scaled(std::max(1.0f, impl.properties.table_header_height));
        const float row_height = scaled(std::max(1.0f, impl.properties.table_row_height));
        return header_height +
               row_height * static_cast<float>(impl.properties.table_rows.size());
    }
    if (!is_container(impl)) {
        return default_height(impl);
    }

    if (impl.node.kind == WidgetKind::Button) {
        float max_child_height = 0.0f;
        for (const WidgetImpl* child : impl.node.children) {
            if (is_visible(child)) {
                max_child_height = std::max(max_child_height, preferred_height(*child));
            }
        }
        return std::max(default_height(impl), button_content_padding(impl) * 2.0f + max_child_height);
    }

    if (impl.node.kind == WidgetKind::TreeView) {
        float content_height = 0.0f;
        for (const WidgetImpl* child : impl.node.children) {
            if (is_visible(child) && child->node.kind == WidgetKind::TreeItem) {
                content_height += preferred_tree_item_height(*child);
            }
        }
        return padding(impl) * 2.0f + content_height;
    }

    const float padding_size = padding(impl) * 2.0f;
    if (impl.node.kind == WidgetKind::Row || impl.node.kind == WidgetKind::Toolbar ||
        impl.node.kind == WidgetKind::MenuBar) {
        float max_child_height = 0.0f;
        for (const WidgetImpl* child : impl.node.children) {
            if (is_visible(child)) {
                max_child_height = std::max(max_child_height, preferred_height(*child));
            }
        }
        const float content_height = padding_size + max_child_height;
        if (impl.node.kind == WidgetKind::Toolbar || impl.node.kind == WidgetKind::MenuBar) {
            return std::max(default_height(impl), content_height);
        }
        return content_height;
    }

    float content_height = 0.0f;
    std::uint32_t child_count = 0;
    for (const WidgetImpl* child : impl.node.children) {
        if (!is_visible(child)) {
            continue;
        }
        if (!is_flexible_height(*child)) {
            content_height += preferred_height(*child);
        }
        ++child_count;
    }
    if (child_count > 1) {
        content_height += gap(impl) * static_cast<float>(child_count - 1);
    }
    return padding_size + content_height;
}

float LayoutSystem::preferred_width(const WidgetImpl& impl) const
{
    if (!impl.properties.visible) {
        return 0.0f;
    }
    if (impl.node.kind == WidgetKind::Dialog) {
        return 0.0f;
    }
    if (is_fixed_width(impl)) {
        return requested_width(impl);
    }

    if (impl.node.kind == WidgetKind::Button && !impl.node.children.empty()) {
        float content_width = 0.0f;
        std::uint32_t child_count = 0;
        for (const WidgetImpl* child : impl.node.children) {
            if (is_visible(child)) {
                content_width += preferred_width(*child);
                ++child_count;
            }
        }
        if (child_count > 1) {
            content_width += gap(impl) * static_cast<float>(child_count - 1);
        }
        return std::max(scaled(72.0f), button_content_padding(impl) * 2.0f + content_width);
    }

    switch (impl.node.kind) {
    case WidgetKind::Text:
        return scaled(std::max(24.0f, 12.0f + estimated_text_width(impl.properties.text)));
    case WidgetKind::Button:
        return scaled(std::max(72.0f, 28.0f + estimated_text_width(impl.properties.text)));
    case WidgetKind::CheckBox:
        return scaled(std::max(80.0f, 36.0f + estimated_text_width(impl.properties.text)));
    case WidgetKind::RadioButton:
        return scaled(std::max(88.0f, 36.0f + estimated_text_width(impl.properties.text)));
    case WidgetKind::Switch:
        return scaled(std::max(96.0f, 56.0f + estimated_text_width(impl.properties.text)));
    case WidgetKind::Input:
        return scaled(160.0f);
    case WidgetKind::TextArea:
        return scaled(260.0f);
    case WidgetKind::Select:
        return scaled(180.0f);
    case WidgetKind::SplitView:
        return scaled(320.0f);
    case WidgetKind::ListView:
        return scaled(220.0f);
    case WidgetKind::ListItem:
        return scaled(std::max(120.0f, 32.0f + estimated_text_width(impl.properties.text)));
    case WidgetKind::TreeView:
        return scaled(240.0f);
    case WidgetKind::TreeItem:
        return scaled(std::max(140.0f, 44.0f + estimated_text_width(impl.properties.text)));
    case WidgetKind::TableView:
        return scaled(360.0f);
    case WidgetKind::Tabs:
        return scaled(220.0f);
    case WidgetKind::Toolbar:
        return scaled(240.0f);
    case WidgetKind::SelectOption:
        return scaled(std::max(120.0f, 32.0f + estimated_text_width(impl.properties.text)));
    case WidgetKind::MenuItem:
        return scaled(std::max(44.0f, 20.0f + estimated_text_width(impl.properties.text)));
    case WidgetKind::Image:
        return scaled(120.0f);
    case WidgetKind::Progress:
        return scaled(120.0f);
    case WidgetKind::Slider:
        return scaled(160.0f);
    case WidgetKind::Separator:
        return scaled(1.0f);
    case WidgetKind::Spacer:
        return scaled(8.0f);
    default:
        break;
    }

    if (!is_container(impl)) {
        return scaled(32.0f);
    }

    const float padding_size = padding(impl) * 2.0f;
    if (impl.node.kind == WidgetKind::Row || impl.node.kind == WidgetKind::Toolbar ||
        impl.node.kind == WidgetKind::MenuBar) {
        float content_width = 0.0f;
        std::uint32_t child_count = 0;
        for (const WidgetImpl* child : impl.node.children) {
            if (!is_visible(child)) {
                continue;
            }
            if (!is_row_flexible_width(*child)) {
                content_width += preferred_width(*child);
            }
            ++child_count;
        }
        if (child_count > 1) {
            content_width += gap(impl) * static_cast<float>(child_count - 1);
        }
        return padding_size + content_width;
    }

    float max_child_width = 0.0f;
    for (const WidgetImpl* child : impl.node.children) {
        if (is_visible(child)) {
            max_child_width = std::max(max_child_width, preferred_width(*child));
        }
    }
    return padding_size + max_child_width;
}

void LayoutSystem::arrange_node(WidgetImpl& impl,
                                float x,
                                float y,
                                float width,
                                float height,
                                const Rect& root_bounds,
                                LayoutResult& result)
{
    if (!impl.properties.visible) {
        clear_layout_bounds_recursive(impl);
        return;
    }
    ++result.arranged_nodes;
    impl.dirty.bounds = Rect{x, y, width, height};
    impl.dirty.clip_bounds = impl.dirty.bounds;
    impl.dirty.paint_bounds = inflate(impl.dirty.bounds, scaled(1.0f));

    const float padding_size = padding(impl);
    const float gap_size = gap(impl);
    const float child_x = x + padding_size;
    const float child_y = y + padding_size;
    const float child_width = std::max(0.0f, width - padding_size * 2.0f);
    const float child_height = std::max(0.0f, height - padding_size * 2.0f);

    if (impl.node.kind == WidgetKind::TableView) {
        const float header_height = scaled(std::max(1.0f, impl.properties.table_header_height));
        const float row_height = scaled(std::max(1.0f, impl.properties.table_row_height));
        impl.properties.scroll_content_height =
            row_height * static_cast<float>(impl.properties.table_rows.size());
        const float max_offset =
            std::max(0.0f, impl.properties.scroll_content_height -
                               std::max(0.0f, height - header_height));
        impl.properties.scroll_offset_y =
            std::min(std::max(0.0f, impl.properties.scroll_offset_y), max_offset);
    }
    if (impl.node.kind == WidgetKind::TextArea) {
        const float line_height = scaled(20.0f);
        const std::size_t line_count =
            1u + static_cast<std::size_t>(
                     std::count(impl.properties.text.begin(), impl.properties.text.end(), '\n'));
        impl.properties.scroll_content_height = line_height * static_cast<float>(line_count);
        const float max_offset =
            std::max(0.0f, impl.properties.scroll_content_height - height);
        impl.properties.scroll_offset_y =
            std::min(std::max(0.0f, impl.properties.scroll_offset_y), max_offset);
    }

    if (impl.node.children.empty()) {
        return;
    }

    if (impl.node.kind == WidgetKind::TreeItem) {
        return;
    }

    if (impl.node.kind == WidgetKind::Dialog) {
        if (!impl.properties.dialog_open) {
            for (WidgetImpl* child : impl.node.children) {
                if (child != nullptr) {
                    clear_layout_bounds_recursive(*child);
                }
            }
            return;
        }
        impl.dirty.bounds = root_bounds;
        impl.dirty.clip_bounds = root_bounds;
        impl.dirty.paint_bounds = root_bounds;
        if (impl.node.children.empty()) {
            return;
        }
        WidgetImpl* panel = impl.node.children.front();
        if (!is_visible(panel)) {
            if (panel != nullptr) {
                clear_layout_bounds_recursive(*panel);
            }
            return;
        }
        const float overlay_width = std::max(1.0f, root_bounds.width);
        const float overlay_height = std::max(1.0f, root_bounds.height);
        const float max_panel_width = std::max(1.0f, overlay_width - scaled(48.0f));
        const float max_panel_height = std::max(1.0f, overlay_height - scaled(48.0f));
        const float desired_panel_width =
            is_fixed_width(impl) ? requested_width(impl)
                                 : std::max(scaled(320.0f), preferred_width(*panel));
        const float desired_panel_height =
            is_fixed_height(impl) ? requested_height(impl)
                                  : std::max(scaled(180.0f), preferred_height(*panel));
        const float panel_width = std::min(max_panel_width, desired_panel_width);
        const float panel_height =
            std::min(max_panel_height, desired_panel_height);
        const float centered_panel_x =
            root_bounds.x + std::max(0.0f, (overlay_width - panel_width) * 0.5f);
        const float centered_panel_y =
            root_bounds.y + std::max(0.0f, (overlay_height - panel_height) * 0.5f);
        const float min_panel_x = root_bounds.x;
        const float min_panel_y = root_bounds.y;
        const float max_panel_x = root_bounds.x + std::max(0.0f, overlay_width - panel_width);
        const float max_panel_y = root_bounds.y + std::max(0.0f, overlay_height - panel_height);
        const float panel_x =
            std::min(std::max(min_panel_x, centered_panel_x + impl.properties.dialog_offset_x),
                     max_panel_x);
        const float panel_y =
            std::min(std::max(min_panel_y, centered_panel_y + impl.properties.dialog_offset_y),
                     max_panel_y);
        arrange_node(*panel, panel_x, panel_y, panel_width, panel_height, root_bounds, result);
        impl.dirty.paint_bounds = union_rect(impl.dirty.paint_bounds, panel->dirty.paint_bounds);
        for (std::size_t index = 1; index < impl.node.children.size(); ++index) {
            WidgetImpl* child = impl.node.children[index];
            if (child != nullptr) {
                clear_layout_bounds_recursive(*child);
            }
        }
        return;
    }

    if (impl.node.kind == WidgetKind::MenuItem) {
        if (!impl.properties.menu_popup_open) {
            for (WidgetImpl* child : impl.node.children) {
                if (child != nullptr) {
                    clear_layout_bounds_recursive(*child);
                }
            }
            return;
        }

        const bool nested_popup =
            impl.node.parent != nullptr && impl.node.parent->node.kind == WidgetKind::MenuItem;
        const float popup_width = scaled(menu_item_popup_width(impl));
        const float item_height = default_height(impl);
        const float popup_x = nested_popup ? x + width : x;
        float cursor_y = nested_popup ? y : y + height;
        Rect popup_paint_bounds = Rect{popup_x, cursor_y, popup_width, 0.0f};
        for (WidgetImpl* child : impl.node.children) {
            if (!is_visible(child)) {
                if (child != nullptr) {
                    clear_layout_bounds_recursive(*child);
                }
                continue;
            }
            const float popup_child_height =
                child->node.kind == WidgetKind::Separator ? default_height(*child) : item_height;
            arrange_node(*child, popup_x, cursor_y, popup_width, popup_child_height,
                         root_bounds, result);
            popup_paint_bounds = union_rect(popup_paint_bounds, child->dirty.paint_bounds);
            cursor_y += popup_child_height;
        }
        impl.dirty.paint_bounds = union_rect(impl.dirty.paint_bounds, popup_paint_bounds);
        return;
    }

    if (impl.node.kind == WidgetKind::ScrollView && impl.node.children.size() == 1) {
        WidgetImpl* child = impl.node.children.front();
        if (is_visible(child)) {
            const float content_height = std::max(child_height, preferred_height(*child));
            impl.properties.scroll_content_height = content_height;
            const float max_offset = std::max(0.0f, content_height - child_height);
            impl.properties.scroll_offset_y =
                std::min(std::max(0.0f, impl.properties.scroll_offset_y), max_offset);
            arrange_node(*child, child_x, child_y - impl.properties.scroll_offset_y,
                         child_width, content_height, root_bounds, result);
            child->dirty.clip_bounds = impl.dirty.clip_bounds;
        }
        return;
    }

    if (impl.node.kind == WidgetKind::TreeView) {
        float cursor_y = child_y;
        for (WidgetImpl* child : impl.node.children) {
            if (is_visible(child) && child->node.kind == WidgetKind::TreeItem) {
                cursor_y += arrange_tree_item(*child, child_x, cursor_y, child_width,
                                              root_bounds, result, 0u);
            } else if (child != nullptr) {
                clear_layout_bounds_recursive(*child);
            }
        }
        return;
    }

    if (impl.node.kind == WidgetKind::SplitView) {
        if (impl.node.children.size() == 1) {
            WidgetImpl* child = impl.node.children.front();
            if (is_visible(child)) {
                arrange_node(*child, child_x, child_y, child_width, child_height, root_bounds,
                             result);
            } else if (child != nullptr) {
                clear_layout_bounds_recursive(*child);
            }
            return;
        }
        if (impl.node.children.size() >= 2) {
            WidgetImpl* first = impl.node.children[0];
            WidgetImpl* second = impl.node.children[1];
            const bool horizontal =
                impl.properties.split_orientation == SplitOrientation::Horizontal;
            const float handle_size = scaled(std::max(1.0f, impl.properties.split_handle_size));
            const float min_pane = scaled(std::max(0.0f, impl.properties.split_min_pane_size));
            if (horizontal) {
                const float available = std::max(0.0f, child_width - handle_size);
                const float max_first = std::max(0.0f, available - std::min(min_pane, available));
                const float min_first = std::min(min_pane, available);
                const float first_width =
                    std::min(max_first, std::max(min_first, available * impl.properties.split_ratio));
                const float second_width = std::max(0.0f, available - first_width);
                if (is_visible(first)) {
                    arrange_node(*first, child_x, child_y, first_width, child_height, root_bounds,
                                 result);
                    first->dirty.clip_bounds = Rect{child_x, child_y, first_width, child_height};
                } else if (first != nullptr) {
                    clear_layout_bounds_recursive(*first);
                }
                if (is_visible(second)) {
                    arrange_node(*second, child_x + first_width + handle_size, child_y,
                                 second_width, child_height, root_bounds, result);
                    second->dirty.clip_bounds =
                        Rect{child_x + first_width + handle_size, child_y, second_width,
                             child_height};
                } else if (second != nullptr) {
                    clear_layout_bounds_recursive(*second);
                }
            } else {
                const float available = std::max(0.0f, child_height - handle_size);
                const float max_first = std::max(0.0f, available - std::min(min_pane, available));
                const float min_first = std::min(min_pane, available);
                const float first_height =
                    std::min(max_first, std::max(min_first, available * impl.properties.split_ratio));
                const float second_height = std::max(0.0f, available - first_height);
                if (is_visible(first)) {
                    arrange_node(*first, child_x, child_y, child_width, first_height, root_bounds,
                                 result);
                    first->dirty.clip_bounds = Rect{child_x, child_y, child_width, first_height};
                } else if (first != nullptr) {
                    clear_layout_bounds_recursive(*first);
                }
                if (is_visible(second)) {
                    arrange_node(*second, child_x, child_y + first_height + handle_size,
                                 child_width, second_height, root_bounds, result);
                    second->dirty.clip_bounds =
                        Rect{child_x, child_y + first_height + handle_size, child_width,
                             second_height};
                } else if (second != nullptr) {
                    clear_layout_bounds_recursive(*second);
                }
            }
            return;
        }
    }

    if (impl.node.kind == WidgetKind::Select) {
        if (!impl.properties.select_popup_open) {
            for (WidgetImpl* child : impl.node.children) {
                if (child != nullptr) {
                    clear_layout_bounds_recursive(*child);
                }
            }
            return;
        }

        const float popup_width = std::max(width, scaled(160.0f));
        const float item_height = scaled(28.0f);
        const float popup_height = item_height * static_cast<float>(visible_child_count(impl));
        const float root_right = root_bounds.x + root_bounds.width;
        const float popup_x =
            std::max(root_bounds.x, std::min(x, root_right - popup_width));
        const float below_y = y + height;
        const float above_y = y - popup_height;
        const float root_bottom = root_bounds.y + root_bounds.height;
        const float space_below = root_bottom - below_y;
        const float space_above = y - root_bounds.y;
        float cursor_y = below_y;
        if (popup_height > space_below && space_above > space_below) {
            cursor_y = std::max(root_bounds.y, above_y);
        }
        Rect popup_paint_bounds = Rect{popup_x, cursor_y, popup_width, 0.0f};
        for (WidgetImpl* child : impl.node.children) {
            if (!is_visible(child)) {
                if (child != nullptr) {
                    clear_layout_bounds_recursive(*child);
                }
                continue;
            }
            arrange_node(*child, popup_x, cursor_y, popup_width, item_height, root_bounds, result);
            popup_paint_bounds = union_rect(popup_paint_bounds, child->dirty.paint_bounds);
            cursor_y += item_height;
        }
        impl.dirty.paint_bounds = union_rect(impl.dirty.paint_bounds, popup_paint_bounds);
        return;
    }

    if (impl.node.kind == WidgetKind::Tabs) {
        const float header_height = scaled(38.0f);
        const float content_y = y + header_height;
        const float content_height = std::max(0.0f, height - header_height);
        const std::uint32_t selected =
            impl.node.children.empty()
                ? 0u
                : std::min<std::uint32_t>(impl.properties.selected_tab_index,
                                          static_cast<std::uint32_t>(
                                              impl.node.children.size() - 1u));
        impl.properties.selected_tab_index = selected;
        for (std::uint32_t index = 0;
             index < static_cast<std::uint32_t>(impl.node.children.size()); ++index) {
            WidgetImpl* child = impl.node.children[index];
            if (!is_visible(child)) {
                if (child != nullptr) {
                    clear_layout_bounds_recursive(*child);
                }
                continue;
            }
            if (index == selected) {
                arrange_node(*child, x, content_y, width, content_height, root_bounds, result);
                child->dirty.clip_bounds = Rect{x, content_y, width, content_height};
            } else {
                clear_layout_bounds_recursive(*child);
            }
        }
        return;
    }

    if (impl.node.kind == WidgetKind::Button && !impl.node.children.empty()) {
        const float content_padding = button_content_padding(impl);
        const float content_x = x + content_padding;
        const float content_y = y + content_padding;
        const float content_width = std::max(0.0f, width - content_padding * 2.0f);
        const float content_height = std::max(0.0f, height - content_padding * 2.0f);
        if (visible_child_count(impl) == 1) {
            WidgetImpl* child = impl.node.children.front();
            if (!is_visible(child)) {
                for (WidgetImpl* candidate : impl.node.children) {
                    if (is_visible(candidate)) {
                        child = candidate;
                        break;
                    }
                }
            }
            if (is_visible(child)) {
                arrange_node(*child, content_x, content_y, content_width, content_height,
                             root_bounds, result);
            }
            return;
        }
        const std::uint32_t button_child_count = visible_child_count(impl);
        const float total_gap =
            gap_size * static_cast<float>(button_child_count > 0 ? button_child_count - 1u : 0u);
        float cursor_x = content_x;
        float occupied_width = 0.0f;
        for (WidgetImpl* child : impl.node.children) {
            if (is_visible(child)) {
                occupied_width += preferred_width(*child);
            } else if (child != nullptr) {
                clear_layout_bounds_recursive(*child);
            }
        }
        const float extra_width =
            std::max(0.0f, content_width - total_gap - occupied_width);
        for (WidgetImpl* child : impl.node.children) {
            if (!is_visible(child)) {
                if (child != nullptr) {
                    clear_layout_bounds_recursive(*child);
                }
                continue;
            }
            const float item_width = preferred_width(*child);
            const float item_height = std::min(preferred_height(*child), content_height);
            const float item_y = content_y + std::max(0.0f, (content_height - item_height) * 0.5f);
            arrange_node(*child, cursor_x + extra_width * 0.5f, item_y, item_width, item_height,
                         root_bounds, result);
            cursor_x += item_width + gap_size;
        }
        return;
    }

    if (fills_single_child(impl)) {
        WidgetImpl* child = impl.node.children.front();
        if (is_visible(child)) {
            arrange_node(*child, child_x, child_y, child_width, child_height, root_bounds, result);
        } else if (child != nullptr) {
            clear_layout_bounds_recursive(*child);
        }
        return;
    }

    if (impl.node.kind == WidgetKind::MenuBar) {
        float cursor_x = child_x;
        for (WidgetImpl* child : impl.node.children) {
            if (!is_visible(child)) {
                if (child != nullptr) {
                    clear_layout_bounds_recursive(*child);
                }
                continue;
            }
            const float item_width =
                is_fixed_width(*child)
                    ? requested_width(*child)
                    : scaled(std::max(44.0f,
                                      20.0f + static_cast<float>(child->properties.text.size()) * 7.0f));
            const float item_height =
                is_fixed_height(*child)
                    ? std::min(requested_height(*child), child_height)
                    : std::min(default_height(*child), child_height);
            const float item_y = child_y + std::max(0.0f, (child_height - item_height) * 0.5f);
            arrange_node(*child, cursor_x, item_y, item_width, item_height, root_bounds, result);
            cursor_x += item_width + gap_size;
        }
        return;
    }

    if (impl.node.kind == WidgetKind::Row || impl.node.kind == WidgetKind::Toolbar) {
        const std::uint32_t row_child_count = visible_child_count(impl);
        const float total_gap =
            gap_size * static_cast<float>(row_child_count > 0 ? row_child_count - 1u : 0u);
        const float available_width = std::max(0.0f, child_width - total_gap);
        float occupied_width = 0.0f;
        float flexible_weight = 0.0f;
        for (WidgetImpl* child : impl.node.children) {
            if (!is_visible(child)) {
                if (child != nullptr) {
                    clear_layout_bounds_recursive(*child);
                }
                continue;
            }
            if (is_row_flexible_width(*child)) {
                flexible_weight += flex_weight(*child);
            } else {
                occupied_width += preferred_width(*child);
            }
        }
        const float remaining_width = std::max(0.0f, available_width - occupied_width);
        float cursor_x = child_x;
        for (WidgetImpl* child : impl.node.children) {
            if (!is_visible(child)) {
                if (child != nullptr) {
                    clear_layout_bounds_recursive(*child);
                }
                continue;
            }
            float item_width = preferred_width(*child);
            if (is_row_flexible_width(*child)) {
                item_width =
                    flexible_weight > 0.0f
                        ? std::max(1.0f,
                                   remaining_width * (flex_weight(*child) / flexible_weight))
                        : 0.0f;
            }
            const float item_height = is_fixed_height(*child)
                                          ? std::min(requested_height(*child),
                                                     child_height)
                                          : child_height;
            arrange_node(*child, cursor_x, child_y, item_width,
                         item_height, root_bounds, result);
            cursor_x += item_width + gap_size;
        }
        return;
    }

    const std::uint32_t column_child_count = visible_child_count(impl);
    const float total_gap =
        gap_size * static_cast<float>(column_child_count > 0 ? column_child_count - 1u : 0u);
    float fixed_height = 0.0f;
    float flexible_weight = 0.0f;
    for (WidgetImpl* child : impl.node.children) {
        if (!is_visible(child)) {
            if (child != nullptr) {
                clear_layout_bounds_recursive(*child);
            }
            continue;
        }
        if (is_flexible_height(*child)) {
            flexible_weight += flex_weight(*child);
        } else {
            fixed_height += preferred_height(*child);
        }
    }

    const float available_height = std::max(0.0f, child_height - total_gap);
    const float remaining_height = std::max(0.0f, available_height - fixed_height);

    float cursor_y = child_y;
    for (WidgetImpl* child : impl.node.children) {
        if (!is_visible(child)) {
            if (child != nullptr) {
                clear_layout_bounds_recursive(*child);
            }
            continue;
        }
        const float item_height =
            is_flexible_height(*child)
                ? (flexible_weight > 0.0f
                       ? std::max(1.0f,
                                  remaining_height * (flex_weight(*child) / flexible_weight))
                       : 0.0f)
                : std::min(preferred_height(*child), std::max(1.0f, child_height));
        arrange_node(*child, child_x, cursor_y, default_width(*child, child_width), item_height,
                     root_bounds, result);
        cursor_y += item_height + gap_size;
    }
}

} // namespace fiui
