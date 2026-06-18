#pragma once

#include "fiui/export.h"
#include "fiui/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace fiui {

using ResourceId = std::uint64_t;

enum class ResourceKind {
    Font,
    Image,
    TextLayout,
    D3DTexture,
    ExternalTexture,
};

enum class ResourceCacheState {
    Uncached,
    Cached,
    Released,
};

struct TextMetrics {
    bool valid = false;
    bool directwrite = false;
    float width = 0.0f;
    float height = 0.0f;
    float layout_width = 0.0f;
    float layout_height = 0.0f;
    float baseline = 0.0f;
    float font_size = 0.0f;
    std::uint32_t line_count = 0;
    std::uint32_t dpi = 96;
};

struct ImageMetadata {
    bool valid = false;
    bool wic = false;
    bool fallback = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t dpi_x = 96;
    std::uint32_t dpi_y = 96;
    std::uint32_t frame_count = 0;
    std::string pixel_format;
};

struct TextureMetadata {
    bool valid = false;
    bool uploaded = false;
    bool fallback = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::string format;
    std::uint64_t upload_generation = 0;
    std::uint32_t upload_count = 0;
    std::string last_failure;
};

struct ResourceRecord {
    ResourceId id = 0;
    ResourceKind kind = ResourceKind::Image;
    ObjectId owner_object_id = 0;
    std::string owner_path;
    std::string key;
    ResourceCacheState cache_state = ResourceCacheState::Uncached;
    TextMetrics text_metrics;
    ImageMetadata image_metadata;
    TextureMetadata texture_metadata;
};

class ResourceSystem {
public:
    [[nodiscard]] FIUI_API ResourceId register_resource(ResourceKind kind,
                                                        ObjectId owner_object_id,
                                                        const char* owner_path,
                                                        const char* key,
                                                        ResourceCacheState cache_state);
    [[nodiscard]] FIUI_API ResourceId find_or_register_resource(ResourceKind kind,
                                                                ObjectId owner_object_id,
                                                                const char* owner_path,
                                                                const char* key,
                                                                ResourceCacheState cache_state);
    FIUI_API bool release_resource(ResourceId id);
    FIUI_API bool update_text_metrics(ResourceId id, const TextMetrics& metrics);
    FIUI_API bool update_image_metadata(ResourceId id, const ImageMetadata& metadata);
    FIUI_API bool update_texture_metadata(ResourceId id, const TextureMetadata& metadata);

    [[nodiscard]] FIUI_API const ResourceRecord* find(ResourceId id) const;
    [[nodiscard]] FIUI_API std::uint32_t live_resource_count() const noexcept;

private:
    ResourceId next_resource_id_ = 1;
    std::vector<ResourceRecord> resources_;
};

FIUI_API const char* resource_kind_name(ResourceKind kind) noexcept;
FIUI_API const char* resource_cache_state_name(ResourceCacheState state) noexcept;

} // namespace fiui
