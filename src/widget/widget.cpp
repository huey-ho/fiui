#include "fiui/widget.h"

#include "core/widget_impl.h"
#include "diagnostics/diagnostics_internal.h"
#include "runtime/runtime.h"

#include <algorithm>
#include <utility>

namespace fiui {
namespace {

void sort_table_rows(WidgetImpl& table, std::uint32_t column, bool ascending)
{
    if (table.node.kind != WidgetKind::TableView || column >= table.properties.table_columns.size()) {
        return;
    }
    std::sort(table.properties.table_rows.begin(), table.properties.table_rows.end(),
              [column, ascending](const std::vector<std::string>& left,
                                  const std::vector<std::string>& right) {
                  const std::string left_value = column < left.size() ? left[column] : "";
                  const std::string right_value = column < right.size() ? right[column] : "";
                  if (ascending) {
                      return left_value < right_value;
                  }
                  return right_value < left_value;
              });
    table.properties.table_sort_column = column;
    table.properties.table_sort_ascending = ascending;
    if (!table.properties.table_rows.empty()) {
        table.properties.selected_index =
            std::min<std::uint32_t>(table.properties.selected_index,
                                    static_cast<std::uint32_t>(
                                        table.properties.table_rows.size() - 1u));
        table.properties.has_selected_index = true;
        const std::vector<std::string>& row =
            table.properties.table_rows[table.properties.selected_index];
        table.properties.selected_text_cache = row.empty() ? "" : row.front();
    }
}

void clamp_table_selection(WidgetImpl& table)
{
    if (table.node.kind != WidgetKind::TableView) {
        return;
    }
    if (table.properties.table_rows.empty()) {
        table.properties.selected_index = 0;
        table.properties.has_selected_index = false;
        table.properties.selected_text_cache.clear();
        table.properties.scroll_offset_y = 0.0f;
        table.properties.scroll_content_height = 0.0f;
        return;
    }
    table.properties.selected_index =
        std::min<std::uint32_t>(table.properties.selected_index,
                                static_cast<std::uint32_t>(
                                    table.properties.table_rows.size() - 1u));
    table.properties.has_selected_index = true;
    const std::vector<std::string>& row =
        table.properties.table_rows[table.properties.selected_index];
    table.properties.selected_text_cache = row.empty() ? "" : row.front();
}

} // namespace

Widget::Widget()
    : impl_(make_widget_impl(WidgetKind::Widget, nullptr))
{
}

Widget::Widget(WidgetKind kind, const char* initial_text)
    : impl_(make_widget_impl(kind, initial_text))
{
}

Widget::Widget(const Widget& other) noexcept
    : impl_(other.impl_)
{
    retain_widget_impl(impl_);
}

Widget::Widget(Widget&& other) noexcept
    : impl_(std::exchange(other.impl_, nullptr))
{
}

Widget& Widget::operator=(const Widget& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    retain_widget_impl(other.impl_);
    release_widget_impl(impl_);
    impl_ = other.impl_;
    return *this;
}

Widget& Widget::operator=(Widget&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    release_widget_impl(impl_);
    impl_ = std::exchange(other.impl_, nullptr);
    return *this;
}

Widget::~Widget()
{
    release_widget_impl(impl_);
}

bool Widget::valid() const noexcept
{
    return impl_ != nullptr;
}

ObjectId Widget::object_id() const noexcept
{
    return impl_ == nullptr ? 0 : impl_->object.object_id;
}

std::uint32_t Widget::generation() const noexcept
{
    return impl_ == nullptr ? 0 : impl_->object.generation;
}

std::uint32_t Widget::use_count() const noexcept
{
    return impl_ == nullptr ? 0 : impl_->object.ref_count.load();
}

WidgetKind Widget::kind() const noexcept
{
    return impl_ == nullptr ? WidgetKind::Widget : impl_->node.kind;
}

LifecycleState Widget::lifecycle_state() const noexcept
{
    return impl_ == nullptr ? LifecycleState::Destroyed : impl_->object.lifecycle_state;
}

Rect Widget::bounds() const noexcept
{
    return impl_ == nullptr ? Rect{} : impl_->dirty.bounds;
}

Rect Widget::paint_bounds() const noexcept
{
    return impl_ == nullptr ? Rect{} : impl_->dirty.paint_bounds;
}

Rect Widget::clip_bounds() const noexcept
{
    return impl_ == nullptr ? Rect{} : impl_->dirty.clip_bounds;
}

DirtyReason Widget::dirty_reason() const noexcept
{
    return impl_ == nullptr ? DirtyReason::None : impl_->dirty.reason;
}

std::uint64_t Widget::last_mutation_frame() const noexcept
{
    return impl_ == nullptr ? 0 : impl_->dirty.last_mutation_frame;
}

const char* Widget::debug_id() const noexcept
{
    if (impl_ == nullptr) {
        return "";
    }
    return impl_->node.debug_id.empty() ? impl_->node.fallback_id.c_str()
                                        : impl_->node.debug_id.c_str();
}

const char* Widget::path() const noexcept
{
    if (impl_ == nullptr) {
        return "";
    }
    impl_->node.cached_path = widget_path(*impl_);
    return impl_->node.cached_path.c_str();
}

std::uint32_t Widget::child_count() const noexcept
{
    return impl_ == nullptr ? 0u : static_cast<std::uint32_t>(impl_->node.children.size());
}

bool Widget::enabled() const noexcept
{
    return impl_ == nullptr ? false : impl_->properties.enabled;
}

bool Widget::visible() const noexcept
{
    return impl_ == nullptr ? false : impl_->properties.visible;
}

Widget& Widget::debug_id(const char* value)
{
    if (impl_ != nullptr) {
        impl_->node.debug_id = value == nullptr ? "" : value;
        impl_->node.cached_path.clear();
        mutate_widget(*impl_, DirtyReason::ApiMutation, "debug_id");
    }
    return *this;
}

Widget& Widget::style(const char* value)
{
    if (impl_ != nullptr) {
        impl_->properties.style_name = value == nullptr ? "" : value;
        mutate_widget(*impl_, DirtyReason::StyleChanged | DirtyReason::Paint, "style");
    }
    return *this;
}

Widget& Widget::enabled(bool value)
{
    if (impl_ != nullptr && impl_->properties.enabled != value) {
        impl_->properties.enabled = value;
        mutate_widget(*impl_, DirtyReason::Input | DirtyReason::StyleChanged |
                                  DirtyReason::Paint,
                      value ? "enabled" : "disabled");
    }
    return *this;
}

Widget& Widget::visible(bool value)
{
    if (impl_ != nullptr && impl_->properties.visible != value) {
        impl_->properties.visible = value;
        mutate_widget(*impl_, DirtyReason::Layout | DirtyReason::Paint,
                      value ? "visible" : "hidden");
    }
    return *this;
}

Widget& Widget::padding(float value)
{
    if (impl_ != nullptr) {
        impl_->properties.padding = value;
        mutate_widget(*impl_, DirtyReason::Layout | DirtyReason::Paint, "padding");
    }
    return *this;
}

Widget& Widget::gap(float value)
{
    if (impl_ != nullptr) {
        impl_->properties.gap = value;
        mutate_widget(*impl_, DirtyReason::Layout | DirtyReason::Paint, "gap");
    }
    return *this;
}

Widget& Widget::size(float width, float height)
{
    if (impl_ != nullptr) {
        impl_->properties.requested_width = std::max(0.0f, width);
        impl_->properties.requested_height = std::max(0.0f, height);
        impl_->properties.width_mode =
            width > 0.0f ? LayoutSizeMode::Fixed : LayoutSizeMode::Auto;
        impl_->properties.height_mode =
            height > 0.0f ? LayoutSizeMode::Fixed : LayoutSizeMode::Auto;
        impl_->dirty.bounds.width = width;
        impl_->dirty.bounds.height = height;
        impl_->dirty.paint_bounds = impl_->dirty.bounds;
        impl_->dirty.clip_bounds = impl_->dirty.bounds;
        mutate_widget(*impl_, DirtyReason::Resize | DirtyReason::Layout | DirtyReason::Paint,
                      "size");
    }
    return *this;
}

Widget& Widget::fill_width()
{
    if (impl_ != nullptr) {
        impl_->properties.width_mode = LayoutSizeMode::Fill;
        mutate_widget(*impl_, DirtyReason::Layout | DirtyReason::Paint, "fill_width");
    }
    return *this;
}

Widget& Widget::fill_height()
{
    if (impl_ != nullptr) {
        impl_->properties.height_mode = LayoutSizeMode::Fill;
        mutate_widget(*impl_, DirtyReason::Layout | DirtyReason::Paint, "fill_height");
    }
    return *this;
}

Widget& Widget::fill()
{
    fill_width();
    fill_height();
    return *this;
}

Widget& Widget::flex(float grow)
{
    if (impl_ != nullptr) {
        impl_->properties.width_mode = LayoutSizeMode::Flex;
        impl_->properties.height_mode = LayoutSizeMode::Flex;
        impl_->properties.flex_grow = std::max(0.0f, grow);
        mutate_widget(*impl_, DirtyReason::Layout | DirtyReason::Paint, "flex");
    }
    return *this;
}

Widget& Widget::overflow(OverflowPolicy policy)
{
    if (impl_ != nullptr) {
        impl_->properties.overflow_policy = policy;
        impl_->properties.overflow_policy_explicit = true;
        mutate_widget(*impl_, DirtyReason::Layout | DirtyReason::Paint, "overflow");
    }
    return *this;
}

Widget& Widget::tooltip(const char* value)
{
    if (impl_ != nullptr) {
        impl_->properties.tooltip = value == nullptr ? "" : value;
        mutate_widget(*impl_, DirtyReason::ApiMutation | DirtyReason::Paint, "tooltip");
    }
    return *this;
}

Widget& Widget::mark_dirty(DirtyReason reason)
{
    if (impl_ != nullptr) {
        mutate_widget(*impl_, reason, "mark_dirty");
    }
    return *this;
}

bool Widget::add(const Widget& child)
{
    if (impl_ == nullptr || child.impl_ == nullptr) {
        return false;
    }
    return attach_child(*impl_, *child.impl_);
}

bool Widget::detach()
{
    if (impl_ == nullptr) {
        return false;
    }
    return detach_widget(*impl_);
}

WidgetImpl* Widget::impl() const noexcept
{
    return impl_;
}

Window::Window()
    : Widget(WidgetKind::Window)
{
}

Window::Window(const char* title)
    : Widget(WidgetKind::Window, title)
{
}

Window& Window::title(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.text = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Paint, "title");
    }
    return *this;
}

