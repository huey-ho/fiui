#include "dirty/dirty_tracker.h"

#include <algorithm>

namespace fiui {

DirtyCategory operator|(DirtyCategory left, DirtyCategory right)
{
    return static_cast<DirtyCategory>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

DirtyCategory& operator|=(DirtyCategory& left, DirtyCategory right)
{
    left = left | right;
    return left;
}

bool has_dirty_category(DirtyCategory value, DirtyCategory flag)
{
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
}

DirtyPlan DirtyTracker::plan(WidgetImpl& root, const DirtyThresholds& thresholds)
{
    DirtyPlan plan;
    collect(root, root.dirty.clip_bounds, false, plan.rects);
    plan.original_rect_count = static_cast<std::uint32_t>(plan.rects.size());

    for (const DirtyRect& dirty : plan.rects) {
        plan.merged_rect = union_rect(plan.merged_rect, dirty.rect);
        add_category_counts(plan, dirty.category);
    }
    plan.merged_rect_count = plan.rects.empty() ? 0u : 1u;

    const float window_area =
        std::max(1.0f, root.dirty.bounds.width * root.dirty.bounds.height);
    const float dirty_area = std::max(0.0f, plan.merged_rect.width * plan.merged_rect.height);
    if (plan.rects.size() > thresholds.max_dirty_rects) {
        plan.full_repaint = true;
        plan.fallback_reason = "dirty_rect_count_threshold";
    } else if ((dirty_area / window_area) > thresholds.max_dirty_area_ratio) {
        plan.full_repaint = true;
        plan.fallback_reason = "dirty_area_threshold";
    }

    return plan;
}

void DirtyTracker::clear(WidgetImpl& root)
{
    root.dirty.reason = DirtyReason::None;
    for (WidgetImpl* child : root.node.children) {
        clear(*child);
    }
}

void DirtyTracker::collect(const WidgetImpl& impl,
                           const Rect& parent_clip,
                           bool has_parent_clip,
                           std::vector<DirtyRect>& out)
{
    Rect node_clip = impl.dirty.clip_bounds;
    if (has_parent_clip) {
        node_clip = intersect_rect(node_clip, parent_clip);
    }

    if (impl.dirty.reason != DirtyReason::None) {
        const Rect original_rect = impl.dirty.paint_bounds;
        const Rect clipped_rect = has_parent_clip ? intersect_rect(original_rect, parent_clip)
                                                  : original_rect;
        if (!is_empty(clipped_rect)) {
            out.push_back(DirtyRect{original_rect,
                                    clipped_rect,
                                    impl.object.object_id,
                                    widget_path(impl),
                                    impl.dirty.reason,
                                    classify(impl.dirty.reason),
                                    has_parent_clip && (clipped_rect.x != original_rect.x ||
                                                        clipped_rect.y != original_rect.y ||
                                                        clipped_rect.width != original_rect.width ||
                                                        clipped_rect.height != original_rect.height)});
        }
    }
    for (const WidgetImpl* child : impl.node.children) {
        collect(*child, node_clip, true, out);
    }
}

void DirtyTracker::add_category_counts(DirtyPlan& plan, DirtyCategory category) const
{
    if (has_dirty_category(category, DirtyCategory::Layout)) {
        ++plan.layout_dirty_count;
    }
    if (has_dirty_category(category, DirtyCategory::Paint)) {
        ++plan.paint_dirty_count;
    }
    if (has_dirty_category(category, DirtyCategory::Input)) {
        ++plan.input_dirty_count;
    }
    if (has_dirty_category(category, DirtyCategory::Style)) {
        ++plan.style_dirty_count;
    }
    if (has_dirty_category(category, DirtyCategory::Resource)) {
        ++plan.resource_dirty_count;
    }
    if (has_dirty_category(category, DirtyCategory::Theme)) {
        ++plan.theme_dirty_count;
    }
    if (has_dirty_category(category, DirtyCategory::Hierarchy)) {
        ++plan.hierarchy_dirty_count;
    }
}

DirtyCategory DirtyTracker::classify(DirtyReason reason) const
{
    DirtyCategory category = DirtyCategory::None;
    if (has_dirty_reason(reason, DirtyReason::ApiMutation)) {
        category |= DirtyCategory::Api;
    }
    if (has_dirty_reason(reason, DirtyReason::Attach) ||
        has_dirty_reason(reason, DirtyReason::Detach)) {
        category |= DirtyCategory::Hierarchy | DirtyCategory::Layout | DirtyCategory::Paint;
    }
    if (has_dirty_reason(reason, DirtyReason::Layout) ||
        has_dirty_reason(reason, DirtyReason::Resize)) {
        category |= DirtyCategory::Layout;
    }
    if (has_dirty_reason(reason, DirtyReason::Paint) ||
        has_dirty_reason(reason, DirtyReason::Resize)) {
        category |= DirtyCategory::Paint;
    }
    if (has_dirty_reason(reason, DirtyReason::Input)) {
        category |= DirtyCategory::Input | DirtyCategory::Paint;
    }
    if (has_dirty_reason(reason, DirtyReason::TextChanged)) {
        category |= DirtyCategory::Layout | DirtyCategory::Paint;
    }
    if (has_dirty_reason(reason, DirtyReason::StyleChanged)) {
        category |= DirtyCategory::Style | DirtyCategory::Paint;
    }
    if (has_dirty_reason(reason, DirtyReason::ThemeChanged)) {
        category |= DirtyCategory::Theme | DirtyCategory::Style | DirtyCategory::Paint;
    }
    if (has_dirty_reason(reason, DirtyReason::ResourceChanged)) {
        category |= DirtyCategory::Resource | DirtyCategory::Paint;
    }
    return category;
}

Rect DirtyTracker::intersect_rect(Rect a, Rect b) const
{
    const float left = std::max(a.x, b.x);
    const float top = std::max(a.y, b.y);
    const float right = std::min(a.x + a.width, b.x + b.width);
    const float bottom = std::min(a.y + a.height, b.y + b.height);
    if (right <= left || bottom <= top) {
        return Rect{left, top, 0.0f, 0.0f};
    }
    return Rect{left, top, right - left, bottom - top};
}

bool DirtyTracker::is_empty(Rect rect) const
{
    return rect.width <= 0.0f || rect.height <= 0.0f;
}

Rect DirtyTracker::union_rect(Rect a, Rect b) const
{
    if (a.width <= 0.0f || a.height <= 0.0f) {
        return b;
    }
    const float left = std::min(a.x, b.x);
    const float top = std::min(a.y, b.y);
    const float right = std::max(a.x + a.width, b.x + b.width);
    const float bottom = std::max(a.y + a.height, b.y + b.height);
    return Rect{left, top, right - left, bottom - top};
}

const char* dirty_category_names(DirtyCategory category)
{
    if (category == DirtyCategory::None) {
        return "none";
    }
    if (has_dirty_category(category, DirtyCategory::Input)) {
        return "input|paint";
    }
    if (has_dirty_category(category, DirtyCategory::Hierarchy)) {
        return "hierarchy|layout|paint";
    }
    if (has_dirty_category(category, DirtyCategory::Theme)) {
        return "theme|style|paint";
    }
    if (has_dirty_category(category, DirtyCategory::Style)) {
        return "style|paint";
    }
    if (has_dirty_category(category, DirtyCategory::Resource)) {
        return "resource|paint";
    }
    if (has_dirty_category(category, DirtyCategory::Layout) &&
        has_dirty_category(category, DirtyCategory::Paint)) {
        return "layout|paint";
    }
    if (has_dirty_category(category, DirtyCategory::Layout)) {
        return "layout";
    }
    if (has_dirty_category(category, DirtyCategory::Paint)) {
        return "paint";
    }
    if (has_dirty_category(category, DirtyCategory::Api)) {
        return "api";
    }
    return "mixed";
}

} // namespace fiui
