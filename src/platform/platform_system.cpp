#include "platform/platform_system.h"

#include "core/widget_impl.h"
#include "diagnostics/diagnostics_internal.h"
#include "event/event_system.h"
#include "fiui/app.h"
#include "fiui/widget.h"
#include "runtime/runtime.h"

#include <algorithm>
#include <cstring>
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

namespace fiui {
namespace {

constexpr std::uint32_t default_window_width = 900;
constexpr std::uint32_t default_window_height = 600;
constexpr std::uint32_t key_modifier_ctrl = 1u << 18;
constexpr std::uint32_t key_code_mask = 0xffffu;

#if defined(_WIN32)

std::uint32_t loword_uint32(LPARAM value)
{
    return static_cast<std::uint32_t>(LOWORD(static_cast<DWORD_PTR>(value)));
}

std::uint32_t hiword_uint32(LPARAM value)
{
    return static_cast<std::uint32_t>(HIWORD(static_cast<DWORD_PTR>(value)));
}

RECT adjusted_window_rect_for_client(std::uint32_t width, std::uint32_t height)
{
    constexpr DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 != nullptr) {
        using AdjustWindowRectExForDpiFn = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
        auto* adjust_for_dpi = reinterpret_cast<AdjustWindowRectExForDpiFn>(
            GetProcAddress(user32, "AdjustWindowRectExForDpi"));
        if (adjust_for_dpi != nullptr) {
            const UINT dpi = GetDpiForSystem();
            if (adjust_for_dpi(&rect, style, FALSE, 0, dpi) != FALSE) {
                return rect;
            }
        }
    }

    AdjustWindowRectEx(&rect, style, FALSE, 0);
    return rect;
}

float dpi_scale(std::uint32_t dpi) noexcept
{
    return std::max(0.25f, static_cast<float>(std::max<std::uint32_t>(1, dpi)) / 96.0f);
}

std::uint32_t scaled_u32(float value, std::uint32_t dpi) noexcept
{
    return static_cast<std::uint32_t>(std::max(1.0f, value * dpi_scale(dpi) + 0.5f));
}

void ensure_process_dpi_awareness()
{
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 != nullptr) {
        using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto* set_awareness_context = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (set_awareness_context != nullptr &&
            set_awareness_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE) {
            diagnostics_event_ex("platform", "dpi_awareness", 0, 0,
                                 default_runtime().current_frame_id(), "",
                                 "per-monitor-v2");
            return;
        }
    }

    if (SetProcessDPIAware() != FALSE) {
        diagnostics_event_ex("platform", "dpi_awareness", 0, 0,
                             default_runtime().current_frame_id(), "",
                             "system-aware-fallback");
        return;
    }

    diagnostics_event_ex("platform", "dpi_awareness_skipped", 0, 0,
                         default_runtime().current_frame_id(), "",
                         "process awareness already set or unavailable");
}

float mouse_x(LPARAM value)
{
    return static_cast<float>(static_cast<short>(LOWORD(static_cast<DWORD_PTR>(value))));
}

float mouse_y(LPARAM value)
{
    return static_cast<float>(static_cast<short>(HIWORD(static_cast<DWORD_PTR>(value))));
}

POINT screen_point_from_lparam(LPARAM value)
{
    return POINT{static_cast<LONG>(static_cast<short>(LOWORD(static_cast<DWORD_PTR>(value)))),
                 static_cast<LONG>(static_cast<short>(HIWORD(static_cast<DWORD_PTR>(value))))};
}

float wheel_delta_from_wparam(WPARAM value)
{
    return static_cast<float>(static_cast<short>(HIWORD(static_cast<DWORD_PTR>(value))));
}

std::wstring utf8_to_wide(const std::string& text)
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

std::string wide_to_utf8(const wchar_t* text)
{
    if (text == nullptr || *text == L'\0') {
        return {};
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), needed, nullptr, nullptr);
    return utf8;
}

bool write_os_clipboard_text(void* native_handle, const std::string& text)
{
    HWND hwnd = static_cast<HWND>(native_handle);
    if (hwnd == nullptr || OpenClipboard(hwnd) == FALSE) {
        return false;
    }

    bool ok = false;
    if (EmptyClipboard() != FALSE) {
        const std::wstring wide = utf8_to_wide(text);
        const SIZE_T bytes = (wide.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory != nullptr) {
            void* locked = GlobalLock(memory);
            if (locked != nullptr) {
                std::memcpy(locked, wide.c_str(), bytes);
                GlobalUnlock(memory);
                if (SetClipboardData(CF_UNICODETEXT, memory) != nullptr) {
                    memory = nullptr;
                    ok = true;
                }
            }
            if (memory != nullptr) {
                GlobalFree(memory);
            }
        }
    }

    CloseClipboard();
    return ok;
}