Window& Window::content(const Widget& child)
{
    add(child);
    return *this;
}

Window& Window::size(float width, float height)
{
    Widget::size(width, height);
    return *this;
}

Text::Text(const char* value)
    : Widget(WidgetKind::Text, value)
{
}

Text& Text::text(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.text = value == nullptr ? "" : value;
        impl()->properties.text_cursor = impl()->properties.text.size();
        impl()->properties.text_selection_anchor = impl()->properties.text_cursor;
        impl()->properties.text_selection_dragging = false;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout | DirtyReason::Paint,
                      "text");
    }
    return *this;
}

Text& Text::style(const char* value)
{
    Widget::style(value);
    return *this;
}

Text& Text::multiline(bool enabled)
{
    if (impl() != nullptr) {
        impl()->properties.text_multiline = enabled;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout | DirtyReason::Paint,
                      enabled ? "text_multiline_on" : "text_multiline_off");
    }
    return *this;
}

Button::Button(const char* label)
    : Widget(WidgetKind::Button, label)
{
}

Button& Button::label(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.text = value == nullptr ? "" : value;
        impl()->properties.text_cursor = impl()->properties.text.size();
        impl()->properties.text_selection_anchor = impl()->properties.text_cursor;
        impl()->properties.text_selection_dragging = false;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout | DirtyReason::Paint,
                      "label");
    }
    return *this;
}

