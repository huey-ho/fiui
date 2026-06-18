# fiui v0 Control Contract

Date: 2026-06-17

This document is the user-facing contract for controls and layout behavior in
the v0 stabilization line. It describes behavior that tests should preserve.

## Authoring Model

- Widgets are shallow value handles. Copying a widget preserves the same object
  id and generation.
- A widget implementation can be attached to one parent at a time.
- Duplicate attach returns `false` and emits diagnostics.
- Prefer explicit `debug_id()` values for every meaningful node.
- `visible(false)` keeps object identity and state but removes the node from
  layout, rendering, hit-testing, and focus traversal.
- Public callbacks use `void (*)(void*)` in v0.

## Layout Rules

- `size(width, height)` uses positive values as fixed size.
- Zero or negative size values are auto size.
- `fill_width()` and `fill_height()` request available parent space on that
  axis.
- `flex(grow)` participates in remaining-space allocation on the active axis.
- `Column` stacks children vertically. Non-fixed children fill available width.
- `Row` and `Toolbar` stack children horizontally. Non-fixed children fill
  available height.
- `ScrollView` clips to its viewport and tracks scroll content height.
- `SplitView` lays out exactly two panes plus a handle. Ratio is clamped to a
  useful range.
- `OverflowPolicy::Clip` clips drawing and hit testing to the parent.
- `OverflowPolicy::Visible` allows child hit testing outside parent bounds.
- Invisible children do not consume row/column gap or flex space.

## Core Controls

| Control | v0 commitment |
| --- | --- |
| `Window` | Root model, fixed title/size, visible `run()` and deterministic `render_frame()` paths. |
| `Text` | Plain text, title/body/caption styles, multiline flag, text metrics diagnostics. |
| `Button` | Label/content composition, click callback, keyboard activation, state styling. |
| `Input` | Single-line text, value getter, change callback, focus, caret, selection, UTF-8 cursor movement, clipboard paste/copy/cut. |
| `TextArea` | Multiline text input, value getter, change callback, newline insertion, basic line navigation, scrolling; not a rich text editor. |
| `Image` | Resource path, fit mode metadata, basic texture upload/draw path. |
| `Progress` | Numeric 0..1 visual state. |
| `Slider` | Numeric 0..1 value, pointer and keyboard adjustment, change callback. |
| `Select` | Options, selected index/text, popup open/close, keyboard navigation. |
| `ListView` | Item list, selected index/text, keyboard and pointer selection. |
| `TreeView` | Tree items, expand/collapse, selection. |
| `TableView` | Columns, rows, selection, simple sorting, scroll diagnostics. |
| `Tabs` | Tab labels/content and selected index. |
| `Dialog` | Modal overlay, content, escape/backdrop close policy. |
| `MenuBar`/`MenuItem` | Menu composition, disabled/checked/shortcut display, keyboard shortcuts. |

## Input And Focus

- Tab moves forward through enabled focusable controls.
- Shift+Tab moves backward.
- Disabled and destroyed controls are skipped.
- Invisible controls are skipped.
- Pointer down focuses focusable controls and captures pointer input.
- Window focus loss clears focus, capture, and hover targets.
- `Ctrl+A` selects focused editable text.
- `Ctrl+C` copies selected text, or the whole focused editable text when no
  selection exists.
- `Ctrl+V` pastes focused editable text.
- `Ctrl+X` copies and deletes selected focused editable text.
- Programmatic `Input::value(...)` and `TextArea::value(...)` mutate state but
  do not invoke `on_change`; user/platform text edits do invoke `on_change`.
- IME events are diagnosed with the focused target, but v0 does not implement a
  composition UI.

## Diagnostics

`DebugMode::AiFriendly` must produce:

- `fiui-trace.jsonl`
- `fiui-frame.json`
- `fiui-leaks.json`

Frame JSON must include dirty data, render tree summary, layer tree summary,
display list summary, backend summary, scheduler summary, timing data, and the
widget tree with layout metadata.

## Explicit Non-Goals

- Cross-platform backend.
- Selector/cascade styling.
- Full rich text editing.
- Full accessibility layer.
- Virtualized list requirement.
- True layer-backed shadows, opacity, transforms, and rounded masks as a v0
  stabilization blocker.