bool read_os_clipboard_text(void* native_handle, std::string& out)
{
    HWND hwnd = static_cast<HWND>(native_handle);
    if (hwnd == nullptr || OpenClipboard(hwnd) == FALSE) {
        return false;
    }

    bool ok = false;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data != nullptr) {
        const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(data));
        if (text != nullptr) {
            out = wide_to_utf8(text);
            GlobalUnlock(data);
            ok = true;
        }
    }

    CloseClipboard();
    return ok;
}

LRESULT CALLBACK fiui_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    PlatformSystem& platform = default_runtime().platform_system();
    switch (message) {
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        platform.record_input(PlatformEventType::WindowDestroyed);
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(hwnd, &paint);
        platform.record_paint();
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_SIZE:
        platform.record_resize(loword_uint32(lparam), hiword_uint32(lparam));
        return 0;
    case WM_DPICHANGED:
        platform.record_dpi_changed(static_cast<std::uint32_t>(HIWORD(wparam)));
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_MOUSEMOVE:
        platform.record_pointer_event(EventType::PointerMove, mouse_x(lparam), mouse_y(lparam));
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        platform.record_pointer_event(EventType::PointerDown, mouse_x(lparam), mouse_y(lparam));
        return 0;
    case WM_LBUTTONUP:
        platform.record_pointer_event(EventType::PointerUp, mouse_x(lparam), mouse_y(lparam));
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        platform.record_input(PlatformEventType::Mouse);
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_MOUSEWHEEL: {
        POINT point = screen_point_from_lparam(lparam);
        ScreenToClient(hwnd, &point);
        platform.record_wheel_event(static_cast<float>(point.x), static_cast<float>(point.y),
                                    wheel_delta_from_wparam(wparam));
        return 0;
    }
    case WM_KEYDOWN: {
        const bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        std::uint32_t key_code = static_cast<std::uint32_t>(wparam);
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
            key_code |= 1u << 16;
        }
        if ((GetKeyState(VK_MENU) & 0x8000) != 0) {
            key_code |= 1u << 17;
        }
        if (ctrl_down) {
            key_code |= 1u << 18;
        }

        const EventDispatchResult result =
            platform.record_keyboard_event(EventType::KeyDown, key_code, 0);
        if (result.menu_shortcut_triggered) {
            return 0;
        }
        if (ctrl_down && (wparam == 'A' || wparam == 'a')) {
            if (default_runtime_select_focused_input_all()) {
                RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            }
            return 0;
        }
        if (ctrl_down && (wparam == 'C' || wparam == 'c')) {
            platform.record_clipboard_copy();
            return 0;
        }
        if (ctrl_down && (wparam == 'V' || wparam == 'v')) {
            platform.record_clipboard_paste();
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    case WM_SYSKEYDOWN: {
        std::uint32_t key_code = static_cast<std::uint32_t>(wparam) | (1u << 17);
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
            key_code |= 1u << 16;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            key_code |= 1u << 18;
        }
        (void)platform.record_keyboard_event(EventType::KeyDown, key_code, 0);
        return 0;
    }
    case WM_KEYUP:
        (void)platform.record_keyboard_event(EventType::KeyUp,
                                             static_cast<std::uint32_t>(wparam), 0);
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_SYSKEYUP:
        (void)platform.record_keyboard_event(EventType::KeyUp,
                                             static_cast<std::uint32_t>(wparam) | (1u << 17), 0);
        return 0;
    case WM_CHAR:
        (void)platform.record_keyboard_event(EventType::TextInput, 0,
                                             static_cast<char32_t>(wparam));
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_KILLFOCUS:
        platform.record_focus_lost();
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_IME_STARTCOMPOSITION:
        platform.record_ime_event(PlatformImePhase::StartComposition);
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_IME_COMPOSITION:
        platform.record_ime_event(PlatformImePhase::Composition);
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_IME_ENDCOMPOSITION:
        platform.record_ime_event(PlatformImePhase::EndComposition);
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_TIMER:
        platform.record_timer_tick();
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

#else

bool write_os_clipboard_text(void*, const std::string&)
{
    return false;
}

bool read_os_clipboard_text(void*, std::string&)
{
    return false;
}

#endif

} // namespace

