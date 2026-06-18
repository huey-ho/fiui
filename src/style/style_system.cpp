#include "style/style_system.h"

#include <cstring>

namespace fiui {
namespace {

constexpr Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
{
    return Color{r, g, b, a};
}

ComponentStateStyle state_style(Color background,
                                Color text,
                                Color border,
                                float border_width,
                                float radius)
{
    return ComponentStateStyle{background, text, border, border_width, radius};
}

ButtonTheme make_button_theme(const ColorScheme& colors, const RadiusScale& radius, bool dark)
{
    const Color hover = dark ? rgba(38, 45, 54) : rgba(239, 246, 255);
    const Color pressed = dark ? rgba(48, 56, 67) : rgba(219, 234, 254);
    const Color disabled_bg = dark ? rgba(35, 38, 43) : rgba(242, 244, 247);
    const Color disabled_text = dark ? rgba(103, 111, 124) : rgba(145, 152, 161);

    ButtonTheme theme;
    theme.normal = state_style(colors.surface, colors.text, colors.border, 1.0f, radius.md);
    theme.hover = state_style(hover, colors.text, colors.accent, 1.0f, radius.md);
    theme.pressed = state_style(pressed, colors.text, colors.accent, 1.0f, radius.md);
    theme.focused = state_style(colors.surface, colors.text, colors.accent, 2.0f, radius.md);
    theme.disabled = state_style(disabled_bg, disabled_text, colors.border, 1.0f, radius.md);
    theme.error = state_style(colors.surface, colors.error, colors.error, 1.0f, radius.md);
    return theme;
}

Theme make_theme(const ThemeTokens& tokens, bool dark)
{
    Theme theme;
    theme.name = tokens.name;
    theme.colors = ColorScheme{
        tokens.background,
        tokens.surface,
        tokens.text,
        tokens.muted_text,
        tokens.accent,
        tokens.border,
        dark ? rgba(255, 122, 122) : rgba(202, 35, 35),
    };
    theme.typography = Typography{tokens.font_body, tokens.font_title, 13.0f};
    theme.spacing = SpacingScale{tokens.space_sm, tokens.space_md, tokens.space_lg, tokens.space_lg};
    theme.radius = RadiusScale{tokens.radius_sm, tokens.radius_md, tokens.radius_md + 4.0f};
    theme.shadow = dark ? ShadowScale{8.0f, 18.0f, 28.0f} : ShadowScale{6.0f, 14.0f, 24.0f};
    theme.motion = MotionTokens{90.0f, 160.0f, 240.0f};
    theme.density = DensityTokens{40.0f, 32.0f};
    theme.components.button = make_button_theme(theme.colors, theme.radius, dark);
    return theme;
}

const Theme& modern_light_theme()
{
    static const Theme theme = make_theme(theme_tokens("modern.light"), false);
    return theme;
}

const Theme& modern_dark_theme()
{
    static const Theme theme = make_theme(theme_tokens("modern.dark"), true);
    return theme;
}

} // namespace

const Theme& StyleSystem::resolve_theme(const char* name) const
{
    if (name != nullptr && std::strcmp(name, "modern.dark") == 0) {
        return modern_dark_theme();
    }
    return modern_light_theme();
}

void StyleSystem::set_active_theme(const char* name)
{
    active_theme_name_ = resolve_theme(name).name;
}

const Theme& StyleSystem::active_theme() const
{
    return resolve_theme(active_theme_name_.c_str());
}

const char* StyleSystem::active_theme_name() const noexcept
{
    return active_theme_name_.c_str();
}

const ComponentStateStyle& StyleSystem::resolve_button_style(const Theme& theme,
                                                             ControlState state) const
{
    switch (state) {
    case ControlState::Normal:
        return theme.components.button.normal;
    case ControlState::Hover:
        return theme.components.button.hover;
    case ControlState::Pressed:
        return theme.components.button.pressed;
    case ControlState::Focused:
        return theme.components.button.focused;
    case ControlState::Disabled:
        return theme.components.button.disabled;
    case ControlState::Error:
        return theme.components.button.error;
    }
    return theme.components.button.normal;
}

const char* control_state_name(ControlState state) noexcept
{
    switch (state) {
    case ControlState::Normal:
        return "normal";
    case ControlState::Hover:
        return "hover";
    case ControlState::Pressed:
        return "pressed";
    case ControlState::Focused:
        return "focused";
    case ControlState::Disabled:
        return "disabled";
    case ControlState::Error:
        return "error";
    }
    return "unknown";
}

} // namespace fiui
