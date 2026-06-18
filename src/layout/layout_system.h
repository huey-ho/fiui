#pragma once

#include "core/widget_impl.h"
#include "fiui/export.h"

namespace fiui {

struct LayoutConstraints {
    float max_width = 0.0f;
    float max_height = 0.0f;
};

struct LayoutResult {
    std::uint32_t arranged_nodes = 0;
};

class LayoutSystem {
public:
    [[nodiscard]] FIUI_API LayoutResult arrange(WidgetImpl& root,
                                                const LayoutConstraints& constraints);

private:
    [[nodiscard]] bool is_container(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] bool fills_single_child(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] bool is_flexible_height(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] bool is_fixed_width(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] bool is_fixed_height(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] bool is_row_flexible_width(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] float flex_weight(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] float dpi_scale() const noexcept;
    [[nodiscard]] float scaled(float value) const noexcept;
    [[nodiscard]] float requested_width(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] float requested_height(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] float padding(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] float gap(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] float button_content_padding(const WidgetImpl& impl) const noexcept;
    [[nodiscard]] float preferred_width(const WidgetImpl& impl) const;
    [[nodiscard]] float preferred_height(const WidgetImpl& impl) const;
    [[nodiscard]] float default_height(const WidgetImpl& impl) const;
    [[nodiscard]] float default_width(const WidgetImpl& impl, float parent_width) const;
    [[nodiscard]] Rect inflate(Rect rect, float amount) const;
    [[nodiscard]] float preferred_tree_item_height(const WidgetImpl& item) const;
    [[nodiscard]] float arrange_tree_item(WidgetImpl& item,
                                          float x,
                                          float y,
                                          float width,
                                          const Rect& root_bounds,
                                          LayoutResult& result,
                                          std::uint32_t depth);
    void arrange_node(WidgetImpl& impl, float x, float y, float width, float height,
                      const Rect& root_bounds, LayoutResult& result);
};

} // namespace fiui
