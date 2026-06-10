# fiui Framework Design

Date: 2026-06-10

## Summary

`fiui` is a modern Windows UI framework for C++ applications. Its first version targets Windows 10/11 x64 with MSVC, ships as a small native DLL, uses a simple declarative C++ API, renders through D3D11 and DirectWrite, and includes built-in modern visual styles that are suitable for AI-generated desktop tools.

The framework has two first-class goals:

- AI can generate useful, good-looking desktop tool interfaces with minimal code.
- AI can debug failures from structured diagnostics instead of guessing from noisy logs.

## Product Positioning

`fiui` is designed for:

- AI-generated desktop tools.
- Settings panels, local dashboards, admin utilities, and productivity tools.
- Simple image and media display tools.
- Native Windows applications that need low runtime overhead and a small distribution footprint.

The first version does not target:

- Cross-platform UI.
- WebView or Chromium-based rendering.
- Windows 7/8 compatibility.
- A full game engine or 3D editor.
- A complete Qt/WPF-scale widget ecosystem.
- A first-release focus on full media zero-copy pipelines.

## Success Criteria

The first usable release should satisfy these criteria:

- Apps can ship with `fiui.dll`, `fiui.lib`, and `include/fiui/*.h`.
- A simple window can be written in a few dozen lines of C++.
- The default visual result is modern and acceptable without hand-tuned styling.
- Controls are responsive and high-DPI aware.
- Rendering is self-drawn through D3D11 and supports dirty node and dirty rectangle invalidation.
- Diagnostics can explain crashes, exceptions, layout issues, rendering issues, resource leaks, and suspicious lifetime mistakes in structured files that AI can read.
- The renderer and resource model leave a clear path for future D3D texture input, video surfaces, shared handles, and zero-copy media workflows.

## Architecture

`fiui` should be implemented as one DLL with internally separated modules and a stable C++ header API.

```text
App / User Code
   |
fiui Explicit Declarative C++ API
   |
Widget Tree + State + Event System
   |
Layout Engine ---- Style / Theme System
   |                    |
Render Tree ------ Resource System
   |
Display List + Dirty Rects
   |
D3D11 Renderer + DirectWrite Text
   |
Win32 Window / Input / DPI / IME
```

### Core

`Core` owns common infrastructure:

- Object IDs and object lifetime state.
- Handles and validation.
- Error categories and result types.
- Strings, time, thread identity, and small utility containers where needed.
- Debug metadata for object creation, attachment, detachment, and destruction.

### App

`App` wraps native application concerns:

- `fiui::run()`.
- Win32 message loop.
- Window lifecycle.
- Timers and task queue.
- Application-level theme and debug configuration.

Users should not need to write raw Win32 message dispatch for normal applications.

### Platform

`Platform` wraps Windows-specific APIs:

- HWND creation and destruction.
- DPI and monitor changes.
- Mouse, keyboard, touch, wheel, and IME input.
- Clipboard and file dialogs.
- System theme detection.
- Non-client area behavior where needed.

### Widget

`Widget` owns the user-visible UI tree:

- Window, text, buttons, inputs, lists, images, progress bars, dialogs, tabs, and toolbars.
- State and event callbacks.
- Debug IDs and generated tree paths.
- Public API that is simple enough for AI to generate and modify safely.

### Layout

`Layout` computes size and position using a constraint-based model:

- Parents pass constraints to children.
- Children return measured sizes.
- Containers position children.
- Layout invalidation propagates only as far as required.

The first layout set should include `Row`, `Column`, `Stack`, `Grid`, `Wrap`, `Padding`, `Align`, `Center`, `SizedBox`, and `ScrollView`.

### Style

`Style` owns design tokens, themes, and control states:

- Color tokens.
- Font tokens.
- Spacing tokens.
- Radius tokens.
- Density modes.
- Hover, pressed, focused, disabled, selected, and error states.

AI-generated code should usually choose themes and tokens, not raw colors and pixel values.

### Render

`Render` converts layout results into a retained render tree and then into a per-frame display list. The first backend uses:

- D3D11.
- DXGI swap chains.
- DirectWrite for text.
- WIC for initial image decoding.

The renderer should batch compatible drawing operations and use scissor/clip state for dirty rectangle repaint.

### Resource

`Resource` manages:

- Images.
- Fonts.
- D3D textures.
- DirectWrite resources.
- Caches.
- Future external texture ownership models.

The resource model must not assume every visual asset starts as a CPU bitmap. This keeps the path open for shared D3D textures and video surfaces.

### Diagnostics

`Diagnostics` is a first-class module and must be integrated across every layer. It records structured context for AI and developer debugging:

