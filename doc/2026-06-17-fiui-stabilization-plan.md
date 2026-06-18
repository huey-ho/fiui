# fiui v0 Stabilization Plan

Date: 2026-06-17

This document turns the four-week stabilization plan into repository policy.
The goal is to make `fiui` stable enough for small native tools before adding
more widgets or large rendering features.

## Stabilization Priorities

1. Document the v0 contract and keep examples easy to run.
2. Lock layout behavior for `size`, `fill`, `flex`, `overflow`, `ScrollView`,
   and `SplitView`.
3. Harden focus, keyboard input, clipboard, selection, IME diagnostics, and
   destroyed-target cleanup.
4. Keep diagnostics schemas and backend smoke behavior covered by tests.
5. Add install and packaging basics without changing the public API.

## v0 Contract

The public API remains source-compatible. Additive helpers are allowed only when
they do not change existing examples.

Committed v0 behaviors:

- Public widgets are shallow value handles with runtime-owned object identity.
- Duplicate attach fails and is diagnosed.
- Every widget has bounds, paint bounds, clip bounds, dirty reason, and path
  metadata after layout.
- `Row` allocates remaining width only to `fill_width()` and `flex()` children.
- `Column` allocates remaining height only to `fill_height()` and `flex()`
  children.
- Single-child viewport-like containers fill their content area by default.
- Keyboard focus traversal skips disabled and destroyed targets.
- Keyboard focus traversal skips invisible targets.
- Text input uses UTF-8 boundary-aware cursor movement and deletion.
- `Input` is single-line; `TextArea` allows newline insertion, line navigation,
  and basic internal scrolling.
- Diagnostics must include trace, frame, and leak files in `AiFriendly` mode.

Non-goals for this stabilization pass:

- No cross-platform backend.
- No selector/cascade styling.
- No visual designer.
- No full rich text editor.
- No virtualized list requirement.
- No full accessibility layer.
- No complete layer-backed shadow, opacity, transform, or rounded-mask renderer.

## Work Prompts

### Master Prompt

```text
You are in D:\fiui. Stabilize fiui v0 so it is usable for small Windows C++ tools.
Prioritize documentation, layout behavior, input/focus hardening, diagnostics,
tests, examples, and packaging basics. Do not make breaking API changes, do not
add more widgets, and do not rewrite the architecture. Keep Debug build and CTest
green after every change.
```

### Week 1 Prompt

```text
Implement user-facing baseline documentation. Read doc/, include/fiui/, examples/,
and CMakeLists.txt first. Update README and stabilization docs with supported
platforms, build/run commands, examples, v0 widget contract, limitations, and
repository hygiene. Do not change runtime behavior.
```

### Week 2 Prompt

```text
Lock layout behavior. Add focused tests for Row, Column, ScrollView, SplitView,
DPI scaling, overflow clip/visible hit-testing, and frame JSON layout fields.
Only fix implementation gaps discovered by those tests.
```

### Week 3 Prompt

```text
Harden input and focus. Add focused tests for Tab and Shift+Tab traversal,
disabled controls, click focus, Ctrl+A/C/V/X, Home/End, Backspace/Delete,
Unicode cursor boundaries, TextArea newline insertion, IME diagnostics, and
destroyed focus target cleanup. Only make compatible fixes.
```

### Week 4 Prompt

```text
Finish regression and release basics. Add diagnostics schema checks, hidden-window
backend smoke, install rules, example acceptance checks, and packaging notes.
Keep settings_panel and control_center_demo as public API-only acceptance samples.
```

## Verification

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Run `fiui_control_center_demo --run` manually when validating visible-window
interaction. Use `--run-ai` only when diagnosing frame/trace output because it
enables heavier diagnostics in the visible window.
