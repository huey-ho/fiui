#include <fiui/fiui.h>

#include "dirty/dirty_tracker.h"
#include "layout/layout_system.h"
#include "platform/platform_system.h"
#include "render/render_system.h"
#include "resource/resource_system.h"
#include "runtime/runtime.h"
#include "style/style_system.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <crtdbg.h>
#include <windows.h>
#endif

namespace {

#define FIUI_TEST_ASSERT(condition)                                                                \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "fiui test assertion failed: %s (%s:%d)\n", #condition, __FILE__, \
                         __LINE__);                                                               \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

void throwing_callback(void*)
{
    throw 42;
}

void increment_int_callback(void* user_data)
{
    int* value = static_cast<int*>(user_data);
    if (value != nullptr) {
        ++(*value);
    }
}

constexpr std::uint32_t test_key_shift = 1u << 16;
constexpr std::uint32_t test_key_alt = 1u << 17;
constexpr std::uint32_t test_key_ctrl = 1u << 18;

class TestColumn : public fiui::Column {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestRow : public fiui::Row {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestToolbar : public fiui::Toolbar {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestButton : public fiui::Button {
public:
    explicit TestButton(const char* label = "")
        : fiui::Button(label)
    {
    }

    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestCheckBox : public fiui::CheckBox {
public:
    explicit TestCheckBox(const char* label = "")
        : fiui::CheckBox(label)
    {
    }

    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestRadioButton : public fiui::RadioButton {
public:
    explicit TestRadioButton(const char* label = "")
        : fiui::RadioButton(label)
    {
    }

    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestSwitch : public fiui::Switch {
public:
    explicit TestSwitch(const char* label = "")
        : fiui::Switch(label)
    {
    }

    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestMenuBar : public fiui::MenuBar {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestMenuItem : public fiui::MenuItem {
public:
    explicit TestMenuItem(const char* label = "")
        : fiui::MenuItem(label)
    {
    }

    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestSeparator : public fiui::Separator {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestWindow : public fiui::Window {
public:
    TestWindow()
        : fiui::Window("Test Window")
    {
    }

    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestScrollView : public fiui::ScrollView {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestInput : public fiui::Input {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }

    [[nodiscard]] const std::string& value_text() const noexcept
    {
        return impl()->properties.text;
    }

    [[nodiscard]] std::size_t cursor() const noexcept
    {
        return impl()->properties.text_cursor;
    }

    [[nodiscard]] std::size_t selection_anchor() const noexcept
    {
        return impl()->properties.text_selection_anchor;
    }

    [[nodiscard]] bool selection_dragging() const noexcept
    {
        return impl()->properties.text_selection_dragging;
    }
};

class TestTextArea : public fiui::TextArea {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }

    [[nodiscard]] const std::string& value_text() const noexcept
    {
        return impl()->properties.text;
    }

    [[nodiscard]] std::size_t cursor() const noexcept
    {
        return impl()->properties.text_cursor;
    }

    [[nodiscard]] std::size_t selection_anchor() const noexcept
    {
        return impl()->properties.text_selection_anchor;
    }
};

class TestImage : public fiui::Image {
public:
    explicit TestImage(const char* path = "")
        : fiui::Image(path)
    {
    }

    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestSlider : public fiui::Slider {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestSelect : public fiui::Select {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestListView : public fiui::ListView {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestTreeView : public fiui::TreeView {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestTreeItem : public fiui::TreeItem {
public:
    explicit TestTreeItem(const char* label = "")
        : fiui::TreeItem(label)
    {
    }

    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestTableView : public fiui::TableView {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestTabs : public fiui::Tabs {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestDialog : public fiui::Dialog {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

class TestSplitView : public fiui::SplitView {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept
    {
        return impl();
    }
};

std::string read_text_file(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

void write_u16(std::ofstream& file, std::uint16_t value)
{
    file.put(static_cast<char>(value & 0xffu));
    file.put(static_cast<char>((value >> 8u) & 0xffu));
}

void write_u32(std::ofstream& file, std::uint32_t value)
{
    file.put(static_cast<char>(value & 0xffu));
    file.put(static_cast<char>((value >> 8u) & 0xffu));
    file.put(static_cast<char>((value >> 16u) & 0xffu));
    file.put(static_cast<char>((value >> 24u) & 0xffu));
}

void write_test_bmp(const char* path, std::uint32_t width, std::uint32_t height)
{
    const std::uint32_t row_stride = ((width * 3u + 3u) / 4u) * 4u;
    const std::uint32_t pixel_bytes = row_stride * height;
    const std::uint32_t file_bytes = 14u + 40u + pixel_bytes;
    std::ofstream file(path, std::ios::binary);
    file.put('B');
    file.put('M');
    write_u32(file, file_bytes);
    write_u16(file, 0);
    write_u16(file, 0);
    write_u32(file, 14u + 40u);
    write_u32(file, 40u);
    write_u32(file, width);
    write_u32(file, height);
    write_u16(file, 1);
    write_u16(file, 24);
    write_u32(file, 0);
    write_u32(file, pixel_bytes);
    write_u32(file, 2835);
    write_u32(file, 2835);
    write_u32(file, 0);
    write_u32(file, 0);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            file.put(static_cast<char>(x % 256u));
            file.put(static_cast<char>(y % 256u));
            file.put(static_cast<char>(200));
        }
        for (std::uint32_t pad = width * 3u; pad < row_stride; ++pad) {
            file.put('\0');
        }
    }
}

bool same_color(fiui::Color left, fiui::Color right)
{
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

bool has_button_rect_state(const fiui::RenderFrameResult& render_result,
                           fiui::ObjectId object_id,
                           const char* state,
                           const fiui::ComponentStateStyle& expected)
{
    for (const fiui::DisplayCommand& command : render_result.display_list.commands) {
        if (command.kind != fiui::DisplayCommandKind::Rect ||
            command.widget_kind != fiui::WidgetKind::Button || command.object_id != object_id) {
            continue;
        }
        return std::strcmp(command.style.control_state, state) == 0 &&
               same_color(command.style.fill, expected.background) &&
               same_color(command.style.text, expected.text) &&
               same_color(command.style.border, expected.border) &&
               command.style.border_width == expected.border_width &&
               command.style.radius == expected.radius;
    }
    return false;
}

bool has_layer_kind(const fiui::RenderFrameResult& render_result, fiui::LayerKind kind)
{
    for (const fiui::LayerNode& layer : render_result.layer_tree.nodes) {
        if (layer.kind == kind) {
            return true;
        }
    }
    return false;
}

bool has_layer_for_object(const fiui::RenderFrameResult& render_result,
                          fiui::LayerKind kind,
                          fiui::ObjectId object_id)
{
    for (const fiui::LayerNode& layer : render_result.layer_tree.nodes) {
        if (layer.kind == kind && layer.object_id == object_id) {
            return true;
        }
    }
    return false;
}

bool has_layer_path(const fiui::RenderFrameResult& render_result,
                    fiui::ObjectId object_id,
                    const char* path_fragment)
{
    for (const fiui::LayerNode& layer : render_result.layer_tree.nodes) {
        if (layer.object_id == object_id &&
            layer.path.find(path_fragment == nullptr ? "" : path_fragment) !=
                std::string::npos) {
            return true;
        }
    }
    return false;
}

bool all_command_layers_are_linked(const fiui::RenderFrameResult& render_result)
{
    for (const fiui::LayerNode& layer : render_result.layer_tree.nodes) {
        if (layer.display_command_count == 0) {
            continue;
        }
        if (layer.display_command_index == fiui::invalid_display_command_index) {
            return false;
        }
        if (layer.display_command_index + layer.display_command_count >
            render_result.display_list.commands.size()) {
            return false;
        }
    }
    return true;
}

bool has_text_layer_metrics(const fiui::RenderFrameResult& render_result, fiui::ObjectId object_id)
{
    for (const fiui::LayerNode& layer : render_result.layer_tree.nodes) {
        if (layer.kind != fiui::LayerKind::Text || layer.object_id != object_id) {
            continue;
        }
        return layer.resource.kind == fiui::ResourceKind::TextLayout &&
               layer.resource.text_metrics.valid && layer.resource.text_metrics.width > 0.0f &&
               layer.resource.text_metrics.height > 0.0f &&
               layer.resource.text_metrics.font_size == layer.style.font_size &&
               layer.resource.text_metrics.line_count > 0;
    }
    return false;
}

bool has_image_layer_metadata(const fiui::RenderFrameResult& render_result,
                              fiui::ObjectId object_id)
{
    for (const fiui::LayerNode& layer : render_result.layer_tree.nodes) {
        if (layer.kind != fiui::LayerKind::Image || layer.object_id != object_id) {
            continue;
        }
        return layer.resource.kind == fiui::ResourceKind::Image &&
               layer.resource.image_metadata.valid &&
               layer.resource.image_metadata.width > 0 &&
               layer.resource.image_metadata.height > 0 &&
               !layer.resource.image_metadata.pixel_format.empty();
    }
    return false;
}

const fiui::DisplayCommand* find_rect_command(const fiui::RenderFrameResult& render_result,
                                              fiui::ObjectId object_id)
{
    for (const fiui::DisplayCommand& command : render_result.display_list.commands) {
        if (command.kind == fiui::DisplayCommandKind::Rect && command.object_id == object_id) {
            return &command;
        }
    }
    return nullptr;
}

const fiui::DisplayCommand* find_rect_command_with_path(
    const fiui::RenderFrameResult& render_result,
    fiui::ObjectId object_id,
    const char* path_fragment)
{
    for (const fiui::DisplayCommand& command : render_result.display_list.commands) {
        if (command.kind == fiui::DisplayCommandKind::Rect && command.object_id == object_id &&
            command.path.find(path_fragment == nullptr ? "" : path_fragment) !=
                std::string::npos) {
            return &command;
        }
    }
    return nullptr;
}

const fiui::DisplayCommand* find_text_command(const fiui::RenderFrameResult& render_result,
                                              fiui::ObjectId object_id)
{
    for (const fiui::DisplayCommand& command : render_result.display_list.commands) {
        if (command.kind == fiui::DisplayCommandKind::Text && command.object_id == object_id) {
            return &command;
        }
    }
    return nullptr;
}

const fiui::DisplayCommand* find_text_command_with_path(
    const fiui::RenderFrameResult& render_result,
    fiui::ObjectId object_id,
    const char* path_fragment)
{
    for (const fiui::DisplayCommand& command : render_result.display_list.commands) {
        if (command.kind == fiui::DisplayCommandKind::Text && command.object_id == object_id &&
            command.path.find(path_fragment == nullptr ? "" : path_fragment) !=
                std::string::npos) {
            return &command;
        }
    }
    return nullptr;
}

const fiui::DisplayCommand* find_image_command(const fiui::RenderFrameResult& render_result,
                                               fiui::ObjectId object_id)
{
    for (const fiui::DisplayCommand& command : render_result.display_list.commands) {
        if (command.kind == fiui::DisplayCommandKind::Image && command.object_id == object_id) {
            return &command;
        }
    }
    return nullptr;
}

bool has_input_caret_command(const fiui::RenderFrameResult& render_result,
                             fiui::ObjectId object_id)
{
    for (const fiui::DisplayCommand& command : render_result.display_list.commands) {
        if (command.kind == fiui::DisplayCommandKind::Text && command.object_id == object_id &&
            (command.widget_kind == fiui::WidgetKind::Input ||
             command.widget_kind == fiui::WidgetKind::TextArea) &&
            command.path.find("/caret") != std::string::npos && command.text == "|") {
            return true;
        }
    }
    return false;
}

const fiui::DisplayCommand* find_input_caret_command(
    const fiui::RenderFrameResult& render_result,
    fiui::ObjectId object_id)
{
    for (const fiui::DisplayCommand& command : render_result.display_list.commands) {
        if (command.kind == fiui::DisplayCommandKind::Text && command.object_id == object_id &&
            (command.widget_kind == fiui::WidgetKind::Input ||
             command.widget_kind == fiui::WidgetKind::TextArea) &&
            command.path.find("/caret") != std::string::npos && command.text == "|") {
            return &command;
        }
    }
    return nullptr;
}

const fiui::DisplayCommand* find_input_selection_command(
    const fiui::RenderFrameResult& render_result,
    fiui::ObjectId object_id)
{
    for (const fiui::DisplayCommand& command : render_result.display_list.commands) {
        if (command.kind == fiui::DisplayCommandKind::Rect && command.object_id == object_id &&
            (command.widget_kind == fiui::WidgetKind::Input ||
             command.widget_kind == fiui::WidgetKind::TextArea) &&
            command.path.find("/selection") != std::string::npos) {
            return &command;
        }
    }
    return nullptr;
}

#if defined(_WIN32)

HWND create_hidden_backend_test_window()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* class_name = L"fiui_backend_test_window";
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    RegisterClassW(&wc);
    return CreateWindowExW(0, class_name, L"fiui backend test", WS_OVERLAPPEDWINDOW, 0, 0, 320,
                           240, nullptr, nullptr, instance, nullptr);
}

#endif

} // namespace

int main()
{
#if defined(_WIN32)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    fiui::DiagnosticsConfig diagnostics;
    diagnostics.mode = fiui::DebugMode::AiFriendly;
    diagnostics.output_directory = "fiui-test-diagnostics";
    fiui::configure_diagnostics(diagnostics);

    TestColumn root;
    root.debug_id("root").size(320, 240);

    fiui::Button button("Run");
    button.debug_id("run_button");
    FIUI_TEST_ASSERT(button.use_count() == 1);
    FIUI_TEST_ASSERT(fiui::default_runtime_lookup_object(button.object_id(), button.generation()));
    FIUI_TEST_ASSERT(!fiui::default_runtime_lookup_object(button.object_id(), button.generation() + 1));
    const fiui::FrameSchedulerState scheduler_after_create =
        fiui::default_runtime_frame_scheduler_state();
    FIUI_TEST_ASSERT(scheduler_after_create.pending);
    FIUI_TEST_ASSERT(scheduler_after_create.requested_count > 0);
    FIUI_TEST_ASSERT(scheduler_after_create.coalesced_count > 0);

    fiui::Button copy = button;
    FIUI_TEST_ASSERT(button.object_id() == copy.object_id());
    FIUI_TEST_ASSERT(button.use_count() == 2);

    FIUI_TEST_ASSERT(root.add(button));
    FIUI_TEST_ASSERT(button.lifecycle_state() == fiui::LifecycleState::Attached);
    FIUI_TEST_ASSERT(!root.add(copy));
    FIUI_TEST_ASSERT(root.child_count() == 1);

    button.mark_dirty(fiui::DirtyReason::Input);
    FIUI_TEST_ASSERT(fiui::has_dirty_reason(button.dirty_reason(), fiui::DirtyReason::Input));

    fiui::Column detach_root;
    detach_root.debug_id("detach_root");
    fiui::Button detach_button("Detach");
    detach_button.debug_id("detach_button");
    FIUI_TEST_ASSERT(detach_root.add(detach_button));
    FIUI_TEST_ASSERT(detach_button.detach());
    FIUI_TEST_ASSERT(detach_button.lifecycle_state() == fiui::LifecycleState::Detached);

    fiui::LayoutSystem layout_system;
    fiui::LayoutResult layout_result =
        layout_system.arrange(*root.expose_impl(), fiui::LayoutConstraints{320.0f, 240.0f});
    FIUI_TEST_ASSERT(layout_result.arranged_nodes == 2);
    FIUI_TEST_ASSERT(button.bounds().height == 40.0f);

    fiui::default_runtime_record_platform_dpi_changed(144);
    TestColumn dpi_root;
    dpi_root.debug_id("dpi_root").size(320, 240);
    fiui::Button dpi_button("DPI");
    dpi_button.debug_id("dpi_button");
    FIUI_TEST_ASSERT(dpi_root.add(dpi_button));
    const fiui::LayoutResult dpi_layout =
        layout_system.arrange(*dpi_root.expose_impl(), fiui::LayoutConstraints{320.0f, 240.0f});
    FIUI_TEST_ASSERT(dpi_layout.arranged_nodes == 2);
    FIUI_TEST_ASSERT(dpi_button.bounds().height == 60.0f);
    fiui::RenderSystem dpi_render_system;
    const fiui::RenderFrameResult dpi_render =
        dpi_render_system.build_frame(*dpi_root.expose_impl());
    const fiui::DisplayCommand* dpi_button_text =
        find_text_command(dpi_render, dpi_button.object_id());
    FIUI_TEST_ASSERT(dpi_button_text != nullptr);
    FIUI_TEST_ASSERT(dpi_button_text->style.font_size == 24.0f);
    fiui::default_runtime_record_platform_dpi_changed(96);

    TestRow responsive_root;
    responsive_root.debug_id("responsive_root").size(1080, 720).padding(24.0f).gap(16.0f);
    TestColumn fixed_nav;
    fixed_nav.debug_id("fixed_nav").size(220, 0);
    TestColumn flexible_content;
    flexible_content.debug_id("flexible_content").flex();
    FIUI_TEST_ASSERT(responsive_root.add(fixed_nav));
    FIUI_TEST_ASSERT(responsive_root.add(flexible_content));
    const fiui::LayoutResult responsive_first = layout_system.arrange(
        *responsive_root.expose_impl(), fiui::LayoutConstraints{1080.0f, 720.0f});
    FIUI_TEST_ASSERT(responsive_first.arranged_nodes == 3);
    const float first_nav_width = fixed_nav.bounds().width;
    const float first_content_width = flexible_content.bounds().width;
    responsive_root.size(1600, 720);
    const fiui::LayoutResult responsive_second = layout_system.arrange(
        *responsive_root.expose_impl(), fiui::LayoutConstraints{1600.0f, 720.0f});
    FIUI_TEST_ASSERT(responsive_second.arranged_nodes == 3);
    FIUI_TEST_ASSERT(fixed_nav.bounds().width == first_nav_width);
    FIUI_TEST_ASSERT(fixed_nav.bounds().width == 220.0f);
    FIUI_TEST_ASSERT(flexible_content.bounds().width > first_content_width);

    TestRow auto_row;
    auto_row.debug_id("auto_row").size(500, 80).gap(10.0f);
    fiui::Button auto_left("Auto");
    auto_left.debug_id("auto_left");
    fiui::Button auto_right("Auto right");
    auto_right.debug_id("auto_right");
    FIUI_TEST_ASSERT(auto_row.add(auto_left));
    FIUI_TEST_ASSERT(auto_row.add(auto_right));
    const fiui::LayoutResult auto_row_first =
        layout_system.arrange(*auto_row.expose_impl(), fiui::LayoutConstraints{500.0f, 80.0f});
    FIUI_TEST_ASSERT(auto_row_first.arranged_nodes == 3);
    const float auto_left_first_width = auto_left.bounds().width;
    const float auto_right_first_width = auto_right.bounds().width;
    auto_row.size(900, 80);
    const fiui::LayoutResult auto_row_second =
        layout_system.arrange(*auto_row.expose_impl(), fiui::LayoutConstraints{900.0f, 80.0f});
    FIUI_TEST_ASSERT(auto_row_second.arranged_nodes == 3);
    FIUI_TEST_ASSERT(auto_left.bounds().width == auto_left_first_width);
    FIUI_TEST_ASSERT(auto_right.bounds().width == auto_right_first_width);

    TestRow zero_size_row;
    zero_size_row.debug_id("zero_size_row").size(400, 60);
    fiui::Button zero_size_button("Zero");
    zero_size_button.debug_id("zero_size_button").size(0, 0);
    fiui::Button fill_width_button("Fill");
    fill_width_button.debug_id("fill_width_button").fill_width();
    FIUI_TEST_ASSERT(zero_size_row.add(zero_size_button));
    FIUI_TEST_ASSERT(zero_size_row.add(fill_width_button));
    const fiui::LayoutResult zero_size_row_layout = layout_system.arrange(
        *zero_size_row.expose_impl(), fiui::LayoutConstraints{400.0f, 60.0f});
    FIUI_TEST_ASSERT(zero_size_row_layout.arranged_nodes == 3);
    FIUI_TEST_ASSERT(zero_size_button.bounds().width < fill_width_button.bounds().width);
    FIUI_TEST_ASSERT(zero_size_button.bounds().width == 72.0f);

    TestColumn cross_axis_column;
    cross_axis_column.debug_id("cross_axis_column").size(320, 120).padding(10.0f);
    fiui::Button column_auto_width("Column auto width");
    column_auto_width.debug_id("column_auto_width");
    fiui::Button column_fixed_width("Column fixed");
    column_fixed_width.debug_id("column_fixed_width").size(96, 0);
    FIUI_TEST_ASSERT(cross_axis_column.add(column_auto_width));
    FIUI_TEST_ASSERT(cross_axis_column.add(column_fixed_width));
    const fiui::LayoutResult cross_axis_column_layout = layout_system.arrange(
        *cross_axis_column.expose_impl(), fiui::LayoutConstraints{320.0f, 120.0f});
    FIUI_TEST_ASSERT(cross_axis_column_layout.arranged_nodes == 3);
    FIUI_TEST_ASSERT(column_auto_width.bounds().width == 300.0f);
    FIUI_TEST_ASSERT(column_fixed_width.bounds().width == 96.0f);

    TestRow cross_axis_row;
    cross_axis_row.debug_id("cross_axis_row").size(320, 90).padding(10.0f);
    fiui::Button row_auto_height("Row auto height");
    row_auto_height.debug_id("row_auto_height");
    fiui::Button row_fixed_height("Row fixed");
    row_fixed_height.debug_id("row_fixed_height").size(80, 24);
    FIUI_TEST_ASSERT(cross_axis_row.add(row_auto_height));
    FIUI_TEST_ASSERT(cross_axis_row.add(row_fixed_height));
    const fiui::LayoutResult cross_axis_row_layout = layout_system.arrange(
        *cross_axis_row.expose_impl(), fiui::LayoutConstraints{320.0f, 90.0f});
    FIUI_TEST_ASSERT(cross_axis_row_layout.arranged_nodes == 3);
    FIUI_TEST_ASSERT(row_auto_height.bounds().height == 70.0f);
    FIUI_TEST_ASSERT(row_fixed_height.bounds().height == 24.0f);

    TestColumn toolbar_root;
    toolbar_root.debug_id("toolbar_root").size(420, 120);
    TestToolbar toolbar;
    toolbar.debug_id("toolbar");
    fiui::Button toolbar_apply("Apply");
    toolbar_apply.debug_id("toolbar_apply");
    fiui::Button toolbar_theme("Theme");
    toolbar_theme.debug_id("toolbar_theme");
    fiui::Select toolbar_select;
    toolbar_select.debug_id("toolbar_select");
    toolbar_select.size(120, 0);
    toolbar_select.placeholder("Mode");
    toolbar_select.add_option("Basic");
    toolbar_select.add_option("Verbose");
    int toolbar_click_count = 0;
    toolbar_apply.on_click(increment_int_callback, &toolbar_click_count);
    FIUI_TEST_ASSERT(toolbar.add(toolbar_apply));
    FIUI_TEST_ASSERT(toolbar.add(toolbar_theme));
    FIUI_TEST_ASSERT(toolbar.add(toolbar_select));
    FIUI_TEST_ASSERT(toolbar_root.add(toolbar));
    const fiui::LayoutResult toolbar_layout =
        layout_system.arrange(*toolbar_root.expose_impl(), fiui::LayoutConstraints{420.0f, 120.0f});
    FIUI_TEST_ASSERT(toolbar.kind() == fiui::WidgetKind::Toolbar);
    FIUI_TEST_ASSERT(toolbar_layout.arranged_nodes == 5);
    FIUI_TEST_ASSERT(toolbar.bounds().height == 56.0f);
    FIUI_TEST_ASSERT(toolbar_apply.bounds().x == toolbar.bounds().x + 8.0f);
    FIUI_TEST_ASSERT(toolbar_apply.bounds().height == 40.0f);
    FIUI_TEST_ASSERT(toolbar_theme.bounds().x > toolbar_apply.bounds().x);
    FIUI_TEST_ASSERT(toolbar_select.bounds().x > toolbar_theme.bounds().x);
    FIUI_TEST_ASSERT(toolbar_select.bounds().width == 120.0f);
    FIUI_TEST_ASSERT(toolbar_apply.click());
    FIUI_TEST_ASSERT(toolbar_click_count == 1);
    fiui::RenderSystem toolbar_render_system;
    const fiui::RenderFrameResult toolbar_render =
        toolbar_render_system.build_frame(*toolbar_root.expose_impl());
    const fiui::DisplayCommand* toolbar_surface =
        find_rect_command(toolbar_render, toolbar.object_id());
    FIUI_TEST_ASSERT(toolbar_surface != nullptr);
    FIUI_TEST_ASSERT(toolbar_surface->widget_kind == fiui::WidgetKind::Toolbar);
    FIUI_TEST_ASSERT(toolbar_surface->style.border_width == 1.0f);

    TestRow overflow_clip_root;
    overflow_clip_root.debug_id("overflow_clip_root").size(100, 50);
    fiui::Button overflow_clip_button("Overflow child");
    overflow_clip_button.debug_id("overflow_clip_button").size(180, 36);
    FIUI_TEST_ASSERT(overflow_clip_root.add(overflow_clip_button));
    const fiui::LayoutResult overflow_clip_layout = layout_system.arrange(
        *overflow_clip_root.expose_impl(), fiui::LayoutConstraints{100.0f, 50.0f});
    FIUI_TEST_ASSERT(overflow_clip_layout.arranged_nodes == 2);
    fiui::RenderSystem overflow_render_system;
    const fiui::RenderFrameResult overflow_clip_render =
        overflow_render_system.build_frame(*overflow_clip_root.expose_impl());
    FIUI_TEST_ASSERT(overflow_clip_render.backend.clip_command_count >= 1);
    FIUI_TEST_ASSERT(overflow_clip_render.backend.clip_end_command_count >= 1);
    const fiui::RuntimeEventRouteProbe clipped_probe =
        fiui::default_runtime_route_pointer_event(*overflow_clip_root.expose_impl(),
                                                  fiui::EventType::PointerMove, 150.0f, 10.0f);
    FIUI_TEST_ASSERT(!clipped_probe.hit);
    overflow_clip_root.overflow(fiui::OverflowPolicy::Visible);
    const fiui::RenderFrameResult overflow_visible_render =
        overflow_render_system.build_frame(*overflow_clip_root.expose_impl());
    FIUI_TEST_ASSERT(overflow_visible_render.backend.clip_command_count == 0);
    FIUI_TEST_ASSERT(overflow_visible_render.backend.clip_end_command_count == 0);
    const fiui::RuntimeEventRouteProbe visible_probe =
        fiui::default_runtime_route_pointer_event(*overflow_clip_root.expose_impl(),
                                                  fiui::EventType::PointerMove, 150.0f, 10.0f);
    FIUI_TEST_ASSERT(visible_probe.hit);
    FIUI_TEST_ASSERT(visible_probe.target_object_id == overflow_clip_button.object_id());

    TestRow contained_clip_root;
    contained_clip_root.debug_id("contained_clip_root").size(160, 50);
    fiui::Button contained_clip_button("Contained child");
    contained_clip_button.debug_id("contained_clip_button").size(80, 36);
    FIUI_TEST_ASSERT(contained_clip_root.add(contained_clip_button));
    const fiui::LayoutResult contained_clip_layout = layout_system.arrange(
        *contained_clip_root.expose_impl(), fiui::LayoutConstraints{160.0f, 50.0f});
    FIUI_TEST_ASSERT(contained_clip_layout.arranged_nodes == 2);
    const fiui::RenderFrameResult contained_clip_render =
        overflow_render_system.build_frame(*contained_clip_root.expose_impl());
    FIUI_TEST_ASSERT(contained_clip_render.backend.clip_command_count == 0);
    FIUI_TEST_ASSERT(contained_clip_render.backend.clip_end_command_count == 0);

    contained_clip_root.overflow(fiui::OverflowPolicy::Clip);
    const fiui::RenderFrameResult explicit_clip_render =
        overflow_render_system.build_frame(*contained_clip_root.expose_impl());
    FIUI_TEST_ASSERT(explicit_clip_render.backend.clip_command_count >= 1);
    FIUI_TEST_ASSERT(explicit_clip_render.backend.clip_end_command_count >= 1);

    TestRow weighted_row;
    weighted_row.debug_id("weighted_row").size(300, 100);
    TestColumn weighted_left;
    weighted_left.debug_id("weighted_left").flex(1.0f);
    TestColumn weighted_right;
    weighted_right.debug_id("weighted_right").flex(2.0f);
    FIUI_TEST_ASSERT(weighted_row.add(weighted_left));
    FIUI_TEST_ASSERT(weighted_row.add(weighted_right));
    const fiui::LayoutResult weighted_row_layout =
        layout_system.arrange(*weighted_row.expose_impl(), fiui::LayoutConstraints{300.0f, 100.0f});
    FIUI_TEST_ASSERT(weighted_row_layout.arranged_nodes == 3);
    FIUI_TEST_ASSERT(weighted_left.bounds().width == 100.0f);
    FIUI_TEST_ASSERT(weighted_right.bounds().width == 200.0f);

    TestColumn vertical_root;
    vertical_root.debug_id("vertical_root").size(300, 500).padding(10.0f).gap(5.0f);
    fiui::Text vertical_header("Header");
    vertical_header.debug_id("vertical_header");
    TestColumn vertical_flex;
    vertical_flex.debug_id("vertical_flex").flex();
    fiui::Button vertical_footer("Footer");
    vertical_footer.debug_id("vertical_footer");
    FIUI_TEST_ASSERT(vertical_root.add(vertical_header));
    FIUI_TEST_ASSERT(vertical_root.add(vertical_flex));
    FIUI_TEST_ASSERT(vertical_root.add(vertical_footer));
    const fiui::LayoutResult vertical_first = layout_system.arrange(
        *vertical_root.expose_impl(), fiui::LayoutConstraints{300.0f, 500.0f});
    FIUI_TEST_ASSERT(vertical_first.arranged_nodes == 4);
    const float first_flex_height = vertical_flex.bounds().height;
    vertical_root.size(300, 800);
    const fiui::LayoutResult vertical_second = layout_system.arrange(
        *vertical_root.expose_impl(), fiui::LayoutConstraints{300.0f, 800.0f});
    FIUI_TEST_ASSERT(vertical_second.arranged_nodes == 4);
    FIUI_TEST_ASSERT(vertical_header.bounds().height == 28.0f);
    FIUI_TEST_ASSERT(vertical_footer.bounds().height == 40.0f);
    FIUI_TEST_ASSERT(vertical_flex.bounds().height > first_flex_height);

    TestColumn weighted_column;
    weighted_column.debug_id("weighted_column").size(120, 300);
    TestColumn weighted_top;
    weighted_top.debug_id("weighted_top").flex(1.0f);
    TestColumn weighted_bottom;
    weighted_bottom.debug_id("weighted_bottom").flex(2.0f);
    FIUI_TEST_ASSERT(weighted_column.add(weighted_top));
    FIUI_TEST_ASSERT(weighted_column.add(weighted_bottom));
    const fiui::LayoutResult weighted_column_layout = layout_system.arrange(
        *weighted_column.expose_impl(), fiui::LayoutConstraints{120.0f, 300.0f});
    FIUI_TEST_ASSERT(weighted_column_layout.arranged_nodes == 3);
    FIUI_TEST_ASSERT(weighted_top.bounds().height == 100.0f);
    FIUI_TEST_ASSERT(weighted_bottom.bounds().height == 200.0f);

    TestWindow fill_window;
    fill_window.debug_id("fill_window").size(640, 480);
    TestRow fill_shell;
    fill_shell.debug_id("fill_shell");
    FIUI_TEST_ASSERT(fill_window.content(fill_shell).child_count() == 1);
    const fiui::LayoutResult fill_window_layout =
        layout_system.arrange(*fill_window.expose_impl(), fiui::LayoutConstraints{640.0f, 480.0f});
    FIUI_TEST_ASSERT(fill_window_layout.arranged_nodes == 2);
    FIUI_TEST_ASSERT(fill_shell.bounds().width == 640.0f);
    FIUI_TEST_ASSERT(fill_shell.bounds().height == 480.0f);

    TestScrollView fill_scroll;
    fill_scroll.debug_id("fill_scroll").size(300, 200);
    TestColumn fill_scroll_child;
    fill_scroll_child.debug_id("fill_scroll_child");
    FIUI_TEST_ASSERT(fill_scroll.add(fill_scroll_child));
    const fiui::LayoutResult fill_scroll_layout =
        layout_system.arrange(*fill_scroll.expose_impl(), fiui::LayoutConstraints{300.0f, 200.0f});
    FIUI_TEST_ASSERT(fill_scroll_layout.arranged_nodes == 2);
    FIUI_TEST_ASSERT(fill_scroll_child.bounds().width == 300.0f);
    FIUI_TEST_ASSERT(fill_scroll_child.bounds().height == 200.0f);

    fiui::default_runtime_clear_event_targets();
    const fiui::RuntimeEventRouteProbe move_probe =
        fiui::default_runtime_route_pointer_event(*root.expose_impl(),
                                                  fiui::EventType::PointerMove, 10.0f, 10.0f);
    FIUI_TEST_ASSERT(move_probe.hit);
    FIUI_TEST_ASSERT(move_probe.handled);
    FIUI_TEST_ASSERT(move_probe.target_object_id == button.object_id());
    FIUI_TEST_ASSERT(move_probe.target_generation == button.generation());
    FIUI_TEST_ASSERT(move_probe.route_count == 2);
    FIUI_TEST_ASSERT(move_probe.hover_target == button.object_id());
    FIUI_TEST_ASSERT(move_probe.focus_target == 0);
    FIUI_TEST_ASSERT(move_probe.capture_target == 0);

    const fiui::RuntimeEventRouteProbe down_probe =
        fiui::default_runtime_route_pointer_event(*root.expose_impl(),
                                                  fiui::EventType::PointerDown, 10.0f, 10.0f);
    FIUI_TEST_ASSERT(down_probe.target_object_id == button.object_id());
    FIUI_TEST_ASSERT(down_probe.focus_target == button.object_id());
    FIUI_TEST_ASSERT(down_probe.capture_target == button.object_id());
    FIUI_TEST_ASSERT(down_probe.hover_target == button.object_id());

    const fiui::RuntimeEventRouteProbe up_probe =
        fiui::default_runtime_route_pointer_event(*root.expose_impl(),
                                                  fiui::EventType::PointerUp, 10.0f, 10.0f);
    FIUI_TEST_ASSERT(up_probe.target_object_id == button.object_id());
    FIUI_TEST_ASSERT(up_probe.capture_target == 0);
    FIUI_TEST_ASSERT(up_probe.hover_target == button.object_id());

    const fiui::RuntimeEventRouteProbe miss_probe =
        fiui::default_runtime_route_pointer_event(*root.expose_impl(),
                                                  fiui::EventType::PointerMove, -10.0f, -10.0f);
    FIUI_TEST_ASSERT(!miss_probe.hit);
    FIUI_TEST_ASSERT(!miss_probe.handled);

    fiui::DirtyTracker dirty_tracker;
    fiui::DirtyThresholds thresholds;
    thresholds.max_dirty_rects = 1;
    fiui::DirtyPlan dirty_plan = dirty_tracker.plan(*root.expose_impl(), thresholds);
    FIUI_TEST_ASSERT(dirty_plan.original_rect_count >= 1);
    FIUI_TEST_ASSERT(dirty_plan.full_repaint);
    FIUI_TEST_ASSERT(dirty_plan.fallback_reason != nullptr);
    FIUI_TEST_ASSERT(dirty_plan.input_dirty_count >= 1);
    FIUI_TEST_ASSERT(dirty_plan.paint_dirty_count >= 1);
    dirty_tracker.clear(*root.expose_impl());
    FIUI_TEST_ASSERT(root.dirty_reason() == fiui::DirtyReason::None);

    TestColumn clipped_root;
    clipped_root.debug_id("clipped_root").size(50, 50);
    fiui::Button clipped_button("Clip");
    clipped_button.debug_id("clipped_button").size(120, 36);
    FIUI_TEST_ASSERT(clipped_root.add(clipped_button));
    const fiui::LayoutResult clipped_layout =
        layout_system.arrange(*clipped_root.expose_impl(), fiui::LayoutConstraints{50.0f, 50.0f});
    FIUI_TEST_ASSERT(clipped_layout.arranged_nodes == 2);
    clipped_button.mark_dirty(fiui::DirtyReason::Paint);
    fiui::DirtyPlan clipped_plan = dirty_tracker.plan(*clipped_root.expose_impl(), thresholds);
    FIUI_TEST_ASSERT(!clipped_plan.rects.empty());
    FIUI_TEST_ASSERT(clipped_plan.rects.back().clipped_by_parent);
    FIUI_TEST_ASSERT(clipped_plan.rects.back().rect.width <= 50.0f);

    TestColumn render_root;
    render_root.debug_id("render_root").size(240, 160);
    fiui::Text render_text("Title");
    render_text.debug_id("title");
    fiui::Button render_button("Apply");
    render_button.debug_id("apply");
    fiui::Image render_image("render.png");
    render_image.debug_id("preview");
    FIUI_TEST_ASSERT(render_root.add(render_text));
    FIUI_TEST_ASSERT(render_root.add(render_button));
    FIUI_TEST_ASSERT(render_root.add(render_image));
    const fiui::LayoutResult render_layout =
        layout_system.arrange(*render_root.expose_impl(), fiui::LayoutConstraints{240.0f, 160.0f});
    FIUI_TEST_ASSERT(render_layout.arranged_nodes == 4);
    fiui::RenderSystem render_system;
    const fiui::RenderFrameResult render_result =
        render_system.build_frame(*render_root.expose_impl());
    FIUI_TEST_ASSERT(render_result.render_tree.nodes.size() == 4);
    FIUI_TEST_ASSERT(render_result.layer_tree.nodes.size() > render_result.render_tree.nodes.size());
    FIUI_TEST_ASSERT(has_layer_kind(render_result, fiui::LayerKind::Rect));
    FIUI_TEST_ASSERT(has_layer_kind(render_result, fiui::LayerKind::RoundedRect));
    FIUI_TEST_ASSERT(has_layer_kind(render_result, fiui::LayerKind::Text));
    FIUI_TEST_ASSERT(has_layer_kind(render_result, fiui::LayerKind::Image));
    FIUI_TEST_ASSERT(has_layer_kind(render_result, fiui::LayerKind::Shadow));
    FIUI_TEST_ASSERT(has_layer_for_object(render_result, fiui::LayerKind::RoundedRect,
                                render_button.object_id()));
    FIUI_TEST_ASSERT(has_layer_for_object(render_result, fiui::LayerKind::Text, render_text.object_id()));
    FIUI_TEST_ASSERT(has_layer_for_object(render_result, fiui::LayerKind::Image, render_image.object_id()));
    FIUI_TEST_ASSERT(has_text_layer_metrics(render_result, render_text.object_id()));
    FIUI_TEST_ASSERT(has_text_layer_metrics(render_result, render_button.object_id()));
    FIUI_TEST_ASSERT(has_image_layer_metadata(render_result, render_image.object_id()));
    FIUI_TEST_ASSERT(all_command_layers_are_linked(render_result));
    FIUI_TEST_ASSERT(render_result.backend.command_count ==
           static_cast<std::uint32_t>(render_result.display_list.commands.size()));
    FIUI_TEST_ASSERT(render_result.backend.batch_count == render_result.batches.size());
    FIUI_TEST_ASSERT(render_result.backend.rect_command_count > 0);
    FIUI_TEST_ASSERT(render_result.backend.text_command_count >= 2);
    FIUI_TEST_ASSERT(render_result.backend.image_command_count == 1);
    bool found_text_command = false;
    bool found_image_command = false;
    bool found_text_resource = false;
    bool found_image_resource = false;
    for (const fiui::DisplayCommand& command : render_result.display_list.commands) {
        found_text_command =
            found_text_command || command.kind == fiui::DisplayCommandKind::Text;
        found_image_command =
            found_image_command || command.kind == fiui::DisplayCommandKind::Image;
        if (command.kind == fiui::DisplayCommandKind::Text) {
            found_text_resource =
                command.resource.id != 0 &&
                command.resource.kind == fiui::ResourceKind::TextLayout &&
                command.resource.cache_state == fiui::ResourceCacheState::Cached &&
                command.resource.owner_object_id == command.object_id &&
                command.resource.text_metrics.valid &&
                command.resource.text_metrics.width > 0.0f &&
                command.resource.text_metrics.height > 0.0f;
        }
        if (command.kind == fiui::DisplayCommandKind::Image) {
            found_image_resource =
                command.resource.id != 0 && command.resource.kind == fiui::ResourceKind::Image &&
                command.resource.owner_object_id == command.object_id &&
                command.resource.key == "render.png" && command.resource.image_metadata.valid &&
                command.resource.image_metadata.width > 0 &&
                command.resource.image_metadata.height > 0 &&
                command.resource.image_metadata.pixel_format == "fallback";
        }
    }
    FIUI_TEST_ASSERT(found_text_command);
    FIUI_TEST_ASSERT(found_image_command);
    FIUI_TEST_ASSERT(found_text_resource);
    FIUI_TEST_ASSERT(found_image_resource);
    FIUI_TEST_ASSERT(!has_layer_path(render_result, render_image.object_id(), "/placeholder"));
    const fiui::DisplayCommand* button_text_command =
        find_text_command(render_result, render_button.object_id());
    FIUI_TEST_ASSERT(button_text_command != nullptr);
    FIUI_TEST_ASSERT(std::strcmp(button_text_command->style.text_align, "center") == 0);
    FIUI_TEST_ASSERT(std::strcmp(button_text_command->style.paragraph_align, "center") == 0);
    FIUI_TEST_ASSERT(std::strcmp(button_text_command->style.overflow, "ellipsis") == 0);
    FIUI_TEST_ASSERT(button_text_command->style.text_padding_x > 0.0f);
    FIUI_TEST_ASSERT(button_text_command->bounds.x > render_button.bounds().x);
    FIUI_TEST_ASSERT(button_text_command->bounds.width < render_button.bounds().width);

    TestButton complex_button;
    complex_button.debug_id("complex_button");
    complex_button.size(220.0f, 64.0f);
    complex_button.normal_image("complex-normal.png")
        .hover_image("complex-hover.png")
        .pressed_image("complex-pressed.png")
        .image_fit(fiui::ImageFit::Cover)
        .background(fiui::Color{10, 20, 30, 255})
        .hover_background(fiui::Color{20, 40, 60, 255})
        .pressed_background(fiui::Color{4, 8, 12, 255})
        .radius(18.0f)
        .text_padding(11.0f);
    TestRow complex_content;
    complex_content.debug_id("complex_content").gap(4.0f);
    fiui::Image complex_icon("complex-icon.png");
    complex_icon.debug_id("complex_icon").size(18.0f, 18.0f);
    fiui::Text complex_label("Complex");
    complex_label.debug_id("complex_label");
    FIUI_TEST_ASSERT(complex_content.add(complex_icon));
    FIUI_TEST_ASSERT(complex_content.add(complex_label));
    complex_button.content(complex_content);
    TestColumn complex_root;
    complex_root.debug_id("complex_root").size(260.0f, 96.0f);
    FIUI_TEST_ASSERT(complex_root.add(complex_button));
    const fiui::LayoutResult complex_layout = layout_system.arrange(
        *complex_root.expose_impl(), fiui::LayoutConstraints{260.0f, 96.0f});
    FIUI_TEST_ASSERT(complex_layout.arranged_nodes == 5);
    FIUI_TEST_ASSERT(complex_content.bounds().x > complex_button.bounds().x);
    FIUI_TEST_ASSERT(find_text_command(render_system.build_frame(*complex_root.expose_impl()),
                             complex_button.object_id()) == nullptr);
    const fiui::RuntimeEventRouteProbe complex_hover_probe =
        fiui::default_runtime_route_pointer_event(*complex_root.expose_impl(),
                                                  fiui::EventType::PointerMove,
                                                  complex_icon.bounds().x + 1.0f,
                                                  complex_icon.bounds().y + 1.0f);
    FIUI_TEST_ASSERT(complex_hover_probe.hit);
    FIUI_TEST_ASSERT(complex_hover_probe.target_object_id == complex_button.object_id());
    const fiui::RenderFrameResult complex_hover_render =
        render_system.build_frame(*complex_root.expose_impl());
    const fiui::DisplayCommand* complex_rect =
        find_rect_command(complex_hover_render, complex_button.object_id());
    FIUI_TEST_ASSERT(complex_rect != nullptr);
    FIUI_TEST_ASSERT(complex_rect->style.fill.r == 20);
    FIUI_TEST_ASSERT(complex_rect->style.fill.g == 40);
    FIUI_TEST_ASSERT(complex_rect->style.fill.b == 60);
    FIUI_TEST_ASSERT(complex_rect->style.radius == 18.0f);
    const fiui::DisplayCommand* complex_state_image = find_image_command(
        complex_hover_render, complex_button.object_id());
    FIUI_TEST_ASSERT(complex_state_image != nullptr);
    FIUI_TEST_ASSERT(complex_state_image->resource_key == "complex-hover.png");
    FIUI_TEST_ASSERT(complex_state_image->image_fit == fiui::ImageFit::Cover);
    FIUI_TEST_ASSERT(find_image_command(complex_hover_render, complex_icon.object_id()) != nullptr);
    const fiui::DisplayCommand* complex_label_text =
        find_text_command(complex_hover_render, complex_label.object_id());
    FIUI_TEST_ASSERT(complex_label_text != nullptr);
    FIUI_TEST_ASSERT(complex_label_text->resource.text_metrics.valid);
    FIUI_TEST_ASSERT(complex_label_text->bounds.width + 1.0f >=
                     complex_label_text->resource.text_metrics.width);
    int complex_click_count = 0;
    complex_button.on_click(increment_int_callback, &complex_click_count);
    fiui::default_runtime_bind_platform_root(*complex_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown,
                                                        complex_icon.bounds().x + 2.0f,
                                                        complex_icon.bounds().y + 2.0f);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == complex_button.object_id());
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == complex_button.object_id());
    const fiui::RenderFrameResult complex_pressed_render =
        render_system.build_frame(*complex_root.expose_impl());
    const fiui::DisplayCommand* complex_pressed_image =
        find_image_command(complex_pressed_render, complex_button.object_id());
    FIUI_TEST_ASSERT(complex_pressed_image != nullptr);
    FIUI_TEST_ASSERT(complex_pressed_image->resource_key == "complex-pressed.png");
    FIUI_TEST_ASSERT(find_image_command(complex_pressed_render, complex_icon.object_id()) != nullptr);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp,
                                                        complex_icon.bounds().x + 2.0f,
                                                        complex_icon.bounds().y + 2.0f);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == 0);
    FIUI_TEST_ASSERT(complex_click_count == 1);
    (void)fiui::default_runtime_route_pointer_event(*complex_root.expose_impl(),
                                                    fiui::EventType::PointerMove,
                                                    -100.0f,
                                                    -100.0f);

    TestColumn check_box_root;
    check_box_root.debug_id("check_box_root").size(260.0f, 120.0f).gap(6.0f);
    TestCheckBox check_box("Enable diagnostics");
    check_box.debug_id("check_box");
    TestCheckBox second_check_box("Fast mode");
    second_check_box.debug_id("second_check_box");
    FIUI_TEST_ASSERT(!check_box.checked());
    check_box.checked(true);
    FIUI_TEST_ASSERT(check_box.checked());
    int check_box_change_count = 0;
    check_box.on_change(increment_int_callback, &check_box_change_count);
    FIUI_TEST_ASSERT(check_box.toggle());
    FIUI_TEST_ASSERT(!check_box.checked());
    FIUI_TEST_ASSERT(check_box_change_count == 1);
    FIUI_TEST_ASSERT(check_box_root.add(check_box));
    FIUI_TEST_ASSERT(check_box_root.add(second_check_box));
    const fiui::LayoutResult check_box_layout = layout_system.arrange(
        *check_box_root.expose_impl(), fiui::LayoutConstraints{260.0f, 120.0f});
    FIUI_TEST_ASSERT(check_box_layout.arranged_nodes == 3);
    FIUI_TEST_ASSERT(check_box.bounds().height >= 40.0f);
    const fiui::RenderFrameResult unchecked_check_box_render =
        render_system.build_frame(*check_box_root.expose_impl());
    const fiui::DisplayCommand* check_box_text =
        find_text_command(unchecked_check_box_render, check_box.object_id());
    const fiui::DisplayCommand* check_box_box =
        find_rect_command_with_path(unchecked_check_box_render, check_box.object_id(), "/box");
    FIUI_TEST_ASSERT(check_box_text != nullptr);
    FIUI_TEST_ASSERT(check_box_text->text == "Enable diagnostics");
    FIUI_TEST_ASSERT(std::strcmp(check_box_text->style.text_align, "leading") == 0);
    FIUI_TEST_ASSERT(check_box_text->bounds.x > check_box.bounds().x);
    FIUI_TEST_ASSERT(check_box_box != nullptr);
    FIUI_TEST_ASSERT(find_rect_command_with_path(unchecked_check_box_render, check_box.object_id(),
                                       "/mark") == nullptr);

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*check_box_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown,
                                                        check_box.bounds().x + 8.0f,
                                                        check_box.bounds().y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp,
                                                        check_box.bounds().x + 8.0f,
                                                        check_box.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(check_box.checked());
    FIUI_TEST_ASSERT(check_box_change_count == 2);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == check_box.object_id());
    const fiui::RenderFrameResult checked_check_box_render =
        render_system.build_frame(*check_box_root.expose_impl());
    const fiui::DisplayCommand* check_box_mark =
        find_rect_command_with_path(checked_check_box_render, check_box.object_id(), "/mark");
    FIUI_TEST_ASSERT(check_box_mark != nullptr);
    FIUI_TEST_ASSERT(check_box_mark->style.fill.a == 255);

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*check_box_root.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == check_box.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x20, 0);
    FIUI_TEST_ASSERT(!check_box.checked());
    FIUI_TEST_ASSERT(check_box_change_count == 3);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == second_check_box.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x0d, 0);
    FIUI_TEST_ASSERT(second_check_box.checked());
    fiui::default_runtime_clear_event_targets();

    TestColumn radio_root;
    radio_root.debug_id("radio_root").size(260.0f, 140.0f).gap(6.0f);
    TestRadioButton radio_auto("Auto");
    radio_auto.debug_id("radio_auto");
    radio_auto.group("mode").checked(true);
    TestRadioButton radio_manual("Manual");
    radio_manual.debug_id("radio_manual");
    radio_manual.group("mode");
    TestRadioButton radio_custom("Custom");
    radio_custom.debug_id("radio_custom");
    radio_custom.group("mode");
    int radio_change_count = 0;
    radio_manual.on_change(increment_int_callback, &radio_change_count);
    FIUI_TEST_ASSERT(radio_auto.checked());
    FIUI_TEST_ASSERT(!radio_manual.checked());
    FIUI_TEST_ASSERT(radio_root.add(radio_auto));
    FIUI_TEST_ASSERT(radio_root.add(radio_manual));
    FIUI_TEST_ASSERT(radio_root.add(radio_custom));
    const fiui::LayoutResult radio_layout =
        layout_system.arrange(*radio_root.expose_impl(), fiui::LayoutConstraints{260.0f, 140.0f});
    FIUI_TEST_ASSERT(radio_layout.arranged_nodes == 4);
    const fiui::RenderFrameResult radio_initial_render =
        render_system.build_frame(*radio_root.expose_impl());
    FIUI_TEST_ASSERT(find_rect_command_with_path(radio_initial_render, radio_auto.object_id(), "/outer") !=
           nullptr);
    FIUI_TEST_ASSERT(find_rect_command_with_path(radio_initial_render, radio_auto.object_id(), "/dot") !=
           nullptr);
    FIUI_TEST_ASSERT(find_rect_command_with_path(radio_initial_render, radio_manual.object_id(), "/dot") ==
           nullptr);
    const fiui::DisplayCommand* radio_manual_text =
        find_text_command(radio_initial_render, radio_manual.object_id());
    FIUI_TEST_ASSERT(radio_manual_text != nullptr);
    FIUI_TEST_ASSERT(radio_manual_text->text == "Manual");
    FIUI_TEST_ASSERT(radio_manual_text->bounds.x > radio_manual.bounds().x);

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*radio_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown,
                                                        radio_manual.bounds().x + 8.0f,
                                                        radio_manual.bounds().y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp,
                                                        radio_manual.bounds().x + 8.0f,
                                                        radio_manual.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(!radio_auto.checked());
    FIUI_TEST_ASSERT(radio_manual.checked());
    FIUI_TEST_ASSERT(!radio_custom.checked());
    FIUI_TEST_ASSERT(radio_change_count == 1);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == radio_manual.object_id());

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*radio_root.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == radio_auto.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == radio_manual.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == radio_custom.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x20, 0);
    FIUI_TEST_ASSERT(!radio_auto.checked());
    FIUI_TEST_ASSERT(!radio_manual.checked());
    FIUI_TEST_ASSERT(radio_custom.checked());
    const fiui::RenderFrameResult radio_custom_render =
        render_system.build_frame(*radio_root.expose_impl());
    FIUI_TEST_ASSERT(find_rect_command_with_path(radio_custom_render, radio_custom.object_id(), "/dot") !=
           nullptr);
    FIUI_TEST_ASSERT(find_rect_command_with_path(radio_custom_render, radio_manual.object_id(), "/dot") ==
           nullptr);
    fiui::default_runtime_clear_event_targets();

    TestColumn switch_root;
    switch_root.debug_id("switch_root").size(260.0f, 120.0f).gap(6.0f);
    TestSwitch switch_control("Live preview");
    switch_control.debug_id("switch_control");
    TestSwitch second_switch("Compact mode");
    second_switch.debug_id("second_switch");
    FIUI_TEST_ASSERT(!switch_control.checked());
    switch_control.checked(true);
    FIUI_TEST_ASSERT(switch_control.checked());
    int switch_change_count = 0;
    switch_control.on_change(increment_int_callback, &switch_change_count);
    FIUI_TEST_ASSERT(switch_control.toggle());
    FIUI_TEST_ASSERT(!switch_control.checked());
    FIUI_TEST_ASSERT(switch_change_count == 1);
    FIUI_TEST_ASSERT(switch_root.add(switch_control));
    FIUI_TEST_ASSERT(switch_root.add(second_switch));
    const fiui::LayoutResult switch_layout =
        layout_system.arrange(*switch_root.expose_impl(), fiui::LayoutConstraints{260.0f, 120.0f});
    FIUI_TEST_ASSERT(switch_layout.arranged_nodes == 3);
    const fiui::RenderFrameResult switch_initial_render =
        render_system.build_frame(*switch_root.expose_impl());
    const fiui::DisplayCommand* switch_track =
        find_rect_command_with_path(switch_initial_render, switch_control.object_id(), "/track");
    const fiui::DisplayCommand* switch_thumb =
        find_rect_command_with_path(switch_initial_render, switch_control.object_id(), "/thumb");
    const fiui::DisplayCommand* switch_text =
        find_text_command(switch_initial_render, switch_control.object_id());
    FIUI_TEST_ASSERT(switch_track != nullptr);
    FIUI_TEST_ASSERT(switch_thumb != nullptr);
    FIUI_TEST_ASSERT(switch_text != nullptr);
    FIUI_TEST_ASSERT(switch_text->text == "Live preview");
    FIUI_TEST_ASSERT(switch_text->bounds.x > switch_control.bounds().x + 40.0f);
    const float switch_thumb_off_x = switch_thumb->bounds.x;

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*switch_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown,
                                                        switch_control.bounds().x + 8.0f,
                                                        switch_control.bounds().y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp,
                                                        switch_control.bounds().x + 8.0f,
                                                        switch_control.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(switch_control.checked());
    FIUI_TEST_ASSERT(switch_change_count == 2);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == switch_control.object_id());
    const fiui::RenderFrameResult switch_checked_render =
        render_system.build_frame(*switch_root.expose_impl());
    const fiui::DisplayCommand* switch_checked_thumb =
        find_rect_command_with_path(switch_checked_render, switch_control.object_id(), "/thumb");
    FIUI_TEST_ASSERT(switch_checked_thumb != nullptr);
    FIUI_TEST_ASSERT(switch_checked_thumb->bounds.x > switch_thumb_off_x);

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*switch_root.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == switch_control.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x20, 0);
    FIUI_TEST_ASSERT(!switch_control.checked());
    FIUI_TEST_ASSERT(switch_change_count == 3);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == second_switch.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x0d, 0);
    FIUI_TEST_ASSERT(second_switch.checked());
    fiui::default_runtime_clear_event_targets();

    TestColumn slider_root;
    slider_root.debug_id("slider_root").size(240.0f, 90.0f);
    TestSlider slider;
    slider.debug_id("slider");
    slider.value(1.25f);
    FIUI_TEST_ASSERT(slider.value() == 1.0f);
    slider.value(-0.25f);
    FIUI_TEST_ASSERT(slider.value() == 0.0f);
    int slider_change_count = 0;
    slider.on_change(increment_int_callback, &slider_change_count);
    FIUI_TEST_ASSERT(slider_root.add(slider));
    const fiui::LayoutResult slider_layout =
        layout_system.arrange(*slider_root.expose_impl(), fiui::LayoutConstraints{240.0f, 90.0f});
    FIUI_TEST_ASSERT(slider_layout.arranged_nodes == 2);
    FIUI_TEST_ASSERT(slider.bounds().height >= 32.0f);
    const fiui::RenderFrameResult slider_initial_render =
        render_system.build_frame(*slider_root.expose_impl());
    const fiui::DisplayCommand* slider_track =
        find_rect_command_with_path(slider_initial_render, slider.object_id(), "/track");
    const fiui::DisplayCommand* slider_fill =
        find_rect_command_with_path(slider_initial_render, slider.object_id(), "/fill");
    const fiui::DisplayCommand* slider_thumb =
        find_rect_command_with_path(slider_initial_render, slider.object_id(), "/thumb");
    FIUI_TEST_ASSERT(slider_track != nullptr);
    FIUI_TEST_ASSERT(slider_fill != nullptr);
    FIUI_TEST_ASSERT(slider_thumb != nullptr);
    FIUI_TEST_ASSERT(slider_fill->bounds.width <= 1.0f);
    FIUI_TEST_ASSERT(slider_thumb->bounds.x <= slider_track->bounds.x + 1.0f);

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*slider_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown,
                                                        slider.bounds().x +
                                                            slider.bounds().width * 0.5f,
                                                        slider.bounds().y + 10.0f);
    FIUI_TEST_ASSERT(slider.value() > 0.45f && slider.value() < 0.55f);
    FIUI_TEST_ASSERT(slider_change_count == 1);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == slider.object_id());
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == slider.object_id());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerMove,
                                                        slider.bounds().x +
                                                            slider.bounds().width - 2.0f,
                                                        slider.bounds().y + 10.0f);
    FIUI_TEST_ASSERT(slider.value() > 0.95f);
    FIUI_TEST_ASSERT(slider_change_count == 2);
    const fiui::RenderFrameResult slider_drag_render =
        render_system.build_frame(*slider_root.expose_impl());
    const fiui::DisplayCommand* slider_drag_fill =
        find_rect_command_with_path(slider_drag_render, slider.object_id(), "/fill");
    const fiui::DisplayCommand* slider_drag_thumb =
        find_rect_command_with_path(slider_drag_render, slider.object_id(), "/thumb");
    FIUI_TEST_ASSERT(slider_drag_fill != nullptr);
    FIUI_TEST_ASSERT(slider_drag_thumb != nullptr);
    FIUI_TEST_ASSERT(slider_drag_fill->bounds.width > slider_track->bounds.width * 0.9f);
    FIUI_TEST_ASSERT(slider_drag_thumb->bounds.x > slider_thumb->bounds.x);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp,
                                                        slider.bounds().x +
                                                            slider.bounds().width - 2.0f,
                                                        slider.bounds().y + 10.0f);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == 0);

    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x24, 0);
    FIUI_TEST_ASSERT(slider.value() == 0.0f);
    FIUI_TEST_ASSERT(slider_change_count == 3);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x27, 0);
    FIUI_TEST_ASSERT(slider.value() > 0.0f);
    FIUI_TEST_ASSERT(slider_change_count == 4);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x23, 0);
    FIUI_TEST_ASSERT(slider.value() == 1.0f);
    FIUI_TEST_ASSERT(slider_change_count == 5);
    fiui::default_runtime_clear_event_targets();

    TestColumn select_root;
    select_root.debug_id("select_root").size(260.0f, 150.0f);
    TestSelect select;
    select.debug_id("select");
    select.placeholder("Choose mode")
        .add_option("Balanced")
        .add_option("Performance")
        .add_option("Diagnostics");
    FIUI_TEST_ASSERT(select.child_count() == 3);
    FIUI_TEST_ASSERT(select.selected_index() == 0);
    FIUI_TEST_ASSERT(std::string(select.selected_text()) == "Balanced");
    int select_change_count = 0;
    select.on_change(increment_int_callback, &select_change_count);
    FIUI_TEST_ASSERT(select_root.add(select));
    const fiui::LayoutResult select_closed_layout =
        layout_system.arrange(*select_root.expose_impl(), fiui::LayoutConstraints{260.0f, 150.0f});
    FIUI_TEST_ASSERT(select_closed_layout.arranged_nodes == 2);
    const fiui::RenderFrameResult select_closed_render =
        render_system.build_frame(*select_root.expose_impl());
    const fiui::DisplayCommand* select_text =
        find_text_command(select_closed_render, select.object_id());
    FIUI_TEST_ASSERT(select_text != nullptr);
    FIUI_TEST_ASSERT(select_text->text == "Balanced");
    FIUI_TEST_ASSERT(find_rect_command_with_path(select_closed_render, select.object_id(), "/popup") ==
           nullptr);

    FIUI_TEST_ASSERT(select.open());
    const fiui::LayoutResult select_open_layout =
        layout_system.arrange(*select_root.expose_impl(), fiui::LayoutConstraints{260.0f, 150.0f});
    FIUI_TEST_ASSERT(select_open_layout.arranged_nodes == 5);
    fiui::WidgetImpl* balanced_option = select.expose_impl()->node.children[0];
    fiui::WidgetImpl* performance_option = select.expose_impl()->node.children[1];
    FIUI_TEST_ASSERT(balanced_option != nullptr);
    FIUI_TEST_ASSERT(performance_option != nullptr);
    FIUI_TEST_ASSERT(performance_option->dirty.bounds.y >= select.bounds().y + select.bounds().height);
    const fiui::RenderFrameResult select_open_render =
        render_system.build_frame(*select_root.expose_impl());
    FIUI_TEST_ASSERT(find_rect_command_with_path(select_open_render, select.object_id(), "/popup") !=
           nullptr);
    FIUI_TEST_ASSERT(find_text_command(select_open_render, performance_option->object.object_id) != nullptr);
    const fiui::DisplayCommand* balanced_option_rect =
        find_rect_command(select_open_render, balanced_option->object.object_id);
    const fiui::DisplayCommand* performance_option_rect =
        find_rect_command(select_open_render, performance_option->object.object_id);
    FIUI_TEST_ASSERT(balanced_option_rect != nullptr);
    FIUI_TEST_ASSERT(performance_option_rect != nullptr);
    const fiui::Color select_accent = balanced_option_rect->style.fill;
    FIUI_TEST_ASSERT(select_accent.a == 255);
    FIUI_TEST_ASSERT(!same_color(performance_option_rect->style.fill, select_accent));

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*select_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, performance_option->dirty.bounds.x + 8.0f,
        performance_option->dirty.bounds.y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, performance_option->dirty.bounds.x + 8.0f,
        performance_option->dirty.bounds.y + 8.0f);
    FIUI_TEST_ASSERT(select.selected_index() == 1);
    FIUI_TEST_ASSERT(std::string(select.selected_text()) == "Performance");
    FIUI_TEST_ASSERT(select_change_count == 1);
    FIUI_TEST_ASSERT(!select.expose_impl()->properties.select_popup_open);

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*select_root.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == select.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x20, 0);
    FIUI_TEST_ASSERT(select.expose_impl()->properties.select_popup_open);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x28, 0);
    FIUI_TEST_ASSERT(select.selected_index() == 2);
    FIUI_TEST_ASSERT(std::string(select.selected_text()) == "Diagnostics");
    FIUI_TEST_ASSERT(select_change_count == 2);
    fiui::WidgetImpl* diagnostics_option = select.expose_impl()->node.children[2];
    FIUI_TEST_ASSERT(diagnostics_option != nullptr);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x24, 0);
    FIUI_TEST_ASSERT(select.selected_index() == 0);
    FIUI_TEST_ASSERT(std::string(select.selected_text()) == "Balanced");
    FIUI_TEST_ASSERT(select_change_count == 3);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x23, 0);
    FIUI_TEST_ASSERT(select.selected_index() == 2);
    FIUI_TEST_ASSERT(std::string(select.selected_text()) == "Diagnostics");
    FIUI_TEST_ASSERT(select_change_count == 4);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x1b, 0);
    FIUI_TEST_ASSERT(!select.expose_impl()->properties.select_popup_open);
    fiui::default_runtime_clear_event_targets();

    TestColumn select_focus_root;
    select_focus_root.debug_id("select_focus_root").size(260.0f, 150.0f);
    TestSelect focus_select;
    focus_select.debug_id("focus_select");
    focus_select.placeholder("Focus").add_option("One").add_option("Two");
    TestButton focus_after_select("After");
    focus_after_select.debug_id("focus_after_select");
    FIUI_TEST_ASSERT(select_focus_root.add(focus_select));
    FIUI_TEST_ASSERT(select_focus_root.add(focus_after_select));
    const fiui::LayoutResult select_focus_layout =
        layout_system.arrange(*select_focus_root.expose_impl(),
                              fiui::LayoutConstraints{260.0f, 150.0f});
    FIUI_TEST_ASSERT(select_focus_layout.arranged_nodes == 3);
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*select_focus_root.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == focus_select.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x20, 0);
    FIUI_TEST_ASSERT(focus_select.expose_impl()->properties.select_popup_open);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == focus_after_select.object_id());
    FIUI_TEST_ASSERT(!focus_select.expose_impl()->properties.select_popup_open);
    fiui::default_runtime_clear_event_targets();

    TestColumn select_overlap_root;
    select_overlap_root.debug_id("select_overlap_root").size(260.0f, 170.0f).gap(0.0f);
    TestSelect overlapping_select;
    overlapping_select.debug_id("overlapping_select");
    overlapping_select.placeholder("Overlap")
        .add_option("Basic")
        .add_option("Balanced")
        .add_option("Verbose");
    TestListView overlapping_list;
    overlapping_list.debug_id("overlapping_list").size(220.0f, 90.0f);
    overlapping_list.add_item("Dirty merge").add_item("Resource cache");
    int overlapping_list_change_count = 0;
    overlapping_list.on_change(increment_int_callback, &overlapping_list_change_count);
    FIUI_TEST_ASSERT(select_overlap_root.add(overlapping_select));
    FIUI_TEST_ASSERT(select_overlap_root.add(overlapping_list));
    overlapping_select.open();
    const fiui::LayoutResult select_overlap_layout =
        layout_system.arrange(*select_overlap_root.expose_impl(),
                              fiui::LayoutConstraints{260.0f, 170.0f});
    FIUI_TEST_ASSERT(select_overlap_layout.arranged_nodes == 8);
    fiui::WidgetImpl* verbose_option = overlapping_select.expose_impl()->node.children[2];
    FIUI_TEST_ASSERT(verbose_option != nullptr);
    FIUI_TEST_ASSERT(verbose_option->dirty.bounds.y >= overlapping_list.bounds().y);
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*select_overlap_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, verbose_option->dirty.bounds.x + 8.0f,
        verbose_option->dirty.bounds.y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, verbose_option->dirty.bounds.x + 8.0f,
        verbose_option->dirty.bounds.y + 8.0f);
    FIUI_TEST_ASSERT(overlapping_select.selected_index() == 2);
    FIUI_TEST_ASSERT(std::string(overlapping_select.selected_text()) == "Verbose");
    FIUI_TEST_ASSERT(overlapping_list.selected_index() == 0);
    FIUI_TEST_ASSERT(overlapping_list_change_count == 0);
    FIUI_TEST_ASSERT(!overlapping_select.expose_impl()->properties.select_popup_open);
    fiui::default_runtime_clear_event_targets();

    TestColumn bottom_select_root;
    bottom_select_root.debug_id("bottom_select_root").size(260.0f, 180.0f);
    TestColumn bottom_spacer;
    bottom_spacer.debug_id("bottom_spacer").size(0.0f, 90.0f);
    TestSelect bottom_select;
    bottom_select.debug_id("bottom_select");
    bottom_select.placeholder("Bottom")
        .add_option("Alpha")
        .add_option("Beta")
        .add_option("Gamma");
    bottom_select_root.add(bottom_spacer);
    bottom_select_root.add(bottom_select);
    bottom_select.open();
    const fiui::LayoutResult bottom_select_layout =
        layout_system.arrange(*bottom_select_root.expose_impl(),
                              fiui::LayoutConstraints{260.0f, 180.0f});
    FIUI_TEST_ASSERT(bottom_select_layout.arranged_nodes == 6);
    fiui::WidgetImpl* bottom_alpha_option = bottom_select.expose_impl()->node.children[0];
    fiui::WidgetImpl* bottom_gamma_option = bottom_select.expose_impl()->node.children[2];
    FIUI_TEST_ASSERT(bottom_alpha_option != nullptr);
    FIUI_TEST_ASSERT(bottom_gamma_option != nullptr);
    FIUI_TEST_ASSERT(bottom_select.bounds().y == bottom_spacer.bounds().height);
    FIUI_TEST_ASSERT(bottom_alpha_option->dirty.bounds.y < bottom_select.bounds().y);
    FIUI_TEST_ASSERT(bottom_gamma_option->dirty.bounds.y < bottom_select.bounds().y);

    TestRow right_select_root;
    right_select_root.debug_id("right_select_root").size(220.0f, 160.0f);
    TestColumn right_spacer;
    right_spacer.debug_id("right_spacer").size(160.0f, 28.0f);
    TestSelect right_select;
    right_select.debug_id("right_select").size(180.0f, 0.0f);
    right_select.placeholder("Right")
        .add_option("Alpha")
        .add_option("Beta")
        .add_option("Gamma");
    right_select_root.add(right_spacer);
    right_select_root.add(right_select);
    right_select.open();
    const fiui::LayoutResult right_select_layout =
        layout_system.arrange(*right_select_root.expose_impl(),
                              fiui::LayoutConstraints{220.0f, 160.0f});
    FIUI_TEST_ASSERT(right_select_layout.arranged_nodes == 6);
    fiui::WidgetImpl* right_gamma_option = right_select.expose_impl()->node.children[2];
    FIUI_TEST_ASSERT(right_gamma_option != nullptr);
    FIUI_TEST_ASSERT(right_gamma_option->dirty.bounds.x + right_gamma_option->dirty.bounds.width <=
           right_select_root.bounds().width + 0.1f);
    FIUI_TEST_ASSERT(right_gamma_option->dirty.bounds.x <= right_select.bounds().x);

    TestRow outside_close_root;
    outside_close_root.debug_id("outside_close_root").size(300.0f, 180.0f).gap(6.0f);
    TestSelect outside_close_select;
    outside_close_select.debug_id("outside_close_select");
    outside_close_select.size(100.0f, 0.0f);
    outside_close_select.placeholder("Outside")
        .add_option("One")
        .add_option("Two")
        .add_option("Three");
    TestListView outside_close_list;
    outside_close_list.debug_id("outside_close_list");
    outside_close_list.size(170.0f, 70.0f);
    outside_close_list.add_item("Target").add_item("Other");
    int outside_close_list_count = 0;
    outside_close_list.on_change(increment_int_callback, &outside_close_list_count);
    outside_close_root.add(outside_close_select);
    outside_close_root.add(outside_close_list);
    outside_close_select.open();
    const fiui::LayoutResult outside_close_layout =
        layout_system.arrange(*outside_close_root.expose_impl(),
                              fiui::LayoutConstraints{300.0f, 180.0f});
    FIUI_TEST_ASSERT(outside_close_layout.arranged_nodes == 8);
    FIUI_TEST_ASSERT(outside_close_select.expose_impl()->properties.select_popup_open);
    fiui::WidgetImpl* outside_target_item = outside_close_list.expose_impl()->node.children[1];
    FIUI_TEST_ASSERT(outside_target_item != nullptr);
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*outside_close_root.expose_impl());
    const float outside_click_x = outside_target_item->dirty.bounds.x +
                                  outside_target_item->dirty.bounds.width - 12.0f;
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, outside_click_x,
        outside_target_item->dirty.bounds.y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, outside_click_x,
        outside_target_item->dirty.bounds.y + 8.0f);
    FIUI_TEST_ASSERT(!outside_close_select.expose_impl()->properties.select_popup_open);
    FIUI_TEST_ASSERT(outside_close_list.selected_index() == 1);
    FIUI_TEST_ASSERT(outside_close_list_count == 1);
    fiui::default_runtime_clear_event_targets();

    TestColumn dialog_root;
    dialog_root.debug_id("dialog_root").size(360.0f, 240.0f);
    TestButton dialog_underlay("Underlay");
    dialog_underlay.debug_id("dialog_underlay");
    TestDialog dialog;
    dialog.debug_id("dialog");
    TestColumn dialog_panel;
    dialog_panel.debug_id("dialog_panel").padding(12.0f).gap(8.0f);
    fiui::Text dialog_title("Dialog title");
    TestButton dialog_ok("OK");
    dialog_ok.debug_id("dialog_ok");
    int dialog_ok_count = 0;
    dialog_ok.on_click(increment_int_callback, &dialog_ok_count);
    dialog_panel.add(dialog_title);
    dialog_panel.add(dialog_ok);
    dialog.content(dialog_panel).open();
    dialog_root.add(dialog_underlay);
    dialog_root.add(dialog);
    const fiui::LayoutResult dialog_layout =
        layout_system.arrange(*dialog_root.expose_impl(), fiui::LayoutConstraints{360.0f, 240.0f});
    FIUI_TEST_ASSERT(dialog_layout.arranged_nodes == 6);
    FIUI_TEST_ASSERT(dialog.is_open());
    FIUI_TEST_ASSERT(dialog.bounds().width == dialog_root.bounds().width);
    FIUI_TEST_ASSERT(dialog_panel.bounds().x > 0.0f);
    const fiui::RenderFrameResult dialog_render =
        render_system.build_frame(*dialog_root.expose_impl());
    FIUI_TEST_ASSERT(find_rect_command_with_path(dialog_render, dialog.object_id(), "/backdrop") != nullptr);
    FIUI_TEST_ASSERT(find_text_command(dialog_render, dialog_title.object_id()) != nullptr);

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*dialog_root.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == dialog_ok.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == dialog_ok.object_id());
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, dialog_ok.bounds().x + 8.0f, dialog_ok.bounds().y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, dialog_ok.bounds().x + 8.0f, dialog_ok.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(dialog_ok_count == 1);
    FIUI_TEST_ASSERT(dialog.is_open());
    const float dialog_panel_x_before_drag = dialog_panel.bounds().x;
    const float dialog_panel_y_before_drag = dialog_panel.bounds().y;
    const float dialog_drag_start_x = dialog_panel.bounds().x + 20.0f;
    const float dialog_drag_start_y = dialog_panel.bounds().y + 18.0f;
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, dialog_drag_start_x, dialog_drag_start_y);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == dialog.object_id());
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerMove, dialog_drag_start_x + 42.0f, dialog_drag_start_y + 24.0f);
    (void)layout_system.arrange(*dialog_root.expose_impl(),
                                fiui::LayoutConstraints{360.0f, 240.0f});
    FIUI_TEST_ASSERT(dialog_panel.bounds().x > dialog_panel_x_before_drag);
    FIUI_TEST_ASSERT(dialog_panel.bounds().y > dialog_panel_y_before_drag);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, dialog_drag_start_x + 42.0f, dialog_drag_start_y + 24.0f);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == 0);
    FIUI_TEST_ASSERT(dialog.is_open());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown, 8.0f, 8.0f);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp, 8.0f, 8.0f);
    FIUI_TEST_ASSERT(!dialog.is_open());
    dialog.open();
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x1b, 0);
    FIUI_TEST_ASSERT(!dialog.is_open());
    fiui::default_runtime_clear_event_targets();

    TestSplitView split_view;
    split_view.debug_id("split_view").size(320.0f, 120.0f);
    split_view.ratio(0.25f).min_pane_size(40.0f).handle_size(8.0f);
    TestColumn split_left;
    split_left.debug_id("split_left");
    split_left.add(fiui::Text("Left"));
    TestColumn split_right;
    split_right.debug_id("split_right");
    split_right.add(fiui::Text("Right"));
    split_view.first(split_left);
    split_view.second(split_right);
    const fiui::LayoutResult split_layout =
        layout_system.arrange(*split_view.expose_impl(), fiui::LayoutConstraints{320.0f, 120.0f});
    FIUI_TEST_ASSERT(split_layout.arranged_nodes == 5);
    FIUI_TEST_ASSERT(split_left.bounds().width >= 77.0f && split_left.bounds().width <= 79.0f);
    FIUI_TEST_ASSERT(split_right.bounds().x > split_left.bounds().x + split_left.bounds().width);
    const fiui::RenderFrameResult split_render =
        render_system.build_frame(*split_view.expose_impl());
    FIUI_TEST_ASSERT(split_view.kind() == fiui::WidgetKind::SplitView);
    FIUI_TEST_ASSERT(split_view.child_count() == 2);
    FIUI_TEST_ASSERT(has_layer_path(split_render, split_view.object_id(), "/split_handle"));
    FIUI_TEST_ASSERT(has_layer_path(split_render, split_view.object_id(), "/overflow_clip"));
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*split_view.expose_impl());
    const float split_handle_x = split_left.bounds().x + split_left.bounds().width + 4.0f;
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, split_handle_x, split_view.bounds().y + 24.0f);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == split_view.object_id());
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerMove, split_handle_x + 64.0f, split_view.bounds().y + 24.0f);
    FIUI_TEST_ASSERT(split_view.ratio() > 0.40f);
    const fiui::LayoutResult split_drag_layout =
        layout_system.arrange(*split_view.expose_impl(), fiui::LayoutConstraints{320.0f, 120.0f});
    FIUI_TEST_ASSERT(split_drag_layout.arranged_nodes == 5);
    FIUI_TEST_ASSERT(split_left.bounds().width > 120.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, split_handle_x + 64.0f, split_view.bounds().y + 24.0f);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == 0);
    fiui::default_runtime_clear_event_targets();

    TestTreeView tree_view;
    tree_view.debug_id("tree_view").size(260.0f, 160.0f);
    TestTreeItem runtime_group("Runtime");
    runtime_group.debug_id("runtime_group");
    runtime_group.expanded(true);
    TestTreeItem object_table("Object table");
    object_table.debug_id("object_table");
    TestTreeItem frame_clock("Frame clock");
    frame_clock.debug_id("frame_clock");
    runtime_group.add(object_table);
    runtime_group.add(frame_clock);
    TestTreeItem render_group("Render");
    render_group.debug_id("render_group");
    render_group.expanded(false);
    TestTreeItem display_list("Display list");
    display_list.debug_id("display_list");
    render_group.add(display_list);
    tree_view.add_item(runtime_group).add_item(render_group);
    int tree_change_count = 0;
    tree_view.on_change(increment_int_callback, &tree_change_count);
    const fiui::LayoutResult tree_layout =
        layout_system.arrange(*tree_view.expose_impl(), fiui::LayoutConstraints{260.0f, 160.0f});
    FIUI_TEST_ASSERT(tree_layout.arranged_nodes == 5);
    FIUI_TEST_ASSERT(runtime_group.expose_impl()->properties.tree_depth == 0);
    FIUI_TEST_ASSERT(object_table.expose_impl()->properties.tree_depth == 1);
    FIUI_TEST_ASSERT(display_list.bounds().height == 0.0f);
    const fiui::RenderFrameResult tree_render =
        render_system.build_frame(*tree_view.expose_impl());
    FIUI_TEST_ASSERT(tree_view.kind() == fiui::WidgetKind::TreeView);
    FIUI_TEST_ASSERT(runtime_group.kind() == fiui::WidgetKind::TreeItem);
    FIUI_TEST_ASSERT(has_layer_path(tree_render, tree_view.object_id(), "/overflow_clip"));
    FIUI_TEST_ASSERT(has_layer_path(tree_render, runtime_group.object_id(), "/tree_toggle"));
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*tree_view.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, object_table.bounds().x + 48.0f,
        object_table.bounds().y + 10.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, object_table.bounds().x + 48.0f,
        object_table.bounds().y + 10.0f);
    FIUI_TEST_ASSERT(tree_view.selected_id() == object_table.object_id());
    FIUI_TEST_ASSERT(std::string(tree_view.selected_text()) == "Object table");
    FIUI_TEST_ASSERT(object_table.selected());
    FIUI_TEST_ASSERT(tree_change_count == 1);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, runtime_group.bounds().x + 12.0f,
        runtime_group.bounds().y + 10.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, runtime_group.bounds().x + 12.0f,
        runtime_group.bounds().y + 10.0f);
    FIUI_TEST_ASSERT(runtime_group.expanded() == false);
    const fiui::LayoutResult tree_collapsed_layout =
        layout_system.arrange(*tree_view.expose_impl(), fiui::LayoutConstraints{260.0f, 160.0f});
    FIUI_TEST_ASSERT(tree_collapsed_layout.arranged_nodes == 3);
    FIUI_TEST_ASSERT(object_table.bounds().height == 0.0f);
    fiui::default_runtime_clear_event_targets();

    TestTableView table_view;
    table_view.debug_id("table_view").size(360.0f, 150.0f);
    table_view.add_column("Metric", 140.0f)
        .add_column("Value", 90.0f)
        .add_column("State", 0.0f)
        .add_row("Frame", "1.2ms", "ok")
        .add_row("Dirty", "2", "partial")
        .add_row("Resources", "cached", "ready")
        .add_row("Objects", "128", "tracked")
        .add_row("Text", "warm", "hit")
        .add_row("Images", "4", "cached");
    int table_change_count = 0;
    table_view.on_change(increment_int_callback, &table_change_count);
    const fiui::LayoutResult table_layout =
        layout_system.arrange(*table_view.expose_impl(), fiui::LayoutConstraints{360.0f, 150.0f});
    FIUI_TEST_ASSERT(table_layout.arranged_nodes == 1);
    FIUI_TEST_ASSERT(table_view.kind() == fiui::WidgetKind::TableView);
    FIUI_TEST_ASSERT(table_view.column_count() == 3);
    FIUI_TEST_ASSERT(table_view.row_count() == 6);
    FIUI_TEST_ASSERT(table_view.selected_row() == 0);
    FIUI_TEST_ASSERT(std::string(table_view.selected_text()) == "Frame");
    const fiui::RenderFrameResult table_render =
        render_system.build_frame(*table_view.expose_impl());
    FIUI_TEST_ASSERT(has_layer_path(table_render, table_view.object_id(), "/header/cell_0"));
    FIUI_TEST_ASSERT(has_layer_path(table_render, table_view.object_id(), "/row_1/cell_2"));
    FIUI_TEST_ASSERT(table_view.expose_impl()->properties.scroll_content_height > table_view.bounds().height);
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*table_view.expose_impl());
    const float table_row_2_y = table_view.bounds().y + 30.0f + 28.0f + 10.0f;
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, table_view.bounds().x + 24.0f, table_row_2_y);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, table_view.bounds().x + 24.0f, table_row_2_y);
    FIUI_TEST_ASSERT(table_view.selected_row() == 1);
    FIUI_TEST_ASSERT(std::string(table_view.selected_text()) == "Dirty");
    FIUI_TEST_ASSERT(table_change_count == 1);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x28, 0);
    FIUI_TEST_ASSERT(table_view.selected_row() == 2);
    FIUI_TEST_ASSERT(std::string(table_view.selected_text()) == "Resources");
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x24, 0);
    FIUI_TEST_ASSERT(table_view.selected_row() == 0);
    FIUI_TEST_ASSERT(table_change_count >= 3);
    const float table_header_y = table_view.bounds().y + 10.0f;
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, table_view.bounds().x + 24.0f, table_header_y);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, table_view.bounds().x + 24.0f, table_header_y);
    FIUI_TEST_ASSERT(table_view.sorted_column() == 0);
    FIUI_TEST_ASSERT(table_view.sort_ascending());
    FIUI_TEST_ASSERT(std::string(table_view.selected_text()) == "Dirty");
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, table_view.bounds().x + 24.0f, table_header_y);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, table_view.bounds().x + 24.0f, table_header_y);
    FIUI_TEST_ASSERT(table_view.sorted_column() == 0);
    FIUI_TEST_ASSERT(!table_view.sort_ascending());
    FIUI_TEST_ASSERT(!std::string(table_view.selected_text()).empty());
    const float old_column_width = table_view.column_width(0);
    const float divider_x = table_view.bounds().x + old_column_width;
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, divider_x, table_header_y);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == table_view.object_id());
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerMove, divider_x + 32.0f, table_header_y);
    FIUI_TEST_ASSERT(table_view.column_width(0) > old_column_width + 20.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, divider_x + 32.0f, table_header_y);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == 0);
    FIUI_TEST_ASSERT(!table_view.sort_ascending());
    const float table_offset_before_wheel =
        table_view.expose_impl()->properties.scroll_offset_y;
    fiui::default_runtime_route_wheel_event(*table_view.expose_impl(),
                                            table_view.bounds().x + 20.0f,
                                            table_view.bounds().y + 70.0f,
                                            -120.0f);
    FIUI_TEST_ASSERT(table_view.expose_impl()->properties.scroll_offset_y > table_offset_before_wheel);
    const fiui::RenderFrameResult table_scrolled_render =
        render_system.build_frame(*table_view.expose_impl());
    FIUI_TEST_ASSERT(has_layer_path(table_scrolled_render, table_view.object_id(), "/scrollbar_thumb"));
    FIUI_TEST_ASSERT(has_layer_path(table_scrolled_render, table_view.object_id(), "/body_clip"));
    const fiui::DisplayCommand* table_thumb =
        find_rect_command_with_path(table_scrolled_render, table_view.object_id(),
                                    "/scrollbar_thumb");
    FIUI_TEST_ASSERT(table_thumb != nullptr);
    const float table_thumb_x = table_thumb->bounds.x + table_thumb->bounds.width * 0.5f;
    const float table_thumb_y = table_thumb->bounds.y + table_thumb->bounds.height * 0.5f;
    const float table_drag_start_offset = table_view.expose_impl()->properties.scroll_offset_y;
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, table_thumb_x, table_thumb_y);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == table_view.object_id());
    FIUI_TEST_ASSERT(table_view.expose_impl()->properties.scroll_thumb_dragging);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerMove, table_thumb_x, table_thumb_y + 26.0f);
    FIUI_TEST_ASSERT(table_view.expose_impl()->properties.scroll_offset_y >= table_drag_start_offset);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, table_thumb_x, table_thumb_y + 26.0f);
    FIUI_TEST_ASSERT(!table_view.expose_impl()->properties.scroll_thumb_dragging);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x23, 0);
    FIUI_TEST_ASSERT(table_view.selected_row() == 5);
    FIUI_TEST_ASSERT(table_view.expose_impl()->properties.scroll_offset_y > 0.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, table_view.bounds().x + 24.0f,
        table_view.bounds().y + 42.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, table_view.bounds().x + 24.0f,
        table_view.bounds().y + 42.0f);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == table_view.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x21, 0);
    FIUI_TEST_ASSERT(table_view.selected_row() < 5);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x22, 0);
    FIUI_TEST_ASSERT(table_view.selected_row() > 0);
    table_view.clear_rows();
    FIUI_TEST_ASSERT(table_view.row_count() == 0);
    FIUI_TEST_ASSERT(table_view.column_count() == 3);
    FIUI_TEST_ASSERT(std::string(table_view.selected_text()).empty());
    FIUI_TEST_ASSERT(!table_view.expose_impl()->properties.has_selected_index);
    FIUI_TEST_ASSERT(table_view.expose_impl()->properties.scroll_offset_y == 0.0f);
    table_view.add_row("Live", "true", "ok").add_row("Trace", "on", "ready");
    FIUI_TEST_ASSERT(table_view.row_count() == 2);
    FIUI_TEST_ASSERT(table_view.selected_row() == 0);
    FIUI_TEST_ASSERT(std::string(table_view.selected_text()) == "Live");
    table_view.selected_row(1);
    table_view.clear_rows().add_row("Only", "1", "single");
    FIUI_TEST_ASSERT(table_view.selected_row() == 0);
    FIUI_TEST_ASSERT(std::string(table_view.selected_text()) == "Only");
    table_view.clear_columns();
    FIUI_TEST_ASSERT(table_view.column_count() == 0);
    FIUI_TEST_ASSERT(table_view.row_count() == 0);
    FIUI_TEST_ASSERT(table_view.sorted_column() == 0xffffffffu);
    fiui::default_runtime_clear_event_targets();

    TestColumn list_root;
    list_root.debug_id("list_root").size(280.0f, 170.0f);
    TestListView list_view;
    list_view.debug_id("list_view").size(240.0f, 0.0f);
    list_view.add_item("Runtime")
        .add_item("Resources")
        .add_item("Diagnostics")
        .add_item("Themes");
    FIUI_TEST_ASSERT(list_view.child_count() == 4);
    FIUI_TEST_ASSERT(list_view.selected_index() == 0);
    FIUI_TEST_ASSERT(std::string(list_view.selected_text()) == "Runtime");
    int list_change_count = 0;
    list_view.on_change(increment_int_callback, &list_change_count);
    FIUI_TEST_ASSERT(list_root.add(list_view));
    const fiui::LayoutResult list_layout =
        layout_system.arrange(*list_root.expose_impl(), fiui::LayoutConstraints{280.0f, 170.0f});
    FIUI_TEST_ASSERT(list_layout.arranged_nodes == 6);
    FIUI_TEST_ASSERT(list_view.bounds().height == 134.0f);
    fiui::WidgetImpl* resource_item = list_view.expose_impl()->node.children[1];
    fiui::WidgetImpl* diagnostics_item = list_view.expose_impl()->node.children[2];
    FIUI_TEST_ASSERT(resource_item != nullptr);
    FIUI_TEST_ASSERT(diagnostics_item != nullptr);
    FIUI_TEST_ASSERT(resource_item->dirty.bounds.y > list_view.bounds().y);
    const fiui::RenderFrameResult list_render =
        render_system.build_frame(*list_root.expose_impl());
    const fiui::DisplayCommand* list_surface =
        find_rect_command(list_render, list_view.object_id());
    const fiui::DisplayCommand* first_item_rect =
        find_rect_command(list_render, list_view.expose_impl()->node.children[0]->object.object_id);
    const fiui::DisplayCommand* resource_text =
        find_text_command(list_render, resource_item->object.object_id);
    FIUI_TEST_ASSERT(list_surface != nullptr);
    FIUI_TEST_ASSERT(first_item_rect != nullptr);
    FIUI_TEST_ASSERT(first_item_rect->style.fill.a == 255);
    FIUI_TEST_ASSERT(resource_text != nullptr);
    FIUI_TEST_ASSERT(resource_text->text == "Resources");
    FIUI_TEST_ASSERT(has_layer_path(list_render, list_view.object_id(), "/overflow_clip"));

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*list_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, resource_item->dirty.bounds.x + 8.0f,
        resource_item->dirty.bounds.y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, resource_item->dirty.bounds.x + 8.0f,
        resource_item->dirty.bounds.y + 8.0f);
    FIUI_TEST_ASSERT(list_view.selected_index() == 1);
    FIUI_TEST_ASSERT(std::string(list_view.selected_text()) == "Resources");
    FIUI_TEST_ASSERT(list_change_count == 1);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == resource_item->object.object_id);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x28, 0);
    FIUI_TEST_ASSERT(list_view.selected_index() == 2);
    FIUI_TEST_ASSERT(std::string(list_view.selected_text()) == "Diagnostics");
    FIUI_TEST_ASSERT(list_change_count == 2);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == diagnostics_item->object.object_id);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x24, 0);
    FIUI_TEST_ASSERT(list_view.selected_index() == 0);
    FIUI_TEST_ASSERT(list_change_count == 3);
    fiui::default_runtime_clear_event_targets();

    TestColumn disabled_root;
    disabled_root.debug_id("disabled_root").size(260.0f, 150.0f).gap(6.0f);
    TestButton disabled_button("Disabled");
    disabled_button.debug_id("disabled_button");
    disabled_button.enabled(false);
    int disabled_button_count = 0;
    disabled_button.on_click(increment_int_callback, &disabled_button_count);
    TestSelect disabled_select;
    disabled_select.debug_id("disabled_select");
    disabled_select.placeholder("Disabled select").add_option("A").add_option("B");
    disabled_select.enabled(false);
    TestButton enabled_after_disabled("Enabled");
    enabled_after_disabled.debug_id("enabled_after_disabled");
    FIUI_TEST_ASSERT(!disabled_button.enabled());
    FIUI_TEST_ASSERT(disabled_root.add(disabled_button));
    FIUI_TEST_ASSERT(disabled_root.add(disabled_select));
    FIUI_TEST_ASSERT(disabled_root.add(enabled_after_disabled));
    FIUI_TEST_ASSERT(layout_system.arrange(*disabled_root.expose_impl(),
                                 fiui::LayoutConstraints{260.0f, 150.0f})
               .arranged_nodes == 4);
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*disabled_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown,
                                                        disabled_button.bounds().x + 8.0f,
                                                        disabled_button.bounds().y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp,
                                                        disabled_button.bounds().x + 8.0f,
                                                        disabled_button.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(disabled_button_count == 0);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown,
                                                        disabled_select.bounds().x + 8.0f,
                                                        disabled_select.bounds().y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp,
                                                        disabled_select.bounds().x + 8.0f,
                                                        disabled_select.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(!disabled_select.expose_impl()->properties.select_popup_open);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == enabled_after_disabled.object_id());
    const fiui::RenderFrameResult disabled_render =
        render_system.build_frame(*disabled_root.expose_impl());
    const fiui::DisplayCommand* disabled_button_text =
        find_text_command(disabled_render, disabled_button.object_id());
    FIUI_TEST_ASSERT(disabled_button_text != nullptr);
    const fiui::DisplayCommand* enabled_button_text =
        find_text_command(disabled_render, enabled_after_disabled.object_id());
    FIUI_TEST_ASSERT(enabled_button_text != nullptr);
    FIUI_TEST_ASSERT(!same_color(disabled_button_text->style.text, enabled_button_text->style.text));
    fiui::default_runtime_clear_event_targets();

    TestColumn visible_root;
    visible_root.debug_id("visible_root").size(260.0f, 150.0f).gap(6.0f);
    TestButton visible_before("Before");
    visible_before.debug_id("visible_before");
    TestButton hidden_button("Hidden");
    hidden_button.debug_id("hidden_button").visible(false);
    TestButton visible_after("After");
    visible_after.debug_id("visible_after");
    FIUI_TEST_ASSERT(!hidden_button.visible());
    FIUI_TEST_ASSERT(visible_root.add(visible_before));
    FIUI_TEST_ASSERT(visible_root.add(hidden_button));
    FIUI_TEST_ASSERT(visible_root.add(visible_after));
    const fiui::LayoutResult hidden_layout =
        layout_system.arrange(*visible_root.expose_impl(), fiui::LayoutConstraints{260.0f, 150.0f});
    FIUI_TEST_ASSERT(hidden_layout.arranged_nodes == 3);
    FIUI_TEST_ASSERT(hidden_button.bounds().width == 0.0f);
    FIUI_TEST_ASSERT(hidden_button.bounds().height == 0.0f);
    FIUI_TEST_ASSERT(visible_after.bounds().y < 90.0f);
    const fiui::RenderFrameResult hidden_render =
        render_system.build_frame(*visible_root.expose_impl());
    FIUI_TEST_ASSERT(find_text_command(hidden_render, hidden_button.object_id()) == nullptr);
    for (const fiui::RenderNode& node : hidden_render.render_tree.nodes) {
        FIUI_TEST_ASSERT(node.object_id != hidden_button.object_id());
    }
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*visible_root.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == visible_before.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == visible_after.object_id());
    const fiui::RuntimeEventRouteProbe hidden_probe =
        fiui::default_runtime_route_pointer_event(*visible_root.expose_impl(),
                                                  fiui::EventType::PointerDown,
                                                  hidden_button.bounds().x + 4.0f,
                                                  hidden_button.bounds().y + 4.0f);
    FIUI_TEST_ASSERT(hidden_probe.target_object_id != hidden_button.object_id());
    hidden_button.visible(true);
    FIUI_TEST_ASSERT(hidden_button.visible());
    const fiui::LayoutResult visible_again_layout =
        layout_system.arrange(*visible_root.expose_impl(), fiui::LayoutConstraints{260.0f, 150.0f});
    FIUI_TEST_ASSERT(visible_again_layout.arranged_nodes == 4);
    FIUI_TEST_ASSERT(hidden_button.bounds().height > 0.0f);
    FIUI_TEST_ASSERT(find_text_command(render_system.build_frame(*visible_root.expose_impl()),
                                       hidden_button.object_id()) != nullptr);
    fiui::default_runtime_clear_event_targets();

    TestTabs tabs;
    tabs.debug_id("tabs").size(300.0f, 160.0f);
    TestColumn tab_overview;
    tab_overview.debug_id("tab_overview").gap(4.0f);
    fiui::Text overview_text("Overview content");
    overview_text.debug_id("overview_text");
    tab_overview.add(overview_text);
    TestColumn tab_metrics;
    tab_metrics.debug_id("tab_metrics").gap(4.0f);
    fiui::Text metrics_tab_text("Metrics content");
    metrics_tab_text.debug_id("metrics_tab_text");
    tab_metrics.add(metrics_tab_text);
    int tabs_change_count = 0;
    tabs.add_tab("Overview", tab_overview)
        .add_tab("Metrics", tab_metrics)
        .on_change(increment_int_callback, &tabs_change_count);
    TestColumn tabs_root;
    tabs_root.debug_id("tabs_root").size(320.0f, 180.0f);
    FIUI_TEST_ASSERT(tabs_root.add(tabs));
    FIUI_TEST_ASSERT(layout_system.arrange(*tabs_root.expose_impl(),
                                 fiui::LayoutConstraints{320.0f, 180.0f})
               .arranged_nodes == 4);
    FIUI_TEST_ASSERT(tabs.selected_index() == 0);
    FIUI_TEST_ASSERT(tab_overview.bounds().height > 0.0f);
    FIUI_TEST_ASSERT(tab_metrics.bounds().height == 0.0f);
    const fiui::RenderFrameResult tabs_initial_render =
        render_system.build_frame(*tabs_root.expose_impl());
    FIUI_TEST_ASSERT(find_rect_command_with_path(tabs_initial_render, tabs.object_id(), "/header") !=
           nullptr);
    FIUI_TEST_ASSERT(find_text_command_with_path(tabs_initial_render, tabs.object_id(), "/tab_0/label") !=
           nullptr);
    FIUI_TEST_ASSERT(find_rect_command_with_path(tabs_initial_render, tabs.object_id(),
                                       "/tab_0/underline") != nullptr);
    FIUI_TEST_ASSERT(has_layer_path(tabs_initial_render, tabs.object_id(), "/overflow_clip"));

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*tabs_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown,
                                                        tabs.bounds().x +
                                                            tabs.bounds().width * 0.75f,
                                                        tabs.bounds().y + 8.0f);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp,
                                                        tabs.bounds().x +
                                                            tabs.bounds().width * 0.75f,
                                                        tabs.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(tabs.selected_index() == 1);
    FIUI_TEST_ASSERT(tabs_change_count == 1);
    FIUI_TEST_ASSERT(layout_system.arrange(*tabs_root.expose_impl(),
                                 fiui::LayoutConstraints{320.0f, 180.0f})
               .arranged_nodes == 4);
    FIUI_TEST_ASSERT(tab_overview.bounds().height == 0.0f);
    FIUI_TEST_ASSERT(tab_metrics.bounds().height > 0.0f);

    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x25, 0);
    FIUI_TEST_ASSERT(tabs.selected_index() == 0);
    FIUI_TEST_ASSERT(tabs_change_count == 2);
    fiui::default_runtime_clear_event_targets();

    TestColumn tooltip_root;
    tooltip_root.debug_id("tooltip_root").size(260.0f, 90.0f);
    fiui::Button tooltip_button("Hover");
    tooltip_button.debug_id("tooltip_button").tooltip("Tooltip text");
    FIUI_TEST_ASSERT(tooltip_root.add(tooltip_button));
    FIUI_TEST_ASSERT(layout_system.arrange(*tooltip_root.expose_impl(),
                                 fiui::LayoutConstraints{260.0f, 90.0f})
               .arranged_nodes == 2);
    const fiui::RuntimeEventRouteProbe tooltip_probe =
        fiui::default_runtime_route_pointer_event(*tooltip_root.expose_impl(),
                                                  fiui::EventType::PointerMove,
                                                  tooltip_button.bounds().x + 8.0f,
                                                  tooltip_button.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(tooltip_probe.hit);
    const fiui::RenderFrameResult tooltip_render =
        render_system.build_frame(*tooltip_root.expose_impl());
    bool found_tooltip_surface = false;
    bool found_tooltip_text = false;
    fiui::Rect tooltip_surface_bounds{};
    for (const fiui::DisplayCommand& command : tooltip_render.display_list.commands) {
        if (command.kind == fiui::DisplayCommandKind::Rect &&
            command.object_id == tooltip_button.object_id() &&
            command.path.size() >= 8 &&
            command.path.compare(command.path.size() - 8, 8, "/tooltip") == 0) {
            found_tooltip_surface = true;
            tooltip_surface_bounds = command.bounds;
            FIUI_TEST_ASSERT(command.style.fill.a == 255);
        }
        if (command.kind == fiui::DisplayCommandKind::Text &&
            command.object_id == tooltip_button.object_id() &&
            command.path.find("/tooltip/text") != std::string::npos &&
            command.text == "Tooltip text") {
            found_tooltip_text = true;
        }
    }
    FIUI_TEST_ASSERT(found_tooltip_surface);
    FIUI_TEST_ASSERT(found_tooltip_text);
    const bool tooltip_overlaps_button =
        tooltip_surface_bounds.x < tooltip_button.bounds().x + tooltip_button.bounds().width &&
        tooltip_surface_bounds.x + tooltip_surface_bounds.width > tooltip_button.bounds().x &&
        tooltip_surface_bounds.y < tooltip_button.bounds().y + tooltip_button.bounds().height &&
        tooltip_surface_bounds.y + tooltip_surface_bounds.height > tooltip_button.bounds().y;
    FIUI_TEST_ASSERT(!tooltip_overlaps_button);
    (void)fiui::default_runtime_route_pointer_event(*tooltip_root.expose_impl(),
                                                    fiui::EventType::PointerDown,
                                                    tooltip_button.bounds().x + 8.0f,
                                                    tooltip_button.bounds().y + 8.0f);
    const fiui::RenderFrameResult tooltip_pressed_render =
        render_system.build_frame(*tooltip_root.expose_impl());
    for (const fiui::DisplayCommand& command : tooltip_pressed_render.display_list.commands) {
        FIUI_TEST_ASSERT(command.path.size() < 8 ||
               command.path.compare(command.path.size() - 8, 8, "/tooltip") != 0);
    }
    (void)fiui::default_runtime_route_pointer_event(*tooltip_root.expose_impl(),
                                                    fiui::EventType::PointerUp,
                                                    tooltip_button.bounds().x + 8.0f,
                                                    tooltip_button.bounds().y + 8.0f);

    TestMenuBar menu_bar;
    menu_bar.debug_id("menu_bar").size(320, 26);
    TestMenuItem file_menu("File");
    file_menu.debug_id("file_menu");
    TestMenuItem file_new_menu("New project");
    file_new_menu.debug_id("file_new_menu");
    TestMenuItem view_menu("View");
    view_menu.debug_id("view_menu");
    TestMenuItem view_tools_menu("Tools >");
    view_tools_menu.debug_id("view_tools_menu");
    TestMenuItem view_layout_menu("Layout");
    view_layout_menu.debug_id("view_layout_menu");
    TestMenuItem view_runtime_menu("Runtime overview");
    view_runtime_menu.debug_id("view_runtime_menu");
    TestMenuItem help_menu("Help");
    help_menu.debug_id("help_menu");
    int menu_click_count = 0;
    int submenu_click_count = 0;
    file_menu.on_click(increment_int_callback, &menu_click_count);
    file_new_menu.on_click(increment_int_callback, &submenu_click_count);
    view_layout_menu.on_click(increment_int_callback, &submenu_click_count);
    view_runtime_menu.on_click(increment_int_callback, &submenu_click_count);
    help_menu.on_click(increment_int_callback, &menu_click_count);
    FIUI_TEST_ASSERT(file_menu.add(file_new_menu));
    FIUI_TEST_ASSERT(view_tools_menu.add(view_layout_menu));
    FIUI_TEST_ASSERT(view_menu.add(view_tools_menu));
    FIUI_TEST_ASSERT(view_menu.add(view_runtime_menu));
    FIUI_TEST_ASSERT(menu_bar.add(file_menu));
    FIUI_TEST_ASSERT(menu_bar.add(view_menu));
    FIUI_TEST_ASSERT(menu_bar.add(help_menu));
    const fiui::LayoutResult menu_layout =
        layout_system.arrange(*menu_bar.expose_impl(), fiui::LayoutConstraints{320.0f, 26.0f});
    FIUI_TEST_ASSERT(menu_layout.arranged_nodes == 4);
    FIUI_TEST_ASSERT(file_menu.bounds().width >= 44.0f);
    FIUI_TEST_ASSERT(view_menu.bounds().x > file_menu.bounds().x);
    const fiui::RenderFrameResult menu_render_result =
        render_system.build_frame(*menu_bar.expose_impl());
    FIUI_TEST_ASSERT(find_rect_command(menu_render_result, menu_bar.object_id()) != nullptr);
    FIUI_TEST_ASSERT(find_rect_command(menu_render_result, file_menu.object_id()) != nullptr);
    const fiui::DisplayCommand* file_text_command =
        find_text_command(menu_render_result, file_menu.object_id());
    FIUI_TEST_ASSERT(file_text_command != nullptr);
    FIUI_TEST_ASSERT(std::strcmp(file_text_command->style.text_align, "center") == 0);
    FIUI_TEST_ASSERT(menu_bar.kind() == fiui::WidgetKind::MenuBar);
    FIUI_TEST_ASSERT(file_menu.kind() == fiui::WidgetKind::MenuItem);
    FIUI_TEST_ASSERT(file_menu.click());
    FIUI_TEST_ASSERT(menu_click_count == 0);
    FIUI_TEST_ASSERT(file_menu.expose_impl()->properties.menu_popup_open);
    const fiui::LayoutResult open_menu_layout =
        layout_system.arrange(*menu_bar.expose_impl(), fiui::LayoutConstraints{320.0f, 26.0f});
    FIUI_TEST_ASSERT(open_menu_layout.arranged_nodes == 5);
    FIUI_TEST_ASSERT(file_new_menu.bounds().y >= file_menu.bounds().y + file_menu.bounds().height);
    const fiui::RenderFrameResult open_menu_render =
        render_system.build_frame(*menu_bar.expose_impl());
    const fiui::DisplayCommand* popup_surface_command =
        find_rect_command_with_path(open_menu_render, file_menu.object_id(), "/popup");
    FIUI_TEST_ASSERT(popup_surface_command != nullptr);
    FIUI_TEST_ASSERT(popup_surface_command->style.fill.a == 255);
    FIUI_TEST_ASSERT(find_rect_command(open_menu_render, file_new_menu.object_id()) != nullptr);
    const fiui::DisplayCommand* file_new_text_command =
        find_text_command(open_menu_render, file_new_menu.object_id());
    FIUI_TEST_ASSERT(file_new_text_command != nullptr);
    FIUI_TEST_ASSERT(std::strcmp(file_new_text_command->style.text_align, "leading") == 0);
    FIUI_TEST_ASSERT(popup_surface_command < file_new_text_command);
    fiui::Application menu_theme_app;
    menu_theme_app.theme("modern.dark");
    const fiui::RenderFrameResult dark_menu_render =
        render_system.build_frame(*menu_bar.expose_impl());
    const fiui::DisplayCommand* dark_menu_bar_rect =
        find_rect_command(dark_menu_render, menu_bar.object_id());
    const fiui::DisplayCommand* dark_popup_surface =
        find_rect_command_with_path(dark_menu_render, file_menu.object_id(), "/popup");
    FIUI_TEST_ASSERT(dark_menu_bar_rect != nullptr);
    FIUI_TEST_ASSERT(dark_popup_surface != nullptr);
    FIUI_TEST_ASSERT(std::strcmp(dark_menu_bar_rect->style.theme_name, "modern.dark") == 0);
    FIUI_TEST_ASSERT(std::strcmp(dark_popup_surface->style.theme_name, "modern.dark") == 0);
    menu_theme_app.theme("modern.light");
    const fiui::RenderFrameResult light_menu_render =
        render_system.build_frame(*menu_bar.expose_impl());
    const fiui::DisplayCommand* light_menu_bar_rect =
        find_rect_command(light_menu_render, menu_bar.object_id());
    const fiui::DisplayCommand* light_popup_surface =
        find_rect_command_with_path(light_menu_render, file_menu.object_id(), "/popup");
    FIUI_TEST_ASSERT(light_menu_bar_rect != nullptr);
    FIUI_TEST_ASSERT(light_popup_surface != nullptr);
    FIUI_TEST_ASSERT(std::strcmp(light_menu_bar_rect->style.theme_name, "modern.light") == 0);
    FIUI_TEST_ASSERT(std::strcmp(light_popup_surface->style.theme_name, "modern.light") == 0);
    FIUI_TEST_ASSERT(dark_menu_bar_rect->style.fill.r != light_menu_bar_rect->style.fill.r ||
           dark_menu_bar_rect->style.fill.g != light_menu_bar_rect->style.fill.g ||
           dark_menu_bar_rect->style.fill.b != light_menu_bar_rect->style.fill.b);
    FIUI_TEST_ASSERT(dark_popup_surface->style.fill.r != light_popup_surface->style.fill.r ||
           dark_popup_surface->style.fill.g != light_popup_surface->style.fill.g ||
           dark_popup_surface->style.fill.b != light_popup_surface->style.fill.b);

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*menu_bar.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerMove,
                                                        view_menu.bounds().x + 4.0f,
                                                        view_menu.bounds().y + 4.0f);
    FIUI_TEST_ASSERT(!file_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(view_menu.expose_impl()->properties.menu_popup_open);
    const fiui::LayoutResult switched_menu_layout =
        layout_system.arrange(*menu_bar.expose_impl(), fiui::LayoutConstraints{320.0f, 26.0f});
    FIUI_TEST_ASSERT(switched_menu_layout.arranged_nodes == 6);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x1b, 0);
    FIUI_TEST_ASSERT(!view_menu.expose_impl()->properties.menu_popup_open);

    fiui::default_runtime_set_event_focus_target(help_menu.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         0x56 | test_key_alt, 0);
    FIUI_TEST_ASSERT(view_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == view_menu.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x1b, 0);
    FIUI_TEST_ASSERT(!view_menu.expose_impl()->properties.menu_popup_open);

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*menu_bar.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         0x56 | test_key_alt, 0);
    FIUI_TEST_ASSERT(view_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == view_menu.object_id());
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x1b, 0);
    FIUI_TEST_ASSERT(!view_menu.expose_impl()->properties.menu_popup_open);

    FIUI_TEST_ASSERT(file_menu.click());
    fiui::default_runtime_set_event_focus_target(file_menu.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x27, 0);
    FIUI_TEST_ASSERT(!file_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(view_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == view_menu.object_id());
    const fiui::LayoutResult keyboard_switched_layout =
        layout_system.arrange(*menu_bar.expose_impl(), fiui::LayoutConstraints{320.0f, 26.0f});
    FIUI_TEST_ASSERT(keyboard_switched_layout.arranged_nodes == 6);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x28, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == view_tools_menu.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x27, 0);
    FIUI_TEST_ASSERT(view_tools_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == view_layout_menu.object_id());
    const fiui::LayoutResult nested_menu_layout =
        layout_system.arrange(*menu_bar.expose_impl(), fiui::LayoutConstraints{320.0f, 26.0f});
    FIUI_TEST_ASSERT(nested_menu_layout.arranged_nodes == 7);
    FIUI_TEST_ASSERT(view_layout_menu.bounds().x >=
           view_tools_menu.bounds().x + view_tools_menu.bounds().width);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerMove,
                                                        view_runtime_menu.bounds().x + 4.0f,
                                                        view_runtime_menu.bounds().y + 4.0f);
    FIUI_TEST_ASSERT(view_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(!view_tools_menu.expose_impl()->properties.menu_popup_open);
    const fiui::LayoutResult leaf_hover_menu_layout =
        layout_system.arrange(*menu_bar.expose_impl(), fiui::LayoutConstraints{320.0f, 26.0f});
    FIUI_TEST_ASSERT(leaf_hover_menu_layout.arranged_nodes == 6);
    FIUI_TEST_ASSERT(view_layout_menu.bounds().width == 0.0f);
    FIUI_TEST_ASSERT(view_layout_menu.bounds().height == 0.0f);
    const fiui::RenderFrameResult leaf_hover_menu_render =
        render_system.build_frame(*menu_bar.expose_impl());
    FIUI_TEST_ASSERT(find_text_command(leaf_hover_menu_render, view_layout_menu.object_id()) == nullptr);

    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerMove,
                                                        view_tools_menu.bounds().x + 4.0f,
                                                        view_tools_menu.bounds().y + 4.0f);
    FIUI_TEST_ASSERT(view_tools_menu.expose_impl()->properties.menu_popup_open);
    const fiui::LayoutResult nested_menu_reopen_layout =
        layout_system.arrange(*menu_bar.expose_impl(), fiui::LayoutConstraints{320.0f, 26.0f});
    FIUI_TEST_ASSERT(nested_menu_reopen_layout.arranged_nodes == 7);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerMove,
                                                        file_menu.bounds().x + 4.0f,
                                                        file_menu.bounds().y + 4.0f);
    FIUI_TEST_ASSERT(file_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(!view_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(!view_tools_menu.expose_impl()->properties.menu_popup_open);
    const fiui::LayoutResult switched_from_nested_layout =
        layout_system.arrange(*menu_bar.expose_impl(), fiui::LayoutConstraints{320.0f, 26.0f});
    FIUI_TEST_ASSERT(switched_from_nested_layout.arranged_nodes == 5);
    FIUI_TEST_ASSERT(view_layout_menu.bounds().width == 0.0f);
    FIUI_TEST_ASSERT(view_layout_menu.bounds().height == 0.0f);
    const fiui::RenderFrameResult switched_from_nested_render =
        render_system.build_frame(*menu_bar.expose_impl());
    FIUI_TEST_ASSERT(find_text_command(switched_from_nested_render, view_layout_menu.object_id()) == nullptr);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x1b, 0);
    FIUI_TEST_ASSERT(!file_menu.expose_impl()->properties.menu_popup_open);

    FIUI_TEST_ASSERT(view_menu.click());
    fiui::default_runtime_set_event_focus_target(view_menu.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x28, 0);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x27, 0);
    FIUI_TEST_ASSERT(view_tools_menu.expose_impl()->properties.menu_popup_open);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x0d, 0);
    FIUI_TEST_ASSERT(submenu_click_count == 1);
    FIUI_TEST_ASSERT(!view_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(!view_tools_menu.expose_impl()->properties.menu_popup_open);

    FIUI_TEST_ASSERT(file_menu.click());
    const fiui::LayoutResult reopened_menu_layout =
        layout_system.arrange(*menu_bar.expose_impl(), fiui::LayoutConstraints{320.0f, 26.0f});
    FIUI_TEST_ASSERT(reopened_menu_layout.arranged_nodes == 5);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown,
                                                        file_new_menu.bounds().x + 4.0f,
                                                        file_new_menu.bounds().y + 4.0f);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp,
                                                        file_new_menu.bounds().x + 4.0f,
                                                        file_new_menu.bounds().y + 4.0f);
    FIUI_TEST_ASSERT(submenu_click_count == 2);
    FIUI_TEST_ASSERT(!file_menu.expose_impl()->properties.menu_popup_open);

    const fiui::LayoutResult closed_menu_layout =
        layout_system.arrange(*menu_bar.expose_impl(), fiui::LayoutConstraints{320.0f, 26.0f});
    FIUI_TEST_ASSERT(closed_menu_layout.arranged_nodes == 4);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerDown, help_menu.bounds().x + 4.0f, help_menu.bounds().y + 4.0f);
    fiui::default_runtime_record_platform_pointer_event(
        fiui::EventType::PointerUp, help_menu.bounds().x + 4.0f, help_menu.bounds().y + 4.0f);
    FIUI_TEST_ASSERT(menu_click_count == 1);
    fiui::default_runtime_clear_event_targets();

    TestMenuBar attributes_menu_bar;
    attributes_menu_bar.debug_id("attributes_menu_bar").size(360, 26);
    TestMenuItem attributes_menu("Options");
    attributes_menu.debug_id("attributes_menu");
    TestMenuItem checked_item("Checked item");
    checked_item.debug_id("checked_item");
    checked_item.checked(true).shortcut("Ctrl+K");
    int checked_shortcut_count = 0;
    checked_item.on_click(increment_int_callback, &checked_shortcut_count);
    TestSeparator menu_separator;
    menu_separator.debug_id("menu_separator");
    TestMenuItem disabled_item("Disabled item");
    disabled_item.debug_id("disabled_item");
    disabled_item.enabled(false).shortcut("Ctrl+D");
    int disabled_click_count = 0;
    disabled_item.on_click(increment_int_callback, &disabled_click_count);
    TestMenuItem hidden_item("Hidden item");
    hidden_item.debug_id("hidden_item");
    hidden_item.visible(false);
    hidden_item.shortcut("Ctrl+H");
    int hidden_click_count = 0;
    hidden_item.on_click(increment_int_callback, &hidden_click_count);
    FIUI_TEST_ASSERT(attributes_menu.add(checked_item));
    FIUI_TEST_ASSERT(attributes_menu.add(menu_separator));
    FIUI_TEST_ASSERT(attributes_menu.add(disabled_item));
    FIUI_TEST_ASSERT(attributes_menu.add(hidden_item));
    FIUI_TEST_ASSERT(attributes_menu_bar.add(attributes_menu));
    FIUI_TEST_ASSERT(attributes_menu.click());
    const fiui::LayoutResult attributes_layout = layout_system.arrange(
        *attributes_menu_bar.expose_impl(), fiui::LayoutConstraints{360.0f, 26.0f});
    FIUI_TEST_ASSERT(attributes_layout.arranged_nodes == 5);
    FIUI_TEST_ASSERT(menu_separator.bounds().height == 1.0f);
    const fiui::RenderFrameResult attributes_render =
        render_system.build_frame(*attributes_menu_bar.expose_impl());
    const fiui::DisplayCommand* checked_text_command =
        find_text_command(attributes_render, checked_item.object_id());
    const fiui::DisplayCommand* checked_shortcut_command =
        find_text_command_with_path(attributes_render, checked_item.object_id(), "/shortcut");
    const fiui::DisplayCommand* disabled_text_command =
        find_text_command(attributes_render, disabled_item.object_id());
    const fiui::DisplayCommand* disabled_shortcut_command =
        find_text_command_with_path(attributes_render, disabled_item.object_id(), "/shortcut");
    FIUI_TEST_ASSERT(find_text_command(attributes_render, hidden_item.object_id()) == nullptr);
    FIUI_TEST_ASSERT(find_text_command_with_path(attributes_render, hidden_item.object_id(),
                                                "/shortcut") == nullptr);
    FIUI_TEST_ASSERT(checked_text_command != nullptr);
    FIUI_TEST_ASSERT(checked_text_command->text.find("[x] Checked item") != std::string::npos);
    FIUI_TEST_ASSERT(checked_text_command->text.find("Ctrl+K") == std::string::npos);
    FIUI_TEST_ASSERT(checked_shortcut_command != nullptr);
    FIUI_TEST_ASSERT(checked_shortcut_command->text == "Ctrl+K");
    FIUI_TEST_ASSERT(std::strcmp(checked_shortcut_command->style.text_align, "trailing") == 0);
    FIUI_TEST_ASSERT(checked_shortcut_command->bounds.x > checked_text_command->bounds.x);
    FIUI_TEST_ASSERT(disabled_text_command != nullptr);
    FIUI_TEST_ASSERT(disabled_text_command->text.find("Ctrl+D") == std::string::npos);
    FIUI_TEST_ASSERT(disabled_shortcut_command != nullptr);
    FIUI_TEST_ASSERT(disabled_shortcut_command->text == "Ctrl+D");
    FIUI_TEST_ASSERT(disabled_item.click());
    FIUI_TEST_ASSERT(disabled_click_count == 0);
    fiui::default_runtime_bind_platform_root(*attributes_menu_bar.expose_impl());
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         'H' | test_key_ctrl, 0);
    FIUI_TEST_ASSERT(hidden_click_count == 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_hover_target() != hidden_item.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         'K' | test_key_ctrl, 0);
    FIUI_TEST_ASSERT(checked_shortcut_count == 1);
    FIUI_TEST_ASSERT(attributes_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(fiui::default_runtime_hover_target() == checked_item.object_id());
    const fiui::RenderFrameResult shortcut_highlight_render =
        render_system.build_frame(*attributes_menu_bar.expose_impl());
    const fiui::DisplayCommand* shortcut_highlight_rect =
        find_rect_command(shortcut_highlight_render, checked_item.object_id());
    FIUI_TEST_ASSERT(shortcut_highlight_rect != nullptr);
    FIUI_TEST_ASSERT(std::strcmp(shortcut_highlight_rect->style.control_state, "hover") == 0);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyUp, 'K', 0);
    FIUI_TEST_ASSERT(checked_shortcut_count == 1);
    FIUI_TEST_ASSERT(!attributes_menu.expose_impl()->properties.menu_popup_open);
    FIUI_TEST_ASSERT(fiui::default_runtime_hover_target() == 0);
    fiui::default_runtime_bind_platform_root(*attributes_menu_bar.expose_impl());
    fiui::default_runtime_record_platform_focus_lost();
    FIUI_TEST_ASSERT(!attributes_menu.expose_impl()->properties.menu_popup_open);

    write_test_bmp("fiui-test-wide.bmp", 100, 50);
    TestRow image_fit_root;
    image_fit_root.debug_id("image_fit_root").size(220, 100).gap(20.0f);
    TestImage contain_image("fiui-test-wide.bmp");
    contain_image.debug_id("contain_image");
    contain_image.size(100, 100);
    contain_image.fit(fiui::ImageFit::Contain);
    contain_image.radius(10.0f);
    TestImage cover_image("fiui-test-wide.bmp");
    cover_image.debug_id("cover_image");
    cover_image.size(100, 100);
    cover_image.fit(fiui::ImageFit::Cover);
    FIUI_TEST_ASSERT(image_fit_root.add(contain_image));
    FIUI_TEST_ASSERT(image_fit_root.add(cover_image));
    const fiui::LayoutResult image_fit_layout = layout_system.arrange(
        *image_fit_root.expose_impl(), fiui::LayoutConstraints{220.0f, 100.0f});
    FIUI_TEST_ASSERT(image_fit_layout.arranged_nodes == 3);
    const fiui::RenderFrameResult image_fit_render_result =
        render_system.build_frame(*image_fit_root.expose_impl());
    const fiui::DisplayCommand* contain_command =
        find_image_command(image_fit_render_result, contain_image.object_id());
    const fiui::DisplayCommand* cover_command =
        find_image_command(image_fit_render_result, cover_image.object_id());
    FIUI_TEST_ASSERT(contain_command != nullptr);
    FIUI_TEST_ASSERT(cover_command != nullptr);
    FIUI_TEST_ASSERT(contain_command->image_fit == fiui::ImageFit::Contain);
    FIUI_TEST_ASSERT(contain_command->bounds.width == 100.0f);
    FIUI_TEST_ASSERT(contain_command->bounds.height == 50.0f);
    FIUI_TEST_ASSERT(contain_command->bounds.y == 25.0f);
    FIUI_TEST_ASSERT(contain_command->image_uv.width == 1.0f);
    FIUI_TEST_ASSERT(cover_command->image_fit == fiui::ImageFit::Cover);
    FIUI_TEST_ASSERT(cover_command->bounds.width == 100.0f);
    FIUI_TEST_ASSERT(cover_command->bounds.height == 100.0f);
    FIUI_TEST_ASSERT(cover_command->image_uv.x == 0.25f);
    FIUI_TEST_ASSERT(cover_command->image_uv.width == 0.5f);
    FIUI_TEST_ASSERT(image_fit_render_result.backend.rounded_clip_command_count >= 1);
    FIUI_TEST_ASSERT(std::strcmp(fiui::image_fit_name(fiui::ImageFit::Cover), "cover") == 0);

    TestColumn multiline_root;
    multiline_root.debug_id("multiline_root").size(100, 70);
    fiui::Text multiline_text("alpha beta gamma delta epsilon zeta eta theta");
    multiline_text.debug_id("multiline_text");
    multiline_text.size(90, 60);
    multiline_text.multiline();
    FIUI_TEST_ASSERT(multiline_root.add(multiline_text));
    const fiui::LayoutResult multiline_layout = layout_system.arrange(
        *multiline_root.expose_impl(), fiui::LayoutConstraints{100.0f, 70.0f});
    FIUI_TEST_ASSERT(multiline_layout.arranged_nodes == 2);
    const fiui::RenderFrameResult multiline_render_result =
        render_system.build_frame(*multiline_root.expose_impl());
    const fiui::DisplayCommand* multiline_command =
        find_text_command(multiline_render_result, multiline_text.object_id());
    FIUI_TEST_ASSERT(multiline_command != nullptr);
    FIUI_TEST_ASSERT(std::strcmp(multiline_command->style.word_wrap, "word") == 0);
    FIUI_TEST_ASSERT(std::strcmp(multiline_command->style.overflow, "clip") == 0);
    FIUI_TEST_ASSERT(multiline_command->resource.text_metrics.valid);
    FIUI_TEST_ASSERT(multiline_command->resource.text_metrics.line_count > 1);
    FIUI_TEST_ASSERT(multiline_command->resource.text_metrics.baseline > 0.0f);

    TestColumn caret_root;
    caret_root.debug_id("caret_root").size(220, 80);
    TestInput caret_input;
    caret_input.debug_id("caret_input");
    caret_input.value("Cursor");
    FIUI_TEST_ASSERT(caret_root.add(caret_input));
    const fiui::LayoutResult caret_layout =
        layout_system.arrange(*caret_root.expose_impl(), fiui::LayoutConstraints{220.0f, 80.0f});
    FIUI_TEST_ASSERT(caret_layout.arranged_nodes == 2);
    fiui::default_runtime_clear_event_targets();
    const fiui::RenderFrameResult caret_unfocused_render =
        render_system.build_frame(*caret_root.expose_impl());
    FIUI_TEST_ASSERT(!has_input_caret_command(caret_unfocused_render, caret_input.object_id()));
    fiui::default_runtime_set_event_focus_target(caret_input.expose_impl());
    const fiui::RenderFrameResult caret_focused_render =
        render_system.build_frame(*caret_root.expose_impl());
    const fiui::DisplayCommand* caret_at_end =
        find_input_caret_command(caret_focused_render, caret_input.object_id());
    FIUI_TEST_ASSERT(caret_at_end != nullptr);
    const float caret_end_x = caret_at_end->bounds.x;
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x25, 0);
    FIUI_TEST_ASSERT(caret_input.cursor() == 5);
    const fiui::RenderFrameResult caret_moved_render =
        render_system.build_frame(*caret_root.expose_impl());
    const fiui::DisplayCommand* caret_after_left =
        find_input_caret_command(caret_moved_render, caret_input.object_id());
    FIUI_TEST_ASSERT(caret_after_left != nullptr);
    FIUI_TEST_ASSERT(caret_after_left->bounds.x < caret_end_x);
    const fiui::RuntimeEventRouteProbe caret_click_left =
        fiui::default_runtime_route_pointer_event(*caret_root.expose_impl(),
                                                  fiui::EventType::PointerDown,
                                                  caret_input.bounds().x + 1.0f,
                                                  caret_input.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(caret_click_left.hit);
    FIUI_TEST_ASSERT(caret_input.cursor() == 0);
    const fiui::RuntimeEventRouteProbe caret_click_right =
        fiui::default_runtime_route_pointer_event(*caret_root.expose_impl(),
                                                  fiui::EventType::PointerDown,
                                                  caret_input.bounds().x +
                                                      caret_input.bounds().width - 2.0f,
                                                  caret_input.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(caret_click_right.hit);
    FIUI_TEST_ASSERT(caret_input.cursor() == caret_input.value_text().size());
    const bool caret_visible_before_timer =
        has_input_caret_command(render_system.build_frame(*caret_root.expose_impl()),
                                caret_input.object_id());
    fiui::default_runtime_record_platform_timer_tick();
    const bool caret_visible_after_timer =
        has_input_caret_command(render_system.build_frame(*caret_root.expose_impl()),
                                caret_input.object_id());
    FIUI_TEST_ASSERT(caret_visible_before_timer != caret_visible_after_timer);
    const fiui::RuntimeEventRouteProbe select_down =
        fiui::default_runtime_route_pointer_event(*caret_root.expose_impl(),
                                                  fiui::EventType::PointerDown,
                                                  caret_input.bounds().x + 1.0f,
                                                  caret_input.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(select_down.hit);
    FIUI_TEST_ASSERT(caret_input.cursor() == 0);
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 0);
    FIUI_TEST_ASSERT(caret_input.selection_dragging());
    const fiui::RuntimeEventRouteProbe select_move =
        fiui::default_runtime_route_pointer_event(*caret_root.expose_impl(),
                                                  fiui::EventType::PointerMove,
                                                  caret_input.bounds().x +
                                                      caret_input.bounds().width - 2.0f,
                                                  caret_input.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(select_move.hit);
    FIUI_TEST_ASSERT(caret_input.cursor() == caret_input.value_text().size());
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 0);
    const fiui::RenderFrameResult selection_render =
        render_system.build_frame(*caret_root.expose_impl());
    const fiui::DisplayCommand* selection_command =
        find_input_selection_command(selection_render, caret_input.object_id());
    FIUI_TEST_ASSERT(selection_command != nullptr);
    FIUI_TEST_ASSERT(selection_command->bounds.width > 1.0f);
    const fiui::RuntimeEventRouteProbe select_up =
        fiui::default_runtime_route_pointer_event(*caret_root.expose_impl(),
                                                  fiui::EventType::PointerUp,
                                                  caret_input.bounds().x +
                                                      caret_input.bounds().width - 2.0f,
                                                  caret_input.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(select_up.hit);
    FIUI_TEST_ASSERT(!caret_input.selection_dragging());
    fiui::default_runtime_record_platform_clipboard_copy();
    FIUI_TEST_ASSERT(std::string(fiui::default_runtime_platform_clipboard_text()) == "Cursor");
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0, U'X');
    FIUI_TEST_ASSERT(caret_input.value_text() == "X");
    FIUI_TEST_ASSERT(caret_input.cursor() == 1);
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 1);
    caret_input.value("SelectAll");
    fiui::default_runtime_set_event_focus_target(caret_input.expose_impl());
    FIUI_TEST_ASSERT(fiui::default_runtime_select_focused_input_all());
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 0);
    FIUI_TEST_ASSERT(caret_input.cursor() == caret_input.value_text().size());
    const fiui::RenderFrameResult select_all_render =
        render_system.build_frame(*caret_root.expose_impl());
    FIUI_TEST_ASSERT(find_input_selection_command(select_all_render, caret_input.object_id()) != nullptr);
    fiui::default_runtime_record_platform_clipboard_copy();
    FIUI_TEST_ASSERT(std::string(fiui::default_runtime_platform_clipboard_text()) == "SelectAll");
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0, U'Z');
    FIUI_TEST_ASSERT(caret_input.value_text() == "Z");
    FIUI_TEST_ASSERT(caret_input.cursor() == 1);
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 1);
    caret_input.value("ABCDE");
    fiui::default_runtime_set_event_focus_target(caret_input.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         test_key_shift | 0x25, 0);
    FIUI_TEST_ASSERT(caret_input.cursor() == 4);
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 5);
    FIUI_TEST_ASSERT(find_input_selection_command(render_system.build_frame(*caret_root.expose_impl()),
                                        caret_input.object_id()) != nullptr);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         test_key_shift | 0x25, 0);
    FIUI_TEST_ASSERT(caret_input.cursor() == 3);
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 5);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x27, 0);
    FIUI_TEST_ASSERT(caret_input.cursor() == 5);
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 5);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x24, 0);
    FIUI_TEST_ASSERT(caret_input.cursor() == 0);
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 0);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         test_key_shift | 0x23, 0);
    FIUI_TEST_ASSERT(caret_input.cursor() == caret_input.value_text().size());
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 0);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x24, 0);
    FIUI_TEST_ASSERT(caret_input.cursor() == 0);
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 0);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         test_key_shift | 0x27, 0);
    FIUI_TEST_ASSERT(caret_input.cursor() == 1);
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 0);

    caret_input.value("fiui-control-center-runtime");
    caret_input.expose_impl()->properties.text_cursor = 4;
    caret_input.expose_impl()->properties.text_selection_anchor = 4;
    fiui::default_runtime_set_event_focus_target(caret_input.expose_impl());
    fiui::RenderFrameResult fiui_caret_render = render_system.build_frame(*caret_root.expose_impl());
    const fiui::DisplayCommand* fiui_caret =
        find_input_caret_command(fiui_caret_render, caret_input.object_id());
    if (fiui_caret == nullptr) {
        fiui::default_runtime_record_platform_timer_tick();
        fiui_caret_render = render_system.build_frame(*caret_root.expose_impl());
        fiui_caret = find_input_caret_command(fiui_caret_render, caret_input.object_id());
    }
    FIUI_TEST_ASSERT(fiui_caret != nullptr);
    caret_input.expose_impl()->properties.text_cursor = caret_input.value_text().size();
    caret_input.expose_impl()->properties.text_selection_anchor = caret_input.value_text().size();
    const fiui::RuntimeEventRouteProbe fiui_click =
        fiui::default_runtime_route_pointer_event(*caret_root.expose_impl(),
                                                  fiui::EventType::PointerDown,
                                                  fiui_caret->bounds.x,
                                                  fiui_caret->bounds.y + 2.0f);
    FIUI_TEST_ASSERT(fiui_click.hit);
    FIUI_TEST_ASSERT(caret_input.cursor() == 4);
    FIUI_TEST_ASSERT(caret_input.selection_anchor() == 4);

    TestColumn text_area_root;
    text_area_root.debug_id("text_area_root").size(260, 160);
    TestTextArea notes_input;
    notes_input.debug_id("notes_input");
    notes_input.placeholder("Runtime notes");
    notes_input.value("alpha\nbeta");
    int text_area_change_count = 0;
    notes_input.on_change(increment_int_callback, &text_area_change_count);
    notes_input.value("alpha\nbeta");
    FIUI_TEST_ASSERT(text_area_change_count == 0);
    FIUI_TEST_ASSERT(std::string(notes_input.value()) == "alpha\nbeta");
    FIUI_TEST_ASSERT(notes_input.kind() == fiui::WidgetKind::TextArea);
    FIUI_TEST_ASSERT(notes_input.expose_impl()->properties.text_multiline);
    FIUI_TEST_ASSERT(text_area_root.add(notes_input));
    const fiui::LayoutResult text_area_layout = layout_system.arrange(
        *text_area_root.expose_impl(), fiui::LayoutConstraints{260.0f, 160.0f});
    FIUI_TEST_ASSERT(text_area_layout.arranged_nodes == 2);
    FIUI_TEST_ASSERT(notes_input.bounds().height > caret_input.bounds().height);
    const fiui::RenderFrameResult text_area_render =
        render_system.build_frame(*text_area_root.expose_impl());
    const fiui::DisplayCommand* text_area_command =
        find_text_command(text_area_render, notes_input.object_id());
    FIUI_TEST_ASSERT(text_area_command != nullptr);
    FIUI_TEST_ASSERT(text_area_command->widget_kind == fiui::WidgetKind::TextArea);
    FIUI_TEST_ASSERT(std::strcmp(text_area_command->style.word_wrap, "word") == 0);
    FIUI_TEST_ASSERT(std::strcmp(text_area_command->style.overflow, "clip") == 0);
    FIUI_TEST_ASSERT(text_area_command->resource.text_metrics.line_count >= 2);
    fiui::default_runtime_clear_event_targets();
    const fiui::RuntimeEventRouteProbe text_area_click =
        fiui::default_runtime_route_pointer_event(*text_area_root.expose_impl(),
                                                  fiui::EventType::PointerDown,
                                                  notes_input.bounds().x + 8.0f,
                                                  notes_input.bounds().y + 8.0f);
    FIUI_TEST_ASSERT(text_area_click.hit);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == notes_input.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0, U'X');
    FIUI_TEST_ASSERT(notes_input.value_text().find('X') != std::string::npos);
    FIUI_TEST_ASSERT(text_area_change_count == 1);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x0d, 0);
    FIUI_TEST_ASSERT(notes_input.value_text().find('\n') != std::string::npos);
    FIUI_TEST_ASSERT(text_area_change_count == 2);
    const std::size_t cursor_after_newline = notes_input.cursor();
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x26, 0);
    FIUI_TEST_ASSERT(notes_input.cursor() < cursor_after_newline);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x28, 0);
    FIUI_TEST_ASSERT(notes_input.cursor() >= cursor_after_newline - 1);
    notes_input.value("one\ntwo\nthree\nfour\nfive\nsix\nseven\neight");
    FIUI_TEST_ASSERT(layout_system.arrange(*text_area_root.expose_impl(),
                                           fiui::LayoutConstraints{260.0f, 160.0f})
                         .arranged_nodes == 2);
    const float text_area_offset_before =
        notes_input.expose_impl()->properties.scroll_offset_y;
    fiui::default_runtime_route_wheel_event(*text_area_root.expose_impl(),
                                            notes_input.bounds().x + 12.0f,
                                            notes_input.bounds().y + 12.0f,
                                            -120.0f);
    FIUI_TEST_ASSERT(notes_input.expose_impl()->properties.scroll_offset_y >=
                     text_area_offset_before);
    FIUI_TEST_ASSERT(fiui::default_runtime_select_focused_input_all());
    FIUI_TEST_ASSERT(notes_input.selection_anchor() == 0);
    FIUI_TEST_ASSERT(notes_input.cursor() == notes_input.value_text().size());
    const fiui::RenderFrameResult text_area_select_all_render =
        render_system.build_frame(*text_area_root.expose_impl());
    FIUI_TEST_ASSERT(find_input_selection_command(text_area_select_all_render, notes_input.object_id()) !=
           nullptr);
    const fiui::DisplayCommand* text_area_caret =
        find_input_caret_command(text_area_select_all_render, notes_input.object_id());
    if (text_area_caret == nullptr) {
        fiui::default_runtime_record_platform_timer_tick();
        text_area_caret =
            find_input_caret_command(render_system.build_frame(*text_area_root.expose_impl()),
                                     notes_input.object_id());
    }
    FIUI_TEST_ASSERT(text_area_caret != nullptr);

    fiui::default_runtime_clear_event_targets();
    FIUI_TEST_ASSERT(fiui::display_command_kind_name(fiui::DisplayCommandKind::Rect) != nullptr);
    FIUI_TEST_ASSERT(fiui::display_command_kind_name(fiui::DisplayCommandKind::Opacity) != nullptr);
    FIUI_TEST_ASSERT(fiui::display_command_kind_name(fiui::DisplayCommandKind::OpacityEnd) != nullptr);
    FIUI_TEST_ASSERT(fiui::display_command_kind_name(fiui::DisplayCommandKind::Transform) != nullptr);
    FIUI_TEST_ASSERT(fiui::display_command_kind_name(fiui::DisplayCommandKind::TransformEnd) != nullptr);
    FIUI_TEST_ASSERT(fiui::display_command_kind_name(fiui::DisplayCommandKind::RoundedClip) != nullptr);
    FIUI_TEST_ASSERT(fiui::display_command_kind_name(fiui::DisplayCommandKind::RoundedClipEnd) != nullptr);
    FIUI_TEST_ASSERT(fiui::layer_kind_name(fiui::LayerKind::RoundedRect) != nullptr);
    const fiui::TextSystemState text_state_after_render =
        fiui::default_runtime_text_system_state();
    FIUI_TEST_ASSERT(text_state_after_render.measure_count >= 2);
    FIUI_TEST_ASSERT(text_state_after_render.factory_create_count >= 1);
    FIUI_TEST_ASSERT(text_state_after_render.factory_initialized);
    const fiui::ImageSystemState image_state_after_render =
        fiui::default_runtime_image_system_state();
    FIUI_TEST_ASSERT(image_state_after_render.metadata_query_count >= 1);
    FIUI_TEST_ASSERT(image_state_after_render.factory_create_count >= 1);
    FIUI_TEST_ASSERT(image_state_after_render.fallback_metadata_count >= 1);

    TestScrollView scroll_root;
    scroll_root.debug_id("scroll_root").size(120, 80);
    fiui::Button scroll_button("Scroll");
    scroll_button.debug_id("scroll_button");
    FIUI_TEST_ASSERT(scroll_root.add(scroll_button));
    const fiui::LayoutResult scroll_layout =
        layout_system.arrange(*scroll_root.expose_impl(), fiui::LayoutConstraints{120.0f, 80.0f});
    FIUI_TEST_ASSERT(scroll_layout.arranged_nodes == 2);
    const fiui::RenderFrameResult scroll_render_result =
        render_system.build_frame(*scroll_root.expose_impl());
    FIUI_TEST_ASSERT(has_layer_kind(scroll_render_result, fiui::LayerKind::Scroll));
    FIUI_TEST_ASSERT(has_layer_kind(scroll_render_result, fiui::LayerKind::Clip));
    FIUI_TEST_ASSERT(has_layer_kind(scroll_render_result, fiui::LayerKind::RoundedRect));
    FIUI_TEST_ASSERT(all_command_layers_are_linked(scroll_render_result));
    FIUI_TEST_ASSERT(scroll_render_result.backend.clip_command_count == 1);
    FIUI_TEST_ASSERT(scroll_render_result.backend.clip_end_command_count == 1);
    FIUI_TEST_ASSERT(scroll_render_result.backend.rounded_clip_command_count == 1);
    FIUI_TEST_ASSERT(scroll_render_result.backend.rounded_clip_end_command_count == 1);
    FIUI_TEST_ASSERT(fiui::display_command_kind_name(fiui::DisplayCommandKind::ClipEnd) != nullptr);
    FIUI_TEST_ASSERT(find_rect_command_with_path(scroll_render_result, scroll_root.object_id(),
                                       "/scrollbar_thumb") == nullptr);

    TestScrollView true_scroll;
    true_scroll.debug_id("true_scroll").size(120, 80);
    TestColumn true_scroll_content;
    true_scroll_content.debug_id("true_scroll_content").gap(6.0f);
    fiui::Button true_scroll_a("One");
    true_scroll_a.debug_id("true_scroll_a");
    fiui::Button true_scroll_b("Two");
    true_scroll_b.debug_id("true_scroll_b");
    fiui::Button true_scroll_c("Three");
    true_scroll_c.debug_id("true_scroll_c");
    fiui::Button true_scroll_d("Four");
    true_scroll_d.debug_id("true_scroll_d");
    fiui::Button true_scroll_e("Five");
    true_scroll_e.debug_id("true_scroll_e");
    FIUI_TEST_ASSERT(true_scroll_content.add(true_scroll_a));
    FIUI_TEST_ASSERT(true_scroll_content.add(true_scroll_b));
    FIUI_TEST_ASSERT(true_scroll_content.add(true_scroll_c));
    FIUI_TEST_ASSERT(true_scroll_content.add(true_scroll_d));
    FIUI_TEST_ASSERT(true_scroll_content.add(true_scroll_e));
    FIUI_TEST_ASSERT(true_scroll.add(true_scroll_content));
    const fiui::LayoutResult true_scroll_layout =
        layout_system.arrange(*true_scroll.expose_impl(), fiui::LayoutConstraints{120.0f, 80.0f});
    FIUI_TEST_ASSERT(true_scroll_layout.arranged_nodes == 7);
    FIUI_TEST_ASSERT(true_scroll.expose_impl()->properties.scroll_content_height >
           true_scroll.bounds().height);
    const float old_content_y = true_scroll_content.bounds().y;
    const fiui::RuntimeEventRouteProbe wheel_probe =
        fiui::default_runtime_route_wheel_event(*true_scroll.expose_impl(), 10.0f, 10.0f,
                                                -120.0f);
    FIUI_TEST_ASSERT(wheel_probe.hit);
    FIUI_TEST_ASSERT(wheel_probe.handled);
    FIUI_TEST_ASSERT(true_scroll.expose_impl()->properties.scroll_offset_y > 0.0f);
    FIUI_TEST_ASSERT(fiui::has_dirty_reason(true_scroll.dirty_reason(), fiui::DirtyReason::Input));
    FIUI_TEST_ASSERT(fiui::has_dirty_reason(true_scroll.dirty_reason(), fiui::DirtyReason::Layout));
    FIUI_TEST_ASSERT(fiui::has_dirty_reason(true_scroll.dirty_reason(), fiui::DirtyReason::Paint));
    const fiui::LayoutResult true_scroll_after_wheel =
        layout_system.arrange(*true_scroll.expose_impl(), fiui::LayoutConstraints{120.0f, 80.0f});
    FIUI_TEST_ASSERT(true_scroll_after_wheel.arranged_nodes == 7);
    FIUI_TEST_ASSERT(true_scroll_content.bounds().y < old_content_y);
    FIUI_TEST_ASSERT(true_scroll_content.expose_impl()->dirty.clip_bounds.height ==
           true_scroll.bounds().height);
    const fiui::RenderFrameResult true_scroll_render_result =
        render_system.build_frame(*true_scroll.expose_impl());
    const fiui::DisplayCommand* scroll_thumb =
        find_rect_command_with_path(true_scroll_render_result, true_scroll.object_id(),
                                    "/scrollbar_thumb");
    FIUI_TEST_ASSERT(scroll_thumb != nullptr);
    FIUI_TEST_ASSERT(scroll_thumb->bounds.x > true_scroll.bounds().x);
    FIUI_TEST_ASSERT(scroll_thumb->bounds.y > true_scroll.bounds().y);
    FIUI_TEST_ASSERT(scroll_thumb->bounds.height < true_scroll.bounds().height);
    FIUI_TEST_ASSERT(true_scroll_render_result.backend.rect_command_count >= 1);
    fiui::default_runtime_clear_event_targets();
    const float thumb_center_x = scroll_thumb->bounds.x + scroll_thumb->bounds.width * 0.5f;
    const float thumb_center_y = scroll_thumb->bounds.y + scroll_thumb->bounds.height * 0.5f;
    const float drag_start_offset = true_scroll.expose_impl()->properties.scroll_offset_y;
    const fiui::RuntimeEventRouteProbe thumb_down =
        fiui::default_runtime_route_pointer_event(*true_scroll.expose_impl(),
                                                  fiui::EventType::PointerDown,
                                                  thumb_center_x,
                                                  thumb_center_y);
    FIUI_TEST_ASSERT(thumb_down.hit);
    FIUI_TEST_ASSERT(thumb_down.handled);
    FIUI_TEST_ASSERT(thumb_down.capture_target == true_scroll.object_id());
    FIUI_TEST_ASSERT(true_scroll.expose_impl()->properties.scroll_thumb_dragging);
    const fiui::RuntimeEventRouteProbe thumb_move =
        fiui::default_runtime_route_pointer_event(*true_scroll.expose_impl(),
                                                  fiui::EventType::PointerMove,
                                                  thumb_center_x,
                                                  thumb_center_y + 18.0f);
    FIUI_TEST_ASSERT(thumb_move.hit);
    FIUI_TEST_ASSERT(thumb_move.handled);
    FIUI_TEST_ASSERT(true_scroll.expose_impl()->properties.scroll_offset_y > drag_start_offset);
    const fiui::RuntimeEventRouteProbe thumb_up =
        fiui::default_runtime_route_pointer_event(*true_scroll.expose_impl(),
                                                  fiui::EventType::PointerUp,
                                                  thumb_center_x,
                                                  thumb_center_y + 18.0f);
    FIUI_TEST_ASSERT(thumb_up.hit);
    FIUI_TEST_ASSERT(thumb_up.capture_target == 0);
    FIUI_TEST_ASSERT(!true_scroll.expose_impl()->properties.scroll_thumb_dragging);
    const float keyboard_start_offset = true_scroll.expose_impl()->properties.scroll_offset_y;
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_set_event_focus_target(true_scroll_content.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x22, 0);
    FIUI_TEST_ASSERT(true_scroll.expose_impl()->properties.scroll_offset_y > keyboard_start_offset);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x24, 0);
    FIUI_TEST_ASSERT(true_scroll.expose_impl()->properties.scroll_offset_y == 0.0f);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x23, 0);
    const float keyboard_max_offset =
        true_scroll.expose_impl()->properties.scroll_content_height - true_scroll.bounds().height;
    FIUI_TEST_ASSERT(true_scroll.expose_impl()->properties.scroll_offset_y >= keyboard_max_offset - 0.01f);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x21, 0);
    FIUI_TEST_ASSERT(true_scroll.expose_impl()->properties.scroll_offset_y < keyboard_max_offset);
    fiui::default_runtime_clear_event_targets();

    const std::uint32_t resource_count_after_render =
        fiui::default_runtime_live_resource_count();
    const fiui::RenderFrameResult repeated_render_result =
        render_system.build_frame(*render_root.expose_impl());
    FIUI_TEST_ASSERT(repeated_render_result.backend.command_count == render_result.backend.command_count);
    FIUI_TEST_ASSERT(fiui::default_runtime_live_resource_count() == resource_count_after_render);

    fiui::Window window("Test");
    window.debug_id("window").size(320, 240);
    FIUI_TEST_ASSERT(window.content(root).child_count() == 1);

    const fiui::FrameReport first = fiui::render_frame(window);
    FIUI_TEST_ASSERT(first.frame_id > 0);
    FIUI_TEST_ASSERT(first.original_dirty_rects > 0);
    const fiui::BackendDeviceState backend_after_first_frame =
        fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_first_frame.initialized);
    FIUI_TEST_ASSERT(backend_after_first_frame.device_available);
    FIUI_TEST_ASSERT(backend_after_first_frame.immediate_context_available);
    FIUI_TEST_ASSERT(!backend_after_first_frame.device_lost);
    FIUI_TEST_ASSERT(backend_after_first_frame.device_create_count >= 1);
    FIUI_TEST_ASSERT(backend_after_first_frame.frame_submit_count >= 1);
    FIUI_TEST_ASSERT(backend_after_first_frame.last_failure == fiui::BackendFailureReason::None);
    const fiui::FrameSchedulerState scheduler_after_first_frame =
        fiui::default_runtime_frame_scheduler_state();
    FIUI_TEST_ASSERT(!scheduler_after_first_frame.pending);
    FIUI_TEST_ASSERT(scheduler_after_first_frame.completed_count >= 1);
    FIUI_TEST_ASSERT(scheduler_after_first_frame.last_completed_frame_id == first.frame_id);

    const fiui::FrameReport idle = fiui::render_frame(window);
    FIUI_TEST_ASSERT(idle.frame_id > first.frame_id);
    FIUI_TEST_ASSERT(idle.original_dirty_rects == 0);
    const fiui::RuntimeSnapshot idle_snapshot = fiui::runtime_snapshot();
    FIUI_TEST_ASSERT(idle_snapshot.last_frame.frame_id == idle.frame_id);
    FIUI_TEST_ASSERT(idle_snapshot.last_frame.original_dirty_rects == idle.original_dirty_rects);
    FIUI_TEST_ASSERT(idle_snapshot.last_platform_event != nullptr);
    const fiui::BackendDeviceState backend_after_idle = fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_idle.frame_submit_count == backend_after_first_frame.frame_submit_count + 1);

    fiui::Window texture_window("Texture");
    texture_window.debug_id("texture_window").size(128, 96);
    fiui::Image missing_texture_image("missing-texture.png");
    missing_texture_image.debug_id("missing_texture");
    FIUI_TEST_ASSERT(texture_window.content(missing_texture_image).child_count() == 1);
    const fiui::BackendDeviceState backend_before_texture =
        fiui::default_runtime_backend_state();
    const fiui::FrameReport texture_frame = fiui::render_frame(texture_window);
    FIUI_TEST_ASSERT(texture_frame.frame_id > idle.frame_id);
    const fiui::BackendDeviceState backend_after_texture =
        fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_texture.texture_create_count >
           backend_before_texture.texture_create_count);
    FIUI_TEST_ASSERT(backend_after_texture.texture_upload_count >
           backend_before_texture.texture_upload_count);
    FIUI_TEST_ASSERT(backend_after_texture.texture_fallback_count >
           backend_before_texture.texture_fallback_count);
    FIUI_TEST_ASSERT(backend_after_texture.live_texture_count >= 1);
    fiui::flush_diagnostics();
    const std::string texture_frame_json =
        read_text_file("fiui-test-diagnostics/fiui-frame.json");
    FIUI_TEST_ASSERT(texture_frame_json.find("\"texture_metadata\"") != std::string::npos);
    FIUI_TEST_ASSERT(texture_frame_json.find("\"uploaded\":true") != std::string::npos);
    FIUI_TEST_ASSERT(texture_frame_json.find("\"fallback\":true") != std::string::npos);
    FIUI_TEST_ASSERT(texture_frame_json.find("\"format\":\"bgra8_unorm\"") != std::string::npos);
    FIUI_TEST_ASSERT(texture_frame_json.find("\"image_draw_count\":") != std::string::npos);

    fiui::default_runtime_simulate_backend_device_lost("backend recovery test");
    const fiui::BackendDeviceState backend_before_recovery =
        fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_before_recovery.device_lost);
    FIUI_TEST_ASSERT(fiui::default_runtime_recover_backend_device());
    const fiui::BackendDeviceState backend_after_recovery =
        fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_recovery.initialized);
    FIUI_TEST_ASSERT(backend_after_recovery.device_available);
    FIUI_TEST_ASSERT(backend_after_recovery.immediate_context_available);
    FIUI_TEST_ASSERT(!backend_after_recovery.device_lost);
    FIUI_TEST_ASSERT(backend_after_recovery.device_recovery_count >= 1);
    FIUI_TEST_ASSERT(backend_after_recovery.device_create_count > backend_after_idle.device_create_count);

