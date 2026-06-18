#include <fiui/fiui.h>

#include "backend/d3d11_backend.h"
#include "core/widget_impl.h"
#include "layout/layout_system.h"
#include "render/render_system.h"
#include "runtime/runtime.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
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

constexpr std::uint32_t key_shift = 1u << 16;
constexpr std::uint32_t key_ctrl = 1u << 18;

class TestColumn : public fiui::Column {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept { return impl(); }
};

class TestRow : public fiui::Row {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept { return impl(); }
};

class TestScrollView : public fiui::ScrollView {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept { return impl(); }
};

class TestSplitView : public fiui::SplitView {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept { return impl(); }
};

class TestInput : public fiui::Input {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept { return impl(); }
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

class TestTextArea : public fiui::TextArea {
public:
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept { return impl(); }
    [[nodiscard]] const std::string& value_text() const noexcept
    {
        return impl()->properties.text;
    }
};

class TestButton : public fiui::Button {
public:
    explicit TestButton(const char* label = "")
        : fiui::Button(label)
    {
    }
    [[nodiscard]] fiui::WidgetImpl* expose_impl() const noexcept { return impl(); }
};

std::string read_text_file(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

void increment(void* user_data)
{
    int* value = static_cast<int*>(user_data);
    if (value != nullptr) {
        ++(*value);
    }
}

bool has_selection_command(const fiui::RenderFrameResult& frame, fiui::ObjectId object_id)
{
    for (const fiui::DisplayCommand& command : frame.display_list.commands) {
        if (command.object_id == object_id &&
            command.widget_kind == fiui::WidgetKind::Input &&
            command.kind == fiui::DisplayCommandKind::Rect &&
            command.path.find("/selection") != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::uint32_t count_commands(const fiui::RenderFrameResult& frame, fiui::DisplayCommandKind kind)
{
    std::uint32_t count = 0;
    for (const fiui::DisplayCommand& command : frame.display_list.commands) {
        if (command.kind == kind) {
            ++count;
        }
    }
    return count;
}

std::uint32_t count_layers(const fiui::RenderFrameResult& frame, fiui::LayerKind kind)
{
    std::uint32_t count = 0;
    for (const fiui::LayerNode& layer : frame.layer_tree.nodes) {
        if (layer.kind == kind) {
            ++count;
        }
    }
    return count;
}

#if defined(_WIN32)
HWND create_hidden_test_window()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* class_name = L"fiui_stability_hidden_window";
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    RegisterClassW(&wc);
    return CreateWindowExW(0, class_name, L"fiui stability backend", WS_OVERLAPPEDWINDOW,
                           0, 0, 240, 160, nullptr, nullptr, instance, nullptr);
}
#endif

void verify_layout_contract()
{
    fiui::LayoutSystem layout;

    TestRow row;
    row.debug_id("stable_row").size(500.0f, 100.0f).padding(10.0f).gap(10.0f);
    TestButton fixed("Fixed");
    fixed.debug_id("fixed").size(120.0f, 0.0f);
    TestButton auto_button("Auto");
    auto_button.debug_id("auto");
    TestButton flex_button("Flex");
    flex_button.debug_id("flex").flex(1.0f);
    FIUI_TEST_ASSERT(row.add(fixed));
    FIUI_TEST_ASSERT(row.add(auto_button));
    FIUI_TEST_ASSERT(row.add(flex_button));
    FIUI_TEST_ASSERT(layout.arrange(*row.expose_impl(), fiui::LayoutConstraints{500.0f, 100.0f})
               .arranged_nodes == 4);
    FIUI_TEST_ASSERT(fixed.bounds().width == 120.0f);
    FIUI_TEST_ASSERT(auto_button.bounds().width > 0.0f);
    FIUI_TEST_ASSERT(flex_button.bounds().width > auto_button.bounds().width);
    FIUI_TEST_ASSERT(fixed.bounds().height == 80.0f);

    TestColumn column;
    column.debug_id("stable_column").size(180.0f, 360.0f).padding(12.0f).gap(8.0f);
    TestButton top("Top");
    top.debug_id("top").size(96.0f, 40.0f);
    TestButton fill("Fill");
    fill.debug_id("fill").fill_height();
    FIUI_TEST_ASSERT(column.add(top));
    FIUI_TEST_ASSERT(column.add(fill));
    FIUI_TEST_ASSERT(layout.arrange(*column.expose_impl(), fiui::LayoutConstraints{180.0f, 360.0f})
               .arranged_nodes == 3);
    FIUI_TEST_ASSERT(top.bounds().width == 96.0f);
    FIUI_TEST_ASSERT(fill.bounds().width == 156.0f);
    FIUI_TEST_ASSERT(fill.bounds().height > top.bounds().height);

    TestScrollView scroll;
    scroll.debug_id("scroll").size(160.0f, 100.0f);
    TestColumn scroll_content;
    scroll_content.debug_id("scroll_content").size(0.0f, 260.0f);
    FIUI_TEST_ASSERT(scroll.add(scroll_content));
    FIUI_TEST_ASSERT(layout.arrange(*scroll.expose_impl(), fiui::LayoutConstraints{160.0f, 100.0f})
               .arranged_nodes == 2);
    FIUI_TEST_ASSERT(scroll_content.bounds().height == 260.0f);
    FIUI_TEST_ASSERT(scroll_content.clip_bounds().width == scroll.bounds().width);

    TestSplitView split;
    split.debug_id("split").size(400.0f, 120.0f);
    split.ratio(0.25f).handle_size(8.0f);
    TestColumn first;
    first.debug_id("first");
    TestColumn second;
    second.debug_id("second");
    FIUI_TEST_ASSERT(split.first(first).second(second).child_count() == 2);
    FIUI_TEST_ASSERT(layout.arrange(*split.expose_impl(), fiui::LayoutConstraints{400.0f, 120.0f})
               .arranged_nodes == 3);
    FIUI_TEST_ASSERT(first.bounds().width == 98.0f);
    FIUI_TEST_ASSERT(second.bounds().width == 294.0f);
}

void verify_input_contract()
{
    fiui::LayoutSystem layout;
    fiui::RenderSystem render;

    TestColumn root;
    root.debug_id("input_root").size(260.0f, 180.0f).gap(6.0f);
    TestInput first;
    first.debug_id("first_input");
    first.value("Alpha");
    TestButton disabled("Disabled");
    disabled.debug_id("disabled_button").enabled(false);
    TestButton action("Action");
    action.debug_id("action_button");
    TestTextArea notes;
    notes.debug_id("notes");
    notes.value("Line");
    FIUI_TEST_ASSERT(root.add(first));
    FIUI_TEST_ASSERT(root.add(disabled));
    FIUI_TEST_ASSERT(root.add(action));
    FIUI_TEST_ASSERT(root.add(notes));
    FIUI_TEST_ASSERT(layout.arrange(*root.expose_impl(), fiui::LayoutConstraints{260.0f, 180.0f})
               .arranged_nodes == 5);

    fiui::default_runtime_clear_event_targets();
    fiui::default_runtime_bind_platform_root(*root.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == first.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == action.object_id());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         key_shift | 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == first.object_id());
    fiui::default_runtime_record_platform_pointer_event(fiui::EventType::PointerDown,
                                                        first.bounds().x + 4.0f,
                                                        first.bounds().y + 4.0f);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == first.object_id());
    fiui::default_runtime_record_platform_focus_lost();
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_capture_target() == 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_hover_target() == 0);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x09, 0);
    FIUI_TEST_ASSERT(fiui::default_runtime_focus_target() == first.object_id());

    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         key_ctrl | 'A', 0);
    FIUI_TEST_ASSERT(first.selection_anchor() == 0);
    FIUI_TEST_ASSERT(first.cursor() == first.value_text().size());
    FIUI_TEST_ASSERT(has_selection_command(render.build_frame(*root.expose_impl()), first.object_id()));

    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         key_ctrl | 'C', 0);
    FIUI_TEST_ASSERT(std::string(fiui::default_runtime_platform_clipboard_text()) == "Alpha");
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0, U'Z');
    FIUI_TEST_ASSERT(first.value_text() == "Z");
    fiui::default_runtime_set_platform_clipboard_text("Beta");
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         key_ctrl | 'V', 0);
    FIUI_TEST_ASSERT(first.value_text() == "ZBeta");
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         key_ctrl | 'A', 0);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown,
                                                         key_ctrl | 'X', 0);
    FIUI_TEST_ASSERT(std::string(fiui::default_runtime_platform_clipboard_text()) == "ZBeta");
    FIUI_TEST_ASSERT(first.value_text().empty());

    fiui::default_runtime_set_event_focus_target(notes.expose_impl());
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x23, 0);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::KeyDown, 0x0d, 0);
    fiui::default_runtime_record_platform_keyboard_event(fiui::EventType::TextInput, 0, U'X');
    FIUI_TEST_ASSERT(notes.value_text() == "Line\nX");
}

