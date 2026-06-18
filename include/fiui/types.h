#pragma once

#include <cstdint>

namespace fiui {

using ObjectId = std::uint64_t;

struct Size {
    float width = 0.0f;
    float height = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

enum class DebugMode {
    Off,
    Basic,
    AiFriendly,
    Verbose,
    Stress,
};

enum class LifecycleState {
    Created,
    Attached,
    Detached,
    Destroying,
    Destroyed,
};

enum class WidgetKind {
    Widget,
    Window,
    Text,
    Button,
    CheckBox,
    RadioButton,
    Switch,
    Input,
    TextArea,
    Image,
    Progress,
    Slider,
    Select,
    SelectOption,
    ListView,
    ListItem,
    TreeView,
    TreeItem,
    TableView,
    Tabs,
    Toolbar,
    Column,
    Row,
    Padding,
    Align,
    SizedBox,
    ScrollView,
    Dialog,
    SplitView,
    MenuBar,
    MenuItem,
    Separator,
    Spacer,
};

enum class ImageFit {
    Stretch,
    Contain,
    Cover,
};

enum class OverflowPolicy {
    Clip,
    Visible,
    Scroll,
};

enum class SplitOrientation {
    Horizontal,
    Vertical,
};

enum class DirtyReason : std::uint32_t {
    None = 0,
    ApiMutation = 1u << 0,
    Attach = 1u << 1,
    Detach = 1u << 2,
    Layout = 1u << 3,
    Paint = 1u << 4,
    TextChanged = 1u << 5,
    StyleChanged = 1u << 6,
    Input = 1u << 7,
    Resize = 1u << 8,
    ThemeChanged = 1u << 9,
    ResourceChanged = 1u << 10,
};

inline DirtyReason operator|(DirtyReason left, DirtyReason right)
{
    return static_cast<DirtyReason>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

inline DirtyReason& operator|=(DirtyReason& left, DirtyReason right)
{
    left = left | right;
    return left;
}

inline bool has_dirty_reason(DirtyReason value, DirtyReason flag)
{
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
}

} // namespace fiui