#if defined(_WIN32)
    HWND backend_test_hwnd = create_hidden_backend_test_window();
    FIUI_TEST_ASSERT(backend_test_hwnd != nullptr);
    FIUI_TEST_ASSERT(fiui::default_runtime_bind_backend_window(backend_test_hwnd, 320, 240));
    const fiui::BackendDeviceState backend_after_bind = fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_bind.window_bound);
    FIUI_TEST_ASSERT(backend_after_bind.swap_chain_available);
    FIUI_TEST_ASSERT(backend_after_bind.render_target_available);
    const fiui::FrameReport draw_frame = fiui::render_frame(window);
    FIUI_TEST_ASSERT(draw_frame.frame_id > idle.frame_id);
    const fiui::BackendDeviceState backend_after_draw = fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_draw.render_target_clear_count >
           backend_after_bind.render_target_clear_count);
    FIUI_TEST_ASSERT(backend_after_draw.rect_draw_count > backend_after_bind.rect_draw_count);
    FIUI_TEST_ASSERT(backend_after_draw.rounded_rect_pipeline_create_count >
           backend_after_bind.rounded_rect_pipeline_create_count);
    FIUI_TEST_ASSERT(backend_after_draw.rounded_rect_draw_count >
           backend_after_bind.rounded_rect_draw_count);
    FIUI_TEST_ASSERT(backend_after_draw.shadow_draw_count > backend_after_bind.shadow_draw_count);
    FIUI_TEST_ASSERT(backend_after_draw.shadow_fallback_count >
           backend_after_bind.shadow_fallback_count);
    FIUI_TEST_ASSERT(backend_after_draw.opacity_command_count >
           backend_after_bind.opacity_command_count);
    FIUI_TEST_ASSERT(backend_after_draw.opacity_end_command_count >
           backend_after_bind.opacity_end_command_count);
    FIUI_TEST_ASSERT(backend_after_draw.opacity_apply_count > backend_after_bind.opacity_apply_count);
    FIUI_TEST_ASSERT(backend_after_draw.transform_command_count >
           backend_after_bind.transform_command_count);
    FIUI_TEST_ASSERT(backend_after_draw.transform_end_command_count >
           backend_after_bind.transform_end_command_count);
    FIUI_TEST_ASSERT(backend_after_draw.transform_apply_count >
           backend_after_bind.transform_apply_count);
    FIUI_TEST_ASSERT(backend_after_draw.present_count > backend_after_bind.present_count);

    fiui::Window text_window("Text");
    text_window.debug_id("text_window").size(180, 80);
    fiui::Text backend_text("Backend Text");
    backend_text.debug_id("backend_text");
    FIUI_TEST_ASSERT(text_window.content(backend_text).child_count() == 1);
    const fiui::BackendDeviceState backend_before_text_draw =
        fiui::default_runtime_backend_state();
    const fiui::FrameReport text_draw_frame = fiui::render_frame(text_window);
    FIUI_TEST_ASSERT(text_draw_frame.frame_id > draw_frame.frame_id);
    const fiui::BackendDeviceState backend_after_text_draw =
        fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_text_draw.text_pipeline_create_count >=
           backend_before_text_draw.text_pipeline_create_count);
    FIUI_TEST_ASSERT(backend_after_text_draw.text_pipeline_create_count > 0);
    FIUI_TEST_ASSERT(backend_after_text_draw.text_draw_count >
           backend_before_text_draw.text_draw_count);

    const fiui::BackendDeviceState backend_before_image_draw =
        fiui::default_runtime_backend_state();
    const fiui::FrameReport image_draw_frame = fiui::render_frame(texture_window);
    FIUI_TEST_ASSERT(image_draw_frame.frame_id > text_draw_frame.frame_id);
    const fiui::BackendDeviceState backend_after_image_draw =
        fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_image_draw.image_pipeline_create_count >
           backend_before_image_draw.image_pipeline_create_count);
    FIUI_TEST_ASSERT(backend_after_image_draw.image_draw_count >
           backend_before_image_draw.image_draw_count);
    FIUI_TEST_ASSERT(backend_after_image_draw.present_count > backend_before_image_draw.present_count);

    fiui::Window clip_window("Clip");
    clip_window.debug_id("clip_window").size(160, 120);
    TestScrollView clip_scroll;
    clip_scroll.debug_id("clip_scroll");
    TestScrollView nested_clip_scroll;
    nested_clip_scroll.debug_id("nested_clip_scroll");
    fiui::Button clip_button("Clip");
    clip_button.debug_id("clip_button");
    FIUI_TEST_ASSERT(nested_clip_scroll.add(clip_button));
    FIUI_TEST_ASSERT(clip_scroll.add(nested_clip_scroll));
    FIUI_TEST_ASSERT(clip_window.content(clip_scroll).child_count() == 1);
    const fiui::BackendDeviceState backend_before_clip_draw =
        fiui::default_runtime_backend_state();
    const fiui::FrameReport clip_draw_frame = fiui::render_frame(clip_window);
    FIUI_TEST_ASSERT(clip_draw_frame.frame_id > image_draw_frame.frame_id);
    const fiui::BackendDeviceState backend_after_clip_draw =
        fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_clip_draw.clip_command_count >
           backend_before_clip_draw.clip_command_count);
    FIUI_TEST_ASSERT(backend_after_clip_draw.clip_end_command_count >
           backend_before_clip_draw.clip_end_command_count);
    FIUI_TEST_ASSERT(backend_after_clip_draw.clip_apply_count >
           backend_before_clip_draw.clip_apply_count);
    FIUI_TEST_ASSERT(backend_after_clip_draw.rounded_clip_command_count >
           backend_before_clip_draw.rounded_clip_command_count);
    FIUI_TEST_ASSERT(backend_after_clip_draw.rounded_clip_end_command_count >
           backend_before_clip_draw.rounded_clip_end_command_count);
    FIUI_TEST_ASSERT(backend_after_clip_draw.rounded_clip_apply_count >
           backend_before_clip_draw.rounded_clip_apply_count);
    FIUI_TEST_ASSERT(backend_after_clip_draw.rounded_clip_fallback_count >
           backend_before_clip_draw.rounded_clip_fallback_count);
    FIUI_TEST_ASSERT(backend_after_clip_draw.clip_stack_depth >= 2);
    FIUI_TEST_ASSERT(backend_after_clip_draw.scissor_state_create_count > 0);
    fiui::default_runtime_release_backend_window_resources();
    DestroyWindow(backend_test_hwnd);