Button& Button::content(const Widget& child)
{
    add(child);
    return *this;
}

namespace {

void set_button_image_resource(WidgetImpl& impl,
                               std::string& path_slot,
                               std::uint64_t& resource_slot,
                               const char* resource_path,
                               const char* action)
{
    path_slot = resource_path == nullptr ? "" : resource_path;
    if (resource_slot != 0) {
        default_runtime().resource_system().release_resource(resource_slot);
        resource_slot = 0;
    }
    if (!path_slot.empty()) {
        resource_slot = default_runtime().resource_system().register_resource(
            ResourceKind::Image, impl.object.object_id, widget_path(impl).c_str(),
            path_slot.c_str(), ResourceCacheState::Uncached);
    }
    mutate_widget(impl, DirtyReason::ResourceChanged | DirtyReason::Paint, action);
    diagnostics_event_ex("resource", action, impl.object.object_id, impl.object.generation,
                         current_frame_id(), widget_path(impl).c_str(), path_slot.c_str());
}

} // namespace

Button& Button::normal_image(const char* resource_path)
{
    if (impl() != nullptr) {
        set_button_image_resource(*impl(), impl()->properties.button_normal_image_path,
                                  impl()->properties.button_normal_image_resource_id,
                                  resource_path, "button_normal_image");
    }
    return *this;
}

Button& Button::hover_image(const char* resource_path)
{
    if (impl() != nullptr) {
        set_button_image_resource(*impl(), impl()->properties.button_hover_image_path,
                                  impl()->properties.button_hover_image_resource_id,
                                  resource_path, "button_hover_image");
    }
    return *this;
}

Button& Button::pressed_image(const char* resource_path)
{
    if (impl() != nullptr) {
        set_button_image_resource(*impl(), impl()->properties.button_pressed_image_path,
                                  impl()->properties.button_pressed_image_resource_id,
                                  resource_path, "button_pressed_image");
    }
    return *this;
}

Button& Button::click_image(const char* resource_path)
{
    return pressed_image(resource_path);
}

Button& Button::image_fit(ImageFit value)
{
    if (impl() != nullptr) {
        impl()->properties.image_fit = value;
        mutate_widget(*impl(), DirtyReason::Paint, "button_image_fit");
    }
    return *this;
}

Button& Button::text_padding(float value)
{
    if (impl() != nullptr) {
        impl()->properties.button_text_padding = std::max(0.0f, value);
        impl()->properties.has_button_text_padding = true;
        mutate_widget(*impl(), DirtyReason::Layout | DirtyReason::Paint, "button_text_padding");
    }
    return *this;
}

Button& Button::background(Color value)
{
    if (impl() != nullptr) {
        impl()->properties.button_background = value;
        impl()->properties.has_button_background = true;
        mutate_widget(*impl(), DirtyReason::StyleChanged | DirtyReason::Paint, "button_background");
    }
    return *this;
}

Button& Button::hover_background(Color value)
{
    if (impl() != nullptr) {
        impl()->properties.button_hover_background = value;
        impl()->properties.has_button_hover_background = true;
        mutate_widget(*impl(), DirtyReason::StyleChanged | DirtyReason::Paint,
                      "button_hover_background");
    }
    return *this;
}

Button& Button::pressed_background(Color value)
{
    if (impl() != nullptr) {
        impl()->properties.button_pressed_background = value;
        impl()->properties.has_button_pressed_background = true;
        mutate_widget(*impl(), DirtyReason::StyleChanged | DirtyReason::Paint,
                      "button_pressed_background");
    }
    return *this;
}

Button& Button::click_background(Color value)
{
    return pressed_background(value);
}

Button& Button::radius(float value)
{
    if (impl() != nullptr) {
        impl()->properties.button_radius = std::max(0.0f, value);
        impl()->properties.has_button_radius = true;
        mutate_widget(*impl(), DirtyReason::StyleChanged | DirtyReason::Paint, "button_radius");
    }
    return *this;
}