void verify_visual_regression_contract()
{
    fiui::LayoutSystem layout;
    fiui::RenderSystem render;

    TestColumn root;
    root.debug_id("visual_root").size(360.0f, 220.0f).padding(10.0f).gap(8.0f);
    fiui::Text title("Visual");
    title.debug_id("visual_title").style(fiui::text::title);
    TestRow row;
    row.debug_id("visual_row").gap(6.0f);
    TestButton primary("Primary");
    primary.debug_id("primary").style("primary");
    TestButton secondary("Secondary");
    secondary.debug_id("secondary");
    fiui::Progress progress;
    progress.debug_id("progress");
    progress.value(0.5f);
    FIUI_TEST_ASSERT(row.add(primary));
    FIUI_TEST_ASSERT(row.add(secondary));
    FIUI_TEST_ASSERT(root.add(title));
    FIUI_TEST_ASSERT(root.add(row));
    FIUI_TEST_ASSERT(root.add(progress));
    FIUI_TEST_ASSERT(layout.arrange(*root.expose_impl(), fiui::LayoutConstraints{360.0f, 220.0f})
               .arranged_nodes == 6);

    const fiui::RenderFrameResult frame = render.build_frame(*root.expose_impl());
    FIUI_TEST_ASSERT(frame.render_tree.nodes.size() == 6);
    FIUI_TEST_ASSERT(frame.layer_tree.nodes.size() >= frame.render_tree.nodes.size());
    FIUI_TEST_ASSERT(frame.backend.command_count == frame.display_list.commands.size());
    FIUI_TEST_ASSERT(frame.backend.batch_count == frame.batches.size());
    FIUI_TEST_ASSERT(count_commands(frame, fiui::DisplayCommandKind::Rect) >= 4);
    FIUI_TEST_ASSERT(count_commands(frame, fiui::DisplayCommandKind::Text) >= 3);
    FIUI_TEST_ASSERT(count_layers(frame, fiui::LayerKind::Text) >= 3);
    FIUI_TEST_ASSERT(count_layers(frame, fiui::LayerKind::Rect) +
               count_layers(frame, fiui::LayerKind::RoundedRect) >=
           3);
}

