#include "text/text_system.h"

#include "diagnostics/diagnostics_internal.h"
#include "runtime/runtime.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwrite.h>
#include <windows.h>
#include <wrl/client.h>
#endif

namespace fiui {
namespace {

float positive_or(float value, float fallback) noexcept
{
    return value > 0.0f ? value : fallback;
}

std::uint32_t normalized_dpi(std::uint32_t dpi) noexcept
{
    return std::max<std::uint32_t>(1, dpi);
}

std::uint32_t text_length(const char* text) noexcept
{
    return text == nullptr ? 0 : static_cast<std::uint32_t>(std::strlen(text));
}

bool string_is(const char* value, const char* expected) noexcept
{
    return value != nullptr && expected != nullptr && std::strcmp(value, expected) == 0;
}

std::uint32_t explicit_line_count(const char* text) noexcept
{
    if (text == nullptr || text[0] == '\0') {
        return 1;
    }
    std::uint32_t lines = 1;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor == '\n') {
            ++lines;
        }
    }
    return lines;
}

#if defined(_WIN32)

std::wstring utf8_to_wide(const char* text)
{
    if (text == nullptr || text[0] == '\0') {
        return L" ";
    }
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (needed <= 1) {
        return L" ";
    }
    std::wstring wide(static_cast<std::size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide.data(), needed);
    return wide;
}

#endif

} // namespace

struct TextSystem::NativeState {
#if defined(_WIN32)
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
#endif
};

TextSystem::TextSystem() : native_(std::make_unique<NativeState>()) {}

TextSystem::~TextSystem() = default;

bool TextSystem::initialize_factory()
{
    if (state_.factory_initialized) {
        return true;
    }

#if defined(_WIN32)
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                     reinterpret_cast<IUnknown**>(
                                         native_->factory.GetAddressOf()));
    state_.factory_create_count += 1;
    state_.last_hresult = static_cast<std::int32_t>(hr);
    if (FAILED(hr)) {
        diagnostics_event_ex("text", "factory_failed", 0, 0,
                             default_runtime().current_frame_id(), "", "DWriteCreateFactory");
        return false;
    }
    state_.factory_initialized = true;
    diagnostics_event_ex("text", "factory_create", 0, 0, default_runtime().current_frame_id(),
                         "", state_.backend_name);
    return true;
#else
    state_.factory_create_count += 1;
    state_.factory_initialized = false;
    state_.last_hresult = 0;
    return false;
#endif
}