Button& Button::on_click(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "on_click");
    }
    return *this;
}

bool Button::click()
{
    if (impl() == nullptr) {
        return false;
    }
    mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Paint, "click");
    EventDispatchResult result = default_runtime().event_system().dispatch_click(*impl());
    return result.handled && !result.callback_failed;
}

CheckBox::CheckBox(const char* label)
    : Widget(WidgetKind::CheckBox, label)
{
}

CheckBox& CheckBox::label(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.text = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout | DirtyReason::Paint,
                      "check_box_label");
    }
    return *this;
}

CheckBox& CheckBox::checked(bool value)
{
    if (impl() != nullptr && impl()->properties.checked != value) {
        impl()->properties.checked = value;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Paint, "check_box_checked");
    }
    return *this;
}

bool CheckBox::checked() const noexcept
{
    return impl() != nullptr && impl()->properties.checked;
}

CheckBox& CheckBox::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "check_box_on_change");
    }
    return *this;
}

bool CheckBox::toggle()
{
    if (impl() == nullptr) {
        return false;
    }
    EventDispatchResult result = default_runtime().event_system().dispatch_click(*impl());
    return result.handled && !result.callback_failed;
}

RadioButton::RadioButton(const char* label)
    : Widget(WidgetKind::RadioButton, label)
{
}

RadioButton& RadioButton::label(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.text = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout | DirtyReason::Paint,
                      "radio_button_label");
    }
    return *this;
}

RadioButton& RadioButton::checked(bool value)
{
    if (impl() != nullptr && impl()->properties.checked != value) {
        impl()->properties.checked = value;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Paint,
                      value ? "radio_button_checked" : "radio_button_unchecked");
    }
    return *this;
}

bool RadioButton::checked() const noexcept
{
    return impl() != nullptr && impl()->properties.checked;
}

RadioButton& RadioButton::group(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.radio_group = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "radio_button_group");
    }
    return *this;
}

RadioButton& RadioButton::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "radio_button_on_change");
    }
    return *this;
}

bool RadioButton::select()
{
    if (impl() == nullptr) {
        return false;
    }
    EventDispatchResult result = default_runtime().event_system().dispatch_click(*impl());
    return result.handled && !result.callback_failed;
}

Switch::Switch(const char* label)
    : Widget(WidgetKind::Switch, label)
{
}

Switch& Switch::label(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.text = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout | DirtyReason::Paint,
                      "switch_label");
    }
    return *this;
}

Switch& Switch::checked(bool value)
{
    if (impl() != nullptr && impl()->properties.checked != value) {
        impl()->properties.checked = value;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Paint,
                      value ? "switch_checked" : "switch_unchecked");
    }
    return *this;
}

bool Switch::checked() const noexcept
{
    return impl() != nullptr && impl()->properties.checked;
}

Switch& Switch::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "switch_on_change");
    }
    return *this;
}

bool Switch::toggle()
{
    if (impl() == nullptr) {
        return false;
    }
    EventDispatchResult result = default_runtime().event_system().dispatch_click(*impl());
    return result.handled && !result.callback_failed;
}

Input::Input()
    : Widget(WidgetKind::Input)
{
}

Input& Input::placeholder(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.placeholder = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Paint, "placeholder");
    }
    return *this;
}

Input& Input::value(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.text = value == nullptr ? "" : value;
        impl()->properties.text_cursor = impl()->properties.text.size();
        impl()->properties.text_selection_anchor = impl()->properties.text_cursor;
        impl()->properties.text_selection_dragging = false;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout | DirtyReason::Paint,
                      "input_value");
    }
    return *this;
}

const char* Input::value() const noexcept
{
    return impl() == nullptr ? "" : impl()->properties.text.c_str();
}

Input& Input::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.text_change_callback = callback;
        impl()->properties.text_change_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "input_on_change");
    }
    return *this;
}

TextArea::TextArea()
    : Widget(WidgetKind::TextArea)
{
    if (impl() != nullptr) {
        impl()->properties.text_multiline = true;
    }
}

TextArea& TextArea::placeholder(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.placeholder = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Paint,
                      "text_area_placeholder");
    }
    return *this;
}

TextArea& TextArea::value(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.text = value == nullptr ? "" : value;
        impl()->properties.text_cursor = impl()->properties.text.size();
        impl()->properties.text_selection_anchor = impl()->properties.text_cursor;
        impl()->properties.text_selection_dragging = false;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout | DirtyReason::Paint,
                      "text_area_value");
    }
    return *this;
}

const char* TextArea::value() const noexcept
{
    return impl() == nullptr ? "" : impl()->properties.text.c_str();
}

TextArea& TextArea::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.text_change_callback = callback;
        impl()->properties.text_change_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "text_area_on_change");
    }
    return *this;
}

Image::Image()
    : Widget(WidgetKind::Image)
{
}

Image::Image(const char* resource_path)
    : Widget(WidgetKind::Image)
{
    source(resource_path);
}

