#pragma once

#include "core/widget_impl.h"

#include <string>

namespace fiui {

class MenuSystem {
public:
    [[nodiscard]] bool has_menu_children(const WidgetImpl& target) const noexcept;
    [[nodiscard]] bool is_menu_navigation_key(std::uint32_t key_code) const noexcept;
    [[nodiscard]] bool is_top_level_menu_item(const WidgetImpl& target) const noexcept;
    [[nodiscard]] WidgetImpl* top_level_menu_item_for(WidgetImpl* target) const noexcept;
    [[nodiscard]] WidgetImpl* first_menu_child(WidgetImpl& target) const noexcept;
    [[nodiscard]] WidgetImpl* last_menu_child(WidgetImpl& target) const noexcept;
    [[nodiscard]] WidgetImpl* sibling_menu_item(WidgetImpl& target, int delta) const noexcept;
    [[nodiscard]] WidgetImpl* open_top_level_menu_item(WidgetImpl& node) const noexcept;
    [[nodiscard]] WidgetImpl* first_top_level_menu_item(WidgetImpl& node) const noexcept;
    [[nodiscard]] WidgetImpl* top_level_menu_item_for_mnemonic(WidgetImpl& node,
                                                               char32_t codepoint) const noexcept;
    [[nodiscard]] WidgetImpl* menu_item_for_shortcut(WidgetImpl& node,
                                                     std::uint32_t key_code) const noexcept;
    [[nodiscard]] WidgetImpl* root_of(WidgetImpl& target) const noexcept;
    [[nodiscard]] bool has_open_menu_popup(const WidgetImpl& node) const noexcept;
    [[nodiscard]] WidgetImpl* shortcut_reveal_target() const noexcept;
    [[nodiscard]] bool ancestors_visible(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] bool target_is_menu_related(const WidgetImpl* target) const noexcept;
    [[nodiscard]] WidgetImpl* hit_test_open_popup(WidgetImpl& node, float x, float y) const;

    [[nodiscard]] bool open_exclusive(WidgetImpl& target);
    [[nodiscard]] bool reveal_shortcut_target(WidgetImpl& root, WidgetImpl& target);
    [[nodiscard]] bool clear_shortcut_reveal(WidgetImpl& root);
    [[nodiscard]] bool close_all(WidgetImpl& node, WidgetImpl* except = nullptr);
    [[nodiscard]] bool close_except_branch(WidgetImpl& node, const WidgetImpl& branch);
    [[nodiscard]] bool update_on_hover(WidgetImpl& root, WidgetImpl& target);

private:
    [[nodiscard]] bool is_ancestor_or_self(const WidgetImpl& ancestor,
                                           const WidgetImpl& target) const noexcept;
    [[nodiscard]] bool shortcut_matches(const std::string& shortcut,
                                        std::uint32_t key_code) const;
    [[nodiscard]] bool open_ancestors(WidgetImpl& target);
    [[nodiscard]] bool rect_contains(const Rect& rect, float x, float y) const noexcept;

    WidgetImpl* shortcut_root_ = nullptr;
    WidgetImpl* shortcut_target_ = nullptr;
};

} // namespace fiui