PlatformWindowResult PlatformSystem::create_window(const Window& window, std::uint64_t frame_id)
{
    (void)frame_id;
    state_.window_object_id = window.object_id();
    state_.window_generation = window.generation();
    bind_window_model(&window);
    bind_root(window.impl());

    const Rect rect = window.bounds();
    state_.logical_width = rect.width > 0.0f ? rect.width : static_cast<float>(default_window_width);
    state_.logical_height =
        rect.height > 0.0f ? rect.height : static_cast<float>(default_window_height);
    state_.width = static_cast<std::uint32_t>(state_.logical_width);
    state_.height = static_cast<std::uint32_t>(state_.logical_height);

#if defined(_WIN32)
    ensure_process_dpi_awareness();

    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* class_name = L"fiui_window_v0";

    WNDCLASSW wc{};
    wc.lpfnWndProc = fiui_window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    std::uint32_t creation_dpi = GetDpiForSystem();
    const std::uint32_t initial_client_width = scaled_u32(state_.logical_width, creation_dpi);
    const std::uint32_t initial_client_height = scaled_u32(state_.logical_height, creation_dpi);
    const RECT adjusted_rect =
        adjusted_window_rect_for_client(initial_client_width, initial_client_height);
    const int window_width = static_cast<int>(adjusted_rect.right - adjusted_rect.left);
    const int window_height = static_cast<int>(adjusted_rect.bottom - adjusted_rect.top);
    HWND hwnd = CreateWindowExW(0, class_name, L"fiui v0", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                               CW_USEDEFAULT, window_width, window_height, nullptr, nullptr,
                               instance, nullptr);
    native_handle_ = hwnd;
    if (hwnd == nullptr) {
        record_event(PlatformEventType::WindowCreateFailed, "CreateWindowExW returned null");
        return PlatformWindowResult{false, nullptr, state_.width, state_.height, state_.dpi};
    }

    state_.dpi = static_cast<std::uint32_t>(GetDpiForWindow(hwnd));
    RECT client_rect{};
    if (GetClientRect(hwnd, &client_rect) != FALSE) {
        state_.width = static_cast<std::uint32_t>(std::max<LONG>(0, client_rect.right - client_rect.left));
        state_.height = static_cast<std::uint32_t>(std::max<LONG>(0, client_rect.bottom - client_rect.top));
    }
    if (root_widget_ != nullptr) {
        root_widget_->dirty.bounds.width = static_cast<float>(state_.width);
        root_widget_->dirty.bounds.height = static_cast<float>(state_.height);
        root_widget_->dirty.paint_bounds = root_widget_->dirty.bounds;
        root_widget_->dirty.clip_bounds = root_widget_->dirty.bounds;
        mutate_widget(*root_widget_, DirtyReason::Resize | DirtyReason::Layout |
                                         DirtyReason::Paint,
                      "platform_window_create_size");
    }
    record_event(PlatformEventType::WindowCreated, "hwnd created");
    SetTimer(hwnd, 1, 500, nullptr);
    const bool backend_bound = default_runtime().backend().bind_window(hwnd, state_.width, state_.height);
    if (!backend_bound) {
        diagnostics_event_ex("platform", "backend_bind_failed", state_.window_object_id,
                             state_.window_generation, default_runtime().current_frame_id(), "",
                             "backend could not bind native window");
    }
    return PlatformWindowResult{true, hwnd, state_.width, state_.height, state_.dpi};
#else
    (void)frame_id;
    native_handle_ = nullptr;
    record_event(PlatformEventType::WindowCreateFailed, "platform window unsupported");
    return PlatformWindowResult{false, nullptr, state_.width, state_.height, state_.dpi};
#endif
}

void PlatformSystem::show_window(void* native_handle)
{
#if defined(_WIN32)
    HWND hwnd = static_cast<HWND>(native_handle);
    if (hwnd != nullptr) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
#else
    (void)native_handle;
#endif
}

int PlatformSystem::run_message_loop()
{
#if defined(_WIN32)
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
#else
    return 0;
#endif
}

void PlatformSystem::record_paint()
{
    ++state_.input_event_count;
    record_event(PlatformEventType::Paint, "paint");
    render_bound_window("render_paint", "paint", state_.window_object_id, state_.window_generation,
                        "", false);
}

