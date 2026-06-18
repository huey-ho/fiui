#include "image/image_system.h"

#include "diagnostics/diagnostics_internal.h"
#include "runtime/runtime.h"

#include <algorithm>
#include <sstream>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <objbase.h>
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace fiui {
namespace {

std::uint32_t positive_dimension(float value, std::uint32_t fallback) noexcept
{
    return value > 0.0f ? static_cast<std::uint32_t>(value) : fallback;
}

#if defined(_WIN32)

std::wstring utf8_to_wide_path(const char* text)
{
    if (text == nullptr || text[0] == '\0') {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (needed <= 1) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), needed);
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

std::string pixel_format_name(const WICPixelFormatGUID& format)
{
    if (format == GUID_WICPixelFormat32bppBGRA) {
        return "32bpp_bgra";
    }
    if (format == GUID_WICPixelFormat32bppRGBA) {
        return "32bpp_rgba";
    }
    if (format == GUID_WICPixelFormat24bppBGR) {
        return "24bpp_bgr";
    }
    if (format == GUID_WICPixelFormat24bppRGB) {
        return "24bpp_rgb";
    }
    if (format == GUID_WICPixelFormat8bppGray) {
        return "8bpp_gray";
    }
    return "unknown";
}

#endif

} // namespace

struct ImageSystem::NativeState {
#if defined(_WIN32)
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    bool co_initialized = false;
#endif
};

ImageSystem::ImageSystem() : native_(std::make_unique<NativeState>()) {}

ImageSystem::~ImageSystem()
{
#if defined(_WIN32)
    native_->factory.Reset();
    if (native_->co_initialized) {
        CoUninitialize();
    }
#endif
}

bool ImageSystem::initialize_factory()
{
    if (state_.factory_initialized) {
        return true;
    }

#if defined(_WIN32)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        native_->co_initialized = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        state_.last_hresult = static_cast<std::int32_t>(hr);
        diagnostics_event_ex("image", "com_failed", 0, 0,
                             default_runtime().current_frame_id(), "", "CoInitializeEx");
        return false;
    }

    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(native_->factory.GetAddressOf()));
    state_.factory_create_count += 1;
    state_.last_hresult = static_cast<std::int32_t>(hr);
    if (FAILED(hr)) {
        diagnostics_event_ex("image", "factory_failed", 0, 0,
                             default_runtime().current_frame_id(), "", "CoCreateInstance");
        return false;
    }

    state_.factory_initialized = true;
    diagnostics_event_ex("image", "factory_create", 0, 0,
                         default_runtime().current_frame_id(), "", state_.backend_name);
    return true;
#else
    state_.factory_create_count += 1;
    state_.factory_initialized = false;
    state_.last_hresult = 0;
    return false;
#endif
}

ImageMetadata ImageSystem::query_metadata(const char* resource_path,
                                          float fallback_width,
                                          float fallback_height,
                                          ObjectId owner_object_id,
                                          const char* owner_path)
{
    state_.metadata_query_count += 1;

#if defined(_WIN32)
    if (!initialize_factory()) {
        return fallback_metadata(resource_path, fallback_width, fallback_height, owner_object_id,
                                 owner_path);
    }

    const std::wstring path = resolve_image_path(utf8_to_wide_path(resource_path));
    if (path.empty()) {
        return fallback_metadata(resource_path, fallback_width, fallback_height, owner_object_id,
                                 owner_path);
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = native_->factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
        decoder.GetAddressOf());
    state_.last_hresult = static_cast<std::int32_t>(hr);
    if (FAILED(hr)) {
        diagnostics_event_ex("image", "metadata_fallback", owner_object_id, 0,
                             default_runtime().current_frame_id(),
                             owner_path == nullptr ? "" : owner_path,
                             resource_path == nullptr ? "" : resource_path);
        return fallback_metadata(resource_path, fallback_width, fallback_height, owner_object_id,
                                 owner_path);
    }

    UINT frame_count = 0;
    decoder->GetFrameCount(&frame_count);

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    state_.last_hresult = static_cast<std::int32_t>(hr);
    if (FAILED(hr)) {
        return fallback_metadata(resource_path, fallback_width, fallback_height, owner_object_id,
                                 owner_path);
    }

    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);

    double dpi_x = 96.0;
    double dpi_y = 96.0;
    frame->GetResolution(&dpi_x, &dpi_y);

    WICPixelFormatGUID pixel_format{};
    frame->GetPixelFormat(&pixel_format);

    ImageMetadata metadata;
    metadata.valid = true;
    metadata.wic = true;
    metadata.fallback = false;
    metadata.width = width;
    metadata.height = height;
    metadata.dpi_x = static_cast<std::uint32_t>(std::max(1.0, dpi_x));
    metadata.dpi_y = static_cast<std::uint32_t>(std::max(1.0, dpi_y));
    metadata.frame_count = frame_count;
    metadata.pixel_format = pixel_format_name(pixel_format);

    std::ostringstream detail;
    detail << "width=" << metadata.width << ";height=" << metadata.height
           << ";frames=" << metadata.frame_count << ";format=" << metadata.pixel_format;
    const std::string detail_text = detail.str();
    diagnostics_event_ex("image", "metadata", owner_object_id, 0,
                         default_runtime().current_frame_id(),
                         owner_path == nullptr ? "" : owner_path, detail_text.c_str());
    return metadata;
#else
    return fallback_metadata(resource_path, fallback_width, fallback_height, owner_object_id,
                             owner_path);
#endif
}

ImageMetadata ImageSystem::fallback_metadata(const char* resource_path,
                                             float fallback_width,
                                             float fallback_height,
                                             ObjectId owner_object_id,
                                             const char* owner_path)
{
    state_.fallback_metadata_count += 1;
    ImageMetadata metadata;
    metadata.valid = true;
    metadata.wic = false;
    metadata.fallback = true;
    metadata.width = positive_dimension(fallback_width, 64);
    metadata.height = positive_dimension(fallback_height, 64);
    metadata.dpi_x = 96;
    metadata.dpi_y = 96;
    metadata.frame_count = 0;
    metadata.pixel_format = "fallback";
    diagnostics_event_ex("image", "metadata_fallback", owner_object_id, 0,
                         default_runtime().current_frame_id(),
                         owner_path == nullptr ? "" : owner_path,
                         resource_path == nullptr ? "" : resource_path);
    return metadata;
}

ImageSystemState ImageSystem::state() const noexcept
{
    return state_;
}

} // namespace fiui