Image& Image::source(const char* resource_path)
{
    if (impl() != nullptr) {
        impl()->properties.resource_path = resource_path == nullptr ? "" : resource_path;
        if (impl()->properties.image_resource_id != 0) {
            default_runtime().resource_system().release_resource(impl()->properties.image_resource_id);
            impl()->properties.image_resource_id = 0;
        }
        if (!impl()->properties.resource_path.empty()) {
            impl()->properties.image_resource_id =
                default_runtime().resource_system().register_resource(
                    ResourceKind::Image, impl()->object.object_id, path(),
                    impl()->properties.resource_path.c_str(), ResourceCacheState::Uncached);
        }
        mutate_widget(*impl(),
                      DirtyReason::ResourceChanged | DirtyReason::Layout | DirtyReason::Paint,
                      "image_source");
        diagnostics_event_ex("resource", "image_source", impl()->object.object_id,
                             impl()->object.generation, current_frame_id(), path(),
                             impl()->properties.resource_path.c_str());
    }
    return *this;
}

Image& Image::fit(ImageFit value)
{
    if (impl() != nullptr) {
        impl()->properties.image_fit = value;
        mutate_widget(*impl(), DirtyReason::Paint, "image_fit");
    }
    return *this;
}

Image& Image::radius(float value)
{
    if (impl() != nullptr) {
        impl()->properties.image_radius = std::max(0.0f, value);
        mutate_widget(*impl(), DirtyReason::Paint, "image_radius");
    }
    return *this;
}

Progress::Progress()
    : Widget(WidgetKind::Progress)
{
}

Progress& Progress::value(float value)
{
    if (impl() != nullptr) {
        impl()->properties.numeric_value = value;
        mutate_widget(*impl(), DirtyReason::Paint, "progress_value");
    }
    return *this;
}

Slider::Slider()
    : Widget(WidgetKind::Slider)
{
}

Slider& Slider::value(float value)
{
    if (impl() != nullptr) {
        const float clamped = std::max(0.0f, std::min(1.0f, value));
        if (impl()->properties.numeric_value != clamped) {
            impl()->properties.numeric_value = clamped;
            mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Paint, "slider_value");
        }
    }
    return *this;
}

float Slider::value() const noexcept
{
    return impl() == nullptr ? 0.0f : impl()->properties.numeric_value;
}

Slider& Slider::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "slider_on_change");
    }
    return *this;
}

Select::Select()
    : Widget(WidgetKind::Select)
{
}

Select& Select::placeholder(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.placeholder = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Paint,
                      "select_placeholder");
    }
    return *this;
}

Select& Select::add_option(const char* label)
{
    if (impl() == nullptr) {
        return *this;
    }
    WidgetImpl* option = make_widget_impl(WidgetKind::SelectOption, label);
    if (option == nullptr) {
        return *this;
    }
    option->node.fallback_id = "option";
    if (attach_child(*impl(), *option)) {
        const std::uint32_t option_index =
            static_cast<std::uint32_t>(impl()->node.children.size() - 1u);
        if (!impl()->properties.has_selected_index) {
            impl()->properties.selected_index = option_index;
            impl()->properties.has_selected_index = true;
            impl()->properties.selected_text_cache = option->properties.text;
            mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Paint,
                          "select_default_option");
        }
    }
    release_widget_impl(option);
    return *this;
}

Select& Select::selected_index(std::uint32_t index)
{
    if (impl() != nullptr && index < impl()->node.children.size()) {
        impl()->properties.selected_index = index;
        impl()->properties.has_selected_index = true;
        WidgetImpl* option = impl()->node.children[index];
        impl()->properties.selected_text_cache =
            option == nullptr ? "" : option->properties.text;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::TextChanged |
                                  DirtyReason::Paint,
                      "select_selected_index");
    }
    return *this;
}

std::uint32_t Select::selected_index() const noexcept
{
    return impl() == nullptr ? 0u : impl()->properties.selected_index;
}

const char* Select::selected_text() const noexcept
{
    return impl() == nullptr ? "" : impl()->properties.selected_text_cache.c_str();
}

Select& Select::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "select_on_change");
    }
    return *this;
}

bool Select::open()
{
    if (impl() == nullptr) {
        return false;
    }
    if (!impl()->properties.select_popup_open) {
        impl()->properties.select_popup_open = true;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      "select_open");
    }
    return true;
}

bool Select::close()
{
    if (impl() == nullptr) {
        return false;
    }
    if (impl()->properties.select_popup_open) {
        impl()->properties.select_popup_open = false;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      "select_close");
    }
    return true;
}

ListView::ListView()
    : Widget(WidgetKind::ListView)
{
    padding(4.0f);
    gap(2.0f);
}

ListView& ListView::add_item(const char* label)
{
    if (impl() == nullptr) {
        return *this;
    }
    WidgetImpl* item = make_widget_impl(WidgetKind::ListItem, label);
    if (item == nullptr) {
        return *this;
    }
    item->node.fallback_id = "item";
    if (attach_child(*impl(), *item)) {
        const std::uint32_t item_index =
            static_cast<std::uint32_t>(impl()->node.children.size() - 1u);
        if (!impl()->properties.has_selected_index) {
            impl()->properties.selected_index = item_index;
            impl()->properties.has_selected_index = true;
            impl()->properties.selected_text_cache = item->properties.text;
            mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Paint,
                          "list_default_item");
        }
    }
    release_widget_impl(item);
    return *this;
}