void PlatformSystem::record_resize(std::uint32_t width, std::uint32_t height)
{
    state_.width = width;
    state_.height = height;
    const float scale = dpi_scale(state_.dpi);
    if (scale > 0.0f) {
        state_.logical_width = static_cast<float>(width) / scale;
        state_.logical_height = static_cast<float>(height) / scale;
    }
    ++state_.resize_count;
    record_event(PlatformEventType::Resize, "window resized");
    if (root_widget_ != nullptr) {
        root_widget_->dirty.bounds.width = static_cast<float>(width);
        root_widget_->dirty.bounds.height = static_cast<float>(height);
        root_widget_->dirty.paint_bounds = root_widget_->dirty.bounds;
        root_widget_->dirty.clip_bounds = root_widget_->dirty.bounds;
        mutate_widget(*root_widget_, DirtyReason::Resize | DirtyReason::Layout |
                                         DirtyReason::Paint,
                      "platform_resize");
    }
    if (native_handle_ != nullptr) {
        const bool resized = default_runtime().backend().resize_render_target(width, height);
        if (!resized) {
            diagnostics_event_ex("platform", "backend_resize_failed", state_.window_object_id,
                                 state_.window_generation, default_runtime().current_frame_id(), "",
                                 "backend render target resize failed");
        }
    }
    default_runtime().frame_scheduler().request_frame("resize", "platform", state_.window_object_id,
                                                      state_.window_generation,
                                                      default_runtime().current_frame_id(), "");
    request_render_after_input("resize", state_.window_object_id, state_.window_generation, "");
}

void PlatformSystem::record_dpi_changed(std::uint32_t dpi)
{
    state_.dpi = std::max<std::uint32_t>(1, dpi);
    record_event(PlatformEventType::DpiChanged, "dpi changed");
    if (root_widget_ != nullptr && state_.logical_width > 0.0f && state_.logical_height > 0.0f) {
        root_widget_->dirty.bounds.width = static_cast<float>(scaled_u32(state_.logical_width, state_.dpi));
        root_widget_->dirty.bounds.height =
            static_cast<float>(scaled_u32(state_.logical_height, state_.dpi));
        root_widget_->dirty.paint_bounds = root_widget_->dirty.bounds;
        root_widget_->dirty.clip_bounds = root_widget_->dirty.bounds;
        mutate_widget(*root_widget_, DirtyReason::Resize | DirtyReason::Layout |
                                         DirtyReason::Paint,
                      "platform_dpi_changed");
    }
    default_runtime().frame_scheduler().request_frame("dpi_changed", "platform",
                                                      state_.window_object_id,
                                                      state_.window_generation,
                                                      default_runtime().current_frame_id(), "");
    request_render_after_input("dpi_changed", state_.window_object_id, state_.window_generation, "");
}

void PlatformSystem::record_input(PlatformEventType type)
{
    ++state_.input_event_count;
    record_event(type, platform_event_type_name(type));
}

void PlatformSystem::bind_root(WidgetImpl* root)
{
    root_widget_ = root;
    if (root != nullptr) {
        state_.window_object_id = root->object.object_id;
        state_.window_generation = root->object.generation;
        diagnostics_event_ex("platform", "bind_root", root->object.object_id,
                             root->object.generation, default_runtime().current_frame_id(),
                             widget_path(*root).c_str(), "root widget bound");
    } else {
        diagnostics_event_ex("platform", "bind_root", 0, 0, default_runtime().current_frame_id(),
                             "", "root widget cleared");
    }
}

void PlatformSystem::bind_window_model(const Window* window)
{
    window_model_ = window;
}

