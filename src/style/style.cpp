#include "fiui/style.h"

#include "diagnostics/diagnostics_internal.h"
#include "runtime/runtime.h"

#include <cstring>

namespace fiui {
namespace {

constexpr Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
{
    return Color{r, g, b, a};
}

const ThemeTokens kModernLight{
    "modern.light",
    rgba(247, 248, 250),
    rgba(255, 255, 255),
    rgba(31, 35, 40),
    rgba(91, 99, 110),
    rgba(30, 111, 235),
    rgba(208, 215, 222),
    4.0f,
    8.0f,
    6.0f,
    12.0f,
    16.0f,
    16.0f,
    26.0f,
};

const ThemeTokens kModernDark{
    "modern.dark",
    rgba(16, 18, 22),
    rgba(28, 31, 36),
    rgba(235, 239, 245),
    rgba(151, 160, 175),
    rgba(88, 166, 255),
    rgba(61, 68, 77),
    4.0f,
    8.0f,
    6.0f,
    12.0f,
    16.0f,
    16.0f,
    26.0f,
};

const ThemeTokens* find_theme(const char* name)
{
    if (name != nullptr && std::strcmp(name, "modern.dark") == 0) {
        return &kModernDark;
    }
    return &kModernLight;
}

} // namespace

Application::Application()
    : theme_(nullptr)
{
}

Application& Application::theme(const char* name)
{
    theme_ = find_theme(name);
    default_runtime().style_system().set_active_theme(theme_->name);
    const Theme& resolved_theme = default_runtime().style_system().resolve_theme(theme_->name);
    diagnostics_event("style", "theme", 0, "", resolved_theme.name);
    default_runtime().frame_scheduler().request_frame("theme_changed", "style", 0, 0,
                                                      default_runtime().current_frame_id(), "");
    return *this;
}

const char* Application::theme() const noexcept
{
    return tokens().name;
}

const ThemeTokens& Application::tokens() const noexcept
{
    return theme_ == nullptr ? kModernLight : *theme_;
}

const ThemeTokens& theme_tokens(const char* name)
{
    const ThemeTokens* tokens = find_theme(name);
    diagnostics_event("style", "theme_tokens", 0, "", tokens->name);
    return *tokens;
}

} // namespace fiui
