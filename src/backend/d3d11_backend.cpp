#include "backend/d3d11_backend.h"

#include "diagnostics/diagnostics_internal.h"
#include "runtime/runtime.h"

#include <array>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <d3dcompiler.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi.h>
#include <objbase.h>
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace fiui {
namespace {

bool is_supported_command(DisplayCommandKind kind) noexcept
{
    switch (kind) {
    case DisplayCommandKind::Rect:
    case DisplayCommandKind::Shadow:
    case DisplayCommandKind::Text:
    case DisplayCommandKind::Image:
    case DisplayCommandKind::Clip:
    case DisplayCommandKind::ClipEnd:
    case DisplayCommandKind::RoundedClip:
    case DisplayCommandKind::RoundedClipEnd:
    case DisplayCommandKind::Opacity:
    case DisplayCommandKind::OpacityEnd:
    case DisplayCommandKind::Transform:
    case DisplayCommandKind::TransformEnd:
        return true;
    }
    return false;
}

bool is_draw_supported_command(DisplayCommandKind kind) noexcept
{
    return kind == DisplayCommandKind::Rect || kind == DisplayCommandKind::Text ||
           kind == DisplayCommandKind::Image || kind == DisplayCommandKind::Shadow ||
           kind == DisplayCommandKind::Clip || kind == DisplayCommandKind::ClipEnd ||
           kind == DisplayCommandKind::RoundedClip ||
           kind == DisplayCommandKind::RoundedClipEnd ||
           kind == DisplayCommandKind::Opacity || kind == DisplayCommandKind::OpacityEnd ||
           kind == DisplayCommandKind::Transform || kind == DisplayCommandKind::TransformEnd;
}

float color_channel(std::uint8_t value) noexcept
{
    return static_cast<float>(value) / 255.0f;
}

std::string submit_detail(const DisplayList& display_list, const std::vector<BackendBatch>& batches)
{
    std::ostringstream detail;
    detail << "commands=" << display_list.commands.size() << ";batches=" << batches.size();
    return detail.str();
}

void set_failure(BackendDeviceState& state, BackendFailureReason reason, std::int32_t hresult = 0)
{
    state.last_failure = reason;
    state.last_hresult = hresult;
}

void emit_backend_failure(const char* action, BackendFailureReason reason, std::int32_t hresult)
{
    std::ostringstream detail;
    detail << backend_failure_reason_name(reason) << ";hresult=" << hresult;
    const std::string text = detail.str();
    diagnostics_event_ex("backend", action, 0, 0, default_runtime().current_frame_id(), "",
                         text.c_str());
}

#if defined(_WIN32)

constexpr DXGI_FORMAT swap_chain_format = DXGI_FORMAT_B8G8R8A8_UNORM;
constexpr DXGI_FORMAT texture_format = DXGI_FORMAT_B8G8R8A8_UNORM;
constexpr DXGI_FORMAT d2d_target_format = DXGI_FORMAT_B8G8R8A8_UNORM;

struct RectVertex {
    float x = 0.0f;
    float y = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};

struct RoundedRectVertex {
    float x = 0.0f;
    float y = 0.0f;
    float pixel_x = 0.0f;
    float pixel_y = 0.0f;
    float rect_left = 0.0f;
    float rect_top = 0.0f;
    float rect_right = 0.0f;
    float rect_bottom = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
    float radius = 0.0f;
};

struct ImageVertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float opacity = 1.0f;
};

struct ClipSnapshot {
    bool active = false;
    Rect rect;
    bool rounded = false;
    float radius = 0.0f;
};

struct CommandEffectSnapshot {
    float opacity = 1.0f;
    float translate_x = 0.0f;
    float translate_y = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
};

std::uint32_t feature_level_value(D3D_FEATURE_LEVEL level) noexcept
{
    return static_cast<std::uint32_t>(level);
}

float ndc_x(float x, float width) noexcept
{
    return width > 0.0f ? (x / width) * 2.0f - 1.0f : -1.0f;
}

float ndc_y(float y, float height) noexcept
{
    return height > 0.0f ? 1.0f - (y / height) * 2.0f : 1.0f;
}

std::uint8_t scaled_alpha(std::uint8_t alpha, float opacity) noexcept
{
    const float scaled = static_cast<float>(alpha) * std::max(0.0f, std::min(1.0f, opacity));
    return static_cast<std::uint8_t>(std::max(0.0f, std::min(255.0f, scaled)));
}

Rect apply_effect(Rect rect, const CommandEffectSnapshot& effect) noexcept
{
    rect.x = rect.x * effect.scale_x + effect.translate_x;
    rect.y = rect.y * effect.scale_y + effect.translate_y;
    rect.width *= effect.scale_x;
    rect.height *= effect.scale_y;
    if (rect.width < 0.0f) {
        rect.x += rect.width;
        rect.width = -rect.width;
    }
    if (rect.height < 0.0f) {
        rect.y += rect.height;
        rect.height = -rect.height;
    }
    return rect;
}

DisplayCommand apply_effect(DisplayCommand command, const CommandEffectSnapshot& effect)
{
    command.bounds = apply_effect(command.bounds, effect);
    command.style.fill.a = scaled_alpha(command.style.fill.a, effect.opacity);
    command.style.text.a = scaled_alpha(command.style.text.a, effect.opacity);
    command.style.border.a = scaled_alpha(command.style.border.a, effect.opacity);
    return command;
}

bool is_fully_transparent_fill(const DisplayCommand& command) noexcept
{
    return command.style.fill.a == 0;
}

void append_rect_vertices(std::vector<RectVertex>& vertices,
                          const DisplayCommand& command,
                          float target_width,
                          float target_height)
{
    if (command.bounds.width <= 0.0f || command.bounds.height <= 0.0f ||
        is_fully_transparent_fill(command)) {
        return;
    }

    const float left = ndc_x(command.bounds.x, target_width);
    const float right = ndc_x(command.bounds.x + command.bounds.width, target_width);
    const float top = ndc_y(command.bounds.y, target_height);
    const float bottom = ndc_y(command.bounds.y + command.bounds.height, target_height);
    const float r = color_channel(command.style.fill.r);
    const float g = color_channel(command.style.fill.g);
    const float b = color_channel(command.style.fill.b);
    const float a = color_channel(command.style.fill.a);

    vertices.push_back(RectVertex{left, top, r, g, b, a});
    vertices.push_back(RectVertex{right, top, r, g, b, a});
    vertices.push_back(RectVertex{left, bottom, r, g, b, a});
    vertices.push_back(RectVertex{left, bottom, r, g, b, a});
    vertices.push_back(RectVertex{right, top, r, g, b, a});
    vertices.push_back(RectVertex{right, bottom, r, g, b, a});
}

void append_rounded_rect_vertices(std::vector<RoundedRectVertex>& vertices,
                                  const DisplayCommand& command,
                                  float target_width,
                                  float target_height)
{
    if (command.bounds.width <= 0.0f || command.bounds.height <= 0.0f ||
        is_fully_transparent_fill(command)) {
        return;
    }

    const float left_px = command.bounds.x;
    const float right_px = command.bounds.x + command.bounds.width;
    const float top_px = command.bounds.y;
    const float bottom_px = command.bounds.y + command.bounds.height;
    const float left = ndc_x(left_px, target_width);
    const float right = ndc_x(right_px, target_width);
    const float top = ndc_y(top_px, target_height);
    const float bottom = ndc_y(bottom_px, target_height);
    const float radius = std::max(0.0f, std::min(command.style.radius,
                                                 std::min(command.bounds.width, command.bounds.height) * 0.5f));
    const float r = color_channel(command.style.fill.r);
    const float g = color_channel(command.style.fill.g);
    const float b = color_channel(command.style.fill.b);
    const float a = color_channel(command.style.fill.a);

    const auto vertex = [&](float x, float y, float px, float py) {
        return RoundedRectVertex{x, y, px, py, left_px, top_px, right_px, bottom_px, r, g, b, a,
                                 radius};
    };

    vertices.push_back(vertex(left, top, left_px, top_px));
    vertices.push_back(vertex(right, top, right_px, top_px));
    vertices.push_back(vertex(left, bottom, left_px, bottom_px));
    vertices.push_back(vertex(left, bottom, left_px, bottom_px));
    vertices.push_back(vertex(right, top, right_px, top_px));
    vertices.push_back(vertex(right, bottom, right_px, bottom_px));
}

DisplayCommand shadow_fallback_command(const DisplayCommand& command,
                                       std::uint32_t layer,
                                       std::uint32_t layer_count)
{
    DisplayCommand shadow = command;
    const float t = static_cast<float>(layer + 1) / static_cast<float>(layer_count);
    const float spread = std::max(1.0f, command.shadow_blur * 0.35f * t);
    shadow.bounds.x += spread * 0.25f;
    shadow.bounds.y += spread * 0.45f;
    shadow.bounds.width += spread;
    shadow.bounds.height += spread;
    shadow.style.fill = Color{0, 0, 0,
                              static_cast<std::uint8_t>(std::max(8.0f, 44.0f * (1.0f - t)))};
    shadow.style.radius = command.style.radius + spread * 0.5f;
    return shadow;
}

void append_image_vertices(std::vector<ImageVertex>& vertices,
                           const DisplayCommand& command,
                           float target_width,
                           float target_height)
{
    if (command.bounds.width <= 0.0f || command.bounds.height <= 0.0f) {
        return;
    }

    const float left = ndc_x(command.bounds.x, target_width);
    const float right = ndc_x(command.bounds.x + command.bounds.width, target_width);
    const float top = ndc_y(command.bounds.y, target_height);
    const float bottom = ndc_y(command.bounds.y + command.bounds.height, target_height);
    const float opacity = color_channel(command.style.fill.a);
    const float uv_left = command.image_uv.x;
    const float uv_top = command.image_uv.y;
    const float uv_right = command.image_uv.x + command.image_uv.width;
    const float uv_bottom = command.image_uv.y + command.image_uv.height;

    vertices.push_back(ImageVertex{left, top, uv_left, uv_top, opacity});
    vertices.push_back(ImageVertex{right, top, uv_right, uv_top, opacity});
    vertices.push_back(ImageVertex{left, bottom, uv_left, uv_bottom, opacity});
    vertices.push_back(ImageVertex{left, bottom, uv_left, uv_bottom, opacity});
    vertices.push_back(ImageVertex{right, top, uv_right, uv_top, opacity});
    vertices.push_back(ImageVertex{right, bottom, uv_right, uv_bottom, opacity});
}

std::wstring utf8_to_wide_path(const std::string& text)
{
    if (text.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (needed <= 1) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), needed);
    return wide;
}

