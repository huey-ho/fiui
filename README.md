# fiui

`fiui` is a Windows C++20 UI runtime for small native desktop tools. The current
repository is a v0 stabilization build: the runtime, widget handles, layout,
events, dirty tracking, diagnostics, resources, and D3D11 backend are present,
but the project is not yet a broad production UI toolkit.

## Supported Platform

- Windows 10/11 x64
- MSVC with CMake 3.24 or newer
- D3D11, DXGI, DirectWrite, WIC, and Win32

Cross-platform support, browser embedding, script UI, visual designer support,
and full rich text editing are out of scope for v0.

## Build And Test

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

For the full local release smoke:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/verify.ps1 -Config Debug
powershell -ExecutionPolicy Bypass -File scripts/verify.ps1 -Config Release
```

The default build creates `fiui.dll`, core tests, stability tests, and the
example programs. The full verification script also installs `fiui` and builds
`tests/package_smoke` as a downstream `find_package(fiui CONFIG REQUIRED)`
consumer.

## Quick Start

```cpp
#include <fiui/fiui.h>

int main()
{
    fiui::DiagnosticsConfig diagnostics;
    diagnostics.mode = fiui::DebugMode::AiFriendly;
    diagnostics.output_directory = "fiui-diagnostics/hello";
    fiui::configure_diagnostics(diagnostics);

    fiui::Window window("Hello");
    window.debug_id("hello_window").size(640, 360);

    fiui::Column root;
    root.debug_id("root").padding(fiui::space::page).gap(fiui::space::md);

    fiui::Text title("Hello fiui");
    title.debug_id("title").style(fiui::text::title);

    fiui::Button button("Render frame");
    button.debug_id("render_button");

    root.add(title);
    root.add(button);
    window.content(root);

    fiui::render_frame(window);
    fiui::flush_diagnostics();
    return 0;
}
```

Use `render_frame(window)` for deterministic diagnostics and examples. Use
`run(window)` for a visible Win32 window.

## Examples

- `fiui_hello`: minimal frame rendering and diagnostics.
- `fiui_settings_panel`: focused v0 acceptance sample with menu, toolbar,
  layout-mode buttons, tabs, form controls, table, dialog, theme switch, and
  diagnostics. Pass `--run` for visible-window testing, or `--run-ai` when
  collecting high-detail frame and trace diagnostics. The default acceptance
  run writes `fiui-diagnostics/settings_panel/self-test.txt` with frame ids,
  layout sizes, state changes, and callback results.
- `fiui_diagnostics_demo`: duplicate attach and callback failure diagnostics.
- `fiui_control_center_demo`: broader dogfood UI. Pass `--run` for responsive
  visible-window testing. `--run-ai` also enables high-detail diagnostics and is
  intentionally heavier.

## v0 Control Contract

Stable v0 controls are `Window`, `Text`, `Button`, `Input`, `TextArea`,
`Image`, `Progress`, `Slider`, `Select`, `ListView`, `TreeView`, `TableView`,
`Tabs`, `Toolbar`, `Column`, `Row`, `ScrollView`, `Dialog`, `SplitView`,
`MenuBar`, `MenuItem`, `Separator`, and `Spacer`.

Important authoring rules:

- Widgets are small value handles; copying a widget preserves object identity.
- A widget implementation may have only one parent. Duplicate attach is rejected
  and recorded in diagnostics.
- Prefer explicit `debug_id()` values. They make frame and trace diagnostics
  usable.
- Positive `size(width, height)` values are fixed. Zero or negative values mean
  auto sizing.
- `fill_width()`, `fill_height()`, and `fill()` request parent fill.
- `flex(grow)` participates in remaining-space allocation on the active layout
  axis.
- `visible(false)` keeps widget identity alive while removing the node from
  layout, rendering, hit-testing, and focus traversal.
- `overflow(Clip)` clips child drawing and hit-testing, `overflow(Visible)`
  allows hit-testing outside parent bounds, and `overflow(Scroll)` is reserved
  for scrollable surfaces.
- `Input` and `TextArea` expose value getters and `on_change(...)`; programmatic
  `value(...)` mutation does not invoke change callbacks.
- Callbacks use `void (*)(void*)` to keep the v0 ABI small.

## Current Limitations

- Rounded clipping, shadows, opacity, and transforms still include conservative
  backend scaffolds and fallbacks.
- Text editing supports focused input, selection, clipboard, Unicode boundary
  handling, basic multiline `TextArea` navigation/scrolling, and newline input,
  but full IME composition UI, undo/redo, accessibility, and rich text are not
  v0 commitments.
- Resource and rendering diagnostics are stable enough for tests, but cache
  eviction and advanced image/text policy remain future work.

## Documentation

- Architecture source of truth:
  `doc/2026-06-11-fiui-v0-runtime-architecture.md`
- Implementation queue:
  `doc/2026-06-11-fiui-v0-implementation-plan.md`
- Stabilization execution:
  `doc/2026-06-17-fiui-stabilization-plan.md`
- Release checklist:
  `doc/2026-06-17-fiui-release-checklist.md`
- Control contract:
  `doc/2026-06-17-fiui-v0-control-contract.md`
- Alpha distribution guide:
  `doc/2026-06-18-fiui-v0-alpha-distribution.md`