ListView& ListView::selected_index(std::uint32_t index)
{
    if (impl() != nullptr && index < impl()->node.children.size()) {
        impl()->properties.selected_index = index;
        impl()->properties.has_selected_index = true;
        WidgetImpl* item = impl()->node.children[index];
        impl()->properties.selected_text_cache = item == nullptr ? "" : item->properties.text;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::TextChanged |
                                  DirtyReason::Paint,
                      "list_selected_index");
    }
    return *this;
}

std::uint32_t ListView::selected_index() const noexcept
{
    return impl() == nullptr ? 0u : impl()->properties.selected_index;
}

const char* ListView::selected_text() const noexcept
{
    return impl() == nullptr ? "" : impl()->properties.selected_text_cache.c_str();
}

ListView& ListView::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "list_on_change");
    }
    return *this;
}

TreeItem::TreeItem(const char* label)
    : Widget(WidgetKind::TreeItem, label)
{
    impl()->node.fallback_id = "tree_item";
    padding(0.0f);
}

TreeItem& TreeItem::label(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.text = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Paint, "tree_item_label");
    }
    return *this;
}

TreeItem& TreeItem::expanded(bool value)
{
    if (impl() != nullptr && impl()->properties.tree_expanded != value) {
        impl()->properties.tree_expanded = value;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      value ? "tree_item_expand" : "tree_item_collapse");
    }
    return *this;
}

TreeItem& TreeItem::toggle_expand()
{
    if (impl() != nullptr) {
        expanded(!impl()->properties.tree_expanded);
    }
    return *this;
}

TreeItem& TreeItem::selected(bool value)
{
    if (impl() != nullptr && impl()->properties.tree_selected != value) {
        impl()->properties.tree_selected = value;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Paint,
                      value ? "tree_item_selected" : "tree_item_unselected");
    }
    return *this;
}

bool TreeItem::expanded() const noexcept
{
    return impl() != nullptr && impl()->properties.tree_expanded;
}

bool TreeItem::selected() const noexcept
{
    return impl() != nullptr && impl()->properties.tree_selected;
}

bool TreeItem::select()
{
    if (impl() == nullptr) {
        return false;
    }
    EventDispatchResult result = default_runtime().event_system().dispatch_click(*impl());
    return result.handled && !result.callback_failed;
}

TreeView::TreeView()
    : Widget(WidgetKind::TreeView)
{
    padding(4.0f);
    gap(0.0f);
}

TreeView& TreeView::add_item(const TreeItem& item)
{
    add(item);
    return *this;
}

TreeView& TreeView::selected_id(ObjectId object_id)
{
    if (impl() != nullptr && impl()->properties.selected_object_id != object_id) {
        impl()->properties.selected_object_id = object_id;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Paint, "tree_selected_id");
    }
    return *this;
}

ObjectId TreeView::selected_id() const noexcept
{
    return impl() == nullptr ? 0 : impl()->properties.selected_object_id;
}

const char* TreeView::selected_text() const noexcept
{
    return impl() == nullptr ? "" : impl()->properties.selected_text_cache.c_str();
}

TreeView& TreeView::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "tree_on_change");
    }
    return *this;
}

TableView::TableView()
    : Widget(WidgetKind::TableView)
{
    padding(0.0f);
    overflow(OverflowPolicy::Clip);
}

TableView& TableView::add_column(const char* label, float width)
{
    if (impl() != nullptr) {
        impl()->properties.table_columns.push_back(
            TableColumnData{label == nullptr ? "" : label, std::max(0.0f, width)});
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout |
                                   DirtyReason::Paint,
                      "table_add_column");
    }
    return *this;
}

TableView& TableView::add_row(const char* first,
                              const char* second,
                              const char* third,
                              const char* fourth)
{
    if (impl() != nullptr) {
        std::vector<std::string> row;
        row.push_back(first == nullptr ? "" : first);
        row.push_back(second == nullptr ? "" : second);
        row.push_back(third == nullptr ? "" : third);
        row.push_back(fourth == nullptr ? "" : fourth);
        while (!row.empty() && row.back().empty() &&
               row.size() > impl()->properties.table_columns.size()) {
            row.pop_back();
        }
        impl()->properties.table_rows.push_back(std::move(row));
        if (!impl()->properties.has_selected_index) {
            impl()->properties.selected_index = 0;
        }
        clamp_table_selection(*impl());
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout |
                                   DirtyReason::Paint,
                      "table_add_row");
    }
    return *this;
}

TableView& TableView::set_cell(std::uint32_t row, std::uint32_t column, const char* value)
{
    if (impl() != nullptr && row < impl()->properties.table_rows.size()) {
        std::vector<std::string>& table_row = impl()->properties.table_rows[row];
        if (column >= table_row.size()) {
            table_row.resize(column + 1u);
        }
        table_row[column] = value == nullptr ? "" : value;
        if (impl()->properties.has_selected_index &&
            impl()->properties.selected_index == row) {
            impl()->properties.selected_text_cache =
                table_row.empty() ? "" : table_row.front();
        }
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Paint, "table_set_cell");
    }
    return *this;
}

TableView& TableView::clear_rows()
{
    if (impl() != nullptr) {
        impl()->properties.table_rows.clear();
        clamp_table_selection(*impl());
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout |
                                  DirtyReason::Paint,
                      "table_clear_rows");
    }
    return *this;
}

