#include "resource/resource_system.h"

#include "diagnostics/diagnostics_internal.h"

namespace fiui {

ResourceId ResourceSystem::register_resource(ResourceKind kind,
                                             ObjectId owner_object_id,
                                             const char* owner_path,
                                             const char* key,
                                             ResourceCacheState cache_state)
{
    ResourceRecord record;
    record.id = next_resource_id_++;
    record.kind = kind;
    record.owner_object_id = owner_object_id;
    record.owner_path = owner_path == nullptr ? "" : owner_path;
    record.key = key == nullptr ? "" : key;
    record.cache_state = cache_state;
    resources_.push_back(record);

    diagnostics_event("resource", "create", owner_object_id, record.owner_path.c_str(),
                      resource_kind_name(kind));
    diagnostics_event("resource", "cache_state", owner_object_id, record.owner_path.c_str(),
                      resource_cache_state_name(cache_state));
    return record.id;
}

ResourceId ResourceSystem::find_or_register_resource(ResourceKind kind,
                                                     ObjectId owner_object_id,
                                                     const char* owner_path,
                                                     const char* key,
                                                     ResourceCacheState cache_state)
{
    const std::string normalized_key = key == nullptr ? "" : key;
    for (const ResourceRecord& record : resources_) {
        if (record.kind == kind && record.owner_object_id == owner_object_id &&
            record.key == normalized_key && record.cache_state != ResourceCacheState::Released) {
            diagnostics_event("resource", "cache_hit", owner_object_id, record.owner_path.c_str(),
                              resource_kind_name(kind));
            return record.id;
        }
    }

    return register_resource(kind, owner_object_id, owner_path, key, cache_state);
}

bool ResourceSystem::release_resource(ResourceId id)
{
    ResourceRecord* record = nullptr;
    for (ResourceRecord& item : resources_) {
        if (item.id == id) {
            record = &item;
            break;
        }
    }
    if (record == nullptr || record->cache_state == ResourceCacheState::Released) {
        return false;
    }

    record->cache_state = ResourceCacheState::Released;
    diagnostics_event("resource", "release", record->owner_object_id, record->owner_path.c_str(),
                      resource_kind_name(record->kind));
    return true;
}

bool ResourceSystem::update_text_metrics(ResourceId id, const TextMetrics& metrics)
{
    ResourceRecord* record = nullptr;
    for (ResourceRecord& item : resources_) {
        if (item.id == id) {
            record = &item;
            break;
        }
    }
    if (record == nullptr || record->kind != ResourceKind::TextLayout ||
        record->cache_state == ResourceCacheState::Released) {
        return false;
    }

    record->text_metrics = metrics;
    diagnostics_event("resource", "text_metrics", record->owner_object_id,
                      record->owner_path.c_str(),
                      metrics.directwrite ? "directwrite" : "fallback");
    return true;
}

bool ResourceSystem::update_image_metadata(ResourceId id, const ImageMetadata& metadata)
{
    ResourceRecord* record = nullptr;
    for (ResourceRecord& item : resources_) {
        if (item.id == id) {
            record = &item;
            break;
        }
    }
    if (record == nullptr || record->kind != ResourceKind::Image ||
        record->cache_state == ResourceCacheState::Released) {
        return false;
    }

    record->image_metadata = metadata;
    if (metadata.wic) {
        record->cache_state = ResourceCacheState::Cached;
    }
    diagnostics_event("resource", "image_metadata", record->owner_object_id,
                      record->owner_path.c_str(), metadata.wic ? "wic" : "fallback");
    return true;
}

bool ResourceSystem::update_texture_metadata(ResourceId id, const TextureMetadata& metadata)
{
    ResourceRecord* record = nullptr;
    for (ResourceRecord& item : resources_) {
        if (item.id == id) {
            record = &item;
            break;
        }
    }
    if (record == nullptr || record->kind != ResourceKind::Image ||
        record->cache_state == ResourceCacheState::Released) {
        return false;
    }

    record->texture_metadata = metadata;
    if (metadata.uploaded) {
        record->cache_state = ResourceCacheState::Cached;
    }
    diagnostics_event("resource", "texture_metadata", record->owner_object_id,
                      record->owner_path.c_str(), metadata.fallback ? "fallback" : "uploaded");
    return true;
}

const ResourceRecord* ResourceSystem::find(ResourceId id) const
{
    for (const ResourceRecord& record : resources_) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}

std::uint32_t ResourceSystem::live_resource_count() const noexcept
{
    std::uint32_t count = 0;
    for (const ResourceRecord& record : resources_) {
        if (record.cache_state != ResourceCacheState::Released) {
            ++count;
        }
    }
    return count;
}

const char* resource_kind_name(ResourceKind kind) noexcept
{
    switch (kind) {
    case ResourceKind::Font:
        return "font";
    case ResourceKind::Image:
        return "image";
    case ResourceKind::TextLayout:
        return "text_layout";
    case ResourceKind::D3DTexture:
        return "d3d_texture";
    case ResourceKind::ExternalTexture:
        return "external_texture";
    }
    return "unknown";
}

const char* resource_cache_state_name(ResourceCacheState state) noexcept
{
    switch (state) {
    case ResourceCacheState::Uncached:
        return "uncached";
    case ResourceCacheState::Cached:
        return "cached";
    case ResourceCacheState::Released:
        return "released";
    }
    return "unknown";
}

} // namespace fiui
