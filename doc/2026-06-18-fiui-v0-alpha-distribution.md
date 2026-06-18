# fiui v0 Alpha Distribution Guide

Date: 2026-06-18

This document defines what can be distributed as the current v0 alpha and how a
downstream C++ application should consume it.

## Distribution Status

`fiui` is suitable for internal alpha use as a Windows C++20 UI runtime for
settings-style desktop tools. It is not yet a stable public SDK.

Allowed alpha use:

- Build small native tools with the public C++ API.
- Use `Window`, layout containers, menus, forms, tabs, dialogs, tables, trees,
  text input, selection controls, and theme switching.
- Use diagnostics output to debug layout, dirty tracking, callbacks, resource
  behavior, and frame reports.
- Use `settings_panel` and `control_center_demo` as acceptance references.

Not guaranteed in this alpha:

- ABI stability across versions.
- Full IME composition UI.
- Undo/redo and rich text editing.
- Complete accessibility behavior.
- Complete layer-backed shadows, opacity, transforms, and rounded masks.
- Large virtualized data controls.
- Visual designer or script/XML UI loading.

## Build And Install

Run both configurations before handing a build to another project:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/verify.ps1 -Config Debug -BuildDir build-verify-debug -InstallPrefix out/fiui-debug
powershell -ExecutionPolicy Bypass -File scripts/verify.ps1 -Config Release -BuildDir build-verify-release -InstallPrefix out/fiui-release
```

The install tree contains:

- `bin/fiui.dll`
- `lib/fiui.lib`
- `lib/cmake/fiui/fiuiConfig.cmake`
- `lib/cmake/fiui/fiuiTargets.cmake`
- `include/fiui/*.h`
- `share/doc/fiui/*.md`

## Downstream CMake Use

Minimal consumer `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_fiui_tool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(fiui CONFIG REQUIRED)

add_executable(my_fiui_tool main.cpp)
target_link_libraries(my_fiui_tool PRIVATE fiui::fiui)

if(WIN32)
    add_custom_command(TARGET my_fiui_tool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fiui::fiui>
            $<TARGET_FILE_DIR:my_fiui_tool>
    )
endif()
```

Configure it with:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -Dfiui_DIR="D:/fiui/out/fiui-release/lib/cmake/fiui"
cmake --build build --config Release
```

## Minimal Application

```cpp
#include <fiui/fiui.h>

int main()
{
    fiui::Window window("Tool");
    window.debug_id("tool_window").size(640, 360);

    fiui::Column root;
    root.debug_id("root").padding(fiui::space::page).gap(fiui::space::md);

    fiui::Text title("Tool");
    title.debug_id("title").style(fiui::text::title);

    fiui::Button apply("Apply");
    apply.debug_id("apply_button");

    root.add(title);
    root.add(apply);
    window.content(root);

    return fiui::run(window);
}
```

## Required Smoke Checks

Before marking an alpha build usable:

- Run Debug and Release verification scripts.
- Confirm `tests/package_smoke` passes through installed `find_package`.
- Run `build/Debug/fiui_settings_panel.exe --run`.
- Run `build/Debug/fiui_control_center_demo.exe --run`.
- Inspect `fiui-diagnostics/settings_panel/self-test.txt` after the default
  acceptance run.

## Handoff Rules

- Treat `include/fiui/*.h` as the only public API.
- Do not include or depend on `src/*` from downstream applications.
- Keep `fiui.dll` next to the consuming executable or otherwise on `PATH`.
- Prefer explicit `debug_id()` for all meaningful widgets.
- Do not attach the same widget instance to multiple parents.
- Use `visible(false)` for state-preserving conditional UI.
- Keep alpha bug reports with `fiui-frame.json`, `fiui-trace.jsonl`, and
  `self-test.txt` when available.
