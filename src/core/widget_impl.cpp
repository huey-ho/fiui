#include "core/widget_impl.h"

#include "diagnostics/diagnostics_internal.h"
#include "runtime/runtime.h"

#include <cstdio>

namespace fiui {
namespace {

void release_children(WidgetImpl& impl)
{
    for (WidgetImpl* child : impl.node.children) {
        if (child != nullptr) {
            child->node.parent = nullptr;
            child->object.lifecycle_state = LifecycleState::Detached;
            release_widget_impl(child);
        }
    }
    impl.node.children.clear();
}

void release_framework_resources(WidgetImpl& impl)
{
    if (impl.properties.image_resource_id != 0) {
        default_runtime().resource_system().release_resource(impl.properties.image_resource_id);
        impl.properties.image_resource_id = 0;
    }
    if (impl.properties.button_normal_image_resource_id != 0) {
        default_runtime().resource_system().release_resource(
            impl.properties.button_normal_image_resource_id);
        impl.properties.button_normal_image_resource_id = 0;
    }
    if (impl.properties.button_hover_image_resource_id != 0) {
        default_runtime().resource_system().release_resource(
            impl.properties.button_hover_image_resource_id);
        impl.properties.button_hover_image_resource_id = 0;
    }
    if (impl.properties.button_pressed_image_resource_id != 0) {
        default_runtime().resource_system().release_resource(
            impl.properties.button_pressed_image_resource_id);
        impl.properties.button_pressed_image_resource_id = 0;
    }
}

std::string fallback_id_for(WidgetKind kind, ObjectId object_id)
{
    char buffer[96] = {};
    std::snprintf(buffer, sizeof(buffer), "%s_%llu", widget_kind_name(kind),
                  static_cast<unsigned long long>(object_id));
    return buffer;
}

} // namespace

WidgetImpl* make_widget_impl(WidgetKind kind, const char* initial_text)
{
    auto* impl = new WidgetImpl();
    impl->object.object_id = default_runtime().allocate_object_id();
    impl->node.kind = kind;
    impl->node.fallback_id = fallback_id_for(kind, impl->object.object_id);
    if (initial_text != nullptr) {
        impl->properties.text = initial_text;
    }
    default_runtime().register_object(*impl);
    mutate_widget(*impl, DirtyReason::ApiMutation, "create");
    diagnostics_event_ex("lifecycle", "create", impl->object.object_id, impl->object.generation,
                         current_frame_id(), widget_path(*impl).c_str(), widget_kind_name(kind));
    return impl;
}

void retain_widget_impl(WidgetImpl* impl) noexcept
{
    if (impl != nullptr) {
        impl->object.ref_count.fetch_add(1, std::memory_order_relaxed);
    }
}

void release_widget_impl(WidgetImpl* impl) noexcept
{
    if (impl == nullptr) {
        return;
    }
    if (impl->object.ref_count.fetch_sub(1, std::memory_order_acq_rel) != 1) {
        return;
    }
    impl->object.lifecycle_state = LifecycleState::Destroying;
    default_runtime().event_system().clear_targets_for_destroying(*impl);
    diagnostics_event_ex("lifecycle", "destroying", impl->object.object_id,
                         impl->object.generation, current_frame_id(), widget_path(*impl).c_str(),
                         widget_kind_name(impl->node.kind));
    release_children(*impl);
    release_framework_resources(*impl);
    impl->object.lifecycle_state = LifecycleState::Destroyed;
    diagnostics_event_ex("lifecycle", "destroyed", impl->object.object_id,
                         impl->object.generation, current_frame_id(),
                         impl->node.fallback_id.c_str(), widget_kind_name(impl->node.kind));
    default_runtime().unregister_object(*impl);
    delete impl;
}

void mutate_widget(WidgetImpl& impl, DirtyReason reason, const char* action)
{
    impl.dirty.reason |= reason;
    impl.dirty.last_mutation_frame = current_frame_id();
    if (impl.dirty.paint_bounds.width <= 0.0f && impl.dirty.paint_bounds.height <= 0.0f) {
        impl.dirty.paint_bounds = impl.dirty.bounds;
    }
    diagnostics_event_ex("widget", action, impl.object.object_id, impl.object.generation,
                         current_frame_id(), widget_path(impl).c_str(),
                         dirty_reason_name(reason));
    default_runtime().frame_scheduler().request_frame(dirty_reason_name(reason), action,
                                                      impl.object.object_id,
                                                      impl.object.generation, current_frame_id(),
                                                      widget_path(impl).c_str());
}

bool attach_child(WidgetImpl& parent, WidgetImpl& child)
{
    if (&parent == &child) {
        diagnostics_event_ex("lifecycle", "attach_rejected", child.object.object_id,
                             child.object.generation, current_frame_id(),
                             widget_path(child).c_str(), "cannot attach widget to itself");
        return false;
    }
    if (child.node.parent != nullptr) {
        diagnostics_event_ex("lifecycle", "duplicate_attach", child.object.object_id,
                             child.object.generation, current_frame_id(),
                             widget_path(child).c_str(), "child already has a parent");
        mutate_widget(parent, DirtyReason::Attach, "attach_failed");
        return false;
    }

    retain_widget_impl(&child);
    child.node.parent = &parent;
    child.object.lifecycle_state = LifecycleState::Attached;
    parent.node.children.push_back(&child);
    parent.node.cached_path.clear();
    child.node.cached_path.clear();
    mutate_widget(parent, DirtyReason::Attach | DirtyReason::Layout | DirtyReason::Paint,
                  "attach_child");
    mutate_widget(child, DirtyReason::Attach | DirtyReason::Layout | DirtyReason::Paint,
                  "attached_to_parent");
    diagnostics_event_ex("lifecycle", "attach", child.object.object_id, child.object.generation,
                         current_frame_id(), widget_path(child).c_str(),
                         widget_path(parent).c_str());
    return true;
}

bool detach_widget(WidgetImpl& child)
{
    WidgetImpl* parent = child.node.parent;
    if (parent == nullptr) {
        diagnostics_event_ex("lifecycle", "detach_rejected", child.object.object_id,
                             child.object.generation, current_frame_id(),
                             widget_path(child).c_str(), "child has no parent");
        return false;
    }

    for (auto it = parent->node.children.begin(); it != parent->node.children.end(); ++it) {
        if (*it == &child) {
            parent->node.children.erase(it);
            break;
        }
    }
    child.node.parent = nullptr;
    child.object.lifecycle_state = LifecycleState::Detached;
    child.node.cached_path.clear();
    parent->node.cached_path.clear();
    mutate_widget(*parent, DirtyReason::Detach | DirtyReason::Layout | DirtyReason::Paint,
                  "detach_child");
    mutate_widget(child, DirtyReason::Detach, "detached_from_parent");
    diagnostics_event_ex("lifecycle", "detach", child.object.object_id, child.object.generation,
                         current_frame_id(), widget_path(child).c_str(),
                         widget_path(*parent).c_str());
    release_widget_impl(&child);
    return true;
}

const char* widget_kind_name(WidgetKind kind) noexcept
{
    switch (kind) {
    case WidgetKind::Widget:
        return "widget";
    case WidgetKind::Window:
        return "window";
    case WidgetKind::Text:
        return "text";
    case WidgetKind::Button:
        return "button";
    case WidgetKind::CheckBox:
        return "check_box";
    case WidgetKind::RadioButton:
        return "radio_button";
    case WidgetKind::Switch:
        return "switch";
    case WidgetKind::Input:
        return "input";
    case WidgetKind::TextArea:
        return "text_area";
    case WidgetKind::Image:
        return "image";
    case WidgetKind::Progress:
        return "progress";
    case WidgetKind::Slider:
        return "slider";
    case WidgetKind::Select:
        return "select";
    case WidgetKind::SelectOption:
        return "select_option";
    case WidgetKind::ListView:
        return "list_view";
    case WidgetKind::ListItem:
        return "list_item";
    case WidgetKind::TreeView:
        return "tree_view";
    case WidgetKind::TreeItem:
        return "tree_item";
    case WidgetKind::TableView:
        return "table_view";
    case WidgetKind::Tabs:
        return "tabs";
    case WidgetKind::Toolbar:
        return "toolbar";
    case WidgetKind::Column:
        return "column";
    case WidgetKind::Row:
        return "row";
    case WidgetKind::Padding:
        return "padding";
    case WidgetKind::Align:
        return "align";
    case WidgetKind::SizedBox:
        return "sized_box";
    case WidgetKind::ScrollView:
        return "scroll_view";
    case WidgetKind::Dialog:
        return "dialog";
    case WidgetKind::SplitView:
        return "split_view";
    case WidgetKind::MenuBar:
        return "menu_bar";
    case WidgetKind::MenuItem:
        return "menu_item";
    case WidgetKind::Separator:
        return "separator";
    case WidgetKind::Spacer:
        return "spacer";
    }
    return "unknown";
}

const char* lifecycle_state_name(LifecycleState state) noexcept
{
    switch (state) {
    case LifecycleState::Created:
        return "created";
    case LifecycleState::Attached:
        return "attached";
    case LifecycleState::Detached:
        return "detached";
    case LifecycleState::Destroying:
        return "destroying";
    case LifecycleState::Destroyed:
        return "destroyed";
    }
    return "unknown";
}

const char* dirty_reason_name(DirtyReason reason) noexcept
{
    if (reason == DirtyReason::None) {
        return "none";
    }
    if (has_dirty_reason(reason, DirtyReason::ThemeChanged)) {
        return "theme_changed";
    }
    if (has_dirty_reason(reason, DirtyReason::ResourceChanged)) {
        return "resource_changed";
    }
    if (has_dirty_reason(reason, DirtyReason::Resize)) {
        return "resize";
    }
    if (has_dirty_reason(reason, DirtyReason::Input)) {
        return "input";
    }
    if (has_dirty_reason(reason, DirtyReason::StyleChanged)) {
        return "style_changed";
    }
    if (has_dirty_reason(reason, DirtyReason::TextChanged)) {
        return "text_changed";
    }
    if (has_dirty_reason(reason, DirtyReason::Layout)) {
        return "layout";
    }
    if (has_dirty_reason(reason, DirtyReason::Paint)) {
        return "paint";
    }
    if (has_dirty_reason(reason, DirtyReason::Attach)) {
        return "attach";
    }
    if (has_dirty_reason(reason, DirtyReason::Detach)) {
        return "detach";
    }
    if (has_dirty_reason(reason, DirtyReason::ApiMutation)) {
        return "api_mutation";
    }
    return "mixed";
}

std::string widget_path(const WidgetImpl& impl)
{
    const std::string& local =
        impl.node.debug_id.empty() ? impl.node.fallback_id : impl.node.debug_id;
    if (impl.node.parent == nullptr) {
        return local;
    }
    return widget_path(*impl.node.parent) + "/" + local;
}

std::uint64_t next_frame_id()
{
    return default_runtime().next_frame_id();
}

std::uint64_t current_frame_id()
{
    return default_runtime().current_frame_id();
}

} // namespace fiui