void verify_diagnostics_and_backend()
{
    fiui::DiagnosticsConfig diagnostics;
    diagnostics.mode = fiui::DebugMode::AiFriendly;
    diagnostics.output_directory = "fiui-test-diagnostics/stability";
    fiui::configure_diagnostics(diagnostics);

    fiui::Window window("Stability");
    window.debug_id("stability_window").size(240.0f, 160.0f);
    TestColumn root;
    root.debug_id("root").padding(8.0f).gap(6.0f);
    fiui::Text title("Stable");
    title.debug_id("title").style(fiui::text::title);
    TestButton button("Click");
    button.debug_id("button");
    int click_count = 0;
    button.on_click(increment, &click_count);
    FIUI_TEST_ASSERT(root.add(title));
    FIUI_TEST_ASSERT(root.add(button));
    FIUI_TEST_ASSERT(!root.add(button));
    FIUI_TEST_ASSERT(button.click());
    FIUI_TEST_ASSERT(click_count == 1);
    window.content(root);

#if defined(_WIN32)
    HWND hwnd = create_hidden_test_window();
    if (hwnd != nullptr) {
        FIUI_TEST_ASSERT(fiui::default_runtime_bind_backend_window(hwnd, 240, 160));
    }
#endif

    const fiui::FrameReport frame = fiui::render_frame(window);
    FIUI_TEST_ASSERT(frame.frame_id > 0);
    const fiui::BackendDeviceState backend = fiui::default_runtime_backend_state();
    FIUI_TEST_ASSERT(backend.initialized);
    FIUI_TEST_ASSERT(backend.frame_submit_count > 0 || backend.headless_submit_count > 0);

    fiui::default_runtime_simulate_backend_device_lost("stability device lost");
    FIUI_TEST_ASSERT(fiui::default_runtime_backend_state().device_lost);
    FIUI_TEST_ASSERT(fiui::default_runtime_recover_backend_device());
    FIUI_TEST_ASSERT(!fiui::default_runtime_backend_state().device_lost);

    fiui::flush_diagnostics();
    const std::string trace =
        read_text_file("fiui-test-diagnostics/stability/fiui-trace.jsonl");
    const std::string frame_json =
        read_text_file("fiui-test-diagnostics/stability/fiui-frame.json");
    const std::string leaks =
        read_text_file("fiui-test-diagnostics/stability/fiui-leaks.json");
    FIUI_TEST_ASSERT(trace.find("\"schema\":\"fiui.trace.v0\"") != std::string::npos);
    FIUI_TEST_ASSERT(trace.find("\"action\":\"duplicate_attach\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"schema\": \"fiui.frame.v0\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"backend_device_summary\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"scheduler_summary\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"widget_tree\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"layout\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"layer_tree_summary\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"display_list_summary\"") != std::string::npos);
    FIUI_TEST_ASSERT(frame_json.find("\"resource\"") != std::string::npos);
    FIUI_TEST_ASSERT(leaks.find("\"schema\": \"fiui.leaks.v0\"") != std::string::npos);

    const char* update_golden = std::getenv("FIUI_UPDATE_VISUAL_GOLDEN");
    if (update_golden != nullptr && std::strcmp(update_golden, "1") == 0) {
        std::ofstream golden("fiui-test-diagnostics/stability/frame-golden.json",
                             std::ios::binary);
        golden << frame_json;
    }

    fiui::default_runtime_release_backend_window_resources();
#if defined(_WIN32)
    if (hwnd != nullptr) {
        DestroyWindow(hwnd);
    }
#endif
}

} // namespace

int main()
{
    verify_layout_contract();
    verify_input_contract();
    verify_visual_regression_contract();
    verify_diagnostics_and_backend();
    return 0;
}