#endif

    button.on_click(throwing_callback);
    FIUI_TEST_ASSERT(!button.click());
    FIUI_TEST_ASSERT(fiui::default_runtime_last_event_target_object_id() == button.object_id());

    fiui::default_runtime_clear_event_targets();
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_hover_target() == 0);

    fiui::Application app;
    app.theme("modern.dark");
    FIUI_TEST_ASSERT(app.tokens().background.a == 255);
    FIUI_TEST_ASSERT(std::strcmp(fiui::default_runtime_resolved_theme_name("modern.dark"), app.theme()) == 0);
    FIUI_TEST_ASSERT(std::strcmp(fiui::default_runtime_active_theme_name(), "modern.dark") == 0);

    fiui::StyleSystem style_system;
    const fiui::Theme& dark_theme = style_system.resolve_theme("modern.dark");
    const fiui::ComponentStateStyle& normal_button =
        style_system.resolve_button_style(dark_theme, fiui::ControlState::Normal);
    const fiui::ComponentStateStyle& hover_button =
        style_system.resolve_button_style(dark_theme, fiui::ControlState::Hover);
    const fiui::ComponentStateStyle& pressed_button =
        style_system.resolve_button_style(dark_theme, fiui::ControlState::Pressed);
    const fiui::ComponentStateStyle& focused_button =
        style_system.resolve_button_style(dark_theme, fiui::ControlState::Focused);
    FIUI_TEST_ASSERT(dark_theme.colors.background.a == 255);
    FIUI_TEST_ASSERT(dark_theme.motion.normal_ms > 0.0f);
    FIUI_TEST_ASSERT(dark_theme.density.control_height >= 30.0f);
    FIUI_TEST_ASSERT(normal_button.radius == dark_theme.radius.md);
    FIUI_TEST_ASSERT(hover_button.border.a == 255);
    FIUI_TEST_ASSERT(pressed_button.border.a == 255);
    FIUI_TEST_ASSERT(focused_button.border_width > normal_button.border_width);
    FIUI_TEST_ASSERT(fiui::control_state_name(fiui::ControlState::Pressed) != nullptr);

    const fiui::RenderFrameResult dark_render_result =
        render_system.build_frame(*render_root.expose_impl());
    bool found_dark_button_rect_style = false;
    bool found_dark_text_style = false;
    for (const fiui::DisplayCommand& command : dark_render_result.display_list.commands) {
        if (command.kind == fiui::DisplayCommandKind::Rect &&
            command.widget_kind == fiui::WidgetKind::Button) {
            found_dark_button_rect_style =
                std::strcmp(command.style.theme_name, "modern.dark") == 0 &&
                same_color(command.style.fill, normal_button.background) &&
                same_color(command.style.border, normal_button.border) &&
                command.style.radius == normal_button.radius &&
                command.style.border_width == normal_button.border_width;
        }
        if (command.kind == fiui::DisplayCommandKind::Text &&
            command.widget_kind == fiui::WidgetKind::Text) {
            found_dark_text_style = std::strcmp(command.style.theme_name, "modern.dark") == 0 &&
                                    same_color(command.style.text, dark_theme.colors.text) &&
                                    command.style.font_size == dark_theme.typography.body;
        }
    }
    FIUI_TEST_ASSERT(found_dark_button_rect_style);
    FIUI_TEST_ASSERT(found_dark_text_style);

    TestColumn variant_root;
    variant_root.debug_id("variant_root").size(240, 120);
    fiui::Button primary_button("Primary");
    primary_button.debug_id("primary_button");
    primary_button.style("primary");
    fiui::Button danger_button("Danger");
    danger_button.debug_id("danger_button");
    danger_button.style("danger");
    FIUI_TEST_ASSERT(variant_root.add(primary_button));
    FIUI_TEST_ASSERT(variant_root.add(danger_button));
    const fiui::LayoutResult variant_layout =
        layout_system.arrange(*variant_root.expose_impl(), fiui::LayoutConstraints{240.0f, 120.0f});
    FIUI_TEST_ASSERT(variant_layout.arranged_nodes == 3);
    const fiui::RenderFrameResult variant_render_result =
        render_system.build_frame(*variant_root.expose_impl());
    const fiui::DisplayCommand* primary_rect =
        find_rect_command(variant_render_result, primary_button.object_id());
    const fiui::DisplayCommand* primary_text =
        find_text_command(variant_render_result, primary_button.object_id());
    const fiui::DisplayCommand* danger_rect =
        find_rect_command(variant_render_result, danger_button.object_id());
    const fiui::DisplayCommand* danger_text =
        find_text_command(variant_render_result, danger_button.object_id());
    FIUI_TEST_ASSERT(primary_rect != nullptr);
    FIUI_TEST_ASSERT(primary_text != nullptr);
    FIUI_TEST_ASSERT(danger_rect != nullptr);
    FIUI_TEST_ASSERT(danger_text != nullptr);
    FIUI_TEST_ASSERT(same_color(primary_rect->style.fill, dark_theme.colors.accent));
    FIUI_TEST_ASSERT(same_color(primary_text->style.text, fiui::Color{255, 255, 255, 255}));
    FIUI_TEST_ASSERT(same_color(danger_rect->style.fill, dark_theme.colors.error));
    FIUI_TEST_ASSERT(same_color(danger_text->style.text, fiui::Color{255, 255, 255, 255}));

    const std::uint32_t initial_resources = fiui::default_runtime_live_resource_count();
    const fiui::ResourceId font_resource =
        fiui::default_runtime_register_test_resource(fiui::ResourceKind::Font, "default");
    const fiui::ResourceId text_layout_resource =
        fiui::default_runtime_register_test_resource(fiui::ResourceKind::TextLayout, "title");
    const fiui::ResourceId texture_resource =
        fiui::default_runtime_register_test_resource(fiui::ResourceKind::D3DTexture, "texture");
    FIUI_TEST_ASSERT(font_resource != 0);
    FIUI_TEST_ASSERT(text_layout_resource != 0);
    FIUI_TEST_ASSERT(texture_resource != 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_live_resource_count() == initial_resources + 3);
    FIUI_TEST_ASSERT(fiui::default_runtime_release_resource(font_resource));
    FIUI_TEST_ASSERT(fiui::default_runtime_release_resource(text_layout_resource));
    FIUI_TEST_ASSERT(fiui::default_runtime_release_resource(texture_resource));
    FIUI_TEST_ASSERT(fiui::default_runtime_live_resource_count() == initial_resources);
    FIUI_TEST_ASSERT(!fiui::default_runtime_release_resource(texture_resource));
    FIUI_TEST_ASSERT(fiui::resource_kind_name(fiui::ResourceKind::ExternalTexture) != nullptr);
    FIUI_TEST_ASSERT(fiui::resource_cache_state_name(fiui::ResourceCacheState::Cached) != nullptr);

    {
        TestColumn image_root;
        image_root.debug_id("image_root").size(80, 80);
        fiui::Image image;
        image.debug_id("image");
        image.source("demo.png");
        FIUI_TEST_ASSERT(image_root.add(image));
        const fiui::LayoutResult image_layout =
            layout_system.arrange(*image_root.expose_impl(), fiui::LayoutConstraints{80.0f, 80.0f});
        FIUI_TEST_ASSERT(image_layout.arranged_nodes == 2);
        fiui::DirtyPlan image_dirty = dirty_tracker.plan(*image_root.expose_impl(), thresholds);
        FIUI_TEST_ASSERT(image_dirty.resource_dirty_count >= 1);
        FIUI_TEST_ASSERT(fiui::default_runtime_live_resource_count() == initial_resources + 1);
        image.source("demo2.png");
        FIUI_TEST_ASSERT(fiui::default_runtime_live_resource_count() == initial_resources + 1);
    }
    FIUI_TEST_ASSERT(fiui::default_runtime_live_resource_count() == initial_resources);

    const fiui::PlatformState initial_platform = fiui::default_runtime_platform_state();
    dirty_tracker.clear(*root.expose_impl());
    fiui::default_runtime_clear_event_targets();
    dirty_tracker.clear(*root.expose_impl());
    fiui::default_runtime_bind_platform_root(*root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerMove, 10.0f,
                                                        10.0f);
    fiui::PlatformState routed_platform = fiui::default_runtime_platform_state();
    FIUI_TEST_ASSERT(routed_platform.pointer_route_count == initial_platform.pointer_route_count + 1);
    FIUI_TEST_ASSERT(routed_platform.pointer_miss_count == initial_platform.pointer_miss_count);
    FIUI_TEST_ASSERT(fiui::default_runtime_hover_target() == button.object_id());
    fiui::FrameSchedulerState scheduler_after_pointer =
        fiui::default_runtime_frame_scheduler_state();
    FIUI_TEST_ASSERT(scheduler_after_pointer.pending);
    FIUI_TEST_ASSERT(scheduler_after_pointer.last_object_id == button.object_id());
    FIUI_TEST_ASSERT(scheduler_after_pointer.last_reason == "pointer_move" ||
           scheduler_after_pointer.last_reason == "input");
    FIUI_TEST_ASSERT(fiui::has_dirty_reason(button.dirty_reason(), fiui::DirtyReason::Input));
    FIUI_TEST_ASSERT(fiui::has_dirty_reason(button.dirty_reason(), fiui::DirtyReason::Paint));
    fiui::DirtyPlan input_dirty_plan = dirty_tracker.plan(*root.expose_impl(), thresholds);
    FIUI_TEST_ASSERT(input_dirty_plan.input_dirty_count >= 1);
    FIUI_TEST_ASSERT(input_dirty_plan.paint_dirty_count >= 1);
    const fiui::RenderFrameResult hover_render_result =
        render_system.build_frame(*root.expose_impl());
    FIUI_TEST_ASSERT(has_button_rect_state(hover_render_result, button.object_id(), "hover", hover_button));

    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown, 10.0f,
                                                        10.0f);
    routed_platform = fiui::default_runtime_platform_state();
    FIUI_TEST_ASSERT(routed_platform.pointer_route_count == initial_platform.pointer_route_count + 2);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == button.object_id());
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == button.object_id());
    const fiui::RenderFrameResult pressed_render_result =
        render_system.build_frame(*root.expose_impl());
    FIUI_TEST_ASSERT(has_button_rect_state(pressed_render_result, button.object_id(), "pressed",
                                 pressed_button));

    int platform_click_count = 0;
    button.on_click(increment_int_callback, &platform_click_count);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp, 10.0f,
                                                        10.0f);
    routed_platform = fiui::default_runtime_platform_state();
    FIUI_TEST_ASSERT(routed_platform.pointer_route_count == initial_platform.pointer_route_count + 3);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == 0);
    FIUI_TEST_ASSERT(platform_click_count == 1);

    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerMove, -10.0f,
                                                        -10.0f);
    routed_platform = fiui::default_runtime_platform_state();
    FIUI_TEST_ASSERT(routed_platform.pointer_miss_count == initial_platform.pointer_miss_count + 1);
    FIUI_TEST_ASSERT(fiui::has_dirty_reason(button.dirty_reason(), fiui::DirtyReason::Input));
    const fiui::RenderFrameResult focused_render_result =
        render_system.build_frame(*root.expose_impl());
    FIUI_TEST_ASSERT(has_button_rect_state(focused_render_result, button.object_id(), "focused",
                                 focused_button));

    TestColumn focus_root;
    focus_root.debug_id("focus_root").size(260, 180).gap(4.0f);
    TestInput focus_input_a;
    focus_input_a.debug_id("focus_input_a");
    TestMenuItem disabled_focus_item("Disabled");
    disabled_focus_item.debug_id("disabled_focus_item");
    disabled_focus_item.enabled(false);
    TestButton focus_button("Next");
    focus_button.debug_id("focus_button");
    TestInput focus_input_b;
    focus_input_b.debug_id("focus_input_b");
    FIUI_TEST_ASSERT(focus_root.add(focus_input_a));
    FIUI_TEST_ASSERT(focus_root.add(disabled_focus_item));
    FIUI_TEST_ASSERT(focus_root.add(focus_button));
    FIUI_TEST_ASSERT(focus_root.add(focus_input_b));
    FIUI_TEST_ASSERT(layout_system.arrange(*focus_root.expose_impl(),
                                 fiui::LayoutConstraints{260.0f, 180.0f})
               .arranged_nodes == 5);
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*focus_root.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == focus_input_a.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == focus_button.object_id());
    int keyboard_activate_count = 0;
    focus_button.on_click(increment_int_callback, &keyboard_activate_count);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x20, 0);
    FIUI_TEST_ASSERT(keyboard_activate_count == 1);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x0d, 0);
    FIUI_TEST_ASSERT(keyboard_activate_count == 2);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == focus_input_b.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         test_key_shift | 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == focus_button.object_id());

    TestColumn input_root;
    input_root.debug_id("input_root").size(180, 80);
    TestInput platform_input;
    platform_input.debug_id("platform_input");
    platform_input.placeholder("Name");
    FIUI_TEST_ASSERT(input_root.add(platform_input));
    const fiui::LayoutResult platform_input_layout =
        layout_system.arrange(*input_root.expose_impl(), fiui::LayoutConstraints{180.0f, 80.0f});
    FIUI_TEST_ASSERT(platform_input_layout.arranged_nodes == 2);
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*input_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown, 10.0f,
                                                        10.0f);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == platform_input.object_id());
    const fiui::PlatformState before_keyboard_platform =
        fiui::default_runtime_platform_state();
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0, U'A');
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0, U'B');
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x25, 0);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0, U'x');
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x08, 0);
    const fiui::PlatformState after_keyboard_platform =
        fiui::default_runtime_platform_state();
    FIUI_TEST_ASSERT(platform_input.value_text() == "AB");
    FIUI_TEST_ASSERT(platform_input.cursor() == 1);
    FIUI_TEST_ASSERT(after_keyboard_platform.keyboard_event_count ==
           before_keyboard_platform.keyboard_event_count + 5);
    FIUI_TEST_ASSERT(after_keyboard_platform.keyboard_route_count ==
           before_keyboard_platform.keyboard_route_count + 5);
    FIUI_TEST_ASSERT(after_keyboard_platform.keyboard_miss_count ==
           before_keyboard_platform.keyboard_miss_count);
    FIUI_TEST_ASSERT(after_keyboard_platform.text_input_count ==
           before_keyboard_platform.text_input_count + 3);
    FIUI_TEST_ASSERT(fiui::has_dirty_reason(platform_input.dirty_reason(), fiui::DirtyReason::Input));
    FIUI_TEST_ASSERT(fiui::has_dirty_reason(platform_input.dirty_reason(), fiui::DirtyReason::TextChanged));
    FIUI_TEST_ASSERT(fiui::event_type_name(fiui::EventType::TextInput) != nullptr);

    const fiui::PlatformState before_clipboard_platform =
        fiui::default_runtime_platform_state();
    fiui::default_runtime_set_platform_clipboard_text("YZ");
    fiui::default_runtime_record_platform_clipboard_paste();
    FIUI_TEST_ASSERT(platform_input.value_text() == "AYZB");
    FIUI_TEST_ASSERT(platform_input.cursor() == 3);
    fiui::default_runtime_record_platform_clipboard_copy();
    FIUI_TEST_ASSERT(std::string(fiui::default_runtime_platform_clipboard_text()) == "AYZB");
    const fiui::PlatformState after_clipboard_platform =
        fiui::default_runtime_platform_state();
    FIUI_TEST_ASSERT(after_clipboard_platform.clipboard_paste_count ==
           before_clipboard_platform.clipboard_paste_count + 1);
    FIUI_TEST_ASSERT(after_clipboard_platform.clipboard_copy_count ==
           before_clipboard_platform.clipboard_copy_count + 1);
    FIUI_TEST_ASSERT(after_clipboard_platform.clipboard_read_count ==
           before_clipboard_platform.clipboard_read_count + 2);
    FIUI_TEST_ASSERT(after_clipboard_platform.clipboard_write_count ==
           before_clipboard_platform.clipboard_write_count + 2);
    FIUI_TEST_ASSERT(after_clipboard_platform.clipboard_failure_count ==
           before_clipboard_platform.clipboard_failure_count);

    platform_input.value("");
    fiui::default_runtime_set_event_focus_target(platform_input.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0,
                                                         static_cast<char32_t>(0x4f60));
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0,
                                                         static_cast<char32_t>(0x597d));
    const std::string utf8_ni = "\xe4\xbd\xa0";
    const std::string utf8_hao = "\xe5\xa5\xbd";
    const std::string utf8_shi = "\xe4\xb8\x96";
    const std::string utf8_jie = "\xe7\x95\x8c";
    FIUI_TEST_ASSERT(platform_input.value_text() == utf8_ni + utf8_hao);
    FIUI_TEST_ASSERT(platform_input.cursor() == (utf8_ni + utf8_hao).size());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x25, 0);
    FIUI_TEST_ASSERT(platform_input.cursor() == utf8_ni.size());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x08, 0);
    FIUI_TEST_ASSERT(platform_input.value_text() == utf8_hao);
    FIUI_TEST_ASSERT(platform_input.cursor() == 0);
    fiui::default_runtime_set_platform_clipboard_text((utf8_shi + utf8_jie).c_str());
    fiui::default_runtime_record_platform_clipboard_paste();
    FIUI_TEST_ASSERT(platform_input.value_text() == utf8_shi + utf8_jie + utf8_hao);
    FIUI_TEST_ASSERT(platform_input.cursor() == (utf8_shi + utf8_jie).size());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x2e, 0);
    FIUI_TEST_ASSERT(platform_input.value_text() == utf8_shi + utf8_jie);
    FIUI_TEST_ASSERT(platform_input.cursor() == (utf8_shi + utf8_jie).size());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x08, 0);
    FIUI_TEST_ASSERT(platform_input.value_text() == utf8_shi);
    FIUI_TEST_ASSERT(platform_input.cursor() == utf8_shi.size());

    const fiui::PlatformState before_ime_platform = fiui::default_runtime_platform_state();
    fiui::default_runtime_record_platform_ime_event(fiui::PlatformImePhase::StartComposition);
    fiui::default_runtime_record_platform_ime_event(fiui::PlatformImePhase::Composition);
    fiui::default_runtime_record_platform_ime_event(fiui::PlatformImePhase::EndComposition);
    const fiui::PlatformState after_ime_platform = fiui::default_runtime_platform_state();
    FIUI_TEST_ASSERT(after_ime_platform.ime_event_count == before_ime_platform.ime_event_count + 3);
    FIUI_TEST_ASSERT(after_ime_platform.ime_start_count == before_ime_platform.ime_start_count + 1);
    FIUI_TEST_ASSERT(after_ime_platform.ime_composition_count ==
           before_ime_platform.ime_composition_count + 1);
    FIUI_TEST_ASSERT(after_ime_platform.ime_end_count == before_ime_platform.ime_end_count + 1);
    FIUI_TEST_ASSERT(fiui::platform_ime_phase_name(fiui::PlatformImePhase::Composition) != nullptr);

    fiui::ObjectId destroyed_focus_id = 0;
    std::uint32_t destroyed_focus_generation = 0;
    {
        TestInput transient_input;
        transient_input.debug_id("transient_input");
        destroyed_focus_id = transient_input.object_id();
        destroyed_focus_generation = transient_input.generation();
        FIUI_TEST_ASSERT(fiui::default_runtime_lookup_object(destroyed_focus_id,
                                                   destroyed_focus_generation));
        fiui::default_runtime_set_event_focus_target(transient_input.expose_impl());
        FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == destroyed_focus_id);
    }
    FIUI_TEST_ASSERT(!fiui::default_runtime_lookup_object(destroyed_focus_id, destroyed_focus_generation));
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == 0);
    const fiui::PlatformState before_destroyed_keyboard_platform =
        fiui::default_runtime_platform_state();
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0, U'Z');
    fiui::default_runtime_record_platform_clipboard_paste();
    const fiui::PlatformState after_destroyed_keyboard_platform =
        fiui::default_runtime_platform_state();
    FIUI_TEST_ASSERT(after_destroyed_keyboard_platform.keyboard_miss_count ==
           before_destroyed_keyboard_platform.keyboard_miss_count + 1);
    FIUI_TEST_ASSERT(after_destroyed_keyboard_platform.clipboard_failure_count ==
           before_destroyed_keyboard_platform.clipboard_failure_count + 1);

    fiui::default_runtime_record_platform_resize(640, 480);
    fiui::default_runtime_record_platform_dpi_changed(144);
    fiui::default_runtime_record_platform_input(fiui::PlatformEventType::Mouse);
    fiui::default_runtime_record_platform_timer_tick();
    fiui::default_runtime_record_platform_device_lost("test device lost");
    const fiui::PlatformState platform_state = fiui::default_runtime_platform_state();
    FIUI_TEST_ASSERT(platform_state.width == 640);
    FIUI_TEST_ASSERT(platform_state.height == 480);
    FIUI_TEST_ASSERT(platform_state.dpi == 144);
    FIUI_TEST_ASSERT(platform_state.resize_count == initial_platform.resize_count + 1);
    FIUI_TEST_ASSERT(platform_state.input_event_count == initial_platform.input_event_count + 31);
    FIUI_TEST_ASSERT(platform_state.pointer_route_count == initial_platform.pointer_route_count + 4);
    FIUI_TEST_ASSERT(platform_state.pointer_miss_count == initial_platform.pointer_miss_count + 1);
    FIUI_TEST_ASSERT(platform_state.keyboard_event_count == initial_platform.keyboard_event_count + 18);
    FIUI_TEST_ASSERT(platform_state.keyboard_route_count == initial_platform.keyboard_route_count + 17);
    FIUI_TEST_ASSERT(platform_state.keyboard_miss_count == initial_platform.keyboard_miss_count + 1);
    FIUI_TEST_ASSERT(platform_state.text_input_count == initial_platform.text_input_count + 6);
    FIUI_TEST_ASSERT(platform_state.clipboard_paste_count == initial_platform.clipboard_paste_count + 3);
    FIUI_TEST_ASSERT(platform_state.clipboard_copy_count == initial_platform.clipboard_copy_count + 1);
    FIUI_TEST_ASSERT(platform_state.clipboard_failure_count == initial_platform.clipboard_failure_count + 1);
    FIUI_TEST_ASSERT(platform_state.ime_event_count == initial_platform.ime_event_count + 3);
    FIUI_TEST_ASSERT(platform_state.timer_tick_count == initial_platform.timer_tick_count + 1);
    FIUI_TEST_ASSERT(platform_state.device_lost_count == initial_platform.device_lost_count + 1);
    FIUI_TEST_ASSERT(platform_state.last_event == fiui::PlatformEventType::DeviceLost);
    FIUI_TEST_ASSERT(fiui::platform_event_type_name(fiui::PlatformEventType::DpiChanged) != nullptr);
    const fiui::FrameSchedulerState scheduler_after_platform =
        fiui::default_runtime_frame_scheduler_state();
    FIUI_TEST_ASSERT(scheduler_after_platform.pending);
    FIUI_TEST_ASSERT(scheduler_after_platform.requested_count >= scheduler_after_pointer.requested_count);
    FIUI_TEST_ASSERT(scheduler_after_platform.last_reason == "device_lost");
    fiui::default_runtime_record_platform_dpi_changed(96);

    fiui::Window live_window("Live");
    live_window.debug_id("live_window").size(220, 120);
    TestColumn live_root;
    live_root.debug_id("live_root").size(220, 120);
    fiui::Button live_button("Live Click");
    live_button.debug_id("live_button");
    int live_click_count = 0;
    live_button.on_click(increment_int_callback, &live_click_count);
    FIUI_TEST_ASSERT(live_root.add(live_button));
    FIUI_TEST_ASSERT(live_window.content(live_root).child_count() == 1);
    const fiui::FrameReport live_initial_frame = fiui::render_frame(live_window);
    FIUI_TEST_ASSERT(live_initial_frame.frame_id > 0);
    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_window_model(live_window);
    fiui::default_runtime_bind_platform_root(*live_root.expose_impl());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown, 10.0f,
                                                        10.0f);
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerUp, 10.0f,
                                                        10.0f);
    FIUI_TEST_ASSERT(live_click_count == 1);

    fiui::default_runtime_simulate_backend_device_lost("backend test device lost");
    const fiui::BackendDeviceState backend_after_device_lost =
        fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_device_lost.initialized);
    FIUI_TEST_ASSERT(backend_after_device_lost.device_lost);
    FIUI_TEST_ASSERT(backend_after_device_lost.device_lost_count >= 1);
    FIUI_TEST_ASSERT(backend_after_device_lost.last_failure == fiui::BackendFailureReason::DeviceLost);
    const fiui::FrameReport device_lost_frame = fiui::render_frame(window);
    FIUI_TEST_ASSERT(device_lost_frame.frame_id > idle.frame_id);
    const fiui::BackendDeviceState backend_after_failed_submit =
        fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend_after_failed_submit.device_lost);
    FIUI_TEST_ASSERT(backend_after_failed_submit.frame_submit_count ==
           backend_after_device_lost.frame_submit_count);
    FIUI_TEST_ASSERT(std::strcmp(fiui::backend_failure_reason_name(fiui::BackendFailureReason::DeviceLost),
                       "device_lost") == 0);

    TestColumn late_duplicate_attach_root;
    late_duplicate_attach_root.debug_id("late_duplicate_attach_root");
    fiui::Button late_duplicate_attach_button("Late duplicate");
    late_duplicate_attach_button.debug_id("late_duplicate_attach_button");
    fiui::Button late_duplicate_attach_copy = late_duplicate_attach_button;
    FIUI_TEST_ASSERT(late_duplicate_attach_root.add(late_duplicate_attach_button));
    FIUI_TEST_ASSERT(!late_duplicate_attach_root.add(late_duplicate_attach_copy));
    FIUI_TEST_ASSERT(fiui::default_runtime_lookup_object(late_duplicate_attach_button.object_id(),
                                               late_duplicate_attach_button.generation()));
    FIUI_TEST_ASSERT(!fiui::default_runtime_lookup_object(late_duplicate_attach_button.object_id(),
                                                late_duplicate_attach_button.generation() + 1));
    fiui::ObjectId late_dead_object_id = 0;
    std::uint32_t late_dead_generation = 0;
    {
        fiui::Button late_dead_button("Late dead");
        late_dead_button.debug_id("late_dead_button");
        late_dead_object_id = late_dead_button.object_id();
        late_dead_generation = late_dead_button.generation();
    }
    FIUI_TEST_ASSERT(!fiui::default_runtime_lookup_object(late_dead_object_id, late_dead_generation));

    fiui::flush_diagnostics();

    const std::string trace_json = read_text_file("fiui-test-diagnostics/fiui-trace.jsonl");
    FIUI_TEST_ASSERT(trace_json.find("\"schema\":\"fiui.trace.v0\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"session_id\":\"fiui-session-") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"frame_id\":") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"generation\":") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"duplicate_attach\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"callback_exception\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"hit_test\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"hit_test_miss\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"route_pointer\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"focus_target\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"capture_target\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"category\":\"platform\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"category\":\"object\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"object_register\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"object_unregister\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"object_lookup\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"object_lookup_dead\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"stale_generation\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"bind_root\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"pointer_routed\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"pointer_missed\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"click_dispatched\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"render_after_input\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"keyboard_routed\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"route_keyboard\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"text_insert\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"text_backspace\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"text_cursor_left\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"clipboard_write\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"clipboard_read\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"clipboard_paste\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"clipboard_copy\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"route_text_input\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"ime_event\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("ime_start_composition") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("ime_end_composition") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"resize\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"device_lost\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"cache_hit\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"category\":\"text\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"measure\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"text_metrics\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"category\":\"image\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"metadata_fallback\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"image_metadata\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"category\":\"scheduler\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"request_frame\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"complete_frame\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"category\":\"backend\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"initialize\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"submit_begin\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"submit_end\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"submit_failed\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"recover\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"pipeline_create\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"rounded_rect_pipeline_create\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"draw_rounded_rect\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"draw_shadow\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"shadow_fallback\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"opacity_push\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"opacity_pop\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"transform_push\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"transform_pop\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"text_pipeline_create\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"text_target_create\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"draw_text\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"draw_texts\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"image_pipeline_create\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"draw_rects\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"draw_image\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"draw_images\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"clip_command\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"clip_push\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"clip_pop\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"clip_apply\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"rounded_clip_command\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"rounded_clip_apply\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"rounded_clip_fallback\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"rounded_clip_pop\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"clip_pipeline_create\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"present\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"texture_create\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"texture_upload\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"texture_fallback\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"texture_release\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace_json.find("\"action\":\"texture_metadata\"") != std::string::npos);

    const std::string frame_json = read_text_file("fiui-test-diagnostics/fiui-frame.json");
    FIUI_TEST_ASSERT(frame_json.find("\"schema\": \"fiui.frame.v0\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"session_id\": \"fiui-session-") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"generation\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"layout\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"width_mode\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"height_mode\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"overflow_policy\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"overflow_policy_explicit\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"requested_width\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"requested_height\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"flex_grow\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"paint_bounds\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"clip_bounds\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"child_union_bounds\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"overflow_x\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"overflow_y\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"overflow_right\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"overflow_bottom\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"layer_tree_summary\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"layer_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"kind_counts\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"rounded_rect\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"opacity\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"transform\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"shadow\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"display_command_index\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"backend_summary\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"backend_device_summary\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"failure_reason\":\"device_lost\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"backend_name\":\"d3d11\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"device_available\":true") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"immediate_context_available\":true") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"device_recovery_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"headless_submit_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"render_target_clear_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"rect_draw_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"rounded_rect_draw_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"rounded_rect_pipeline_create_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"shadow_command_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"shadow_draw_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"shadow_fallback_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"opacity_command_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"opacity_end_command_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"opacity_apply_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"transform_command_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"transform_end_command_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"transform_apply_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"text_draw_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"text_pipeline_create_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"text_draw_failure_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"image_draw_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"image_pipeline_create_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"clip_command_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"clip_end_command_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"clip_apply_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"clip_stack_depth\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"rounded_clip_command_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"rounded_clip_end_command_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"rounded_clip_apply_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"rounded_clip_fallback_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"scissor_state_create_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"present_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"unsupported_draw_command_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"texture_create_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"texture_upload_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"texture_fallback_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"texture_release_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"live_texture_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"last_hresult\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"scheduler_summary\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"last_reason\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"style\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"state\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"theme\":\"modern.dark\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"resource\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"kind\":\"text_layout\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"cache_state\":\"cached\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"text_metrics\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"directwrite\":true") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"line_count\":") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"dpi\":") != std::string::npos);

    const std::string leaks_json = read_text_file("fiui-test-diagnostics/fiui-leaks.json");
    FIUI_TEST_ASSERT(leaks_json.find("\"schema\": \"fiui.leaks.v0\"") != std::string::npos);
    FIUI_TEST_ASSERT(leaks_json.find("\"session_id\": \"fiui-session-") != std::string::npos);
    return 0;
}
