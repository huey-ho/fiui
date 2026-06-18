#include "menu/menu_system.h"

#include "diagnostics/diagnostics_internal.h"
#include "runtime/runtime.h"

#include <cctype>
#include <string>
#include <vector>

namespace fiui {

namespace {

constexpr std::uint32_t key_code_mask = 0xffffu;
constexpr std::uint32_t key_modifier_shift = 1u << 16;
constexpr std::uint32_t key_modifier_alt = 1u << 17;
constexpr std::uint32_t key_modifier_ctrl = 1u << 18;

std::string normalized_shortcut_text(const std::string& shortcut)
{
    std::string normalized;
    normalized.reserve(shortcut.size());
    for (char ch : shortcut) {
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            continue;
        }
        normalized.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

std::string key_name_for(std::uint32_t key_code)
{
    const std::uint32_t key = key_code & key_code_mask;
    if (key >= 'A' && key <= 'Z') {
        return std::string(1, static_cast<char>(key));
    }
    if (key >= '0' && key <= '9') {
        return std::string(1, static_cast<char>(key));
    }
    if (key >= 0x70 && key <= 0x7b) {
        return "F" + std::to_string((key - 0x70) + 1);
    }
    switch (key) {
    case 0x08:
        return "BACKSPACE";
    case 0x09:
        return "TAB";
    case 0x0d:
        return "ENTER";
    case 0x1b:
        return "ESC";
    case 0x20:
        return "SPACE";
    case 0x2e:
        return "DELETE";
    default:
        return {};
    }
}

} // namespace

bool MenuSystem::has_menu_children(const WidgetImpl& target) const noexcept
{
    return target.node.kind == WidgetKind::MenuItem && target.properties.menu_enabled &&
           target.properties.visible &&
           !target.node.children.empty();
}

bool MenuSystem::is_menu_navigation_key(std::uint32_t key_code) const noexcept
{
    switch (key_code & key_code_mask) {
    case 0x0d:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
        return true;
    default:
        return false;
    }
}

bool MenuSystem::is_top_level_menu_item(const WidgetImpl& target) const noexcept
{
    return target.node.kind == WidgetKind::MenuItem && target.node.parent != nullptr &&
           target.node.parent->node.kind == WidgetKind::MenuBar && target.properties.visible;
}

WidgetImpl* MenuSystem::top_level_menu_item_for(WidgetImpl* target) const noexcept
{
    WidgetImpl* cursor = target;
    while (cursor != nullptr) {
        if (is_top_level_menu_item(*cursor)) {
            return cursor;
        }
        cursor = cursor->node.parent;
    }
    return nullptr;
}

WidgetImpl* MenuSystem::first_menu_child(WidgetImpl& target) const noexcept
{
    for (WidgetImpl* child : target.node.children) {
        if (child != nullptr && child->node.kind == WidgetKind::MenuItem &&
            child->properties.visible) {
            return child;
        }
    }
    return nullptr;
}

WidgetImpl* MenuSystem::last_menu_child(WidgetImpl& target) const noexcept
{
    for (auto it = target.node.children.rbegin(); it != target.node.children.rend(); ++it) {
        WidgetImpl* child = *it;
        if (child != nullptr && child->node.kind == WidgetKind::MenuItem &&
            child->properties.visible) {
            return child;
        }
    }
    return nullptr;
}

WidgetImpl* MenuSystem::sibling_menu_item(WidgetImpl& target, int delta) const noexcept
{
    WidgetImpl* parent = target.node.parent;
    if (parent == nullptr || parent->node.children.empty()) {
        return nullptr;
    }

    int current_index = -1;
    std::vector<WidgetImpl*> items;
    for (WidgetImpl* child : parent->node.children) {
        if (child == nullptr || child->node.kind != WidgetKind::MenuItem ||
            !child->properties.visible) {
            continue;
        }
        if (child == &target) {
            current_index = static_cast<int>(items.size());
        }
        items.push_back(child);
    }
    if (items.empty() || current_index < 0) {
        return nullptr;
    }

    const int count = static_cast<int>(items.size());
    const int next_index = (current_index + delta + count) % count;
    return items[static_cast<std::size_t>(next_index)];
}

WidgetImpl* MenuSystem::open_top_level_menu_item(WidgetImpl& node) const noexcept
{
    if (is_top_level_menu_item(node) && node.properties.menu_popup_open) {
        return &node;
    }
    for (WidgetImpl* child : node.node.children) {
        if (child == nullptr || !child->properties.visible) {
            continue;
        }
        if (WidgetImpl* open = open_top_level_menu_item(*child)) {
            return open;
        }
    }
    return nullptr;
}

WidgetImpl* MenuSystem::first_top_level_menu_item(WidgetImpl& node) const noexcept
{
    if (is_top_level_menu_item(node)) {
        return &node;
    }
    for (WidgetImpl* child : node.node.children) {
        if (child == nullptr || !child->properties.visible) {
            continue;
        }
        if (WidgetImpl* found = first_top_level_menu_item(*child)) {
            return found;
        }
    }
    return nullptr;
}

WidgetImpl* MenuSystem::top_level_menu_item_for_mnemonic(WidgetImpl& node,
                                                         char32_t codepoint) const noexcept
{
    const int wanted =
        codepoint <= 0x7f ? std::toupper(static_cast<unsigned char>(codepoint)) : 0;
    if (wanted == 0) {
        return nullptr;
    }
    if (is_top_level_menu_item(node) && node.properties.visible && !node.properties.text.empty()) {
        const int actual =
            std::toupper(static_cast<unsigned char>(node.properties.text.front()));
        if (actual == wanted) {
            return &node;
        }
    }
    for (WidgetImpl* child : node.node.children) {
        if (child == nullptr || !child->properties.visible) {
            continue;
        }
        if (WidgetImpl* found = top_level_menu_item_for_mnemonic(*child, codepoint)) {
            return found;
        }
    }
    return nullptr;
}

WidgetImpl* MenuSystem::menu_item_for_shortcut(WidgetImpl& node,
                                               std::uint32_t key_code) const noexcept
{
    if (node.node.kind == WidgetKind::MenuItem && node.properties.menu_enabled &&
        node.properties.visible &&
        !node.properties.menu_shortcut.empty() &&
        shortcut_matches(node.properties.menu_shortcut, key_code)) {
        return &node;
    }
    for (WidgetImpl* child : node.node.children) {
        if (child == nullptr || !child->properties.visible) {
            continue;
        }
        if (WidgetImpl* found = menu_item_for_shortcut(*child, key_code)) {
            return found;
        }
    }
    return nullptr;
}

WidgetImpl* MenuSystem::root_of(WidgetImpl& target) const noexcept
{
    WidgetImpl* cursor = &target;
    while (cursor->node.parent != nullptr) {
        cursor = cursor->node.parent;
    }
    return cursor;
}

bool MenuSystem::has_open_menu_popup(const WidgetImpl& node) const noexcept
{
    if (node.node.kind == WidgetKind::MenuItem && node.properties.menu_popup_open &&
        node.properties.visible) {
        return true;
    }
    for (const WidgetImpl* child : node.node.children) {
        if (child != nullptr && child->properties.visible && has_open_menu_popup(*child)) {
            return true;
        }
    }
    return false;
}

WidgetImpl* MenuSystem::shortcut_reveal_target() const noexcept
{
    return shortcut_target_;
}

bool MenuSystem::ancestors_visible(const WidgetImpl& impl) const noexcept
{
    const WidgetImpl* cursor = impl.node.parent;
    while (cursor != nullptr) {
        if (!cursor->properties.visible) {
            return false;
        }
        if (cursor->node.kind == WidgetKind::MenuItem && !cursor->properties.menu_popup_open) {
            return false;
        }
        cursor = cursor->node.parent;
    }
    return true;
}

bool MenuSystem::target_is_menu_related(const WidgetImpl* target) const noexcept
{
    const WidgetImpl* cursor = target;
    while (cursor != nullptr) {
        if (!cursor->properties.visible) {
            return false;
        }
        if (cursor->node.kind == WidgetKind::MenuBar || cursor->node.kind == WidgetKind::MenuItem) {
            return true;
        }
        cursor = cursor->node.parent;
    }
    return false;
}

WidgetImpl* MenuSystem::hit_test_open_popup(WidgetImpl& node, float x, float y) const
{
    if (!node.properties.visible) {
        return nullptr;
    }
    if (node.node.kind == WidgetKind::MenuItem && !ancestors_visible(node)) {
        return nullptr;
    }

    for (auto it = node.node.children.rbegin(); it != node.node.children.rend(); ++it) {
        WidgetImpl* child = *it;
        if (child == nullptr || !child->properties.visible) {
            continue;
        }
        if (WidgetImpl* popup_hit = hit_test_open_popup(*child, x, y)) {
            return popup_hit;
        }
    }

    if (node.node.kind != WidgetKind::MenuItem || !node.properties.menu_popup_open) {
        return nullptr;
    }

    for (auto it = node.node.children.rbegin(); it != node.node.children.rend(); ++it) {
        WidgetImpl* child = *it;
        if (child == nullptr || !child->properties.visible) {
            continue;
        }
        if (WidgetImpl* nested_hit = hit_test_open_popup(*child, x, y)) {
            return nested_hit;
        }
        if (rect_contains(child->dirty.bounds, x, y)) {
            return child;
        }
    }
    return nullptr;
}

bool MenuSystem::open_exclusive(WidgetImpl& target)
{
    if (!has_menu_children(target)) {
        return false;
    }
    WidgetImpl* root = root_of(target);
    bool changed = close_except_branch(*root, target);
    changed = open_ancestors(target) || changed;
    if (!target.properties.menu_popup_open) {
        target.properties.menu_popup_open = true;
        mutate_widget(target, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      "menu_popup_open");
        diagnostics_event_ex("event", "menu_popup_open", target.object.object_id,
                             target.object.generation, current_frame_id(),
                             widget_path(target).c_str(), "opened");
        changed = true;
    }
    return changed;
}

bool MenuSystem::reveal_shortcut_target(WidgetImpl& root, WidgetImpl& target)
{
    shortcut_root_ = &root;
    shortcut_target_ = &target;
    bool changed = close_except_branch(root, target);
    changed = open_ancestors(target) || changed;
    mutate_widget(target, DirtyReason::Input | DirtyReason::Paint, "menu_shortcut_highlight");
    diagnostics_event_ex("event", "menu_shortcut_highlight", target.object.object_id,
                         target.object.generation, current_frame_id(),
                         widget_path(target).c_str(), target.properties.menu_shortcut.c_str());
    return true;
}

bool MenuSystem::clear_shortcut_reveal(WidgetImpl& root)
{
    if (shortcut_target_ == nullptr) {
        return false;
    }
    WidgetImpl* target = shortcut_target_;
    WidgetImpl* reveal_root = shortcut_root_ != nullptr ? shortcut_root_ : &root;
    shortcut_root_ = nullptr;
    shortcut_target_ = nullptr;
    const bool changed = close_all(*reveal_root);
    mutate_widget(*target, DirtyReason::Input | DirtyReason::Paint, "menu_shortcut_clear");
    diagnostics_event_ex("event", "menu_shortcut_clear", target->object.object_id,
                         target->object.generation, current_frame_id(),
                         widget_path(*target).c_str(), "cleared");
    (void)changed;
    return true;
}

bool MenuSystem::close_all(WidgetImpl& node, WidgetImpl* except)
{
    if (except == nullptr) {
        shortcut_root_ = nullptr;
        shortcut_target_ = nullptr;
    }
    bool changed = false;
    if (&node != except && node.node.kind == WidgetKind::MenuItem &&
        node.properties.menu_popup_open) {
        node.properties.menu_popup_open = false;
        mutate_widget(node, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      "menu_popup_close");
        diagnostics_event_ex("event", "menu_popup_close", node.object.object_id,
                             node.object.generation, current_frame_id(), widget_path(node).c_str(),
                             "closed");
        changed = true;
    }
    for (WidgetImpl* child : node.node.children) {
        if (child != nullptr) {
            changed = close_all(*child, except) || changed;
        }
    }
    return changed;
}

bool MenuSystem::close_except_branch(WidgetImpl& node, const WidgetImpl& branch)
{
    bool changed = false;
    if (node.node.kind == WidgetKind::MenuItem && node.properties.menu_popup_open &&
        &node != &branch && !is_ancestor_or_self(node, branch)) {
        node.properties.menu_popup_open = false;
        mutate_widget(node, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                      "menu_popup_close");
        diagnostics_event_ex("event", "menu_popup_close", node.object.object_id,
                             node.object.generation, current_frame_id(), widget_path(node).c_str(),
                             "closed");
        changed = true;
    }
    for (WidgetImpl* child : node.node.children) {
        if (child != nullptr) {
            changed = close_except_branch(*child, branch) || changed;
        }
    }
    return changed;
}

bool MenuSystem::update_on_hover(WidgetImpl& root, WidgetImpl& target)
{
    if (!has_open_menu_popup(root) || target.node.kind != WidgetKind::MenuItem) {
        return false;
    }
    if (!is_top_level_menu_item(target) &&
        (target.node.parent == nullptr || target.node.parent->node.kind != WidgetKind::MenuItem)) {
        return false;
    }
    if (target.properties.menu_enabled && has_menu_children(target)) {
        return target.properties.menu_popup_open ? close_except_branch(root, target)
                                                 : open_exclusive(target);
    }
    return close_except_branch(root, target);
}

bool MenuSystem::is_ancestor_or_self(const WidgetImpl& ancestor,
                                     const WidgetImpl& target) const noexcept
{
    const WidgetImpl* cursor = &target;
    while (cursor != nullptr) {
        if (cursor == &ancestor) {
            return true;
        }
        cursor = cursor->node.parent;
    }
    return false;
}

bool MenuSystem::shortcut_matches(const std::string& shortcut, std::uint32_t key_code) const
{
    const std::string key_name = key_name_for(key_code);
    if (key_name.empty()) {
        return false;
    }

    std::string expected;
    if ((key_code & key_modifier_ctrl) != 0) {
        expected += "CTRL+";
    }
    if ((key_code & key_modifier_alt) != 0) {
        expected += "ALT+";
    }
    if ((key_code & key_modifier_shift) != 0) {
        expected += "SHIFT+";
    }
    expected += key_name;

    const std::string actual = normalized_shortcut_text(shortcut);
    if (actual == expected) {
        return true;
    }
    if (actual.size() > 3 && actual.rfind("DEL") == actual.size() - 3) {
        std::string delete_alias = actual;
        delete_alias.replace(delete_alias.size() - 3, 3, "DELETE");
        return delete_alias == expected;
    }
    return false;
}

bool MenuSystem::open_ancestors(WidgetImpl& target)
{
    bool changed = false;
    WidgetImpl* cursor = target.node.parent;
    while (cursor != nullptr) {
        if (cursor->node.kind == WidgetKind::MenuItem && !cursor->properties.menu_popup_open) {
            cursor->properties.menu_popup_open = true;
            mutate_widget(*cursor, DirtyReason::Input | DirtyReason::Layout | DirtyReason::Paint,
                          "menu_popup_open_ancestor");
            diagnostics_event_ex("event", "menu_popup_open_ancestor", cursor->object.object_id,
                                 cursor->object.generation, current_frame_id(),
                                 widget_path(*cursor).c_str(), "opened");
            changed = true;
        }
        cursor = cursor->node.parent;
    }
    return changed;
}

bool MenuSystem::rect_contains(const Rect& rect, float x, float y) const noexcept
{
    return x >= rect.x && y >= rect.y && x <= rect.x + rect.width &&
           y <= rect.y + rect.height;
}

} // namespace fiui