TextMetrics TextSystem::measure_text(const char* text,
                                     float font_size,
                                     float max_width,
                                     float max_height,
                                     std::uint32_t dpi,
                                     ObjectId owner_object_id,
                                     const char* owner_path,
                                     const char* word_wrap,
                                     const char* overflow)
{
    state_.measure_count += 1;
    const float normalized_font_size = positive_or(font_size, 14.0f);
    const float layout_width = positive_or(max_width, 10000.0f);
    const float layout_height = positive_or(max_height, normalized_font_size * 2.0f);
    const std::uint32_t dpi_value = normalized_dpi(dpi);

#if defined(_WIN32)
    if (!initialize_factory()) {
        return fallback_metrics(text, normalized_font_size, layout_width, layout_height, dpi_value,
                                owner_object_id, owner_path, word_wrap, overflow);
    }

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    HRESULT hr = native_->factory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, normalized_font_size, L"", format.GetAddressOf());
    state_.last_hresult = static_cast<std::int32_t>(hr);
    if (FAILED(hr)) {
        diagnostics_event_ex("text", "format_failed", owner_object_id, 0,
                             default_runtime().current_frame_id(),
                             owner_path == nullptr ? "" : owner_path, "CreateTextFormat");
        return fallback_metrics(text, normalized_font_size, layout_width, layout_height, dpi_value,
                                owner_object_id, owner_path, word_wrap, overflow);
    }
    format->SetWordWrapping(string_is(word_wrap, "word") ? DWRITE_WORD_WRAPPING_WRAP
                                                          : DWRITE_WORD_WRAPPING_NO_WRAP);
    if (string_is(overflow, "ellipsis")) {
        DWRITE_TRIMMING trimming{};
        trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
        Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;
        if (SUCCEEDED(native_->factory->CreateEllipsisTrimmingSign(format.Get(),
                                                                   ellipsis.GetAddressOf()))) {
            format->SetTrimming(&trimming, ellipsis.Get());
        }
    }

    const std::wstring wide = utf8_to_wide(text);
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    hr = native_->factory->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()),
                                            format.Get(), layout_width, layout_height,
                                            layout.GetAddressOf());
    state_.last_hresult = static_cast<std::int32_t>(hr);
    if (FAILED(hr)) {
        diagnostics_event_ex("text", "layout_failed", owner_object_id, 0,
                             default_runtime().current_frame_id(),
                             owner_path == nullptr ? "" : owner_path, "CreateTextLayout");
        return fallback_metrics(text, normalized_font_size, layout_width, layout_height, dpi_value,
                                owner_object_id, owner_path, word_wrap, overflow);
    }

    DWRITE_TEXT_METRICS dwrite_metrics{};
    hr = layout->GetMetrics(&dwrite_metrics);
    state_.last_hresult = static_cast<std::int32_t>(hr);
    if (FAILED(hr)) {
        diagnostics_event_ex("text", "metrics_failed", owner_object_id, 0,
                             default_runtime().current_frame_id(),
                             owner_path == nullptr ? "" : owner_path, "GetMetrics");
        return fallback_metrics(text, normalized_font_size, layout_width, layout_height, dpi_value,
                                owner_object_id, owner_path, word_wrap, overflow);
    }

    std::vector<DWRITE_LINE_METRICS> line_metrics(dwrite_metrics.lineCount);
    UINT32 actual_line_count = 0;
    if (!line_metrics.empty()) {
        hr = layout->GetLineMetrics(line_metrics.data(), dwrite_metrics.lineCount,
                                    &actual_line_count);
        state_.last_hresult = static_cast<std::int32_t>(hr);
    }

    TextMetrics metrics;
    metrics.valid = true;
    metrics.directwrite = true;
    metrics.width = dwrite_metrics.widthIncludingTrailingWhitespace;
    metrics.height = dwrite_metrics.height;
    metrics.layout_width = layout_width;
    metrics.layout_height = layout_height;
    metrics.baseline = !line_metrics.empty() && SUCCEEDED(hr)
                           ? line_metrics.front().baseline
                           : normalized_font_size * 0.8f;
    metrics.font_size = normalized_font_size;
    metrics.line_count = dwrite_metrics.lineCount;
    metrics.dpi = dpi_value;

    std::ostringstream detail;
    detail << "width=" << metrics.width << ";height=" << metrics.height
           << ";lines=" << metrics.line_count << ";font=" << metrics.font_size
           << ";dpi=" << metrics.dpi << ";wrap=" << (word_wrap == nullptr ? "" : word_wrap)
           << ";overflow=" << (overflow == nullptr ? "" : overflow);
    const std::string detail_text = detail.str();
    diagnostics_event_ex("text", "measure", owner_object_id, 0,
                         default_runtime().current_frame_id(),
                         owner_path == nullptr ? "" : owner_path, detail_text.c_str());
    return metrics;
#else
    return fallback_metrics(text, normalized_font_size, layout_width, layout_height, dpi_value,
                            owner_object_id, owner_path, word_wrap, overflow);
#endif
}

TextMetrics TextSystem::fallback_metrics(const char* text,
                                         float font_size,
                                         float max_width,
                                         float max_height,
                                         std::uint32_t dpi,
                                         ObjectId owner_object_id,
                                         const char* owner_path,
                                         const char* word_wrap,
                                         const char* overflow)
{
    state_.fallback_measure_count += 1;
    const float raw_width = static_cast<float>(std::max<std::uint32_t>(1, text_length(text))) *
                            font_size * 0.56f;
    std::uint32_t line_count = explicit_line_count(text);
    if (string_is(word_wrap, "word") && max_width > 1.0f) {
        line_count = std::max(line_count,
                              static_cast<std::uint32_t>(std::ceil(raw_width / max_width)));
    }
    const float estimated_width = string_is(word_wrap, "word") ? std::min(max_width, raw_width)
                                                               : raw_width;
    const float estimated_height = std::min(max_height, font_size * 1.2f *
                                                            static_cast<float>(line_count));
    TextMetrics metrics;
    metrics.valid = true;
    metrics.directwrite = false;
    metrics.width = estimated_width;
    metrics.height = estimated_height;
    metrics.layout_width = max_width;
    metrics.layout_height = max_height;
    metrics.baseline = font_size * 0.8f;
    metrics.font_size = font_size;
    metrics.line_count = line_count;
    metrics.dpi = dpi;

    diagnostics_event_ex("text", "measure_fallback", owner_object_id, 0,
                         default_runtime().current_frame_id(),
                         owner_path == nullptr ? "" : owner_path,
                         overflow == nullptr ? "estimated text metrics" : overflow);
    return metrics;
}

TextSystemState TextSystem::state() const noexcept
{
    return state_;
}

} // namespace fiui