TableView& TableView::clear_columns()
{
    if (impl() != nullptr) {
        impl()->properties.table_columns.clear();
        impl()->properties.table_rows.clear();
        impl()->properties.table_sort_column = 0xffffffffu;
        impl()->properties.table_sort_ascending = true;
        impl()->properties.table_column_resizing = false;
        impl()->properties.table_resize_suppress_click = false;
        impl()->properties.table_resize_column = 0xffffffffu;
        clamp_table_selection(*impl());
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout |
                                  DirtyReason::Paint,
                      "table_clear_columns");
    }
    return *this;
}

TableView& TableView::clear()
{
    return clear_columns();
}

TableView& TableView::selected_row(std::uint32_t row)
{
    if (impl() != nullptr && row < impl()->properties.table_rows.size()) {
        impl()->properties.selected_index = row;
        impl()->properties.has_selected_index = true;
        const std::vector<std::string>& table_row = impl()->properties.table_rows[row];
        impl()->properties.selected_text_cache = table_row.empty() ? "" : table_row.front();
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Paint, "table_selected_row");
    }
    return *this;
}

TableView& TableView::sort_by_column(std::uint32_t column, bool ascending)
{
    if (impl() != nullptr && column < impl()->properties.table_columns.size()) {
        sort_table_rows(*impl(), column, ascending);
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::TextChanged |
                                  DirtyReason::Paint,
                      ascending ? "table_sort_ascending" : "table_sort_descending");
    }
    return *this;
}

std::uint32_t TableView::selected_row() const noexcept
{
    return impl() == nullptr ? 0u : impl()->properties.selected_index;
}

const char* TableView::selected_text() const noexcept
{
    return impl() == nullptr ? "" : impl()->properties.selected_text_cache.c_str();
}

std::uint32_t TableView::row_count() const noexcept
{
    return impl() == nullptr ? 0u
                             : static_cast<std::uint32_t>(impl()->properties.table_rows.size());
}

std::uint32_t TableView::column_count() const noexcept
{
    return impl() == nullptr ? 0u
                             : static_cast<std::uint32_t>(impl()->properties.table_columns.size());
}

std::uint32_t TableView::sorted_column() const noexcept
{
    return impl() == nullptr ? 0xffffffffu : impl()->properties.table_sort_column;
}

bool TableView::sort_ascending() const noexcept
{
    return impl() == nullptr ? true : impl()->properties.table_sort_ascending;
}

float TableView::column_width(std::uint32_t column) const noexcept
{
    if (impl() == nullptr || column >= impl()->properties.table_columns.size()) {
        return 0.0f;
    }
    return impl()->properties.table_columns[column].width;
}

TableView& TableView::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "table_on_change");
    }
    return *this;
}

Tabs::Tabs()
    : Widget(WidgetKind::Tabs)
{
}

Tabs& Tabs::add_tab(const char* label, const Widget& content)
{
    const std::uint32_t before = child_count();
    if (impl() != nullptr && add(content) && child_count() == before + 1u) {
        impl()->properties.tab_labels.push_back(label == nullptr ? "" : label);
        if (impl()->node.children.size() == 1) {
            impl()->properties.selected_tab_index = 0;
        }
        mutate_widget(*impl(), DirtyReason::Attach | DirtyReason::Layout | DirtyReason::Paint,
                      "tabs_add_tab");
    }
    return *this;
}

Tabs& Tabs::selected_index(std::uint32_t index)
{
    if (impl() != nullptr && index < impl()->node.children.size() &&
        impl()->properties.selected_tab_index != index) {
        impl()->properties.selected_tab_index = index;
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      "tabs_selected_index");
    }
    return *this;
}

std::uint32_t Tabs::selected_index() const noexcept
{
    return impl() == nullptr ? 0u : impl()->properties.selected_tab_index;
}

Tabs& Tabs::on_change(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "tabs_on_change");
    }
    return *this;
}

Toolbar::Toolbar()
    : Widget(WidgetKind::Toolbar)
{
    padding(8.0f);
    gap(8.0f);
}

Column::Column()
    : Widget(WidgetKind::Column)
{
}

Row::Row()
    : Widget(WidgetKind::Row)
{
}

Padding::Padding(float value)
    : Widget(WidgetKind::Padding)
{
    padding(value);
}

Align::Align()
    : Widget(WidgetKind::Align)
{
}

SizedBox::SizedBox(float width, float height)
    : Widget(WidgetKind::SizedBox)
{
    size(width, height);
}

ScrollView::ScrollView()
    : Widget(WidgetKind::ScrollView)
{
}

Dialog::Dialog()
    : Widget(WidgetKind::Dialog)
{
}

Dialog& Dialog::content(const Widget& child)
{
    add(child);
    return *this;
}

Dialog& Dialog::open(bool value)
{
    if (impl() != nullptr && impl()->properties.dialog_open != value) {
        impl()->properties.dialog_open = value;
        impl()->properties.dialog_dragging = false;
        impl()->properties.dialog_suppress_backdrop_click = false;
        if (value) {
            impl()->properties.dialog_offset_x = 0.0f;
            impl()->properties.dialog_offset_y = 0.0f;
        }
        mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      value ? "dialog_open" : "dialog_close");
    }
    return *this;
}

Dialog& Dialog::close()
{
    return open(false);
}

Dialog& Dialog::modal(bool value)
{
    if (impl() != nullptr && impl()->properties.dialog_modal != value) {
        impl()->properties.dialog_modal = value;
        mutate_widget(*impl(), DirtyReason::ApiMutation | DirtyReason::Paint,
                      value ? "dialog_modal_on" : "dialog_modal_off");
    }
    return *this;
}

