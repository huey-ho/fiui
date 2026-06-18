#pragma once

#include "core/widget_impl.h"
#include "fiui/export.h"
#include "runtime/runtime.h"

#include <string>
#include <vector>

namespace fiui {

enum class DirtyCategory : std::uint32_t {
    None = 0,
    Api = 1u << 0,
    Hierarchy = 1u << 1,
    Layout = 1u << 2,
    Paint = 1u << 3,
    Input = 1u << 4,
    Style = 1u << 5,
    Resource = 1u << 6,
    Theme = 1u << 7,
};

DirtyCategory operator|(DirtyCategory left, DirtyCategory right);
DirtyCategory& operator|=(DirtyCategory& left, DirtyCategory right);
bool has_dirty_category(DirtyCategory value, DirtyCategory flag);

struct DirtyRect {
    Rect original_rect;
    Rect rect;
    ObjectId object_id = 0;
    std::string path;
    DirtyReason reason = DirtyReason::None;
    DirtyCategory category = DirtyCategory::None;
    bool clipped_by_parent = false;
};

struct DirtyPlan {
    std::vector<DirtyRect> rects;
    Rect merged_rect;
    std::uint32_t original_rect_count = 0;
    std::uint32_t merged_rect_count = 0;
    bool full_repaint = false;
    const char* fallback_reason = "none";
    std::uint32_t layout_dirty_count = 0;
    std::uint32_t paint_dirty_count = 0;
    std::uint32_t input_dirty_count = 0;
    std::uint32_t style_dirty_count = 0;
    std::uint32_t resource_dirty_count = 0;
    std::uint32_t theme_dirty_count = 0;
    std::uint32_t hierarchy_dirty_count = 0;
};

class DirtyTracker {
public:
    [[nodiscard]] FIUI_API DirtyPlan plan(WidgetImpl& root, const DirtyThresholds& thresholds);
    FIUI_API void clear(WidgetImpl& root);

private:
    void collect(const WidgetImpl& impl, const Rect& parent_clip, bool has_parent_clip,
                 std::vector<DirtyRect>& out);
    void add_category_counts(DirtyPlan& plan, DirtyCategory category) const;
    [[nodiscard]] DirtyCategory classify(DirtyReason reason) const;
    [[nodiscard]] Rect intersect_rect(Rect a, Rect b) const;
    [[nodiscard]] bool is_empty(Rect rect) const;
    [[nodiscard]] Rect union_rect(Rect a, Rect b) const;
};

const char* dirty_category_names(DirtyCategory category);

} // namespace fiui
