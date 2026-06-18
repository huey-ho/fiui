#pragma once

#include "fiui/export.h"
#include "render/render_system.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace fiui {

enum class BackendFailureReason {
    None,
    DeviceUnavailable,
    DeviceLost,
    UnsupportedCommand,
    SubmitFailed,
    SwapChainUnavailable,
    RenderTargetUnavailable,
    RecoveryFailed,
    PipelineUnavailable,
    DrawFailed,
    PresentFailed,
};

struct BackendDeviceState {
    bool initialized = false;
    bool device_available = false;
    bool immediate_context_available = false;
    bool device_lost = false;
    bool window_bound = false;
    bool swap_chain_available = false;
    bool render_target_available = false;
    std::uint32_t device_create_count = 0;
    std::uint32_t device_lost_count = 0;
    std::uint32_t device_recovery_count = 0;
    std::uint32_t swap_chain_create_count = 0;
    std::uint32_t render_target_create_count = 0;
    std::uint32_t render_target_resize_count = 0;
    std::uint32_t frame_submit_count = 0;
    std::uint32_t headless_submit_count = 0;
    std::uint32_t render_target_clear_count = 0;
    std::uint32_t rect_draw_count = 0;
    std::uint32_t rounded_rect_draw_count = 0;
    std::uint32_t rounded_rect_pipeline_create_count = 0;
    std::uint32_t shadow_draw_count = 0;
    std::uint32_t shadow_fallback_count = 0;
    std::uint32_t opacity_command_count = 0;
    std::uint32_t opacity_end_command_count = 0;
    std::uint32_t opacity_apply_count = 0;
    std::uint32_t transform_command_count = 0;
    std::uint32_t transform_end_command_count = 0;
    std::uint32_t transform_apply_count = 0;
    std::uint32_t text_draw_count = 0;
    std::uint32_t text_pipeline_create_count = 0;
    std::uint32_t text_draw_failure_count = 0;
    std::uint32_t image_draw_count = 0;
    std::uint32_t image_pipeline_create_count = 0;
    std::uint32_t clip_command_count = 0;
    std::uint32_t clip_end_command_count = 0;
    std::uint32_t clip_apply_count = 0;
    std::uint32_t clip_stack_depth = 0;
    std::uint32_t rounded_clip_command_count = 0;
    std::uint32_t rounded_clip_end_command_count = 0;
    std::uint32_t rounded_clip_apply_count = 0;
    std::uint32_t rounded_clip_fallback_count = 0;
    std::uint32_t scissor_state_create_count = 0;
    std::uint32_t present_count = 0;
    std::uint32_t unsupported_command_count = 0;
    std::uint32_t unsupported_draw_command_count = 0;
    std::uint32_t texture_create_count = 0;
    std::uint32_t texture_upload_count = 0;
    std::uint32_t texture_fallback_count = 0;
    std::uint32_t texture_release_count = 0;
    std::uint32_t texture_upload_failure_count = 0;
    std::uint32_t live_texture_count = 0;
    std::uint32_t draw_failure_count = 0;
    std::uint32_t present_failure_count = 0;
    std::uint32_t bound_width = 0;
    std::uint32_t bound_height = 0;
    std::uint32_t feature_level = 0;
    std::int32_t last_hresult = 0;
    const char* backend_name = "d3d11";
    BackendFailureReason last_failure = BackendFailureReason::None;
};

struct BackendFrameResult {
    bool submitted = false;
    BackendFailureReason failure_reason = BackendFailureReason::None;
    std::uint32_t consumed_command_count = 0;
    std::uint32_t unsupported_command_count = 0;
    std::uint32_t rect_draw_count = 0;
    std::uint32_t rounded_rect_draw_count = 0;
    std::uint32_t shadow_draw_count = 0;
    std::uint32_t shadow_fallback_count = 0;
    std::uint32_t opacity_command_count = 0;
    std::uint32_t opacity_end_command_count = 0;
    std::uint32_t opacity_apply_count = 0;
    std::uint32_t transform_command_count = 0;
    std::uint32_t transform_end_command_count = 0;
    std::uint32_t transform_apply_count = 0;
    std::uint32_t text_draw_count = 0;
    std::uint32_t image_draw_count = 0;
    std::uint32_t clip_command_count = 0;
    std::uint32_t clip_end_command_count = 0;
    std::uint32_t clip_apply_count = 0;
    std::uint32_t rounded_clip_command_count = 0;
    std::uint32_t rounded_clip_end_command_count = 0;
    std::uint32_t rounded_clip_apply_count = 0;
    std::uint32_t rounded_clip_fallback_count = 0;
    std::uint32_t unsupported_draw_command_count = 0;
    bool presented = false;
    BackendDeviceState device;
};

class D3D11Backend {
public:
    FIUI_API D3D11Backend();
    FIUI_API ~D3D11Backend();
    D3D11Backend(const D3D11Backend&) = delete;
    D3D11Backend& operator=(const D3D11Backend&) = delete;

    [[nodiscard]] FIUI_API bool initialize();
    [[nodiscard]] FIUI_API bool bind_window(void* native_handle,
                                            std::uint32_t width,
                                            std::uint32_t height);
    [[nodiscard]] FIUI_API bool resize_render_target(std::uint32_t width,
                                                     std::uint32_t height);
    [[nodiscard]] FIUI_API bool recover_device();
    FIUI_API void release_window_resources();
    [[nodiscard]] FIUI_API BackendFrameResult submit(const DisplayList& display_list,
                                                     const std::vector<BackendBatch>& batches);
    FIUI_API void simulate_device_lost(const char* detail);
    [[nodiscard]] FIUI_API BackendDeviceState state() const noexcept;

private:
    struct NativeState;

    [[nodiscard]] bool ensure_rect_pipeline();
    [[nodiscard]] bool ensure_rounded_rect_pipeline();
    [[nodiscard]] bool ensure_alpha_blend_state();
    [[nodiscard]] bool ensure_text_pipeline();
    [[nodiscard]] bool ensure_text_render_target();
    [[nodiscard]] bool ensure_image_pipeline();
    [[nodiscard]] bool ensure_clip_pipeline();
    [[nodiscard]] bool ensure_texture(const DisplayResource& resource);
    void release_textures();
    [[nodiscard]] BackendFailureReason execute_draw_commands(const DisplayList& display_list,
                                                             BackendFrameResult& result);

    BackendDeviceState state_;
    std::unique_ptr<NativeState> native_;
};

FIUI_API const char* backend_failure_reason_name(BackendFailureReason reason) noexcept;

} // namespace fiui