Dialog& Dialog::backdrop_closes(bool value)
{
    if (impl() != nullptr && impl()->properties.dialog_backdrop_closes != value) {
        impl()->properties.dialog_backdrop_closes = value;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "dialog_backdrop_closes");
    }
    return *this;
}

Dialog& Dialog::escape_closes(bool value)
{
    if (impl() != nullptr && impl()->properties.dialog_escape_closes != value) {
        impl()->properties.dialog_escape_closes = value;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "dialog_escape_closes");
    }
    return *this;
}

bool Dialog::is_open() const noexcept
{
    return impl() != nullptr && impl()->properties.dialog_open;
}

SplitView::SplitView()
    : Widget(WidgetKind::SplitView)
{
    overflow(OverflowPolicy::Clip);
}

SplitView& SplitView::first(const Widget& child)
{
    if (impl() != nullptr && impl()->node.children.empty()) {
        add(child);
    }
    return *this;
}

SplitView& SplitView::second(const Widget& child)
{
    if (impl() != nullptr && impl()->node.children.size() == 1) {
        add(child);
    }
    return *this;
}

SplitView& SplitView::orientation(SplitOrientation value)
{
    if (impl() != nullptr && impl()->properties.split_orientation != value) {
        impl()->properties.split_orientation = value;
        mutate_widget(*impl(), DirtyReason::ApiMutation | DirtyReason::Layout |
                                  DirtyReason::Paint,
                      "split_orientation");
    }
    return *this;
}

SplitView& SplitView::ratio(float value)
{
    if (impl() != nullptr) {
        const float clamped = std::max(0.05f, std::min(0.95f, value));
        if (impl()->properties.split_ratio != clamped) {
            impl()->properties.split_ratio = clamped;
            mutate_widget(*impl(), DirtyReason::ApiMutation | DirtyReason::Layout |
                                      DirtyReason::Paint,
                          "split_ratio");
        }
    }
    return *this;
}

SplitView& SplitView::min_pane_size(float value)
{
    if (impl() != nullptr) {
        impl()->properties.split_min_pane_size = std::max(0.0f, value);
        mutate_widget(*impl(), DirtyReason::ApiMutation | DirtyReason::Layout |
                                  DirtyReason::Paint,
                      "split_min_pane_size");
    }
    return *this;
}

SplitView& SplitView::handle_size(float value)
{
    if (impl() != nullptr) {
        impl()->properties.split_handle_size = std::max(1.0f, value);
        mutate_widget(*impl(), DirtyReason::ApiMutation | DirtyReason::Layout |
                                  DirtyReason::Paint,
                      "split_handle_size");
    }
    return *this;
}

float SplitView::ratio() const noexcept
{
    return impl() == nullptr ? 0.5f : impl()->properties.split_ratio;
}

SplitOrientation SplitView::orientation() const noexcept
{
    return impl() == nullptr ? SplitOrientation::Horizontal
                             : impl()->properties.split_orientation;
}

MenuBar::MenuBar()
    : Widget(WidgetKind::MenuBar)
{
    padding(0.0f);
    gap(0.0f);
}

MenuItem::MenuItem(const char* label)
    : Widget(WidgetKind::MenuItem, label)
{
}

MenuItem& MenuItem::label(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.text = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout | DirtyReason::Paint,
                      "menu_item_label");
    }
    return *this;
}

MenuItem& MenuItem::enabled(bool value)
{
    if (impl() != nullptr) {
        impl()->properties.enabled = value;
        impl()->properties.menu_enabled = value;
        mutate_widget(*impl(), DirtyReason::ApiMutation | DirtyReason::Paint,
                      "menu_item_enabled");
    }
    return *this;
}

MenuItem& MenuItem::checked(bool value)
{
    if (impl() != nullptr) {
        impl()->properties.menu_checked = value;
        mutate_widget(*impl(), DirtyReason::ApiMutation | DirtyReason::Paint,
                      "menu_item_checked");
    }
    return *this;
}

MenuItem& MenuItem::shortcut(const char* value)
{
    if (impl() != nullptr) {
        impl()->properties.menu_shortcut = value == nullptr ? "" : value;
        mutate_widget(*impl(), DirtyReason::TextChanged | DirtyReason::Layout | DirtyReason::Paint,
                      "menu_item_shortcut");
    }
    return *this;
}

MenuItem& MenuItem::on_click(EventCallback callback, void* user_data)
{
    if (impl() != nullptr) {
        impl()->properties.click_callback = callback;
        impl()->properties.click_user_data = user_data;
        mutate_widget(*impl(), DirtyReason::ApiMutation, "menu_item_on_click");
    }
    return *this;
}

bool MenuItem::click()
{
    if (impl() == nullptr) {
        return false;
    }
    mutate_widget(*impl(), DirtyReason::Input | DirtyReason::Paint, "menu_item_click");
    EventDispatchResult result = default_runtime().event_system().dispatch_click(*impl());
    return result.handled && !result.callback_failed;
}

Separator::Separator()
    : Widget(WidgetKind::Separator)
{
}

Spacer::Spacer()
    : Widget(WidgetKind::Spacer)
{
}

} // namespace fiui