void PlatformSystem::record_pointer_event(EventType type, float x, float y)
{
    ++state_.input_event_count;
    record_event(PlatformEventType::Mouse, event_type_name(type));

    if (root_widget_ == nullptr) {
        ++state_.pointer_miss_count;
        diagnostics_event_ex("platform", "pointer_unrouted", state_.window_object_id,
                             state_.window_generation, default_runtime().current_frame_id(), "",
                             "root widget is not bound");
        return;
    }

    const EventDispatchResult result =
        default_runtime().event_system().route_pointer_event(*root_widget_, type, x, y);
    if (result.hit_test.hit) {
        ++state_.pointer_route_count;
        const bool quiet_pointer_move = type == EventType::PointerMove && !result.target_changed;
        if (!quiet_pointer_move) {
            diagnostics_event_ex("platform", "pointer_routed", result.hit_test.target_object_id,
                                 result.hit_test.target_generation,
                                 default_runtime().current_frame_id(),
                                 result.hit_test.target_path.c_str(), event_type_name(type));
            default_runtime().frame_scheduler().request_frame(
                event_type_name(type), "platform_input", result.hit_test.target_object_id,
                result.hit_test.target_generation, default_runtime().current_frame_id(),
                result.hit_test.target_path.c_str());
        }
        if (type == EventType::PointerUp && result.hit_test.target != nullptr &&
            (result.hit_test.target->node.kind == WidgetKind::Button ||
             result.hit_test.target->node.kind == WidgetKind::CheckBox ||
             result.hit_test.target->node.kind == WidgetKind::RadioButton ||
             result.hit_test.target->node.kind == WidgetKind::Switch ||
             result.hit_test.target->node.kind == WidgetKind::Select ||
             result.hit_test.target->node.kind == WidgetKind::SelectOption ||
             result.hit_test.target->node.kind == WidgetKind::ListItem ||
             result.hit_test.target->node.kind == WidgetKind::TreeItem ||
             result.hit_test.target->node.kind == WidgetKind::TableView ||
             result.hit_test.target->node.kind == WidgetKind::Tabs ||
             result.hit_test.target->node.kind == WidgetKind::Dialog ||
             result.hit_test.target->node.kind == WidgetKind::MenuItem)) {
            EventDispatchResult click_result =
                default_runtime().event_system().dispatch_click(*result.hit_test.target);
            diagnostics_event_ex("platform", click_result.callback_failed ? "click_failed"
                                                                          : "click_dispatched",
                                 result.hit_test.target_object_id,
                                 result.hit_test.target_generation,
                                 default_runtime().current_frame_id(),
                                 result.hit_test.target_path.c_str(),
                                 click_result.handled ? "handled" : "unhandled");
        }
        if (!quiet_pointer_move) {
            request_render_after_input(event_type_name(type), result.hit_test.target_object_id,
                                       result.hit_test.target_generation,
                                       result.hit_test.target_path.c_str());
        }
    } else {
        ++state_.pointer_miss_count;
        const bool quiet_pointer_move = type == EventType::PointerMove && !result.target_changed;
        if (!quiet_pointer_move) {
            diagnostics_event_ex("platform", "pointer_missed", state_.window_object_id,
                                 state_.window_generation, default_runtime().current_frame_id(),
                                 root_widget_ == nullptr ? "" : widget_path(*root_widget_).c_str(),
                                 event_type_name(type));
            default_runtime().frame_scheduler().request_frame(
                event_type_name(type), "platform_input", state_.window_object_id,
                state_.window_generation, default_runtime().current_frame_id(),
                root_widget_ == nullptr ? "" : widget_path(*root_widget_).c_str());
            request_render_after_input(
                event_type_name(type), state_.window_object_id, state_.window_generation,
                root_widget_ == nullptr ? "" : widget_path(*root_widget_).c_str());
        }
    }
}

void PlatformSystem::record_wheel_event(float x, float y, float delta)
{
    ++state_.input_event_count;
    record_event(PlatformEventType::Wheel, event_type_name(EventType::Wheel));

    if (root_widget_ == nullptr) {
        diagnostics_event_ex("platform", "wheel_unrouted", state_.window_object_id,
                             state_.window_generation, default_runtime().current_frame_id(), "",
                             "root widget is not bound");
        return;
    }

    const EventDispatchResult result =
        default_runtime().event_system().route_wheel_event(*root_widget_, x, y, delta);
    if (!result.hit_test.hit) {
        diagnostics_event_ex("platform", "wheel_missed", state_.window_object_id,
                             state_.window_generation, default_runtime().current_frame_id(),
                             widget_path(*root_widget_).c_str(), event_type_name(EventType::Wheel));
        return;
    }

    diagnostics_event_ex("platform", result.handled ? "wheel_routed" : "wheel_unhandled",
                         result.hit_test.target_object_id, result.hit_test.target_generation,
                         default_runtime().current_frame_id(), result.hit_test.target_path.c_str(),
                         event_type_name(EventType::Wheel));
    if (!result.handled) {
        return;
    }

    default_runtime().frame_scheduler().request_frame(
        event_type_name(EventType::Wheel), "platform_input", result.hit_test.target_object_id,
        result.hit_test.target_generation, default_runtime().current_frame_id(),
        result.hit_test.target_path.c_str());
    request_render_after_input(event_type_name(EventType::Wheel),
                               result.hit_test.target_object_id,
                               result.hit_test.target_generation,
                               result.hit_test.target_path.c_str());
}

