#pragma once

#include "fiui/export.h"
#include "fiui/types.h"

#include <cstdint>
#include <string>

namespace fiui {

struct FrameSchedulerState {
    bool pending = false;
    std::uint64_t requested_count = 0;
    std::uint64_t coalesced_count = 0;
    std::uint64_t completed_count = 0;
    std::uint64_t last_request_frame_id = 0;
    std::uint64_t last_completed_frame_id = 0;
    ObjectId last_object_id = 0;
    std::uint32_t last_generation = 0;
    std::string last_reason = "none";
    std::string last_source = "none";
    std::string last_path;
};

class FrameScheduler {
public:
    FIUI_API void request_frame(const char* reason,
                                const char* source,
                                ObjectId object_id,
                                std::uint32_t generation,
                                std::uint64_t current_frame_id,
                                const char* path);
    FIUI_API void complete_frame(std::uint64_t frame_id);
    [[nodiscard]] FIUI_API const FrameSchedulerState& state() const noexcept;

private:
    FrameSchedulerState state_;
};

} // namespace fiui
