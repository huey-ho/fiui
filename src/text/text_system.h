#pragma once

#include "fiui/export.h"
#include "fiui/types.h"
#include "resource/resource_system.h"

#include <cstdint>
#include <memory>

namespace fiui {

struct TextSystemState {
    bool factory_initialized = false;
    std::uint32_t factory_create_count = 0;
    std::uint32_t measure_count = 0;
    std::uint32_t fallback_measure_count = 0;
    std::int32_t last_hresult = 0;
    const char* backend_name = "directwrite";
};

class TextSystem {
public:
    FIUI_API TextSystem();
    FIUI_API ~TextSystem();
    TextSystem(const TextSystem&) = delete;
    TextSystem& operator=(const TextSystem&) = delete;

    [[nodiscard]] FIUI_API TextMetrics measure_text(const char* text,
                                                    float font_size,
                                                    float max_width,
                                                    float max_height,
                                                    std::uint32_t dpi,
                                                    ObjectId owner_object_id,
                                                    const char* owner_path,
                                                    const char* word_wrap = "none",
                                                    const char* overflow = "clip");
    [[nodiscard]] FIUI_API TextSystemState state() const noexcept;

private:
    struct NativeState;

    [[nodiscard]] bool initialize_factory();
    [[nodiscard]] TextMetrics fallback_metrics(const char* text,
                                               float font_size,
                                               float max_width,
                                               float max_height,
                                               std::uint32_t dpi,
                                               ObjectId owner_object_id,
                                               const char* owner_path,
                                               const char* word_wrap,
                                               const char* overflow);

    TextSystemState state_;
    std::unique_ptr<NativeState> native_;
};

} // namespace fiui