EventDispatchResult PlatformSystem::record_keyboard_event(EventType type,
                                                          std::uint32_t key_code,
                                                          char32_t text_codepoint)
{
    ++state_.input_event_count;
    ++state_.keyboard_event_count;
    if (type == EventType::TextInput) {
        ++state_.text_input_count;
    }
    record_event(PlatformEventType::Keyboard, event_type_name(type));

    const EventDispatchResult result =
        default_runtime().event_system().route_keyboard_event(type, key_code, text_codepoint);
    if (type == EventType::KeyDown && (key_code & key_modifier_ctrl) != 0) {
        const std::uint32_t key = key_code & key_code_mask;
        if (key == 'A' || key == 'a') {
            (void)default_runtime().event_system().select_focused_input_all();
        } else if (key == 'C' || key == 'c') {
            record_clipboard_copy();
        } else if (key == 'V' || key == 'v') {
            record_clipboard_paste();
        } else if (key == 'X' || key == 'x') {
            record_clipboard_copy();
            (void)default_runtime().event_system().route_keyboard_event(EventType::KeyDown,
                                                                        0x2e, 0);
        }
    }
    if (result.hit_test.hit) {
        ++state_.keyboard_route_count;
        diagnostics_event_ex("platform", "keyboard_routed", result.hit_test.target_object_id,
                             result.hit_test.target_generation,
                             default_runtime().current_frame_id(),
                             result.hit_test.target_path.c_str(), event_type_name(type));
        default_runtime().frame_scheduler().request_frame(event_type_name(type), "platform_input",
                                                          result.hit_test.target_object_id,
                                                          result.hit_test.target_generation,
                                                          default_runtime().current_frame_id(),
                                                          result.hit_test.target_path.c_str());
        request_render_after_input(event_type_name(type), result.hit_test.target_object_id,
                                   result.hit_test.target_generation,
                                   result.hit_test.target_path.c_str());
    } else {
        ++state_.keyboard_miss_count;
        diagnostics_event_ex("platform", "keyboard_missed", state_.window_object_id,
                             state_.window_generation, default_runtime().current_frame_id(), "",
                             event_type_name(type));
    }
    return result;
}

WidgetImpl* PlatformSystem::bound_root() const noexcept
{
    return root_widget_;
}

void PlatformSystem::record_focus_lost()
{
    ++state_.input_event_count;
    record_event(PlatformEventType::FocusLost, "focus lost");
    if (root_widget_ == nullptr) {
        (void)default_runtime().event_system().set_focus_target(nullptr);
        (void)default_runtime().event_system().set_capture_target(nullptr);
        (void)default_runtime().event_system().set_hover_target(nullptr);
        return;
    }

    const bool closed = default_runtime().menu_system().close_all(*root_widget_);
    const bool focus_cleared = default_runtime().event_system().set_focus_target(nullptr);
    const bool capture_cleared = default_runtime().event_system().set_capture_target(nullptr);
    const bool hover_cleared = default_runtime().event_system().set_hover_target(nullptr);
    const bool target_cleared = focus_cleared || capture_cleared || hover_cleared;
    diagnostics_event_ex("platform", closed ? "focus_lost_menu_closed" : "focus_lost",
                         state_.window_object_id, state_.window_generation,
                         default_runtime().current_frame_id(), widget_path(*root_widget_).c_str(),
                         closed ? "menus closed"
                                : (target_cleared ? "targets cleared" : "no open menus"));
    if (closed || target_cleared) {
        default_runtime().frame_scheduler().request_frame(
            "focus_lost", "platform_input", state_.window_object_id, state_.window_generation,
            default_runtime().current_frame_id(), widget_path(*root_widget_).c_str());
        request_render_after_input("focus_lost", state_.window_object_id,
                                   state_.window_generation, widget_path(*root_widget_).c_str());
    }
}

void PlatformSystem::set_clipboard_text(const char* text)
{
    clipboard_text_ = text == nullptr ? "" : text;
    ++state_.clipboard_write_count;
    record_event(PlatformEventType::Clipboard, "clipboard_write");
    const bool os_written = write_os_clipboard_text(native_handle_, clipboard_text_);
    std::ostringstream detail;
    detail << "length=" << clipboard_text_.size()
           << ";target=" << (os_written ? "os_clipboard" : "internal_clipboard");
    const std::string text_detail = detail.str();
    diagnostics_event_ex("platform", "clipboard_write", state_.window_object_id,
                         state_.window_generation, default_runtime().current_frame_id(), "",
                         text_detail.c_str());
}

const char* PlatformSystem::clipboard_text() const noexcept
{
    return clipboard_text_.c_str();
}