- Object lifetime.
- Widget tree snapshots.
- Layout tree snapshots.
- Render tree snapshots.
- Display lists.
- Dirty node and dirty rectangle sources.
- Event dispatch paths.
- Resource creation and release.
- Thread identity and thread misuse.
- D3D debug messages when available.
- Crash and exception context.

## AI-Friendly C++ API

The official API style is explicit declarative C++. Chainable helpers may exist, but documentation and AI templates should prefer block-based construction because it is easier to generate, modify, and debug.

Recommended AI-generated style:

```cpp
fiui::Window main_window;
main_window.title("Settings");
main_window.size(900, 600);
main_window.debug_id("main_window");

fiui::Column root;
root.padding(16);
root.gap(12);
root.debug_id("settings_root");

fiui::Text title("Settings");
title.style("title");
title.debug_id("title");

fiui::Input project_name;
project_name.placeholder("Project name");
project_name.debug_id("project_name_input");

fiui::Button run_button("Run");
run_button.debug_id("run_button");
run_button.on_click(run_task);

root.add(title);
root.add(project_name);
root.add(run_button);

main_window.content(root);
return main_window;
```

API rules:

- Use direct names such as `Button`, `Input`, `Column`, `Row`, and `List`.
- Avoid operator-heavy DSLs.
- Avoid excessive overloads and implicit conversions.
- Avoid deep expression nesting in official examples.
- Support `debug_id()` on every widget.
- Generate a stable fallback path when no debug ID is provided.
- Report errors by widget path, such as `main_window/settings_root/run_button`.
- Allow controls to be created, named, configured, and then added to a parent.

## Rendering Pipeline

The renderer should use this flow:

```text
Widget Tree
  -> Layout Tree
  -> Render Tree
  -> Display List
  -> Dirty Rect Collection
  -> GPU Command Batches
  -> D3D11 Draw
```

Rendering principles:

- Widgets do not directly own GPU drawing.
- Layout computes bounds, paint bounds, scroll areas, and clip regions.
- Render nodes describe drawable content such as text, rectangles, borders, images, shadows, and clips.
- Display lists provide a serializable summary of what a frame intends to draw.
- Compatible draw commands are batched where practical.
- Text uses DirectWrite with high-DPI and mixed Chinese/English text support.
- Images initially use WIC decode and D3D texture upload.
- Future APIs may accept external D3D11 textures or shared handles.

## Dirty Node and Dirty Rectangle System

Self-rendering must support dirty node and dirty rectangle invalidation from the start.

```text
State Change / Input / Timer
  -> Dirty Node Marking
  -> Layout Invalidation
  -> Dirty Rect Collection
  -> Rect Merge / Clip
  -> Partial Display List Rebuild
  -> D3D Scissor / Clip Draw
  -> Present
```

Rules:

- Every widget and render node tracks `bounds`, `paint_bounds`, and `clip_bounds`.
- Visual-only changes should repaint only the affected control region.
- Text, size, or layout changes trigger layout invalidation and may expand to affected ancestors.
- Shadows, blur, antialiasing, and rounded corners expand dirty paint bounds.
- Scrollable lists repaint only the viewport and changed/newly exposed areas.
- Multiple dirty rectangles are merged per frame.
- When rect count or total area exceeds thresholds, the frame may fall back to full repaint.
- Debug mode records dirty sources, original rects, merged rects, fallback decisions, and repaint cost.

## Diagnostics Design

Diagnostics should produce structured, AI-readable output.

### Debug Modes

```cpp
enum class DebugMode {
    Off,
    Basic,
    AiFriendly,
    Verbose,
    Stress
};
```

- `Off`: no optional diagnostics beyond critical failures.
- `Basic`: lightweight logs and error context.
- `AiFriendly`: structured reports suitable for AI analysis.
- `Verbose`: detailed trace for difficult reproduction.
- `Stress`: additional validation such as canaries, handle checks, thread assertions, and lifetime checks.

### Diagnostic Files

`fiui-trace.jsonl`

Records event streams:

- Window creation and destruction.
- Widget creation and mutation.
- Input events.
- Event dispatch.
- Layout invalidation.
- Paint invalidation.
- Resource load and release.
- D3D warnings and errors.

`fiui-frame.json`

Records a frame snapshot:

- Widget tree.
- Layout tree.
- Render tree.
- Display list.
- Dirty nodes and dirty rectangles.
- Draw call summary.
- Timing summary.

`fiui-crash.json`

Records crash and exception context:

- Exception type.
- Thread identity.
- Last input events.
- Last widget operations.
- Object lifetime summary.
- Resource table summary.
- Last frame summary.

`fiui-leaks.json`

Records shutdown leaks:

