#pragma once

#include "fiui/export.h"
#include "fiui/types.h"
#include "resource/resource_system.h"

#include <cstdint>
#include <memory>

namespace fiui {

struct ImageSystemState {
    bool factory_initialized = false;
    std::uint32_t factory_create_count = 0;
    std::uint32_t metadata_query_count = 0;
    std::uint32_t fallback_metadata_count = 0;
    std::int32_t last_hresult = 0;
    const char* backend_name = "wic";
};

class ImageSystem {
public:
    FIUI_API ImageSystem();
    FIUI_API ~ImageSystem();
    ImageSystem(const ImageSystem&) = delete;
    ImageSystem& operator=(const ImageSystem&) = delete;

    [[nodiscard]] FIUI_API ImageMetadata query_metadata(const char* resource_path,
                                                        float fallback_width,
                                                        float fallback_height,
                                                        ObjectId owner_object_id,
                                                        const char* owner_path);
    [[nodiscard]] FIUI_API ImageSystemState state() const noexcept;

private:
    struct NativeState;

    [[nodiscard]] bool initialize_factory();
    [[nodiscard]] ImageMetadata fallback_metadata(const char* resource_path,
                                                  float fallback_width,
                                                  float fallback_height,
                                                  ObjectId owner_object_id,
                                                  const char* owner_path);

    ImageSystemState state_;
    std::unique_ptr<NativeState> native_;
};

} // namespace fiui