void PlatformSystem::record_clipboard_copy()
{
    ++state_.input_event_count;
    ++state_.clipboard_copy_count;
    std::string focused_text;
    if (!default_runtime().event_system().focused_input_text(focused_text)) {
        ++state_.clipboard_failure_count;
        record_event(PlatformEventType::Clipboard, "clipboard_copy_failed");
        diagnostics_event_ex("platform", "clipboard_copy_failed", state_.window_object_id,
                             state_.window_generation, default_runtime().current_frame_id(), "",
                             "focused input unavailable");
        return;
    }
    clipboard_text_ = focused_text;
    const bool os_written = write_os_clipboard_text(native_handle_, clipboard_text_);
    ++state_.clipboard_read_count;
    ++state_.clipboard_write_count;
    record_event(PlatformEventType::Clipboard, "clipboard_copy");
    diagnostics_event_ex("platform", "clipboard_read", state_.window_object_id,
                         state_.window_generation, default_runtime().current_frame_id(), "",
                         "focused input text");
    diagnostics_event_ex("platform",
                         os_written ? "clipboard_os_write" : "clipboard_internal_write",
                         state_.window_object_id, state_.window_generation,
                         default_runtime().current_frame_id(), "",
                         os_written ? "system clipboard updated"
                                    : "system clipboard unavailable; internal fallback");
    diagnostics_event_ex("platform", "clipboard_copy", state_.window_object_id,
                         state_.window_generation, default_runtime().current_frame_id(), "",
                         clipboard_text_.c_str());
}

void PlatformSystem::record_clipboard_paste()
{
    ++state_.input_event_count;
    ++state_.clipboard_paste_count;
    ++state_.clipboard_read_count;
    std::string os_clipboard_text;
    const bool os_read = read_os_clipboard_text(native_handle_, os_clipboard_text);
    if (os_read) {
        clipboard_text_ = os_clipboard_text;
    }
    record_event(PlatformEventType::Clipboard, "clipboard_paste");
    diagnostics_event_ex("platform", "clipboard_read", state_.window_object_id,
                         state_.window_generation, default_runtime().current_frame_id(), "",
                         os_read ? "system plain text clipboard"
                                 : "internal plain text clipboard fallback");
    const EventDispatchResult result = default_runtime().event_system().route_text_input_string(
        clipboard_text_.c_str(), "clipboard_paste");
    if (result.hit_test.hit && result.handled) {
        diagnostics_event_ex("platform", "clipboard_paste", result.hit_test.target_object_id,
                             result.hit_test.target_generation,
                             default_runtime().current_frame_id(),
                             result.hit_test.target_path.c_str(), clipboard_text_.c_str());
        default_runtime().frame_scheduler().request_frame(
            "clipboard_paste", "platform_input", result.hit_test.target_object_id,
            result.hit_test.target_generation, default_runtime().current_frame_id(),
            result.hit_test.target_path.c_str());
    } else {
        ++state_.clipboard_failure_count;
        diagnostics_event_ex("platform", "clipboard_paste_failed", state_.window_object_id,
                             state_.window_generation, default_runtime().current_frame_id(), "",
                             result.hit_test.hit ? "paste not handled" : "focused input unavailable");
    }
}

void PlatformSystem::record_ime_event(PlatformImePhase phase)
{
    ++state_.input_event_count;
    ++state_.ime_event_count;
    switch (phase) {
    case PlatformImePhase::StartComposition:
        ++state_.ime_start_count;
        break;
    case PlatformImePhase::Composition:
        ++state_.ime_composition_count;
        break;
    case PlatformImePhase::EndComposition:
        ++state_.ime_end_count;
        break;
    }
    record_event(PlatformEventType::Ime, platform_ime_phase_name(phase));
    const HitTestResult target = default_runtime().event_system().focused_target();
    diagnostics_event_ex("platform", "ime_event", target.target_object_id,
                         target.target_generation, default_runtime().current_frame_id(),
                         target.target_path.c_str(), platform_ime_phase_name(phase));
    if (!target.hit) {
        diagnostics_event_ex("platform", "ime_unfocused", state_.window_object_id,
                             state_.window_generation, default_runtime().current_frame_id(), "",
                             platform_ime_phase_name(phase));
    }
}

void PlatformSystem::record_timer_tick()
{
    ++state_.timer_tick_count;
    record_event(PlatformEventType::Timer, "timer tick");
    default_runtime().frame_scheduler().request_frame("timer", "platform", state_.window_object_id,
                                                      state_.window_generation,
                                                      default_runtime().current_frame_id(), "");
}

