#pragma once

#include "core/widget_impl.h"
#include "fiui/export.h"
#include "fiui/style.h"
#include "fiui/types.h"
#include "resource/resource_system.h"

#include <cstdint>
#include <string>
#include <vector>

namespace fiui {

enum class DisplayCommandKind {
    Rect,
    Shadow,
    Text,
    Image,
    Clip,
    ClipEnd,
    RoundedClip,
    RoundedClipEnd,
    Opacity,
    OpacityEnd,
    Transform,
    TransformEnd,
};

enum class LayerKind {
    Root,
    Rect,
    RoundedRect,
    Text,
    Image,
    Clip,
    Scroll,
    Opacity,
    Transform,
    Shadow,
};

struct RenderNode {
    ObjectId object_id = 0;
    std::uint32_t generation = 0;
    WidgetKind kind = WidgetKind::Widget;
    std::string path;
    Rect bounds;
    Rect paint_bounds;
    Rect clip_bounds;
    std::uint32_t child_count = 0;
};

struct RenderTree {
    std::vector<RenderNode> nodes;
};

struct DisplayStyle {
    const char* theme_name = "modern.light";
    const char* control_state = "normal";
    Color fill;
    Color text;
    Color border;
    float border_width = 0.0f;
    float radius = 0.0f;
    float font_size = 0.0f;
    float text_padding_x = 0.0f;
    float text_padding_y = 0.0f;
    const char* text_align = "leading";
    const char* paragraph_align = "near";
    const char* overflow = "clip";
    const char* word_wrap = "none";
};

struct DisplayResource {
    ResourceId id = 0;
    ResourceKind kind = ResourceKind::Image;
    ResourceCacheState cache_state = ResourceCacheState::Uncached;
    ObjectId owner_object_id = 0;
    std::string owner_path;
    std::string key;
    TextMetrics text_metrics;
    ImageMetadata image_metadata;
    TextureMetadata texture_metadata;
};

struct DisplayCommand {
    DisplayCommandKind kind = DisplayCommandKind::Rect;
    ObjectId object_id = 0;
    WidgetKind widget_kind = WidgetKind::Widget;
    std::string path;
    Rect bounds;
    std::string text;
    std::string resource_key;
    DisplayStyle style;
    DisplayResource resource;
    float shadow_blur = 0.0f;
    float opacity = 1.0f;
    float transform_translate_x = 0.0f;
    float transform_translate_y = 0.0f;
    float transform_scale_x = 1.0f;
    float transform_scale_y = 1.0f;
    ImageFit image_fit = ImageFit::Stretch;
    Rect image_uv = Rect{0.0f, 0.0f, 1.0f, 1.0f};
};

inline constexpr std::uint32_t invalid_display_command_index = 0xffffffffu;
inline constexpr std::uint32_t invalid_layer_id = 0xffffffffu;

struct LayerNode {
    std::uint32_t layer_id = 0;
    std::uint32_t parent_layer_id = invalid_layer_id;
    LayerKind kind = LayerKind::Root;
    ObjectId object_id = 0;
    std::uint32_t generation = 0;
    WidgetKind widget_kind = WidgetKind::Widget;
    std::string path;
    Rect bounds;
    Rect paint_bounds;
    Rect clip_bounds;
    DisplayStyle style;
    DisplayResource resource;
    std::string text;
    std::string resource_key;
    float opacity = 1.0f;
    float radius = 0.0f;
    float shadow_blur = 0.0f;
    std::uint32_t display_command_index = invalid_display_command_index;
    std::uint32_t display_command_count = 0;
    std::uint32_t child_count = 0;
};

struct LayerTree {
    std::vector<LayerNode> nodes;
};

struct DisplayList {
    std::vector<DisplayCommand> commands;
};

struct BackendBatch {
    DisplayCommandKind command_kind = DisplayCommandKind::Rect;
    std::uint32_t first_command = 0;
    std::uint32_t command_count = 0;
};

struct BackendSummary {
    std::uint32_t command_count = 0;
    std::uint32_t batch_count = 0;
    std::uint32_t rect_command_count = 0;
    std::uint32_t shadow_command_count = 0;
    std::uint32_t text_command_count = 0;
    std::uint32_t image_command_count = 0;
    std::uint32_t clip_command_count = 0;
    std::uint32_t clip_end_command_count = 0;
    std::uint32_t rounded_clip_command_count = 0;
    std::uint32_t rounded_clip_end_command_count = 0;
    std::uint32_t opacity_command_count = 0;
    std::uint32_t opacity_end_command_count = 0;
    std::uint32_t transform_command_count = 0;
    std::uint32_t transform_end_command_count = 0;
};

struct RenderFrameResult {
    RenderTree render_tree;
    LayerTree layer_tree;
    DisplayList display_list;
    std::vector<BackendBatch> batches;
    BackendSummary backend;
};

class RenderSystem {
public:
    [[nodiscard]] FIUI_API RenderFrameResult build_frame(const WidgetImpl& root) const;

private:
    void append_hover_tooltip(RenderFrameResult& result) const;
    void collect_node(const WidgetImpl& impl,
                      RenderFrameResult& result,
                      std::uint32_t parent_layer_id) const;
    void collect_open_menu_popups(const WidgetImpl& impl,
                                  RenderFrameResult& result,
                                  std::uint32_t parent_layer_id) const;
    void collect_open_select_popups(const WidgetImpl& impl,
                                    RenderFrameResult& result,
                                    std::uint32_t parent_layer_id) const;
    void collect_open_dialogs(const WidgetImpl& impl,
                              RenderFrameResult& result,
                              std::uint32_t parent_layer_id) const;
    void append_menu_popup_surface(const WidgetImpl& impl,
                                   RenderFrameResult& result,
                                   std::uint32_t parent_layer_id) const;
    void append_select_popup_surface(const WidgetImpl& impl,
                                     RenderFrameResult& result,
                                     std::uint32_t parent_layer_id) const;
    void append_dialog_backdrop(const WidgetImpl& impl,
                                RenderFrameResult& result,
                                std::uint32_t parent_layer_id) const;
    void append_table_commands(const WidgetImpl& impl,
                               RenderFrameResult& result,
                               std::uint32_t parent_layer_id) const;
    void append_node_close_commands(const WidgetImpl& impl,
                                    RenderFrameResult& result,
                                    std::uint32_t layer_id) const;
    [[nodiscard]] std::uint32_t append_layers_and_commands(const WidgetImpl& impl,
                                                           RenderFrameResult& result,
                                                           std::uint32_t parent_layer_id) const;
    void build_batches(RenderFrameResult& result) const;
    [[nodiscard]] std::uint32_t add_command(RenderFrameResult& result, DisplayCommand command) const;
    [[nodiscard]] std::uint32_t add_layer(RenderFrameResult& result,
                                          LayerNode layer,
                                          std::uint32_t parent_layer_id) const;
    [[nodiscard]] bool has_surface_command(const WidgetImpl& impl) const;
    [[nodiscard]] bool clips_child_content(const WidgetImpl& impl) const;
    [[nodiscard]] bool child_content_overflows(const WidgetImpl& impl) const;
    [[nodiscard]] bool has_text_command(const WidgetImpl& impl) const;
    [[nodiscard]] bool has_input_caret(const WidgetImpl& impl) const;
    [[nodiscard]] bool has_input_selection(const WidgetImpl& impl) const;
    [[nodiscard]] Rect text_bounds_for(const WidgetImpl& impl, const DisplayStyle& style) const;
    [[nodiscard]] Rect menu_shortcut_bounds_for(const WidgetImpl& impl,
                                                const DisplayStyle& style) const;
    [[nodiscard]] float text_advance_for(const WidgetImpl& impl,
                                         const DisplayStyle& style,
                                         std::size_t cursor) const;
    [[nodiscard]] Rect caret_bounds_for(const WidgetImpl& impl, const DisplayStyle& style) const;
    [[nodiscard]] Rect selection_bounds_for(const WidgetImpl& impl,
                                            const DisplayStyle& style) const;
    [[nodiscard]] Rect image_bounds_for(const WidgetImpl& impl,
                                        const DisplayResource& resource) const;
    [[nodiscard]] Rect image_bounds_for(const WidgetImpl& impl,
                                        const DisplayResource& resource,
                                        Rect bounds) const;
    [[nodiscard]] Rect image_uv_for(const WidgetImpl& impl,
                                    const DisplayResource& resource) const;
    [[nodiscard]] Rect image_uv_for(const WidgetImpl& impl,
                                    const DisplayResource& resource,
                                    Rect bounds) const;
    [[nodiscard]] std::string text_for(const WidgetImpl& impl) const;
};

FIUI_API const char* display_command_kind_name(DisplayCommandKind kind) noexcept;
FIUI_API const char* layer_kind_name(LayerKind kind) noexcept;
FIUI_API const char* image_fit_name(ImageFit fit) noexcept;

} // namespace fiui