std::wstring resolve_image_path(const std::wstring& path)
{
    if (path.empty()) {
        return {};
    }

    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return path;
    }

    wchar_t module_path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return path;
    }

    std::wstring module_dir(module_path, module_path + length);
    const std::size_t slash = module_dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return path;
    }
    module_dir.resize(slash + 1);

    const std::wstring candidate = module_dir + path;
    attrs = GetFileAttributesW(candidate.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return candidate;
    }

    return path;
}

const char* texture_format_name(DXGI_FORMAT format) noexcept
{
    return format == texture_format ? "bgra8_unorm" : "unknown";
}

D2D1_COLOR_F d2d_color(Color color) noexcept
{
    return D2D1_COLOR_F{color_channel(color.r), color_channel(color.g), color_channel(color.b),
                        color_channel(color.a)};
}

DWRITE_TEXT_ALIGNMENT dwrite_text_alignment(const char* value) noexcept
{
    if (value != nullptr && std::strcmp(value, "center") == 0) {
        return DWRITE_TEXT_ALIGNMENT_CENTER;
    }
    if (value != nullptr && std::strcmp(value, "trailing") == 0) {
        return DWRITE_TEXT_ALIGNMENT_TRAILING;
    }
    return DWRITE_TEXT_ALIGNMENT_LEADING;
}

DWRITE_PARAGRAPH_ALIGNMENT dwrite_paragraph_alignment(const char* value) noexcept
{
    if (value != nullptr && std::strcmp(value, "center") == 0) {
        return DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    }
    if (value != nullptr && std::strcmp(value, "far") == 0) {
        return DWRITE_PARAGRAPH_ALIGNMENT_FAR;
    }
    return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
}

bool is_empty_rect(Rect rect) noexcept
{
    return rect.width <= 0.0f || rect.height <= 0.0f;
}

Rect intersect_rect(Rect a, Rect b) noexcept
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

D3D11_RECT d3d_scissor_rect(Rect rect, float target_width, float target_height) noexcept
{
    const LONG left = static_cast<LONG>(std::max(0.0f, rect.x));
    const LONG top = static_cast<LONG>(std::max(0.0f, rect.y));
    const LONG right = static_cast<LONG>(std::min(target_width, rect.x + rect.width));
    const LONG bottom = static_cast<LONG>(std::min(target_height, rect.y + rect.height));
    return D3D11_RECT{left, top, std::max(left, right), std::max(top, bottom)};
}

D2D1_RECT_F d2d_clip_rect(Rect rect) noexcept
{
    return D2D1_RECT_F{rect.x, rect.y, rect.x + rect.width, rect.y + rect.height};
}

#endif

} // namespace

struct D3D11Backend::NativeState {
#if defined(_WIN32)
    struct BackendTexture {
        ResourceId id = 0;
        std::string key;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool fallback = false;
        std::uint64_t generation = 0;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
    };

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> rect_vertex_shader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> rect_pixel_shader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> rect_input_layout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> rect_vertex_buffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> rounded_rect_vertex_shader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> rounded_rect_pixel_shader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> rounded_rect_input_layout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> rounded_rect_vertex_buffer;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2d_factory;
    Microsoft::WRL::ComPtr<ID2D1Device> d2d_device;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2d_context;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_target_bitmap;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> image_vertex_shader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> image_pixel_shader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> image_input_layout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> image_vertex_buffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> image_sampler_state;
    Microsoft::WRL::ComPtr<ID3D11BlendState> image_blend_state;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> default_rasterizer_state;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> scissor_rasterizer_state;
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory;
    std::unordered_map<ResourceId, BackendTexture> textures;
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_9_1;
    HWND hwnd = nullptr;
    bool co_initialized = false;
    std::uint64_t texture_generation = 0;
#else
    void* native_handle = nullptr;
    std::unordered_map<ResourceId, TextureMetadata> textures;
    std::uint64_t texture_generation = 0;
#endif
};

D3D11Backend::D3D11Backend() : native_(std::make_unique<NativeState>()) {}

D3D11Backend::~D3D11Backend()
{
    release_textures();
#if defined(_WIN32)
    native_->wic_factory.Reset();
    if (native_->co_initialized) {
        CoUninitialize();
    }
#endif
}

