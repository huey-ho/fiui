#include "fiui/app.h"

#include "core/widget_impl.h"
#include "diagnostics/diagnostics_internal.h"
#include "dirty/dirty_tracker.h"
#include "fiui/diagnostics.h"
#include "layout/layout_system.h"
#include "render/render_system.h"
#include "runtime/runtime.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>
#include <string>

namespace fiui {
namespace {

double elapsed_ms(std::chrono::steady_clock::time_point start,
                  std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

FrameReport& last_frame_report_storage()
{
    static FrameReport report;
    return report;
}

bool compact_frame_diagnostics_reason(const char* reason) noexcept
{
    return reason != nullptr &&
           (std::strcmp(reason, "wheel") == 0 ||
            std::strcmp(reason, "pointer_move") == 0 ||
            std::strcmp(reason, "timer") == 0);
}

const char* layout_size_mode_name(LayoutSizeMode mode) noexcept
{
    switch (mode) {
    case LayoutSizeMode::Auto:
        return "auto";
    case LayoutSizeMode::Fixed:
        return "fixed";
    case LayoutSizeMode::Fill:
        return "fill";
    case LayoutSizeMode::Flex:
        return "flex";
    }
    return "unknown";
}

const char* overflow_policy_name(const WidgetImpl& impl) noexcept
{
    if (impl.node.kind == WidgetKind::ScrollView) {
        return "scroll";
    }
    switch (impl.properties.overflow_policy) {
    case OverflowPolicy::Clip:
        return "clip";
    case OverflowPolicy::Visible:
        return "visible";
    case OverflowPolicy::Scroll:
        return "scroll";
    }
    return "unknown";
}

void write_rect_json(std::ostringstream& json, const Rect& rect)
{
    json << "{\"x\":" << rect.x << ",\"y\":" << rect.y << ",\"w\":" << rect.width
         << ",\"h\":" << rect.height << "}";
}

Rect direct_child_union_bounds(const WidgetImpl& impl) noexcept
{
    Rect result{};
    bool initialized = false;
    for (const WidgetImpl* child : impl.node.children) {
        if (child == nullptr || child->dirty.bounds.width <= 0.0f ||
            child->dirty.bounds.height <= 0.0f) {
            continue;
        }
        if (!initialized) {
            result = child->dirty.bounds;
            initialized = true;
            continue;
        }
        const float min_x = std::min(result.x, child->dirty.bounds.x);
        const float min_y = std::min(result.y, child->dirty.bounds.y);
        const float max_x =
            std::max(result.x + result.width, child->dirty.bounds.x + child->dirty.bounds.width);
        const float max_y =
            std::max(result.y + result.height, child->dirty.bounds.y + child->dirty.bounds.height);
        result = Rect{min_x, min_y, max_x - min_x, max_y - min_y};
    }
    return result;
}

void write_layout_json(std::ostringstream& json, const WidgetImpl& impl)
{
    const Rect child_union = direct_child_union_bounds(impl);
    const bool has_child_union = child_union.width > 0.0f && child_union.height > 0.0f;
    const float overflow_left =
        has_child_union ? std::max(0.0f, impl.dirty.bounds.x - child_union.x) : 0.0f;
    const float overflow_top =
        has_child_union ? std::max(0.0f, impl.dirty.bounds.y - child_union.y) : 0.0f;
    const float overflow_right =
        has_child_union ? std::max(0.0f, child_union.x + child_union.width -
                                             (impl.dirty.bounds.x + impl.dirty.bounds.width))
                        : 0.0f;
    const float overflow_bottom =
        has_child_union ? std::max(0.0f, child_union.y + child_union.height -
                                             (impl.dirty.bounds.y + impl.dirty.bounds.height))
                        : 0.0f;
    json << "{";
    json << "\"width_mode\":\"" << layout_size_mode_name(impl.properties.width_mode) << "\",";
    json << "\"height_mode\":\"" << layout_size_mode_name(impl.properties.height_mode) << "\",";
    json << "\"overflow_policy\":\"" << overflow_policy_name(impl) << "\",";
    json << "\"overflow_policy_explicit\":"
         << (impl.properties.overflow_policy_explicit ? "true" : "false") << ",";
    json << "\"requested_width\":" << impl.properties.requested_width << ",";
    json << "\"requested_height\":" << impl.properties.requested_height << ",";
    json << "\"flex_grow\":" << impl.properties.flex_grow << ",";
    json << "\"padding\":" << impl.properties.padding << ",";
    json << "\"gap\":" << impl.properties.gap << ",";
    json << "\"bounds\":";
    write_rect_json(json, impl.dirty.bounds);
    json << ",\"paint_bounds\":";
    write_rect_json(json, impl.dirty.paint_bounds);
    json << ",\"clip_bounds\":";
    write_rect_json(json, impl.dirty.clip_bounds);
    json << ",\"child_union_bounds\":";
    write_rect_json(json, child_union);
    json << ",\"overflow_x\":"
         << (overflow_left > 0.0f || overflow_right > 0.0f ? "true" : "false");
    json << ",\"overflow_y\":"
         << (overflow_top > 0.0f || overflow_bottom > 0.0f ? "true" : "false");
    json << ",\"overflow_left\":" << overflow_left;
    json << ",\"overflow_top\":" << overflow_top;
    json << ",\"overflow_right\":" << overflow_right;
    json << ",\"overflow_bottom\":" << overflow_bottom;
    json << "}";
}

void write_widget_json(std::ostringstream& json, const WidgetImpl& impl, int indent)
{
    const std::string pad(static_cast<std::size_t>(indent), ' ');
    json << pad << "{";
    json << "\"object_id\":" << impl.object.object_id << ",";
    json << "\"generation\":" << impl.object.generation << ",";
    json << "\"kind\":\"" << widget_kind_name(impl.node.kind) << "\",";
    json << "\"path\":\"" << widget_path(impl) << "\",";
    json << "\"lifecycle\":\"" << lifecycle_state_name(impl.object.lifecycle_state) << "\",";
    json << "\"dirty\":\"" << dirty_reason_name(impl.dirty.reason) << "\",";
    json << "\"bounds\":";
    write_rect_json(json, impl.dirty.bounds);
    json << ",";
    json << "\"layout\":";
    write_layout_json(json, impl);
    json << ",";
    json << "\"children\":[";
    if (!impl.node.children.empty()) {
        json << "\n";
        for (std::size_t index = 0; index < impl.node.children.size(); ++index) {
            write_widget_json(json, *impl.node.children[index], indent + 2);
            if (index + 1 < impl.node.children.size()) {
                json << ",";
            }
            json << "\n";
        }
        json << pad;
    }
    json << "]}";
}

void write_dirty_json(std::ostringstream& json, const DirtyPlan& dirty_plan)
{
    json << "[";
    if (!dirty_plan.rects.empty()) {
        json << "\n";
        for (std::size_t index = 0; index < dirty_plan.rects.size(); ++index) {
            const DirtyRect& dirty = dirty_plan.rects[index];
            json << "    {\"object_id\":" << dirty.object_id << ",\"path\":\"" << dirty.path
                 << "\",\"reason\":\"" << dirty_reason_name(dirty.reason) << "\",";
            json << "\"category\":\"" << dirty_category_names(dirty.category) << "\",";
            json << "\"clipped_by_parent\":" << (dirty.clipped_by_parent ? "true" : "false")
                 << ",";
            json << "\"original_rect\":{\"x\":" << dirty.original_rect.x
                 << ",\"y\":" << dirty.original_rect.y << ",\"w\":" << dirty.original_rect.width
                 << ",\"h\":" << dirty.original_rect.height << "},";
            json << "\"rect\":{\"x\":" << dirty.rect.x << ",\"y\":" << dirty.rect.y
                 << ",\"w\":" << dirty.rect.width << ",\"h\":" << dirty.rect.height << "}}";
            if (index + 1 < dirty_plan.rects.size()) {
                json << ",";
            }
            json << "\n";
        }
        json << "  ";
    }
    json << "]";
}

void write_render_tree_summary_json(std::ostringstream& json, const RenderTree& render_tree)
{
    json << "{";
    json << "\"node_count\":" << render_tree.nodes.size();
    json << "}";
}

void write_color_json(std::ostringstream& json, const Color& color)
{
    json << "{\"r\":" << static_cast<int>(color.r) << ",\"g\":" << static_cast<int>(color.g)
         << ",\"b\":" << static_cast<int>(color.b) << ",\"a\":" << static_cast<int>(color.a)
         << "}";
}

void write_display_style_json(std::ostringstream& json, const DisplayStyle& style)
{
    json << "\"style\":{";
    json << "\"theme\":\"" << style.theme_name << "\",";
    json << "\"state\":\"" << style.control_state << "\",";
    json << "\"fill\":";
    write_color_json(json, style.fill);
    json << ",\"text\":";
    write_color_json(json, style.text);
    json << ",\"border\":";
    write_color_json(json, style.border);
    json << ",\"border_width\":" << style.border_width;
    json << ",\"radius\":" << style.radius;
    json << ",\"font_size\":" << style.font_size;
    json << ",\"text_padding_x\":" << style.text_padding_x;
    json << ",\"text_padding_y\":" << style.text_padding_y;
    json << ",\"text_align\":\"" << style.text_align << "\"";
    json << ",\"paragraph_align\":\"" << style.paragraph_align << "\"";
    json << ",\"overflow\":\"" << style.overflow << "\"";
    json << ",\"word_wrap\":\"" << style.word_wrap << "\"";
    json << "}";
}

void write_display_resource_json(std::ostringstream& json, const DisplayResource& resource)
{
    json << "\"resource\":{";
    json << "\"id\":" << resource.id << ",";
    json << "\"kind\":\"" << resource_kind_name(resource.kind) << "\",";
    json << "\"cache_state\":\"" << resource_cache_state_name(resource.cache_state) << "\",";
    json << "\"owner_object_id\":" << resource.owner_object_id << ",";
    json << "\"owner_path\":\"" << resource.owner_path << "\",";
    json << "\"key\":\"" << resource.key << "\"";
    if (resource.kind == ResourceKind::TextLayout && resource.text_metrics.valid) {
        json << ",\"text_metrics\":{";
        json << "\"valid\":" << (resource.text_metrics.valid ? "true" : "false") << ",";
        json << "\"directwrite\":"
             << (resource.text_metrics.directwrite ? "true" : "false") << ",";
        json << "\"width\":" << resource.text_metrics.width << ",";
        json << "\"height\":" << resource.text_metrics.height << ",";
        json << "\"layout_width\":" << resource.text_metrics.layout_width << ",";
        json << "\"layout_height\":" << resource.text_metrics.layout_height << ",";
        json << "\"baseline\":" << resource.text_metrics.baseline << ",";
        json << "\"font_size\":" << resource.text_metrics.font_size << ",";
        json << "\"line_count\":" << resource.text_metrics.line_count << ",";
        json << "\"dpi\":" << resource.text_metrics.dpi;
        json << "}";
    }
    if (resource.kind == ResourceKind::Image && resource.image_metadata.valid) {
        json << ",\"image_metadata\":{";
        json << "\"valid\":" << (resource.image_metadata.valid ? "true" : "false") << ",";
        json << "\"wic\":" << (resource.image_metadata.wic ? "true" : "false") << ",";
        json << "\"fallback\":" << (resource.image_metadata.fallback ? "true" : "false")
             << ",";
        json << "\"width\":" << resource.image_metadata.width << ",";
        json << "\"height\":" << resource.image_metadata.height << ",";
        json << "\"dpi_x\":" << resource.image_metadata.dpi_x << ",";
        json << "\"dpi_y\":" << resource.image_metadata.dpi_y << ",";
        json << "\"frame_count\":" << resource.image_metadata.frame_count << ",";
        json << "\"pixel_format\":\"" << resource.image_metadata.pixel_format << "\"";
        json << "}";
    }
    if (resource.kind == ResourceKind::Image && resource.texture_metadata.valid) {
        json << ",\"texture_metadata\":{";
        json << "\"valid\":" << (resource.texture_metadata.valid ? "true" : "false") << ",";
        json << "\"uploaded\":" << (resource.texture_metadata.uploaded ? "true" : "false")
             << ",";
        json << "\"fallback\":" << (resource.texture_metadata.fallback ? "true" : "false")
             << ",";
        json << "\"width\":" << resource.texture_metadata.width << ",";
        json << "\"height\":" << resource.texture_metadata.height << ",";
        json << "\"format\":\"" << resource.texture_metadata.format << "\",";
        json << "\"upload_generation\":" << resource.texture_metadata.upload_generation << ",";
        json << "\"upload_count\":" << resource.texture_metadata.upload_count << ",";
        json << "\"last_failure\":\"" << resource.texture_metadata.last_failure << "\"";
        json << "}";
    }
    json << "}";
}

void refresh_display_resource(DisplayResource& resource)
{
    if (resource.id == 0) {
        return;
    }
    const ResourceRecord* record = default_runtime().resource_system().find(resource.id);
    if (record == nullptr) {
        return;
    }
    resource.cache_state = record->cache_state;
    resource.text_metrics = record->text_metrics;
    resource.image_metadata = record->image_metadata;
    resource.texture_metadata = record->texture_metadata;
}

void refresh_resource_snapshots(RenderFrameResult& result)
{
    for (DisplayCommand& command : result.display_list.commands) {
        refresh_display_resource(command.resource);
    }
    for (LayerNode& layer : result.layer_tree.nodes) {
        refresh_display_resource(layer.resource);
    }
}

void write_layer_tree_summary_json(std::ostringstream& json, const LayerTree& layer_tree)
{
    std::uint32_t root_count = 0;
    std::uint32_t rect_count = 0;
    std::uint32_t rounded_rect_count = 0;
    std::uint32_t text_count = 0;
    std::uint32_t image_count = 0;
    std::uint32_t clip_count = 0;
    std::uint32_t scroll_count = 0;
    std::uint32_t opacity_count = 0;
    std::uint32_t transform_count = 0;
    std::uint32_t shadow_count = 0;
    for (const LayerNode& layer : layer_tree.nodes) {
        switch (layer.kind) {
        case LayerKind::Root:
            ++root_count;
            break;
        case LayerKind::Rect:
            ++rect_count;
            break;
        case LayerKind::RoundedRect:
            ++rounded_rect_count;
            break;
        case LayerKind::Text:
            ++text_count;
            break;
        case LayerKind::Image:
            ++image_count;
            break;
        case LayerKind::Clip:
            ++clip_count;
            break;
        case LayerKind::Scroll:
            ++scroll_count;
            break;
        case LayerKind::Opacity:
            ++opacity_count;
            break;
        case LayerKind::Transform:
            ++transform_count;
            break;
        case LayerKind::Shadow:
            ++shadow_count;
            break;
        }
    }

    json << "{";
    json << "\"layer_count\":" << layer_tree.nodes.size() << ",";
    json << "\"kind_counts\":{";
    json << "\"root\":" << root_count << ",";
    json << "\"rect\":" << rect_count << ",";
    json << "\"rounded_rect\":" << rounded_rect_count << ",";
    json << "\"text\":" << text_count << ",";
    json << "\"image\":" << image_count << ",";
    json << "\"clip\":" << clip_count << ",";
    json << "\"scroll\":" << scroll_count << ",";
    json << "\"opacity\":" << opacity_count << ",";
    json << "\"transform\":" << transform_count << ",";
    json << "\"shadow\":" << shadow_count;
    json << "},";
    json << "\"layers\":[";
    if (!layer_tree.nodes.empty()) {
        json << "\n";
        for (std::size_t index = 0; index < layer_tree.nodes.size(); ++index) {
            const LayerNode& layer = layer_tree.nodes[index];
            json << "    {\"layer_id\":" << layer.layer_id
                 << ",\"parent_layer_id\":";
            if (layer.parent_layer_id == invalid_layer_id) {
                json << "null";
            } else {
                json << layer.parent_layer_id;
            }
            json << ",\"kind\":\"" << layer_kind_name(layer.kind) << "\"";
            json << ",\"object_id\":" << layer.object_id;
            json << ",\"generation\":" << layer.generation;
            json << ",\"path\":\"" << layer.path << "\"";
            json << ",\"widget_kind\":\"" << widget_kind_name(layer.widget_kind) << "\"";
            json << ",\"bounds\":";
            write_rect_json(json, layer.bounds);
            json << ",\"paint_bounds\":";
            write_rect_json(json, layer.paint_bounds);
            json << ",\"clip_bounds\":";
            write_rect_json(json, layer.clip_bounds);
            json << ",\"opacity\":" << layer.opacity;
            json << ",\"radius\":" << layer.radius;
            json << ",\"shadow_blur\":" << layer.shadow_blur;
            json << ",\"display_command_index\":";
            if (layer.display_command_index == invalid_display_command_index) {
                json << "null";
            } else {
                json << layer.display_command_index;
            }
            json << ",\"display_command_count\":" << layer.display_command_count;
            json << ",\"child_count\":" << layer.child_count << ",";
            write_display_style_json(json, layer.style);
            json << ",";
            write_display_resource_json(json, layer.resource);
            json << "}";
            if (index + 1 < layer_tree.nodes.size()) {
                json << ",";
            }
            json << "\n";
        }
        json << "  ";
    }
    json << "]}";
}

void write_display_list_summary_json(std::ostringstream& json, const DisplayList& display_list)
{
    json << "{";
    json << "\"command_count\":" << display_list.commands.size() << ",";
    json << "\"commands\":[";
    if (!display_list.commands.empty()) {
        json << "\n";
        for (std::size_t index = 0; index < display_list.commands.size(); ++index) {
            const DisplayCommand& command = display_list.commands[index];
            json << "    {\"kind\":\"" << display_command_kind_name(command.kind)
                 << "\",\"object_id\":" << command.object_id << ",\"path\":\"" << command.path
                 << "\",\"widget_kind\":\"" << widget_kind_name(command.widget_kind) << "\",";
            write_display_style_json(json, command.style);
            json << ",";
            write_display_resource_json(json, command.resource);
            if (command.kind == DisplayCommandKind::Opacity ||
                command.kind == DisplayCommandKind::OpacityEnd) {
                json << ",\"opacity\":" << command.opacity;
            }
            if (command.kind == DisplayCommandKind::Transform ||
                command.kind == DisplayCommandKind::TransformEnd) {
                json << ",\"transform\":{";
                json << "\"translate_x\":" << command.transform_translate_x << ",";
                json << "\"translate_y\":" << command.transform_translate_y << ",";
                json << "\"scale_x\":" << command.transform_scale_x << ",";
                json << "\"scale_y\":" << command.transform_scale_y;
                json << "}";
            }
            if (command.kind == DisplayCommandKind::Image) {
                json << ",\"image_fit\":\"" << image_fit_name(command.image_fit) << "\"";
                json << ",\"image_uv\":";
                write_rect_json(json, command.image_uv);
            }
            json << "}";
            if (index + 1 < display_list.commands.size()) {
                json << ",";
            }
            json << "\n";
        }
        json << "  ";
    }
    json << "]}";
}

void write_backend_summary_json(std::ostringstream& json, const BackendSummary& backend)
{
    json << "{";
    json << "\"command_count\":" << backend.command_count << ",";
    json << "\"batch_count\":" << backend.batch_count << ",";
    json << "\"rect_command_count\":" << backend.rect_command_count << ",";
    json << "\"shadow_command_count\":" << backend.shadow_command_count << ",";
    json << "\"text_command_count\":" << backend.text_command_count << ",";
    json << "\"image_command_count\":" << backend.image_command_count << ",";
    json << "\"clip_command_count\":" << backend.clip_command_count << ",";
    json << "\"clip_end_command_count\":" << backend.clip_end_command_count << ",";
    json << "\"rounded_clip_command_count\":" << backend.rounded_clip_command_count << ",";
    json << "\"rounded_clip_end_command_count\":"
         << backend.rounded_clip_end_command_count << ",";
    json << "\"opacity_command_count\":" << backend.opacity_command_count << ",";
    json << "\"opacity_end_command_count\":" << backend.opacity_end_command_count << ",";
    json << "\"transform_command_count\":" << backend.transform_command_count << ",";
    json << "\"transform_end_command_count\":" << backend.transform_end_command_count;
    json << "}";
}

void write_backend_device_summary_json(std::ostringstream& json, const BackendFrameResult& backend)
{
    json << "{";
    json << "\"submitted\":" << (backend.submitted ? "true" : "false") << ",";
    json << "\"failure_reason\":\"" << backend_failure_reason_name(backend.failure_reason)
         << "\",";
    json << "\"consumed_command_count\":" << backend.consumed_command_count << ",";
    json << "\"unsupported_command_count\":" << backend.unsupported_command_count << ",";
    json << "\"rect_draw_count\":" << backend.rect_draw_count << ",";
    json << "\"rounded_rect_draw_count\":" << backend.rounded_rect_draw_count << ",";
    json << "\"shadow_draw_count\":" << backend.shadow_draw_count << ",";
    json << "\"shadow_fallback_count\":" << backend.shadow_fallback_count << ",";
    json << "\"opacity_command_count\":" << backend.opacity_command_count << ",";
    json << "\"opacity_end_command_count\":" << backend.opacity_end_command_count << ",";
    json << "\"opacity_apply_count\":" << backend.opacity_apply_count << ",";
    json << "\"transform_command_count\":" << backend.transform_command_count << ",";
    json << "\"transform_end_command_count\":" << backend.transform_end_command_count << ",";
    json << "\"transform_apply_count\":" << backend.transform_apply_count << ",";
    json << "\"text_draw_count\":" << backend.text_draw_count << ",";
    json << "\"image_draw_count\":" << backend.image_draw_count << ",";
    json << "\"clip_command_count\":" << backend.clip_command_count << ",";
    json << "\"clip_end_command_count\":" << backend.clip_end_command_count << ",";
    json << "\"clip_apply_count\":" << backend.clip_apply_count << ",";
    json << "\"rounded_clip_command_count\":" << backend.rounded_clip_command_count << ",";
    json << "\"rounded_clip_end_command_count\":"
         << backend.rounded_clip_end_command_count << ",";
    json << "\"rounded_clip_apply_count\":" << backend.rounded_clip_apply_count << ",";
    json << "\"rounded_clip_fallback_count\":"
         << backend.rounded_clip_fallback_count << ",";
    json << "\"unsupported_draw_command_count\":"
         << backend.unsupported_draw_command_count << ",";
    json << "\"presented\":" << (backend.presented ? "true" : "false") << ",";
    json << "\"device\":{";
    json << "\"backend_name\":\"" << backend.device.backend_name << "\",";
    json << "\"initialized\":" << (backend.device.initialized ? "true" : "false") << ",";
    json << "\"device_available\":" << (backend.device.device_available ? "true" : "false")
         << ",";
    json << "\"immediate_context_available\":"
         << (backend.device.immediate_context_available ? "true" : "false") << ",";
    json << "\"device_lost\":" << (backend.device.device_lost ? "true" : "false") << ",";
    json << "\"window_bound\":" << (backend.device.window_bound ? "true" : "false") << ",";
    json << "\"swap_chain_available\":"
         << (backend.device.swap_chain_available ? "true" : "false") << ",";
    json << "\"render_target_available\":"
         << (backend.device.render_target_available ? "true" : "false") << ",";
    json << "\"device_create_count\":" << backend.device.device_create_count << ",";
    json << "\"device_lost_count\":" << backend.device.device_lost_count << ",";
    json << "\"device_recovery_count\":" << backend.device.device_recovery_count << ",";
    json << "\"swap_chain_create_count\":" << backend.device.swap_chain_create_count << ",";
    json << "\"render_target_create_count\":" << backend.device.render_target_create_count << ",";
    json << "\"render_target_resize_count\":" << backend.device.render_target_resize_count << ",";
    json << "\"frame_submit_count\":" << backend.device.frame_submit_count << ",";
    json << "\"headless_submit_count\":" << backend.device.headless_submit_count << ",";
    json << "\"render_target_clear_count\":" << backend.device.render_target_clear_count << ",";
    json << "\"rect_draw_count\":" << backend.device.rect_draw_count << ",";
    json << "\"rounded_rect_draw_count\":" << backend.device.rounded_rect_draw_count << ",";
    json << "\"rounded_rect_pipeline_create_count\":"
         << backend.device.rounded_rect_pipeline_create_count << ",";
    json << "\"shadow_draw_count\":" << backend.device.shadow_draw_count << ",";
    json << "\"shadow_fallback_count\":" << backend.device.shadow_fallback_count << ",";
    json << "\"opacity_command_count\":" << backend.device.opacity_command_count << ",";
    json << "\"opacity_end_command_count\":" << backend.device.opacity_end_command_count << ",";
    json << "\"opacity_apply_count\":" << backend.device.opacity_apply_count << ",";
    json << "\"transform_command_count\":" << backend.device.transform_command_count << ",";
    json << "\"transform_end_command_count\":"
         << backend.device.transform_end_command_count << ",";
    json << "\"transform_apply_count\":" << backend.device.transform_apply_count << ",";
    json << "\"text_draw_count\":" << backend.device.text_draw_count << ",";
    json << "\"text_pipeline_create_count\":"
         << backend.device.text_pipeline_create_count << ",";
    json << "\"text_draw_failure_count\":" << backend.device.text_draw_failure_count << ",";
    json << "\"image_draw_count\":" << backend.device.image_draw_count << ",";
    json << "\"image_pipeline_create_count\":"
         << backend.device.image_pipeline_create_count << ",";
    json << "\"clip_command_count\":" << backend.device.clip_command_count << ",";
    json << "\"clip_end_command_count\":" << backend.device.clip_end_command_count << ",";
    json << "\"clip_apply_count\":" << backend.device.clip_apply_count << ",";
    json << "\"clip_stack_depth\":" << backend.device.clip_stack_depth << ",";
    json << "\"rounded_clip_command_count\":"
         << backend.device.rounded_clip_command_count << ",";
    json << "\"rounded_clip_end_command_count\":"
         << backend.device.rounded_clip_end_command_count << ",";
    json << "\"rounded_clip_apply_count\":"
         << backend.device.rounded_clip_apply_count << ",";
    json << "\"rounded_clip_fallback_count\":"
         << backend.device.rounded_clip_fallback_count << ",";
    json << "\"scissor_state_create_count\":"
         << backend.device.scissor_state_create_count << ",";
    json << "\"present_count\":" << backend.device.present_count << ",";
    json << "\"unsupported_command_count\":" << backend.device.unsupported_command_count << ",";
    json << "\"unsupported_draw_command_count\":"
         << backend.device.unsupported_draw_command_count << ",";
    json << "\"texture_create_count\":" << backend.device.texture_create_count << ",";
    json << "\"texture_upload_count\":" << backend.device.texture_upload_count << ",";
    json << "\"texture_fallback_count\":" << backend.device.texture_fallback_count << ",";
    json << "\"texture_release_count\":" << backend.device.texture_release_count << ",";
    json << "\"texture_upload_failure_count\":"
         << backend.device.texture_upload_failure_count << ",";
    json << "\"live_texture_count\":" << backend.device.live_texture_count << ",";
    json << "\"draw_failure_count\":" << backend.device.draw_failure_count << ",";
    json << "\"present_failure_count\":" << backend.device.present_failure_count << ",";
    json << "\"bound_width\":" << backend.device.bound_width << ",";
    json << "\"bound_height\":" << backend.device.bound_height << ",";
    json << "\"feature_level\":" << backend.device.feature_level << ",";
    json << "\"last_hresult\":" << backend.device.last_hresult << ",";
    json << "\"last_failure\":\""
         << backend_failure_reason_name(backend.device.last_failure) << "\"";
    json << "}";
    json << "}";
}

void write_scheduler_summary_json(std::ostringstream& json, const FrameSchedulerState& scheduler)
{
    json << "{";
    json << "\"pending\":" << (scheduler.pending ? "true" : "false") << ",";
    json << "\"requested_count\":" << scheduler.requested_count << ",";
    json << "\"coalesced_count\":" << scheduler.coalesced_count << ",";
    json << "\"completed_count\":" << scheduler.completed_count << ",";
    json << "\"last_request_frame_id\":" << scheduler.last_request_frame_id << ",";
    json << "\"last_completed_frame_id\":" << scheduler.last_completed_frame_id << ",";
    json << "\"last_object_id\":" << scheduler.last_object_id << ",";
    json << "\"last_generation\":" << scheduler.last_generation << ",";
    json << "\"last_reason\":\"" << scheduler.last_reason << "\",";
    json << "\"last_source\":\"" << scheduler.last_source << "\",";
    json << "\"last_path\":\"" << scheduler.last_path << "\"";
    json << "}";
}

} // namespace

FrameReport render_frame(const Window& window)
{
    FrameReport report;
    report.frame_id = next_frame_id();
    WidgetImpl* root = window.impl();
    if (root == nullptr) {
        return report;
    }

    const auto layout_start = std::chrono::steady_clock::now();
    const float width = root->dirty.bounds.width > 0.0f ? root->dirty.bounds.width : 900.0f;
    const float height = root->dirty.bounds.height > 0.0f ? root->dirty.bounds.height : 600.0f;
    LayoutSystem layout_system;
    const LayoutResult layout_result =
        layout_system.arrange(*root, LayoutConstraints{width, height});
    const auto layout_end = std::chrono::steady_clock::now();
    report.layout_ms = elapsed_ms(layout_start, layout_end);
    (void)layout_result;

    DirtyTracker dirty_tracker;
    DirtyPlan dirty_plan = dirty_tracker.plan(*root, default_runtime().dirty_thresholds());
    report.original_dirty_rects = dirty_plan.original_rect_count;
    report.merged_dirty_rects = dirty_plan.merged_rect_count;
    report.full_repaint = dirty_plan.full_repaint;
    report.fallback_reason = dirty_plan.fallback_reason;
    const FrameSchedulerState scheduler_state_before_render =
        default_runtime().frame_scheduler().state();

    const auto display_start = std::chrono::steady_clock::now();
    RenderSystem render_system;
    RenderFrameResult render_result = render_system.build_frame(*root);
    const auto display_end = std::chrono::steady_clock::now();
    report.display_list_ms = elapsed_ms(display_start, display_end);

    const auto backend_start = std::chrono::steady_clock::now();
    const BackendFrameResult backend_result =
        default_runtime().backend().submit(render_result.display_list, render_result.batches);
    const auto backend_end = std::chrono::steady_clock::now();
    const double backend_ms = elapsed_ms(backend_start, backend_end);
    refresh_resource_snapshots(render_result);

    const DebugMode diagnostics = diagnostics_mode();
    if (diagnostics == DebugMode::AiFriendly || diagnostics == DebugMode::Verbose ||
        diagnostics == DebugMode::Stress) {
        const bool compact_diagnostics =
            diagnostics == DebugMode::AiFriendly &&
            compact_frame_diagnostics_reason(scheduler_state_before_render.last_reason.c_str());
        std::ostringstream frame;
        frame << "{\n";
        frame << "  \"schema\": \"fiui.frame.v0\",\n";
        frame << "  \"session_id\": \"" << default_runtime().diagnostics().session_id() << "\",\n";
        frame << "  \"frame_id\": " << report.frame_id << ",\n";
        frame << "  \"diagnostics_detail\": \""
              << (compact_diagnostics ? "compact" : "full") << "\",\n";
        frame << "  \"backend\": \"d3d11-retained-display-list-v0\",\n";
        frame << "  \"dirty\": {\n";
        frame << "    \"original_rect_count\": " << report.original_dirty_rects << ",\n";
        frame << "    \"merged_rect_count\": " << report.merged_dirty_rects << ",\n";
        frame << "    \"full_repaint\": " << (report.full_repaint ? "true" : "false") << ",\n";
        frame << "    \"fallback_reason\": \"" << report.fallback_reason << "\",\n";
        frame << "    \"category_counts\": {";
        frame << "\"layout\": " << dirty_plan.layout_dirty_count << ", ";
        frame << "\"paint\": " << dirty_plan.paint_dirty_count << ", ";
        frame << "\"input\": " << dirty_plan.input_dirty_count << ", ";
        frame << "\"style\": " << dirty_plan.style_dirty_count << ", ";
        frame << "\"resource\": " << dirty_plan.resource_dirty_count << ", ";
        frame << "\"theme\": " << dirty_plan.theme_dirty_count << ", ";
        frame << "\"hierarchy\": " << dirty_plan.hierarchy_dirty_count << "},\n";
        frame << "    \"rects\": ";
        write_dirty_json(frame, dirty_plan);
        frame << "\n  },\n";
        frame << "  \"render_tree_summary\": ";
        write_render_tree_summary_json(frame, render_result.render_tree);
        frame << ",\n";
        frame << "  \"layer_tree_summary\": ";
        write_layer_tree_summary_json(frame, render_result.layer_tree);
        frame << ",\n";
        if (compact_diagnostics) {
            frame << "  \"display_list_summary\": {\"command_count\": "
                  << render_result.display_list.commands.size() << "},\n";
        } else {
            frame << "  \"display_list_summary\": ";
            write_display_list_summary_json(frame, render_result.display_list);
            frame << ",\n";
        }
        frame << "  \"backend_summary\": ";
        write_backend_summary_json(frame, render_result.backend);
        frame << ",\n";
        frame << "  \"backend_device_summary\": ";
        write_backend_device_summary_json(frame, backend_result);
        frame << ",\n";
        frame << "  \"scheduler_summary\": ";
        write_scheduler_summary_json(frame, scheduler_state_before_render);
        frame << ",\n";
        frame << "  \"timing\": {\"layout_ms\": " << report.layout_ms
              << ", \"display_list_ms\": " << report.display_list_ms
              << ", \"backend_ms\": " << backend_ms << "},\n";
        if (compact_diagnostics) {
            frame << "  \"widget_tree\": null,\n";
            frame << "  \"compact_reason\": \"" << scheduler_state_before_render.last_reason
                  << "\"";
        } else {
            frame << "  \"widget_tree\":\n";
            write_widget_json(frame, *root, 2);
        }
        frame << "\n}\n";

        diagnostics_frame_json(frame.str());
    }
    diagnostics_event_ex("render", "frame", root->object.object_id, root->object.generation,
                         report.frame_id, widget_path(*root).c_str(), report.fallback_reason);
    dirty_tracker.clear(*root);
    default_runtime().frame_scheduler().complete_frame(report.frame_id);
    last_frame_report_storage() = report;
    return report;
}

RuntimeSnapshot runtime_snapshot()
{
    const PlatformState platform = default_runtime().platform_system().state();
    RuntimeSnapshot snapshot;
    snapshot.last_frame = last_frame_report_storage();
    snapshot.focus_target = default_runtime().event_system().focus_target();
    snapshot.hover_target = default_runtime().event_system().hover_target();
    snapshot.capture_target = default_runtime().event_system().capture_target();
    snapshot.last_platform_event = platform_event_type_name(platform.last_event);
    snapshot.input_event_count = platform.input_event_count;
    snapshot.pointer_route_count = platform.pointer_route_count;
    snapshot.keyboard_route_count = platform.keyboard_route_count;
    return snapshot;
}

} // namespace fiui