- Undestroyed widgets.
- Unreleased textures.
- Fonts and cached resources.
- Active callbacks and subscriptions.
- Last owner or parent where known.

### Native Failure Support

The framework should integrate with native debugging tools rather than pretending to solve all C++ memory failures alone.

Required direction:

- Catch SEH crashes at framework boundaries where possible.
- Wrap C++ exceptions crossing event and task boundaries.
- Store recent API calls and event paths.
- Track object lifecycle states: created, attached, detached, destroying, destroyed.
- Detect use-after-destroy in debug/stress modes when handles pass through framework APIs.
- Provide optional canary and guard allocation checks for framework-owned objects.
- Stay compatible with ASan, PageHeap, Application Verifier, and the D3D debug layer.

## Widgets and Layout Scope

MVP widgets:

- `Window`
- `Text`
- `Button`
- `Input`
- `TextArea`
- `Checkbox`
- `Radio`
- `Select`
- `Slider`
- `Progress`
- `Image`
- `List`
- `ScrollView`
- `Dialog`
- `Tabs`
- `Toolbar`
- `Separator`
- `Spacer`

`List` should support virtualization in the first version because AI-generated tools commonly display large collections.

MVP layout primitives:

- `Row`
- `Column`
- `Stack`
- `Grid`
- `Wrap`
- `Padding`
- `Align`
- `Center`
- `SizedBox`
- `ScrollView`

## Built-In Visual Styles

The first version should ship with four themes:

- `modern.light`: default light desktop tool style.
- `modern.dark`: default dark desktop tool style.
- `compact.light`: dense admin and parameter-panel style.
- `media.dark`: image, video, and monitoring style.

Style direction:

- Clean controls.
- Moderate radius.
- Stable spacing.
- Full hover, pressed, focused, disabled, selected, and error states.
- High-DPI support.
- Chinese and English text support.
- Minimal need for AI-generated raw colors.

The intended authoring model is:

```cpp
app.theme("modern.light");
root.padding(fiui::space::page);
root.gap(fiui::space::md);
title.style(fiui::text::title);
```

AI should express product structure. `fiui` should provide visual quality through defaults and tokens.

## Distribution

Primary outputs:

- `fiui.dll`
- `fiui.lib`
- `include/fiui/*.h`

Platform and toolchain:

- Windows 10/11.
- x64.
- MSVC.
- CMake.

System dependencies:

- Win32.
- D3D11.
- DXGI.
- DirectWrite.
- WIC.

The framework should not depend on:

- .NET.
- WebView.
- Chromium.
- Large external runtimes.

## Repository Structure

Recommended repository layout:

```text
fiui/
  CMakeLists.txt
  include/
    fiui/
      fiui.h
      app.h
      window.h
      widget.h
      layout.h
      style.h
      diagnostics.h
  src/
    core/
    app/
    platform/
    widget/
    layout/
    style/
    render/
    resource/
    diagnostics/
  examples/
    hello/
    settings_panel/
    diagnostics_demo/
  tests/
    core/
    layout/
    diagnostics/
  docs/
    design/
    api/
    ai-guides/
```

## MVP Milestones

### M1: Window and Message Loop

- Implement `fiui::run()`.
- Create a native Win32 window.
- Handle DPI.
- Route mouse and keyboard input.
- Add basic logging.

### M2: D3D11 Rendering Core

- Create swap chain and render target.
- Clear and present frames.
- Draw rectangles, rounded rectangles, borders, and images.
- Render text with DirectWrite.
- Build the first display list implementation.

### M3: Layout and Controls

- Implement the core widget tree.
- Implement row, column, padding, align, and scroll layout.
- Implement first controls: window, text, button, input, image, progress, dialog.
- Add basic event callbacks.

### M4: Style and Themes

- Implement tokens.
- Implement state styles.
- Ship `modern.light`, `modern.dark`, `compact.light`, and `media.dark`.
- Make examples visually acceptable without local style overrides.

### M5: AI Diagnostics Loop

- Implement `DebugMode::AiFriendly`.
- Emit trace, frame, crash, and leak reports.
- Include dirty rectangle reports.
- Include object lifetime reports.
- Add `diagnostics_demo` that intentionally triggers common mistakes.

## Example Applications

The first examples should be:

- `hello`: minimal window.
- `settings_panel`: common AI-generated desktop tool interface.
- `diagnostics_demo`: controlled failures that produce AI-readable diagnostics.

## Open Design Decisions for Later

These are intentionally outside the first approved scope:

- Whether to expose a stable C ABI for non-C++ language bindings.
- Whether to add XML, JSON, or another UI description format.
- Whether to support D3D12 or a multi-backend renderer.
- Whether to implement a full media pipeline with Media Foundation.
- Whether to build an official visual designer.

