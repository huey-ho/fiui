#pragma once

#include "fiui/export.h"
#include "fiui/style.h"

#include <string>

namespace fiui {

enum class ControlState {
    Normal,
    Hover,
    Pressed,
    Focused,
    Disabled,
    Error,
};

struct ColorScheme {
    Color background;
    Color surface;
    Color text;
    Color muted_text;
    Color accent;
    Color border;
    Color error;
};

struct Typography {
    float body = 16.0f;
    float title = 26.0f;
    float caption = 13.0f;
};

struct SpacingScale {
    float sm = 6.0f;
    float md = 12.0f;
    float lg = 16.0f;
    float page = 16.0f;
};

struct RadiusScale {
    float sm = 4.0f;
    float md = 8.0f;
    float lg = 12.0f;
};

struct ShadowScale {
    float sm_blur = 6.0f;
    float md_blur = 14.0f;
    float lg_blur = 24.0f;
};

struct MotionTokens {
    float fast_ms = 90.0f;
    float normal_ms = 160.0f;
    float slow_ms = 240.0f;
};

struct DensityTokens {
    float control_height = 40.0f;
    float compact_control_height = 32.0f;
};

struct ComponentStateStyle {
    Color background;
    Color text;
    Color border;
    float border_width = 1.0f;
    float radius = 8.0f;
};

struct ButtonTheme {
    ComponentStateStyle normal;
    ComponentStateStyle hover;
    ComponentStateStyle pressed;
    ComponentStateStyle focused;
    ComponentStateStyle disabled;
    ComponentStateStyle error;
};

struct ComponentThemes {
    ButtonTheme button;
};

struct Theme {
    const char* name = "modern.light";
    ColorScheme colors;
    Typography typography;
    SpacingScale spacing;
    RadiusScale radius;
    ShadowScale shadow;
    MotionTokens motion;
    DensityTokens density;
    ComponentThemes components;
};

class StyleSystem {
public:
    [[nodiscard]] FIUI_API const Theme& resolve_theme(const char* name) const;
    FIUI_API void set_active_theme(const char* name);
    [[nodiscard]] FIUI_API const Theme& active_theme() const;
    [[nodiscard]] FIUI_API const char* active_theme_name() const noexcept;
    [[nodiscard]] FIUI_API const ComponentStateStyle& resolve_button_style(
        const Theme& theme, ControlState state) const;

private:
    std::string active_theme_name_ = "modern.light";
};

FIUI_API const char* control_state_name(ControlState state) noexcept;

} // namespace fiui
