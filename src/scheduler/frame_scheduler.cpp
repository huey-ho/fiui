#include "scheduler/frame_scheduler.h"

#include "diagnostics/diagnostics_internal.h"

namespace fiui {

void FrameScheduler::request_frame(const char* reason,
                                   const char* source,
                                   ObjectId object_id,
                                   std::uint32_t generation,
                                   std::uint64_t current_frame_id,
                                   const char* path)
{
    if (state_.pending) {
        ++state_.coalesced_count;
    } else {
        state_.pending = true;
    }
    ++state_.requested_count;
    state_.last_request_frame_id = current_frame_id;
    state_.last_object_id = object_id;
    state_.last_generation = generation;
    state_.last_reason = reason == nullptr ? "" : reason;
    state_.last_source = source == nullptr ? "" : source;
    state_.last_path = path == nullptr ? "" : path;

    diagnostics_event_ex("scheduler", "request_frame", object_id, generation, current_frame_id,
                         state_.last_path.c_str(), state_.last_reason.c_str());
}

void FrameScheduler::complete_frame(std::uint64_t frame_id)
{
    state_.pending = false;
    state_.last_completed_frame_id = frame_id;
    ++state_.completed_count;
    diagnostics_event_ex("scheduler", "complete_frame", state_.last_object_id,
                         state_.last_generation, frame_id, state_.last_path.c_str(),
                         state_.last_reason.c_str());
}

const FrameSchedulerState& FrameScheduler::state() const noexcept
{
    return state_;
}

} // namespace fiui