bool D3D11Backend::initialize()
{
    if (state_.initialized) {
        return !state_.device_lost;
    }

#if defined(_WIN32)
    constexpr D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, feature_levels,
                                   static_cast<UINT>(std::size(feature_levels)),
                                   D3D11_SDK_VERSION, native_->device.GetAddressOf(),
                                   &native_->feature_level, native_->context.GetAddressOf());
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                               D3D11_CREATE_DEVICE_BGRA_SUPPORT, feature_levels,
                               static_cast<UINT>(std::size(feature_levels)), D3D11_SDK_VERSION,
                               native_->device.GetAddressOf(), &native_->feature_level,
                               native_->context.GetAddressOf());
    }

    state_.device_create_count += 1;
    if (FAILED(hr)) {
        state_.initialized = false;
        state_.device_available = false;
        state_.immediate_context_available = false;
        set_failure(state_, BackendFailureReason::DeviceUnavailable, static_cast<std::int32_t>(hr));
        emit_backend_failure("initialize_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    state_.feature_level = feature_level_value(native_->feature_level);
    state_.device_available = native_->device != nullptr;
    state_.immediate_context_available = native_->context != nullptr;
#else
    state_.device_create_count += 1;
    state_.device_available = true;
    state_.immediate_context_available = true;
    state_.feature_level = 0;
#endif

    state_.initialized = true;
    state_.device_lost = false;
    set_failure(state_, BackendFailureReason::None);
    diagnostics_event_ex("backend", "initialize", 0, 0, default_runtime().current_frame_id(), "",
                         state_.backend_name);
    return true;
}

bool D3D11Backend::bind_window(void* native_handle, std::uint32_t width, std::uint32_t height)
{
    if (native_handle == nullptr) {
        set_failure(state_, BackendFailureReason::SwapChainUnavailable);
        emit_backend_failure("bind_window_failed", state_.last_failure, state_.last_hresult);
        return false;
    }
    if (!initialize()) {
        return false;
    }

    release_window_resources();
    state_.window_bound = true;
    state_.bound_width = width;
    state_.bound_height = height;

#if defined(_WIN32)
    native_->hwnd = static_cast<HWND>(native_handle);

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    HRESULT hr = native_->device.As(&dxgi_device);
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::SwapChainUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("swap_chain_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgi_device->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::SwapChainUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("swap_chain_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory> factory;
    hr = adapter->GetParent(__uuidof(IDXGIFactory),
                            reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::SwapChainUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("swap_chain_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width = width;
    desc.BufferDesc.Height = height;
    desc.BufferDesc.Format = swap_chain_format;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.OutputWindow = native_->hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    hr = factory->CreateSwapChain(native_->device.Get(), &desc, native_->swap_chain.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::SwapChainUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("swap_chain_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    state_.swap_chain_available = true;
    state_.swap_chain_create_count += 1;
    diagnostics_event_ex("backend", "swap_chain_create", 0, 0,
                         default_runtime().current_frame_id(), "", "window bound");
    return resize_render_target(width, height);
#else
    native_->native_handle = native_handle;
    state_.swap_chain_available = true;
    state_.render_target_available = true;
    state_.swap_chain_create_count += 1;
    state_.render_target_create_count += 1;
    diagnostics_event_ex("backend", "swap_chain_create", 0, 0,
                         default_runtime().current_frame_id(), "", "window bound");
    diagnostics_event_ex("backend", "render_target_create", 0, 0,
                         default_runtime().current_frame_id(), "", "scaffold target");
    return true;
#endif
}

bool D3D11Backend::resize_render_target(std::uint32_t width, std::uint32_t height)
{
    state_.bound_width = width;
    state_.bound_height = height;

#if defined(_WIN32)
    if (native_->swap_chain == nullptr) {
        set_failure(state_, BackendFailureReason::RenderTargetUnavailable);
        emit_backend_failure("render_target_failed", state_.last_failure, state_.last_hresult);
        return false;
    }
    if (width == 0 || height == 0) {
        state_.render_target_available = false;
        set_failure(state_, BackendFailureReason::RenderTargetUnavailable);
        diagnostics_event_ex("backend", "render_target_resize_skipped", 0, 0,
                             default_runtime().current_frame_id(), "",
                             "zero-sized window");
        return false;
    }

    if (native_->d2d_context != nullptr) {
        native_->d2d_context->SetTarget(nullptr);
    }
    native_->d2d_target_bitmap.Reset();
    native_->render_target_view.Reset();
    if (native_->context != nullptr) {
        native_->context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    HRESULT hr = native_->swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::RenderTargetUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("resize_buffers_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    hr = native_->swap_chain->GetBuffer(
        0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back_buffer.GetAddressOf()));
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::RenderTargetUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("render_target_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    hr = native_->device->CreateRenderTargetView(back_buffer.Get(), nullptr,
                                                 native_->render_target_view.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::RenderTargetUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("render_target_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    state_.render_target_available = true;
    state_.render_target_create_count += 1;
    state_.render_target_resize_count += 1;
    set_failure(state_, BackendFailureReason::None);
    diagnostics_event_ex("backend", "render_target_create", 0, 0,
                         default_runtime().current_frame_id(), "", "back buffer target");
    return true;
#else
    state_.render_target_available = state_.swap_chain_available;
    state_.render_target_resize_count += 1;
    set_failure(state_, state_.render_target_available ? BackendFailureReason::None
                                                       : BackendFailureReason::RenderTargetUnavailable);
    return state_.render_target_available;
#endif
}

bool D3D11Backend::recover_device()
{
    release_window_resources();
    release_textures();
#if defined(_WIN32)
    if (native_->d2d_context != nullptr) {
        native_->d2d_context->SetTarget(nullptr);
    }
    native_->d2d_target_bitmap.Reset();
    native_->d2d_context.Reset();
    native_->d2d_device.Reset();
    native_->context.Reset();
    native_->device.Reset();
#endif
    state_.initialized = false;
    state_.device_available = false;
    state_.immediate_context_available = false;
    state_.device_lost = false;
    state_.device_recovery_count += 1;

    if (!initialize()) {
        set_failure(state_, BackendFailureReason::RecoveryFailed, state_.last_hresult);
        emit_backend_failure("recover_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    diagnostics_event_ex("backend", "recover", 0, 0, default_runtime().current_frame_id(), "",
                         "device recovered");
    return true;
}

void D3D11Backend::release_window_resources()
{
#if defined(_WIN32)
    if (native_->d2d_context != nullptr) {
        native_->d2d_context->SetTarget(nullptr);
    }
    native_->d2d_target_bitmap.Reset();
    native_->render_target_view.Reset();
    native_->swap_chain.Reset();
    native_->hwnd = nullptr;
#else
    native_->native_handle = nullptr;
#endif
    state_.window_bound = false;
    state_.swap_chain_available = false;
    state_.render_target_available = false;
    diagnostics_event_ex("backend", "release_window_resources", 0, 0,
                         default_runtime().current_frame_id(), "", "window resources released");
}

bool D3D11Backend::ensure_rect_pipeline()
{
#if defined(_WIN32)
    if (native_->rect_vertex_shader != nullptr && native_->rect_pixel_shader != nullptr &&
        native_->rect_input_layout != nullptr) {
        return true;
    }

    constexpr const char* shader_source = R"(
struct VSInput {
    float2 position : POSITION;
    float4 color : COLOR0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

PSInput vs_main(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}

float4 ps_main(PSInput input) : SV_TARGET {
    return input.color;
}
)";

    Microsoft::WRL::ComPtr<ID3DBlob> vertex_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixel_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DCompile(shader_source, std::strlen(shader_source), "fiui_rect_shader", nullptr,
                            nullptr, "vs_main", "vs_4_0", 0, 0, vertex_blob.GetAddressOf(),
                            error_blob.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    hr = D3DCompile(shader_source, std::strlen(shader_source), "fiui_rect_shader", nullptr,
                    nullptr, "ps_main", "ps_4_0", 0, 0, pixel_blob.GetAddressOf(),
                    error_blob.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    hr = native_->device->CreateVertexShader(vertex_blob->GetBufferPointer(),
                                             vertex_blob->GetBufferSize(), nullptr,
                                             native_->rect_vertex_shader.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    hr = native_->device->CreatePixelShader(pixel_blob->GetBufferPointer(),
                                            pixel_blob->GetBufferSize(), nullptr,
                                            native_->rect_pixel_shader.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    constexpr D3D11_INPUT_ELEMENT_DESC input_elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = native_->device->CreateInputLayout(input_elements,
                                            static_cast<UINT>(std::size(input_elements)),
                                            vertex_blob->GetBufferPointer(),
                                            vertex_blob->GetBufferSize(),
                                            native_->rect_input_layout.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    diagnostics_event_ex("backend", "pipeline_create", 0, 0,
                         default_runtime().current_frame_id(), "", "rect pipeline");
    return true;
#else
    return true;
#endif
}

bool D3D11Backend::ensure_rounded_rect_pipeline()
{
#if defined(_WIN32)
    if (native_->rounded_rect_vertex_shader != nullptr &&
        native_->rounded_rect_pixel_shader != nullptr &&
        native_->rounded_rect_input_layout != nullptr) {
        return true;
    }
    if (!ensure_alpha_blend_state()) {
        return false;
    }

    constexpr const char* shader_source = R"(
struct VSInput {
    float2 position : POSITION;
    float2 pixel_position : TEXCOORD0;
    float4 rect : TEXCOORD1;
    float4 color : COLOR0;
    float radius : RADIUS0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 pixel_position : TEXCOORD0;
    float4 rect : TEXCOORD1;
    float4 color : COLOR0;
    float radius : RADIUS0;
};

PSInput vs_main(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 0.0f, 1.0f);
    output.pixel_position = input.pixel_position;
    output.rect = input.rect;
    output.color = input.color;
    output.radius = input.radius;
    return output;
}

float4 ps_main(PSInput input) : SV_TARGET {
    float radius = min(input.radius, min(input.rect.z - input.rect.x, input.rect.w - input.rect.y) * 0.5f);
    float2 inner_min = input.rect.xy + radius;
    float2 inner_max = input.rect.zw - radius;
    float2 closest = clamp(input.pixel_position, inner_min, inner_max);
    float distance_to_corner = length(input.pixel_position - closest) - radius;
    float alpha = radius > 0.0f ? saturate(0.5f - distance_to_corner) : 1.0f;
    return float4(input.color.rgb, input.color.a * alpha);
}
)";

    Microsoft::WRL::ComPtr<ID3DBlob> vertex_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixel_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DCompile(shader_source, std::strlen(shader_source),
                            "fiui_rounded_rect_shader", nullptr, nullptr, "vs_main",
                            "vs_4_0", 0, 0, vertex_blob.GetAddressOf(), error_blob.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("rounded_rect_pipeline_failed", state_.last_failure,
                             state_.last_hresult);
        return false;
    }

    hr = D3DCompile(shader_source, std::strlen(shader_source), "fiui_rounded_rect_shader",
                    nullptr, nullptr, "ps_main", "ps_4_0", 0, 0, pixel_blob.GetAddressOf(),
                    error_blob.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("rounded_rect_pipeline_failed", state_.last_failure,
                             state_.last_hresult);
        return false;
    }

    hr = native_->device->CreateVertexShader(vertex_blob->GetBufferPointer(),
                                             vertex_blob->GetBufferSize(), nullptr,
                                             native_->rounded_rect_vertex_shader.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("rounded_rect_pipeline_failed", state_.last_failure,
                             state_.last_hresult);
        return false;
    }

    hr = native_->device->CreatePixelShader(pixel_blob->GetBufferPointer(),
                                            pixel_blob->GetBufferSize(), nullptr,
                                            native_->rounded_rect_pixel_shader.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("rounded_rect_pipeline_failed", state_.last_failure,
                             state_.last_hresult);
        return false;
    }

    constexpr D3D11_INPUT_ELEMENT_DESC input_elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"RADIUS", 0, DXGI_FORMAT_R32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = native_->device->CreateInputLayout(input_elements,
                                            static_cast<UINT>(std::size(input_elements)),
                                            vertex_blob->GetBufferPointer(),
                                            vertex_blob->GetBufferSize(),
                                            native_->rounded_rect_input_layout.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("rounded_rect_pipeline_failed", state_.last_failure,
                             state_.last_hresult);
        return false;
    }

    state_.rounded_rect_pipeline_create_count += 1;
    diagnostics_event_ex("backend", "rounded_rect_pipeline_create", 0, 0,
                         default_runtime().current_frame_id(), "", "sdf rounded rect pipeline");
    return true;
#else
    return true;
#endif
}

bool D3D11Backend::ensure_alpha_blend_state()
{
#if defined(_WIN32)
    if (native_->image_blend_state != nullptr) {
        return true;
    }

    D3D11_BLEND_DESC blend_desc{};
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    const HRESULT hr = native_->device->CreateBlendState(
        &blend_desc, native_->image_blend_state.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("alpha_blend_failed", state_.last_failure, state_.last_hresult);
        return false;
    }
    diagnostics_event_ex("backend", "alpha_blend_create", 0, 0,
                         default_runtime().current_frame_id(), "",
                         "source alpha blend state");
    return true;
#else
    return true;
#endif
}

bool D3D11Backend::ensure_text_pipeline()
{
#if defined(_WIN32)
    if (native_->d2d_context != nullptr && native_->dwrite_factory != nullptr) {
        return ensure_text_render_target();
    }

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                   __uuidof(ID2D1Factory1), nullptr,
                                   reinterpret_cast<void**>(native_->d2d_factory.GetAddressOf()));
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("text_pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(
                                 native_->dwrite_factory.GetAddressOf()));
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("text_pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    hr = native_->device.As(&dxgi_device);
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("text_pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    hr = native_->d2d_factory->CreateDevice(dxgi_device.Get(),
                                            native_->d2d_device.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("text_pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    hr = native_->d2d_device->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE, native_->d2d_context.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("text_pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    state_.text_pipeline_create_count += 1;
    diagnostics_event_ex("backend", "text_pipeline_create", 0, 0,
                         default_runtime().current_frame_id(), "",
                         "directwrite d2d target pipeline");
    return ensure_text_render_target();
#else
    return true;
#endif
}

bool D3D11Backend::ensure_text_render_target()
{
#if defined(_WIN32)
    if (native_->d2d_target_bitmap != nullptr) {
        return true;
    }
    if (native_->swap_chain == nullptr || native_->d2d_context == nullptr) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    HRESULT hr = native_->swap_chain->GetBuffer(
        0, __uuidof(IDXGISurface), reinterpret_cast<void**>(surface.GetAddressOf()));
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::RenderTargetUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("text_target_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    D2D1_BITMAP_PROPERTIES1 properties{};
    properties.pixelFormat = D2D1_PIXEL_FORMAT{d2d_target_format, D2D1_ALPHA_MODE_IGNORE};
    const float dpi =
        static_cast<float>(std::max<std::uint32_t>(1, default_runtime().platform_system().state().dpi));
    properties.dpiX = dpi;
    properties.dpiY = dpi;
    properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    hr = native_->d2d_context->CreateBitmapFromDxgiSurface(
        surface.Get(), &properties, native_->d2d_target_bitmap.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::RenderTargetUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("text_target_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    native_->d2d_context->SetTarget(native_->d2d_target_bitmap.Get());
    native_->d2d_context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
    native_->d2d_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    diagnostics_event_ex("backend", "text_target_create", 0, 0,
                         default_runtime().current_frame_id(), "",
                         "swap chain text target");
    return true;
#else
    return true;
#endif
}

bool D3D11Backend::ensure_image_pipeline()
{
#if defined(_WIN32)
    if (native_->image_vertex_shader != nullptr && native_->image_pixel_shader != nullptr &&
        native_->image_input_layout != nullptr && native_->image_sampler_state != nullptr &&
        native_->image_blend_state != nullptr) {
        return true;
    }

    constexpr const char* shader_source = R"(
struct VSInput {
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float opacity : OPACITY0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float opacity : OPACITY0;
};

Texture2D image_texture : register(t0);
SamplerState image_sampler : register(s0);

PSInput vs_main(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 0.0f, 1.0f);
    output.uv = input.uv;
    output.opacity = input.opacity;
    return output;
}

float4 ps_main(PSInput input) : SV_TARGET {
    float4 sampled = image_texture.Sample(image_sampler, input.uv);
    sampled.a *= input.opacity;
    return sampled;
}
)";

    Microsoft::WRL::ComPtr<ID3DBlob> vertex_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixel_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DCompile(shader_source, std::strlen(shader_source), "fiui_image_shader",
                            nullptr, nullptr, "vs_main", "vs_4_0", 0, 0,
                            vertex_blob.GetAddressOf(), error_blob.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    hr = D3DCompile(shader_source, std::strlen(shader_source), "fiui_image_shader", nullptr,
                    nullptr, "ps_main", "ps_4_0", 0, 0, pixel_blob.GetAddressOf(),
                    error_blob.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    hr = native_->device->CreateVertexShader(vertex_blob->GetBufferPointer(),
                                             vertex_blob->GetBufferSize(), nullptr,
                                             native_->image_vertex_shader.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    hr = native_->device->CreatePixelShader(pixel_blob->GetBufferPointer(),
                                            pixel_blob->GetBufferSize(), nullptr,
                                            native_->image_pixel_shader.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    constexpr D3D11_INPUT_ELEMENT_DESC input_elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"OPACITY", 0, DXGI_FORMAT_R32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = native_->device->CreateInputLayout(input_elements,
                                            static_cast<UINT>(std::size(input_elements)),
                                            vertex_blob->GetBufferPointer(),
                                            vertex_blob->GetBufferSize(),
                                            native_->image_input_layout.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler_desc.MinLOD = 0;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = native_->device->CreateSamplerState(&sampler_desc,
                                             native_->image_sampler_state.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    if (!ensure_alpha_blend_state()) {
        return false;
    }

    state_.image_pipeline_create_count += 1;
    diagnostics_event_ex("backend", "image_pipeline_create", 0, 0,
                         default_runtime().current_frame_id(), "", "image pipeline");
    return true;
#else
    return true;
#endif
}

bool D3D11Backend::ensure_clip_pipeline()
{
#if defined(_WIN32)
    if (native_->default_rasterizer_state != nullptr &&
        native_->scissor_rasterizer_state != nullptr) {
        return true;
    }

    D3D11_RASTERIZER_DESC desc{};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_NONE;
    desc.DepthClipEnable = TRUE;
    desc.ScissorEnable = FALSE;
    HRESULT hr = native_->device->CreateRasterizerState(
        &desc, native_->default_rasterizer_state.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("clip_pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    desc.ScissorEnable = TRUE;
    hr = native_->device->CreateRasterizerState(
        &desc, native_->scissor_rasterizer_state.GetAddressOf());
    if (FAILED(hr)) {
        set_failure(state_, BackendFailureReason::PipelineUnavailable,
                    static_cast<std::int32_t>(hr));
        emit_backend_failure("clip_pipeline_failed", state_.last_failure, state_.last_hresult);
        return false;
    }

    state_.scissor_state_create_count += 1;
    diagnostics_event_ex("backend", "clip_pipeline_create", 0, 0,
                         default_runtime().current_frame_id(), "",
                         "d3d scissor rasterizer states");
    return true;
#else
    return true;
#endif
}

bool D3D11Backend::ensure_texture(const DisplayResource& resource)
{
    if (resource.id == 0 || resource.kind != ResourceKind::Image) {
        return true;
    }
    if (!initialize()) {
        state_.texture_upload_failure_count += 1;
        return false;
    }

#if defined(_WIN32)
    const auto existing = native_->textures.find(resource.id);
    if (existing != native_->textures.end() && existing->second.key == resource.key &&
        existing->second.texture != nullptr && existing->second.shader_resource_view != nullptr) {
        TextureMetadata metadata = resource.texture_metadata;
        metadata.valid = true;
        metadata.uploaded = true;
        metadata.fallback = existing->second.fallback;
        metadata.width = existing->second.width;
        metadata.height = existing->second.height;
        metadata.format = texture_format_name(texture_format);
        metadata.upload_generation = existing->second.generation;
        metadata.upload_count = std::max<std::uint32_t>(metadata.upload_count, 1);
        default_runtime().resource_system().update_texture_metadata(resource.id, metadata);
        diagnostics_event_ex("backend", "texture_cache_hit", resource.owner_object_id, 0,
                             default_runtime().current_frame_id(), resource.owner_path.c_str(),
                             resource.key.c_str());
        return true;
    }

    if (native_->device == nullptr) {
        state_.texture_upload_failure_count += 1;
        diagnostics_event_ex("backend", "texture_failed", resource.owner_object_id, 0,
                             default_runtime().current_frame_id(), resource.owner_path.c_str(),
                             "device unavailable");
        return false;
    }

    std::vector<std::uint8_t> pixels;
    std::uint32_t width = resource.image_metadata.width > 0 ? resource.image_metadata.width : 2;
    std::uint32_t height = resource.image_metadata.height > 0 ? resource.image_metadata.height : 2;
    bool fallback = resource.image_metadata.fallback || resource.key.empty();
    std::string failure;

    if (!fallback) {
        HRESULT hr = S_OK;
        if (native_->wic_factory == nullptr) {
            hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (SUCCEEDED(hr)) {
                native_->co_initialized = true;
            } else if (hr != RPC_E_CHANGED_MODE) {
                failure = "com initialization failed";
                fallback = true;
            }
        }
        if (!fallback && native_->wic_factory == nullptr) {
            hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(native_->wic_factory.GetAddressOf()));
            if (FAILED(hr)) {
                failure = "wic factory failed";
                fallback = true;
            }
        }
        if (!fallback) {
            const std::wstring path = resolve_image_path(utf8_to_wide_path(resource.key));
            Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
            hr = path.empty()
                     ? E_INVALIDARG
                     : native_->wic_factory->CreateDecoderFromFilename(
                           path.c_str(), nullptr, GENERIC_READ,
                           WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
            if (SUCCEEDED(hr)) {
                Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
                hr = decoder->GetFrame(0, frame.GetAddressOf());
                if (SUCCEEDED(hr)) {
                    UINT frame_width = 0;
                    UINT frame_height = 0;
                    frame->GetSize(&frame_width, &frame_height);
                    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
                    hr = native_->wic_factory->CreateFormatConverter(converter.GetAddressOf());
                    if (SUCCEEDED(hr)) {
                        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                                   WICBitmapPaletteTypeCustom);
                    }
                    if (SUCCEEDED(hr) && frame_width > 0 && frame_height > 0) {
                        width = frame_width;
                        height = frame_height;
                        pixels.resize(static_cast<std::size_t>(width) * height * 4);
                        hr = converter->CopyPixels(nullptr, width * 4,
                                                   static_cast<UINT>(pixels.size()),
                                                   pixels.data());
                    }
                }
            }
            if (FAILED(hr) || pixels.empty()) {
                failure = "wic decode failed";
                fallback = true;
            }
        }
    }

    if (fallback) {
        width = std::max<std::uint32_t>(2, width);
        height = std::max<std::uint32_t>(2, height);
        pixels.resize(static_cast<std::size_t>(width) * height * 4);
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                const bool light = ((x / 8) + (y / 8)) % 2 == 0;
                const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4;
                pixels[offset + 0] = light ? 0xCC : 0x66;
                pixels[offset + 1] = light ? 0xCC : 0x66;
                pixels[offset + 2] = light ? 0xCC : 0x66;
                pixels[offset + 3] = 0xFF;
            }
        }
        state_.texture_fallback_count += 1;
        diagnostics_event_ex("backend", "texture_fallback", resource.owner_object_id, 0,
                             default_runtime().current_frame_id(), resource.owner_path.c_str(),
                             resource.key.c_str());
    }

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = texture_format;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = pixels.data();
    data.SysMemPitch = width * 4;

    NativeState::BackendTexture texture;
    texture.id = resource.id;
    texture.key = resource.key;
    texture.width = width;
    texture.height = height;
    texture.fallback = fallback;
    texture.generation = ++native_->texture_generation;

    HRESULT hr = native_->device->CreateTexture2D(&texture_desc, &data, texture.texture.GetAddressOf());
    if (SUCCEEDED(hr)) {
        hr = native_->device->CreateShaderResourceView(texture.texture.Get(), nullptr,
                                                       texture.shader_resource_view.GetAddressOf());
    }
    state_.last_hresult = static_cast<std::int32_t>(hr);
    if (FAILED(hr)) {
        state_.texture_upload_failure_count += 1;
        TextureMetadata metadata;
        metadata.valid = true;
        metadata.uploaded = false;
        metadata.fallback = fallback;
        metadata.width = width;
        metadata.height = height;
        metadata.format = texture_format_name(texture_format);
        metadata.upload_generation = native_->texture_generation;
        metadata.last_failure = "d3d texture creation failed";
        default_runtime().resource_system().update_texture_metadata(resource.id, metadata);
        diagnostics_event_ex("backend", "texture_failed", resource.owner_object_id, 0,
                             default_runtime().current_frame_id(), resource.owner_path.c_str(),
                             metadata.last_failure.c_str());
        return false;
    }

    const bool replacing = native_->textures.find(resource.id) != native_->textures.end();
    if (replacing) {
        state_.texture_release_count += 1;
    }
    native_->textures[resource.id] = std::move(texture);
    state_.texture_create_count += 1;
    state_.texture_upload_count += 1;
    state_.live_texture_count = static_cast<std::uint32_t>(native_->textures.size());

    TextureMetadata metadata = resource.texture_metadata;
    metadata.valid = true;
    metadata.uploaded = true;
    metadata.fallback = fallback;
    metadata.width = width;
    metadata.height = height;
    metadata.format = texture_format_name(texture_format);
    metadata.upload_generation = native_->texture_generation;
    metadata.upload_count += 1;
    metadata.last_failure = failure;
    default_runtime().resource_system().update_texture_metadata(resource.id, metadata);

    diagnostics_event_ex("backend", "texture_create", resource.owner_object_id, 0,
                         default_runtime().current_frame_id(), resource.owner_path.c_str(),
                         resource.key.c_str());
    diagnostics_event_ex("backend", "texture_upload", resource.owner_object_id, 0,
                         default_runtime().current_frame_id(), resource.owner_path.c_str(),
                         metadata.format.c_str());
    return true;
#else
    TextureMetadata metadata = resource.texture_metadata;
    metadata.valid = true;
    metadata.uploaded = true;
    metadata.fallback = true;
    metadata.width = resource.image_metadata.width > 0 ? resource.image_metadata.width : 2;
    metadata.height = resource.image_metadata.height > 0 ? resource.image_metadata.height : 2;
    metadata.format = "fallback";
    metadata.upload_generation = ++native_->texture_generation;
    metadata.upload_count += 1;
    native_->textures[resource.id] = metadata;
    state_.texture_create_count += 1;
    state_.texture_upload_count += 1;
    state_.texture_fallback_count += 1;
    state_.live_texture_count = static_cast<std::uint32_t>(native_->textures.size());
    default_runtime().resource_system().update_texture_metadata(resource.id, metadata);
    diagnostics_event_ex("backend", "texture_upload", resource.owner_object_id, 0,
                         default_runtime().current_frame_id(), resource.owner_path.c_str(),
                         "fallback");
    return true;
#endif
}

void D3D11Backend::release_textures()
{
#if defined(_WIN32)
    const std::uint32_t released = static_cast<std::uint32_t>(native_->textures.size());
    native_->textures.clear();
#else
    const std::uint32_t released = static_cast<std::uint32_t>(native_->textures.size());
    native_->textures.clear();
#endif
    state_.texture_release_count += released;
    state_.live_texture_count = 0;
    if (released > 0) {
        diagnostics_event_ex("backend", "texture_release", 0, 0,
                             default_runtime().current_frame_id(), "", "all backend textures");
    }
}

BackendFailureReason D3D11Backend::execute_draw_commands(const DisplayList& display_list,
                                                         BackendFrameResult& result)
{
#if defined(_WIN32)
    for (const DisplayCommand& command : display_list.commands) {
        if (command.kind == DisplayCommandKind::Image) {
            (void)ensure_texture(command.resource);
        }
    }

    if (native_->context == nullptr || native_->render_target_view == nullptr) {
        state_.headless_submit_count += 1;
        diagnostics_event_ex("backend", "submit_headless", 0, 0,
                             default_runtime().current_frame_id(), "",
                             "no active render target");
        return BackendFailureReason::None;
    }

    if (!ensure_rect_pipeline()) {
        state_.draw_failure_count += 1;
        return BackendFailureReason::PipelineUnavailable;
    }
    if (!ensure_alpha_blend_state()) {
        state_.draw_failure_count += 1;
        return BackendFailureReason::PipelineUnavailable;
    }

    std::array<float, 4> clear_color{0.0f, 0.0f, 0.0f, 1.0f};
    for (const DisplayCommand& command : display_list.commands) {
        if (command.kind == DisplayCommandKind::Rect && command.widget_kind == WidgetKind::Window) {
            clear_color = {color_channel(command.style.fill.r),
                           color_channel(command.style.fill.g),
                           color_channel(command.style.fill.b),
                           color_channel(command.style.fill.a)};
            break;
        }
    }

    native_->context->OMSetRenderTargets(1, native_->render_target_view.GetAddressOf(), nullptr);
    native_->context->ClearRenderTargetView(native_->render_target_view.Get(), clear_color.data());
    state_.render_target_clear_count += 1;

    const float target_width = static_cast<float>(state_.bound_width);
    const float target_height = static_cast<float>(state_.bound_height);
    const auto popup_start_it =
        std::find_if(display_list.commands.begin(), display_list.commands.end(),
                     [](const DisplayCommand& command) {
                         constexpr const char* popup_suffix = "/popup";
                         constexpr const char* backdrop_suffix = "/backdrop";
                         constexpr const char* panel_suffix = "/panel";
                         const auto has_suffix = [](const std::string& value,
                                                    const char* suffix) {
                             const std::size_t suffix_size = std::strlen(suffix);
                             return value.size() >= suffix_size &&
                                    value.compare(value.size() - suffix_size, suffix_size,
                                                  suffix) == 0;
                         };
                         return has_suffix(command.path, popup_suffix) ||
                                has_suffix(command.path, backdrop_suffix) ||
                                has_suffix(command.path, panel_suffix);
                     });
    const std::size_t popup_start =
        popup_start_it == display_list.commands.end()
            ? display_list.commands.size()
            : static_cast<std::size_t>(popup_start_it - display_list.commands.begin());
    std::vector<ClipSnapshot> command_clips(display_list.commands.size());
    std::vector<CommandEffectSnapshot> command_effects(display_list.commands.size());
    std::vector<ClipSnapshot> clip_stack;
    std::vector<CommandEffectSnapshot> effect_stack;
    effect_stack.push_back(CommandEffectSnapshot{});
    std::uint32_t clip_commands = 0;
    std::uint32_t clip_end_commands = 0;
    std::uint32_t rounded_clip_commands = 0;
    std::uint32_t rounded_clip_end_commands = 0;
    std::uint32_t rounded_clip_fallback_commands = 0;
    std::uint32_t opacity_commands = 0;
    std::uint32_t opacity_end_commands = 0;
    std::uint32_t opacity_apply_commands = 0;
    std::uint32_t transform_commands = 0;
    std::uint32_t transform_end_commands = 0;
    std::uint32_t transform_apply_commands = 0;
    std::uint32_t max_clip_depth = 0;
    for (std::size_t index = 0; index < display_list.commands.size(); ++index) {
        const DisplayCommand& command = display_list.commands[index];
        const CommandEffectSnapshot current_effect = effect_stack.back();
        command_effects[index] = current_effect;
        if (!clip_stack.empty()) {
            command_clips[index] = clip_stack.back();
        }
        if (command.kind == DisplayCommandKind::Clip ||
            command.kind == DisplayCommandKind::RoundedClip) {
            const bool rounded_clip = command.kind == DisplayCommandKind::RoundedClip;
            ++clip_commands;
            ++result.clip_command_count;
            if (rounded_clip) {
                ++rounded_clip_commands;
                ++rounded_clip_fallback_commands;
                ++result.rounded_clip_command_count;
                ++result.rounded_clip_fallback_count;
            }
            const Rect clip_bounds = apply_effect(command.bounds, current_effect);
            const Rect next_clip =
                clip_stack.empty() ? clip_bounds
                                   : intersect_rect(clip_stack.back().rect, clip_bounds);
            clip_stack.push_back(ClipSnapshot{true, next_clip, rounded_clip,
                                              rounded_clip ? command.style.radius : 0.0f});
            max_clip_depth = std::max<std::uint32_t>(
                max_clip_depth, static_cast<std::uint32_t>(clip_stack.size()));
            diagnostics_event_ex("backend", "clip_command", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 rounded_clip ? "rounded clip fallback to rectangular clip"
                                              : "rectangular clip");
            if (rounded_clip) {
                diagnostics_event_ex("backend", "rounded_clip_command", command.object_id, 0,
                                     default_runtime().current_frame_id(), command.path.c_str(),
                                     "rounded clip command");
            }
            diagnostics_event_ex("backend", "clip_push", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 rounded_clip ? "rounded clip fallback to rectangular scissor"
                                              : "rectangular clip");
            if (rounded_clip) {
                diagnostics_event_ex("backend", "rounded_clip_fallback", command.object_id, 0,
                                     default_runtime().current_frame_id(), command.path.c_str(),
                                     "rectangular scissor fallback");
            }
            continue;
        }
        if (command.kind == DisplayCommandKind::ClipEnd ||
            command.kind == DisplayCommandKind::RoundedClipEnd) {
            const bool rounded_clip_end = command.kind == DisplayCommandKind::RoundedClipEnd;
            ++clip_end_commands;
            ++result.clip_end_command_count;
            if (rounded_clip_end) {
                ++rounded_clip_end_commands;
                ++result.rounded_clip_end_command_count;
            }
            if (!clip_stack.empty()) {
                const bool popped_rounded = clip_stack.back().rounded;
                clip_stack.pop_back();
                diagnostics_event_ex("backend", "clip_pop", command.object_id, 0,
                                     default_runtime().current_frame_id(), command.path.c_str(),
                                     popped_rounded ? "rounded clip fallback ended"
                                                    : "rectangular clip");
                if (popped_rounded) {
                    diagnostics_event_ex("backend", "rounded_clip_pop", command.object_id, 0,
                                         default_runtime().current_frame_id(),
                                         command.path.c_str(), "rounded clip fallback ended");
                }
            } else {
                diagnostics_event_ex("backend", "clip_pop_underflow", command.object_id, 0,
                                     default_runtime().current_frame_id(), command.path.c_str(),
                                     "clip stack underflow");
            }
            continue;
        }
        if (command.kind == DisplayCommandKind::Opacity) {
            ++opacity_commands;
            ++opacity_apply_commands;
            ++result.opacity_command_count;
            ++result.opacity_apply_count;
            CommandEffectSnapshot next_effect = current_effect;
            next_effect.opacity *= std::max(0.0f, std::min(1.0f, command.opacity));
            effect_stack.push_back(next_effect);
            diagnostics_event_ex("backend", "opacity_push", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 "opacity scaffold applied");
            continue;
        }
        if (command.kind == DisplayCommandKind::OpacityEnd) {
            ++opacity_end_commands;
            ++result.opacity_end_command_count;
            if (effect_stack.size() > 1) {
                effect_stack.pop_back();
            }
            diagnostics_event_ex("backend", "opacity_pop", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 "opacity scaffold ended");
            continue;
        }
        if (command.kind == DisplayCommandKind::Transform) {
            ++transform_commands;
            ++transform_apply_commands;
            ++result.transform_command_count;
            ++result.transform_apply_count;
            CommandEffectSnapshot next_effect = current_effect;
            next_effect.translate_x =
                current_effect.translate_x +
                command.transform_translate_x * current_effect.scale_x;
            next_effect.translate_y =
                current_effect.translate_y +
                command.transform_translate_y * current_effect.scale_y;
            next_effect.scale_x *= command.transform_scale_x;
            next_effect.scale_y *= command.transform_scale_y;
            effect_stack.push_back(next_effect);
            diagnostics_event_ex("backend", "transform_push", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 "transform scaffold applied");
            continue;
        }
        if (command.kind == DisplayCommandKind::TransformEnd) {
            ++transform_end_commands;
            ++result.transform_end_command_count;
            if (effect_stack.size() > 1) {
                effect_stack.pop_back();
            }
            diagnostics_event_ex("backend", "transform_pop", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 "transform scaffold ended");
            continue;
        }
    }
    state_.clip_command_count += clip_commands;
    state_.clip_end_command_count += clip_end_commands;
    state_.rounded_clip_command_count += rounded_clip_commands;
    state_.rounded_clip_end_command_count += rounded_clip_end_commands;
    state_.rounded_clip_fallback_count += rounded_clip_fallback_commands;
    state_.opacity_command_count += opacity_commands;
    state_.opacity_end_command_count += opacity_end_commands;
    state_.opacity_apply_count += opacity_apply_commands;
    state_.transform_command_count += transform_commands;
    state_.transform_end_command_count += transform_end_commands;
    state_.transform_apply_count += transform_apply_commands;
    state_.clip_stack_depth = max_clip_depth;

    auto apply_d3d_clip = [&](const ClipSnapshot& snapshot) -> bool {
        if (!snapshot.active || is_empty_rect(snapshot.rect)) {
            native_->context->RSSetState(native_->default_rasterizer_state.Get());
            return true;
        }
        if (!ensure_clip_pipeline()) {
            return false;
        }
        const D3D11_RECT scissor = d3d_scissor_rect(snapshot.rect, target_width, target_height);
        native_->context->RSSetState(native_->scissor_rasterizer_state.Get());
        native_->context->RSSetScissorRects(1, &scissor);
        ++result.clip_apply_count;
        ++state_.clip_apply_count;
        if (snapshot.rounded) {
            ++result.rounded_clip_apply_count;
            ++state_.rounded_clip_apply_count;
            diagnostics_event_ex("backend", "rounded_clip_apply", 0, 0,
                                 default_runtime().current_frame_id(), "",
                                 "d3d scissor fallback for rounded clip");
        }
        diagnostics_event_ex("backend", "clip_apply", 0, 0,
                             default_runtime().current_frame_id(), "",
                             "d3d scissor rect");
        return true;
    };

    auto draw_rounded_command = [&](const DisplayCommand& rounded_command,
                                    const char* action,
                                    const char* detail,
                                    bool count_as_rect) -> BackendFailureReason {
        if (!ensure_rounded_rect_pipeline()) {
            state_.draw_failure_count += 1;
            return BackendFailureReason::PipelineUnavailable;
        }

        std::vector<RoundedRectVertex> rounded_vertices;
        append_rounded_rect_vertices(rounded_vertices, rounded_command, target_width,
                                     target_height);
        if (rounded_vertices.empty()) {
            return BackendFailureReason::None;
        }

        D3D11_BUFFER_DESC buffer_desc{};
        buffer_desc.ByteWidth =
            static_cast<UINT>(rounded_vertices.size() * sizeof(RoundedRectVertex));
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA buffer_data{};
        buffer_data.pSysMem = rounded_vertices.data();

        native_->rounded_rect_vertex_buffer.Reset();
        const HRESULT hr = native_->device->CreateBuffer(
            &buffer_desc, &buffer_data, native_->rounded_rect_vertex_buffer.GetAddressOf());
        if (FAILED(hr)) {
            state_.draw_failure_count += 1;
            set_failure(state_, BackendFailureReason::DrawFailed, static_cast<std::int32_t>(hr));
            emit_backend_failure("draw_failed", state_.last_failure, state_.last_hresult);
            return BackendFailureReason::DrawFailed;
        }

        D3D11_VIEWPORT viewport{};
        viewport.Width = target_width;
        viewport.Height = target_height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        native_->context->RSSetViewports(1, &viewport);

        constexpr UINT stride = sizeof(RoundedRectVertex);
        constexpr UINT offset = 0;
        ID3D11Buffer* vertex_buffers[] = {native_->rounded_rect_vertex_buffer.Get()};
        const float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        native_->context->IASetVertexBuffers(0, 1, vertex_buffers, &stride, &offset);
        native_->context->IASetInputLayout(native_->rounded_rect_input_layout.Get());
        native_->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        native_->context->VSSetShader(native_->rounded_rect_vertex_shader.Get(), nullptr, 0);
        native_->context->PSSetShader(native_->rounded_rect_pixel_shader.Get(), nullptr, 0);
        native_->context->OMSetBlendState(native_->image_blend_state.Get(), blend_factor,
                                          0xffffffffu);
        native_->context->Draw(static_cast<UINT>(rounded_vertices.size()), 0);
        native_->context->OMSetBlendState(nullptr, blend_factor, 0xffffffffu);

        if (count_as_rect) {
            ++result.rect_draw_count;
            ++result.rounded_rect_draw_count;
            ++state_.rect_draw_count;
            ++state_.rounded_rect_draw_count;
        }
        diagnostics_event_ex("backend", action, rounded_command.object_id, 0,
                             default_runtime().current_frame_id(),
                             rounded_command.path.c_str(), detail);
        return BackendFailureReason::None;
    };

    std::uint32_t drawn_images = 0;
    std::uint32_t drawn_texts = 0;
    bool text_batch_active = false;
    auto end_text_batch = [&]() -> BackendFailureReason {
        if (!text_batch_active || native_->d2d_context == nullptr) {
            text_batch_active = false;
            return BackendFailureReason::None;
        }
        const HRESULT hr = native_->d2d_context->EndDraw();
        text_batch_active = false;
        if (FAILED(hr)) {
            state_.draw_failure_count += 1;
            state_.text_draw_failure_count += 1;
            set_failure(state_, BackendFailureReason::DrawFailed, static_cast<std::int32_t>(hr));
            emit_backend_failure("draw_text_failed", state_.last_failure, state_.last_hresult);
            return BackendFailureReason::DrawFailed;
        }
        return BackendFailureReason::None;
    };
    auto begin_text_batch = [&]() -> BackendFailureReason {
        if (text_batch_active) {
            return BackendFailureReason::None;
        }
        if (!ensure_text_pipeline()) {
            state_.draw_failure_count += 1;
            state_.text_draw_failure_count += 1;
            return BackendFailureReason::PipelineUnavailable;
        }
        native_->context->Flush();
        native_->d2d_context->BeginDraw();
        text_batch_active = true;
        return BackendFailureReason::None;
    };
    auto draw_text_command = [&](const DisplayCommand& command,
                                 const DisplayCommand& effective_command,
                                 const ClipSnapshot& text_clip,
                                 const char* action,
                                 const char* skip_action) -> BackendFailureReason {
        BackendFailureReason begin_failure = begin_text_batch();
        if (begin_failure != BackendFailureReason::None) {
            return begin_failure;
        }

        Microsoft::WRL::ComPtr<IDWriteTextFormat> text_format;
        const float font_size =
            effective_command.style.font_size > 0.0f ? effective_command.style.font_size : 14.0f;
        HRESULT hr = native_->dwrite_factory->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, font_size, L"", text_format.GetAddressOf());
        if (FAILED(hr)) {
            state_.text_draw_failure_count += 1;
            diagnostics_event_ex("backend", skip_action, command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 "text format unavailable");
            return BackendFailureReason::None;
        }

        text_format->SetWordWrapping(std::strcmp(effective_command.style.word_wrap, "word") == 0
                                          ? DWRITE_WORD_WRAPPING_WRAP
                                          : DWRITE_WORD_WRAPPING_NO_WRAP);
        text_format->SetTextAlignment(dwrite_text_alignment(effective_command.style.text_align));
        text_format->SetParagraphAlignment(
            dwrite_paragraph_alignment(effective_command.style.paragraph_align));
        if (std::strcmp(effective_command.style.overflow, "ellipsis") == 0) {
            DWRITE_TRIMMING trimming{};
            trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
            Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;
            hr = native_->dwrite_factory->CreateEllipsisTrimmingSign(
                text_format.Get(), ellipsis.GetAddressOf());
            if (SUCCEEDED(hr)) {
                text_format->SetTrimming(&trimming, ellipsis.Get());
            }
        }

        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
        hr = native_->d2d_context->CreateSolidColorBrush(
            d2d_color(effective_command.style.text), brush.GetAddressOf());
        if (FAILED(hr)) {
            state_.text_draw_failure_count += 1;
            diagnostics_event_ex("backend", skip_action, command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 "text brush unavailable");
            return BackendFailureReason::None;
        }

        const std::wstring text = utf8_to_wide_path(command.text);
        if (text.empty()) {
            return BackendFailureReason::None;
        }
        bool d2d_clip_pushed = false;
        if (text_clip.active && !is_empty_rect(text_clip.rect)) {
            native_->d2d_context->PushAxisAlignedClip(d2d_clip_rect(text_clip.rect),
                                                      D2D1_ANTIALIAS_MODE_ALIASED);
            d2d_clip_pushed = true;
            ++result.clip_apply_count;
            ++state_.clip_apply_count;
            diagnostics_event_ex("backend", "clip_apply_text", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 "d2d axis aligned clip");
        }
        const D2D1_RECT_F rect{effective_command.bounds.x,
                               effective_command.bounds.y,
                               effective_command.bounds.x + effective_command.bounds.width,
                               effective_command.bounds.y + effective_command.bounds.height};
        native_->d2d_context->DrawText(text.c_str(), static_cast<UINT32>(text.size()),
                                       text_format.Get(), rect, brush.Get(),
                                       D2D1_DRAW_TEXT_OPTIONS_CLIP,
                                       DWRITE_MEASURING_MODE_NATURAL);
        ++drawn_texts;
        ++result.text_draw_count;
        diagnostics_event_ex("backend", action, command.object_id, 0,
                             default_runtime().current_frame_id(), command.path.c_str(),
                             command.text.c_str());
        if (d2d_clip_pushed) {
            native_->d2d_context->PopAxisAlignedClip();
        }
        return BackendFailureReason::None;
    };
    auto draw_rect_command = [&](std::size_t command_index,
                                 const DisplayCommand& command,
                                 const DisplayCommand& effective_command,
                                 bool popup_command) -> BackendFailureReason {
        if (!apply_d3d_clip(command_clips[command_index])) {
            state_.draw_failure_count += 1;
            return BackendFailureReason::PipelineUnavailable;
        }
        if (command.kind == DisplayCommandKind::Shadow) {
            const std::uint32_t layer_count = command.shadow_blur > 0.0f ? 3u : 1u;
            for (std::uint32_t layer = 0; layer < layer_count; ++layer) {
                const DisplayCommand shadow =
                    shadow_fallback_command(effective_command, layer, layer_count);
                const BackendFailureReason shadow_failure = draw_rounded_command(
                    shadow,
                    popup_command ? "draw_popup_shadow" : "draw_shadow",
                    popup_command ? "popup shadow fallback layer executed"
                                  : "shadow fallback layer executed",
                    false);
                if (shadow_failure != BackendFailureReason::None) {
                    return shadow_failure;
                }
            }
            ++result.shadow_draw_count;
            ++result.shadow_fallback_count;
            ++state_.shadow_draw_count;
            ++state_.shadow_fallback_count;
            diagnostics_event_ex("backend",
                                 popup_command ? "shadow_fallback" : "shadow_fallback",
                                 command.object_id, 0, default_runtime().current_frame_id(),
                                 command.path.c_str(),
                                 popup_command ? "rounded popup shadow fallback"
                                               : "rounded shadow fallback");
            return BackendFailureReason::None;
        }
        if (command.style.radius > 0.0f) {
            const BackendFailureReason rounded_failure = draw_rounded_command(
                effective_command,
                popup_command ? "draw_popup_rounded_rect" : "draw_rounded_rect",
                popup_command ? "popup rounded rect display command executed"
                              : "rounded rect display command executed",
                true);
            if (rounded_failure != BackendFailureReason::None) {
                return rounded_failure;
            }
            return BackendFailureReason::None;
        }

        std::vector<RectVertex> vertices;
        append_rect_vertices(vertices, effective_command, target_width, target_height);
        if (vertices.empty()) {
            return BackendFailureReason::None;
        }

        D3D11_BUFFER_DESC buffer_desc{};
        buffer_desc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(RectVertex));
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA buffer_data{};
        buffer_data.pSysMem = vertices.data();

        native_->rect_vertex_buffer.Reset();
        HRESULT hr = native_->device->CreateBuffer(&buffer_desc, &buffer_data,
                                                   native_->rect_vertex_buffer.GetAddressOf());
        if (FAILED(hr)) {
            state_.draw_failure_count += 1;
            set_failure(state_, BackendFailureReason::DrawFailed, static_cast<std::int32_t>(hr));
            emit_backend_failure("draw_failed", state_.last_failure, state_.last_hresult);
            return BackendFailureReason::DrawFailed;
        }

        D3D11_VIEWPORT viewport{};
        viewport.Width = target_width;
        viewport.Height = target_height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        native_->context->RSSetViewports(1, &viewport);

        constexpr UINT stride = sizeof(RectVertex);
        constexpr UINT offset = 0;
        ID3D11Buffer* vertex_buffers[] = {native_->rect_vertex_buffer.Get()};
        native_->context->IASetVertexBuffers(0, 1, vertex_buffers, &stride, &offset);
        native_->context->IASetInputLayout(native_->rect_input_layout.Get());
        native_->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        native_->context->VSSetShader(native_->rect_vertex_shader.Get(), nullptr, 0);
        native_->context->PSSetShader(native_->rect_pixel_shader.Get(), nullptr, 0);
        const float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        native_->context->OMSetBlendState(native_->image_blend_state.Get(), blend_factor,
                                          0xffffffffu);
        native_->context->Draw(static_cast<UINT>(vertices.size()), 0);
        native_->context->OMSetBlendState(nullptr, blend_factor, 0xffffffffu);
        ++result.rect_draw_count;
        ++state_.rect_draw_count;
        return BackendFailureReason::None;
    };
    auto draw_image_command = [&](const DisplayCommand& command,
                                  const DisplayCommand& effective_command,
                                  const ClipSnapshot& clip,
                                  bool popup_command) -> BackendFailureReason {
        if (!ensure_texture(command.resource)) {
            diagnostics_event_ex("backend", "draw_image_skipped", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 "texture unavailable");
            return BackendFailureReason::None;
        }
        if (!ensure_image_pipeline()) {
            state_.draw_failure_count += 1;
            return BackendFailureReason::PipelineUnavailable;
        }

        const auto texture = native_->textures.find(command.resource.id);
        if (texture == native_->textures.end() ||
            texture->second.shader_resource_view == nullptr) {
            diagnostics_event_ex("backend", "draw_image_skipped", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 "texture cache miss");
            return BackendFailureReason::None;
        }

        std::vector<ImageVertex> image_vertices;
        append_image_vertices(image_vertices, effective_command, target_width, target_height);
        if (image_vertices.empty()) {
            return BackendFailureReason::None;
        }
        if (!apply_d3d_clip(clip)) {
            state_.draw_failure_count += 1;
            return BackendFailureReason::PipelineUnavailable;
        }

        D3D11_BUFFER_DESC buffer_desc{};
        buffer_desc.ByteWidth = static_cast<UINT>(image_vertices.size() * sizeof(ImageVertex));
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA buffer_data{};
        buffer_data.pSysMem = image_vertices.data();

        native_->image_vertex_buffer.Reset();
        HRESULT hr = native_->device->CreateBuffer(&buffer_desc, &buffer_data,
                                                   native_->image_vertex_buffer.GetAddressOf());
        if (FAILED(hr)) {
            state_.draw_failure_count += 1;
            set_failure(state_, BackendFailureReason::DrawFailed, static_cast<std::int32_t>(hr));
            emit_backend_failure("draw_failed", state_.last_failure, state_.last_hresult);
            return BackendFailureReason::DrawFailed;
        }

        D3D11_VIEWPORT viewport{};
        viewport.Width = target_width;
        viewport.Height = target_height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        native_->context->RSSetViewports(1, &viewport);

        constexpr UINT stride = sizeof(ImageVertex);
        constexpr UINT offset = 0;
        ID3D11Buffer* vertex_buffers[] = {native_->image_vertex_buffer.Get()};
        ID3D11ShaderResourceView* shader_resources[] = {
            texture->second.shader_resource_view.Get()};
        ID3D11SamplerState* samplers[] = {native_->image_sampler_state.Get()};
        const float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        native_->context->IASetVertexBuffers(0, 1, vertex_buffers, &stride, &offset);
        native_->context->IASetInputLayout(native_->image_input_layout.Get());
        native_->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        native_->context->VSSetShader(native_->image_vertex_shader.Get(), nullptr, 0);
        native_->context->PSSetShader(native_->image_pixel_shader.Get(), nullptr, 0);
        native_->context->PSSetShaderResources(0, 1, shader_resources);
        native_->context->PSSetSamplers(0, 1, samplers);
        native_->context->OMSetBlendState(native_->image_blend_state.Get(), blend_factor,
                                          0xffffffffu);
        native_->context->Draw(static_cast<UINT>(image_vertices.size()), 0);

        ID3D11ShaderResourceView* null_resources[] = {nullptr};
        native_->context->PSSetShaderResources(0, 1, null_resources);
        native_->context->OMSetBlendState(nullptr, blend_factor, 0xffffffffu);

        ++drawn_images;
        ++result.image_draw_count;
        diagnostics_event_ex("backend", popup_command ? "draw_popup_image" : "draw_image",
                             command.object_id, 0, default_runtime().current_frame_id(),
                             command.path.c_str(), command.resource.key.c_str());
        return BackendFailureReason::None;
    };

    for (std::size_t index = 0; index < display_list.commands.size(); ++index) {
        const DisplayCommand& command = display_list.commands[index];
        if (!is_draw_supported_command(command.kind)) {
            result.unsupported_draw_command_count += 1;
            state_.unsupported_draw_command_count += 1;
            diagnostics_event_ex("backend", "unsupported_draw_command", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 display_command_kind_name(command.kind));
            continue;
        }

        const bool popup_command = index >= popup_start;
        if (command.kind == DisplayCommandKind::Clip ||
            command.kind == DisplayCommandKind::ClipEnd ||
            command.kind == DisplayCommandKind::RoundedClip ||
            command.kind == DisplayCommandKind::RoundedClipEnd ||
            command.kind == DisplayCommandKind::Opacity ||
            command.kind == DisplayCommandKind::OpacityEnd ||
            command.kind == DisplayCommandKind::Transform ||
            command.kind == DisplayCommandKind::TransformEnd) {
            continue;
        }

        if (command.kind != DisplayCommandKind::Text && text_batch_active) {
            const BackendFailureReason text_end_failure = end_text_batch();
            if (text_end_failure != BackendFailureReason::None) {
                return text_end_failure;
            }
        }

        const DisplayCommand effective_command = apply_effect(command, command_effects[index]);
        switch (command.kind) {
        case DisplayCommandKind::Rect:
        case DisplayCommandKind::Shadow: {
            const BackendFailureReason draw_failure =
                draw_rect_command(index, command, effective_command, popup_command);
            if (draw_failure != BackendFailureReason::None) {
                return draw_failure;
            }
            break;
        }
        case DisplayCommandKind::Image: {
            const BackendFailureReason draw_failure =
                draw_image_command(command, effective_command, command_clips[index],
                                   popup_command);
            if (draw_failure != BackendFailureReason::None) {
                return draw_failure;
            }
            break;
        }
        case DisplayCommandKind::Text: {
            if (command.text.empty()) {
                break;
            }
            const BackendFailureReason draw_failure = draw_text_command(
                command, effective_command, command_clips[index],
                popup_command ? "draw_popup_text" : "draw_text",
                popup_command ? "draw_popup_text_skipped" : "draw_text_skipped");
            if (draw_failure != BackendFailureReason::None) {
                return draw_failure;
            }
            break;
        }
        default:
            break;
        }
    }
    if (text_batch_active) {
        const BackendFailureReason text_end_failure = end_text_batch();
        if (text_end_failure != BackendFailureReason::None) {
            return text_end_failure;
        }
    }
    state_.image_draw_count += drawn_images;
    state_.text_draw_count += drawn_texts;
    diagnostics_event_ex("backend", "draw_rects", 0, 0,
                         default_runtime().current_frame_id(), "",
                         "rect display commands executed");
    if (drawn_images > 0) {
        diagnostics_event_ex("backend", "draw_images", 0, 0,
                             default_runtime().current_frame_id(), "",
                             "image display commands executed");
    }
    if (drawn_texts > 0) {
        diagnostics_event_ex("backend", "draw_texts", 0, 0,
                             default_runtime().current_frame_id(), "",
                             "text display commands executed");
    }

    if (clip_commands > 0) {
        native_->context->RSSetState(native_->default_rasterizer_state.Get());
    }

    if (native_->swap_chain != nullptr) {
        const HRESULT hr = native_->swap_chain->Present(1, 0);
        if (FAILED(hr)) {
            state_.present_failure_count += 1;
            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
                state_.device_lost = true;
                state_.device_lost_count += 1;
                set_failure(state_, BackendFailureReason::DeviceLost,
                            static_cast<std::int32_t>(hr));
                emit_backend_failure("present_failed", state_.last_failure, state_.last_hresult);
                return BackendFailureReason::DeviceLost;
            }
            set_failure(state_, BackendFailureReason::PresentFailed, static_cast<std::int32_t>(hr));
            emit_backend_failure("present_failed", state_.last_failure, state_.last_hresult);
            return BackendFailureReason::PresentFailed;
        }
        state_.present_count += 1;
        result.presented = true;
        diagnostics_event_ex("backend", "present", 0, 0, default_runtime().current_frame_id(), "",
                             "swap chain presented");
    }

    return result.unsupported_draw_command_count > 0 ? BackendFailureReason::UnsupportedCommand
                                                     : BackendFailureReason::None;
#else
    for (const DisplayCommand& command : display_list.commands) {
        if (command.kind == DisplayCommandKind::Image) {
            (void)ensure_texture(command.resource);
        }
        if (command.kind == DisplayCommandKind::Clip ||
            command.kind == DisplayCommandKind::RoundedClip) {
            ++result.clip_command_count;
            ++result.clip_apply_count;
            ++state_.clip_command_count;
            ++state_.clip_apply_count;
            if (command.kind == DisplayCommandKind::RoundedClip) {
                ++result.rounded_clip_command_count;
                ++result.rounded_clip_apply_count;
                ++result.rounded_clip_fallback_count;
                ++state_.rounded_clip_command_count;
                ++state_.rounded_clip_apply_count;
                ++state_.rounded_clip_fallback_count;
            }
        }
        if (command.kind == DisplayCommandKind::ClipEnd ||
            command.kind == DisplayCommandKind::RoundedClipEnd) {
            ++result.clip_end_command_count;
            ++state_.clip_end_command_count;
            if (command.kind == DisplayCommandKind::RoundedClipEnd) {
                ++result.rounded_clip_end_command_count;
                ++state_.rounded_clip_end_command_count;
            }
        }
        if (command.kind == DisplayCommandKind::Opacity) {
            ++result.opacity_command_count;
            ++result.opacity_apply_count;
            ++state_.opacity_command_count;
            ++state_.opacity_apply_count;
        }
        if (command.kind == DisplayCommandKind::OpacityEnd) {
            ++result.opacity_end_command_count;
            ++state_.opacity_end_command_count;
        }
        if (command.kind == DisplayCommandKind::Transform) {
            ++result.transform_command_count;
            ++result.transform_apply_count;
            ++state_.transform_command_count;
            ++state_.transform_apply_count;
        }
        if (command.kind == DisplayCommandKind::TransformEnd) {
            ++result.transform_end_command_count;
            ++state_.transform_end_command_count;
        }
    }
    state_.headless_submit_count += state_.render_target_available ? 0 : 1;
    state_.render_target_clear_count += state_.render_target_available ? 1 : 0;
    state_.present_count += state_.swap_chain_available ? 1 : 0;
    result.presented = state_.swap_chain_available;
    return BackendFailureReason::None;
#endif
}

BackendFrameResult D3D11Backend::submit(const DisplayList& display_list,
                                        const std::vector<BackendBatch>& batches)
{
    BackendFrameResult result;
    if (state_.device_lost) {
        result.failure_reason = BackendFailureReason::DeviceLost;
        set_failure(state_, result.failure_reason);
        result.device = state_;
        diagnostics_event_ex("backend", "submit_failed", 0, 0, default_runtime().current_frame_id(),
                             "", backend_failure_reason_name(result.failure_reason));
        return result;
    }

    if (!initialize()) {
        result.failure_reason = BackendFailureReason::DeviceUnavailable;
        set_failure(state_, result.failure_reason, state_.last_hresult);
        result.device = state_;
        diagnostics_event_ex("backend", "submit_failed", 0, 0, default_runtime().current_frame_id(),
                             "", backend_failure_reason_name(result.failure_reason));
        return result;
    }

    const std::string begin_detail = submit_detail(display_list, batches);
    diagnostics_event_ex("backend", "submit_begin", 0, 0, default_runtime().current_frame_id(), "",
                         begin_detail.c_str());

    std::uint32_t unsupported_count = 0;
    for (const DisplayCommand& command : display_list.commands) {
        if (!is_supported_command(command.kind)) {
            unsupported_count += 1;
            diagnostics_event_ex("backend", "unsupported_command", command.object_id, 0,
                                 default_runtime().current_frame_id(), command.path.c_str(),
                                 display_command_kind_name(command.kind));
        }
    }

    state_.unsupported_command_count += unsupported_count;
    result.unsupported_command_count = unsupported_count;
    result.consumed_command_count = static_cast<std::uint32_t>(display_list.commands.size());

    const BackendFailureReason draw_failure = execute_draw_commands(display_list, result);
    result.submitted =
        draw_failure != BackendFailureReason::PipelineUnavailable &&
        draw_failure != BackendFailureReason::DrawFailed &&
        draw_failure != BackendFailureReason::PresentFailed &&
        draw_failure != BackendFailureReason::DeviceLost;
    result.failure_reason = unsupported_count > 0 ? BackendFailureReason::UnsupportedCommand
                           : draw_failure != BackendFailureReason::None
                               ? draw_failure
                               : BackendFailureReason::None;
    state_.frame_submit_count += 1;
    set_failure(state_, result.failure_reason);
    result.device = state_;

    std::ostringstream end_detail;
    end_detail << "submitted=" << (result.submitted ? "true" : "false")
               << ";consumed_commands=" << result.consumed_command_count
               << ";unsupported_commands=" << result.unsupported_command_count
               << ";failure=" << backend_failure_reason_name(result.failure_reason);
    const std::string detail = end_detail.str();
    diagnostics_event_ex("backend", "submit_end", 0, 0, default_runtime().current_frame_id(), "",
                         detail.c_str());
    return result;
}

void D3D11Backend::simulate_device_lost(const char* detail)
{
    state_.initialized = true;
    state_.device_lost = true;
    state_.device_lost_count += 1;
    set_failure(state_, BackendFailureReason::DeviceLost);
    diagnostics_event_ex("backend", "device_lost", 0, 0, default_runtime().current_frame_id(), "",
                         detail == nullptr ? "" : detail);
}

BackendDeviceState D3D11Backend::state() const noexcept
{
    return state_;
}

const char* backend_failure_reason_name(BackendFailureReason reason) noexcept
{
    switch (reason) {
    case BackendFailureReason::None:
        return "none";
    case BackendFailureReason::DeviceUnavailable:
        return "device_unavailable";
    case BackendFailureReason::DeviceLost:
        return "device_lost";
    case BackendFailureReason::UnsupportedCommand:
        return "unsupported_command";
    case BackendFailureReason::SubmitFailed:
        return "submit_failed";
    case BackendFailureReason::SwapChainUnavailable:
        return "swap_chain_unavailable";
    case BackendFailureReason::RenderTargetUnavailable:
        return "render_target_unavailable";
    case BackendFailureReason::RecoveryFailed:
        return "recovery_failed";
    case BackendFailureReason::PipelineUnavailable:
        return "pipeline_unavailable";
    case BackendFailureReason::DrawFailed:
        return "draw_failed";
    case BackendFailureReason::PresentFailed:
        return "present_failed";
    }
    return "unknown";
}

} // namespace fiui