void PlatformSystem::record_device_lost(const char* detail)
{
    ++state_.device_lost_count;
    record_event(PlatformEventType::DeviceLost, detail == nullptr ? "" : detail);
    default_runtime().backend().simulate_device_lost(detail);
    default_runtime().frame_scheduler().request_frame("device_lost", "platform",
                                                      state_.window_object_id,
                                                      state_.window_generation,
                                                      default_runtime().current_frame_id(), "");
}

void PlatformSystem::request_render_after_input(const char* reason,
                                                ObjectId object_id,
                                                std::uint32_t generation,
                                                const char* path)
{
    invalidate_bound_window("render_after_input", reason, object_id, generation, path);
}

void PlatformSystem::render_bound_window(const char* action,
                                         const char* reason,
                                         ObjectId object_id,
                                         std::uint32_t generation,
                                         const char* path,
                                         bool invalidate_after_render)
{
    if (window_model_ == nullptr) {
        diagnostics_event_ex("platform", "render_skipped", object_id, generation,
                             default_runtime().current_frame_id(), path == nullptr ? "" : path,
                             "window model not bound");
        return;
    }
    const FrameReport report = render_frame(*window_model_);
    diagnostics_event_ex("platform", action == nullptr ? "render" : action, object_id, generation,
                         report.frame_id,
                         path == nullptr ? "" : path, reason == nullptr ? "" : reason);
#if defined(_WIN32)
    if (invalidate_after_render && native_handle_ != nullptr) {
        InvalidateRect(static_cast<HWND>(native_handle_), nullptr, FALSE);
    }
#endif
}

void PlatformSystem::invalidate_bound_window(const char* action,
                                             const char* reason,
                                             ObjectId object_id,
                                             std::uint32_t generation,
                                             const char* path)
{
    if (window_model_ == nullptr) {
        diagnostics_event_ex("platform", "render_skipped", object_id, generation,
                             default_runtime().current_frame_id(), path == nullptr ? "" : path,
                             "window model not bound");
        return;
    }
#if defined(_WIN32)
    if (native_handle_ != nullptr) {
        HWND hwnd = static_cast<HWND>(native_handle_);
        const bool immediate_interaction =
            reason != nullptr &&
            (std::strcmp(reason, "pointer_move") == 0 ||
             std::strcmp(reason, "pointer_down") == 0 ||
             std::strcmp(reason, "pointer_up") == 0 ||
             std::strcmp(reason, "text_input") == 0);
        if (immediate_interaction) {
            RedrawWindow(hwnd, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
        } else {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }
#endif
    diagnostics_event_ex("platform", action == nullptr ? "render_requested" : action, object_id,
                         generation, default_runtime().current_frame_id(),
                         path == nullptr ? "" : path,
                         reason == nullptr ? "paint queued" : reason);
}

const PlatformState& PlatformSystem::state() const noexcept
{
    return state_;
}

void PlatformSystem::record_event(PlatformEventType type, const char* detail)
{
    state_.last_event = type;
    diagnostics_event_ex("platform", platform_event_type_name(type), state_.window_object_id,
                         state_.window_generation, default_runtime().current_frame_id(), "",
                         detail == nullptr ? "" : detail);
}

const char* platform_event_type_name(PlatformEventType type) noexcept
{
    switch (type) {
    case PlatformEventType::WindowCreated:
        return "window_created";
    case PlatformEventType::WindowCreateFailed:
        return "window_create_failed";
    case PlatformEventType::WindowDestroyed:
        return "window_destroyed";
    case PlatformEventType::Paint:
        return "paint";
    case PlatformEventType::Resize:
        return "resize";
    case PlatformEventType::DpiChanged:
        return "dpi_changed";
    case PlatformEventType::Mouse:
        return "mouse";
    case PlatformEventType::Keyboard:
        return "keyboard";
    case PlatformEventType::Wheel:
        return "wheel";
    case PlatformEventType::FocusLost:
        return "focus_lost";
    case PlatformEventType::Touch:
        return "touch";
    case PlatformEventType::Ime:
        return "ime";
    case PlatformEventType::Clipboard:
        return "clipboard";
    case PlatformEventType::Timer:
        return "timer";
    case PlatformEventType::DeviceLost:
        return "device_lost";
    }
    return "unknown";
}

const char* platform_ime_phase_name(PlatformImePhase phase) noexcept
{
    switch (phase) {
    case PlatformImePhase::StartComposition:
        return "ime_start_composition";
    case PlatformImePhase::Composition:
        return "ime_composition";
    case PlatformImePhase::EndComposition:
        return "ime_end_composition";
    }
    return "unknown";
}

} // namespace fiui
