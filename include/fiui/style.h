#pragma once

#include "fiui/export.h"

#include <cstdint>

namespace fiui {

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

struct ThemeTokens {
    const char* name = "modern.light";
    Color background;
    Color surface;
    Color text;
    Color muted_text;
    Color accent;
    Color border;
    float radius_sm = 4.0f;
    float radius_md = 8.0f;
    float space_sm = 6.0f;
    float space_md = 12.0f;
    float space_lg = 16.0f;
    float font_body = 14.0f;
    float font_title = 22.0f;
};

class FIUI_API Application {
public:
    Application();

    Application& theme(const char* name);
    [[nodiscard]] const char* theme() const noexcept;
    [[nodiscard]] const ThemeTokens& tokens() const noexcept;

private:
    const ThemeTokens* theme_ = nullptr;
};

FIUI_API const ThemeTokens& theme_tokens(const char* name);

namespace space {
inline constexpr float sm = 6.0f;
inline constexpr float md = 12.0f;
inline constexpr float lg = 16.0f;
inline constexpr float page = 16.0f;
} // namespace space

namespace text {
inline constexpr const char* body = "body";
inline constexpr const char* title = "title";
inline constexpr const char* caption = "caption";
} // namespace text

} // namespace fiui
