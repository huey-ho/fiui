# fiui v0 Implementation Plan

Date: 2026-06-11

This implementation plan upgrades the current v0 skeleton into the target runtime architecture. The plan preserves the existing public value-handle API while progressively separating runtime, object identity, node storage, events, layout, dirty tracking, style, resources, rendering, platform integration, and diagnostics.

This document intentionally avoids naming external UI frameworks. Future AI agents should implement the module contracts described here instead of importing external framework assumptions.

## Current State

The repository currently has a functional v0 runtime scaffold:

- Public value-handle widgets in `include/fiui/widget.h`.
- Internal `WidgetImpl` split into `ObjectHeader`, `NodeData`, `WidgetProperties`, and `DirtyState`.
- `FiuiRuntime` owns the current internal subsystem instances.
- Diagnostics output for trace, frame, and leak files includes session id, frame id, object id, generation, path, category, action, and detail.
- Theme tokens and internal `Theme` model exist for `modern.light` and `modern.dark`.
- Event, layout, dirty tracking, style, resource, text, image, render, platform, backend, diagnostics, and frame scheduler scaffolds exist.
- `settings_panel` is a focused CTest acceptance sample.
- `control_center_demo` is the broader dogfood CTest sample for multi-section UI coverage.
- CTest currently runs `fiui_core_tests`, `fiui_settings_panel_example`, and `fiui_control_center_demo`.

The current weakness is no longer module separation. The main remaining gap is render depth polish: font resource ownership, text layout policy, layer-backed backend execution, richer text editing, real platform clipboard integration, image drawing polish, and deeper object table reporting still need to be implemented.

## Completed Scaffold Milestones

These milestones are already implemented at scaffold level and should not be restarted from scratch:

- Runtime center with default runtime.
- Object and node data split inside `WidgetImpl`.
- Runtime ObjectTable scaffold with object register/unregister, lookup, stale generation diagnostics, destroyed tombstones, and EventSystem target validation.
- Intrusive public value-handle behavior.
- Duplicate attach detection.
- Structured DiagnosticsHub.
- EventSystem click dispatch, pointer route, hit-test, focus/capture/hover targets, callback exception handling, and input dirty invalidation.
- LayoutSystem basic arrangement.
- DirtyTracker dirty classification, clipping, merge, and fallback policy.
- StyleSystem active theme and button state styles.
- ResourceSystem image resource, image metadata storage, and text layout cache scaffold.
- TextSystem DirectWrite factory, text metrics, fallback metrics, text diagnostics, and text metric resource storage.
- ImageSystem WIC factory, image metadata query, fallback metadata, image diagnostics, and image metadata resource storage.
- RenderSystem render tree, layer tree, display list, display style, display resource, and backend summary scaffold.
- PlatformSystem HWND scaffold, pointer input bridge, keyboard route scaffold, basic single-line `Input` text editing, internal plain-text clipboard scaffold, and IME diagnostics scaffold.
- FrameScheduler request/coalesce/complete scaffold.
- D3D11Backend real device/context creation, native window binding, swap chain scaffold, render target view scaffold, rect command execution, rectangular clip push/pop scissor execution, swap chain present, display list submit scaffold, device lost simulation, explicit recovery, backend failure reason, backend diagnostics, and frame JSON backend device summary.
- D3D11Backend basic DirectWrite/Direct2D text drawing, text target binding, text draw diagnostics, and text draw frame counters.
- D3D11Backend internal image texture cache, fallback texture upload, texture lifecycle counters, texture diagnostics, and texture metadata frame JSON payload.
- D3D11Backend basic image textured-quad drawing, image shader pipeline, sampler/blend state, image draw diagnostics, and image draw frame counters.
- D3D11Backend basic rounded-rect surface drawing from display command radius, rounded-rect shader pipeline, alpha blend state, and rounded-rect draw counters.
- D3D11Backend rounded clip command scaffold with rectangular scissor fallback diagnostics and counters.
- `settings_panel` acceptance example.
- `control_center_demo` dogfood example.

## Phase 1: Runtime Center

Status: scaffold implemented; text metric storage is backed by TextSystem.

Goal:

- Introduce a runtime center that owns subsystem state and removes hidden global state over time.

Implementation:

- Add internal `FiuiRuntime` and `RuntimeContext`.
- Move object id generation, frame id generation, diagnostics session config, dirty thresholds, and runtime settings into the runtime context.
- Add a default runtime for existing simple APIs.
- Keep `render_frame(window)` and `run(window)` working through the default runtime.
- Add `RuntimeConfig` internally first; expose public config only after the internal contract is stable.

Boundaries:

- The runtime coordinates subsystems.
- The runtime does not implement concrete control drawing.
- The runtime does not own user business state.

Acceptance tests:

- Existing `fiui_core_tests` passes unchanged.
- Two runtime sessions can have independent frame counters in internal tests.
- Default runtime behavior remains compatible with current examples.

## Phase 2: ObjectSystem and NodeTree

Status: scaffold implemented inside `WidgetImpl`; runtime ObjectTable lookup and stale generation diagnostics are implemented.

Goal:

- Split identity/lifetime data from widget properties and node data.

Implementation:

- Refactor `WidgetImpl` into these internal parts:
  - `ObjectHeader`: object id, generation, refcount, lifecycle state.
  - `NodeData`: parent, children, debug id, fallback id, cached path, subscriptions.
  - `WidgetProperties`: text, placeholder, style name, resource path, layout hints, numeric value.
  - `DirtyState`: bounds, paint bounds, clip bounds, dirty reason, last mutation frame.
- Keep public value handles unchanged.
- Keep duplicate attach rejection.
- Add stale generation diagnostics for invalid or outdated handles where possible.
- Register objects at creation and mark destroyed tombstones at final release.
- Validate EventSystem focus/capture/hover targets through object id and generation before use.
- Record attach/detach/lifecycle actions through DiagnosticsHub.

Boundaries:

- ObjectSystem handles identity and lifecycle only.
- NodeTree handles hierarchy and stable node data.
- Neither system performs layout or drawing.

Acceptance tests:

- Copying a widget preserves object identity and increments internal refcount.
- Destroying a copy decrements refcount without destroying attached node ownership.
- Duplicate attach is rejected and reported.
- Detach updates lifecycle and dirty state.
- Stale or invalid handle use is reported in diagnostics.
- Destroyed focus targets are cleared before keyboard, clipboard, or IME use.

## Phase 3: EventSystem

Status: scaffold implemented and integrated with PlatformSystem pointer input.

Goal:

- Replace direct callback execution paths with a structured event route.

Implementation:

- Add internal `FiuiEvent`.
- Add event queue and event dispatch functions.
- Track focus target, capture target, hover target, and active event path.
- Add event filter scaffold.
- Change `Button::click()` to enqueue or dispatch an internal click event before invoking the subscription.
- Wrap callback failures and report exception category, event path, object id, generation, and widget path.

Boundaries:

- EventSystem routes events and subscriptions.
- EventSystem does not perform layout or drawing.
- Event-caused state changes must mark dirty state through NodeTree and DirtyTracker.

Acceptance tests:

- Button click produces an event trace with event path.
- Callback exception is reported and does not crash the test process.
- Focus/capture/hover target fields can be set and included in diagnostics.
- Existing public `on_click` behavior remains usable.
- Pointer move/down/up can route from root hit-test to target.
- Target changes mark old and new nodes dirty with input/paint reasons.

## Phase 4: LayoutSystem and DirtyTracker

Status: scaffold implemented; texture metadata and backend upload lifecycle are implemented at scaffold level.

Goal:

- Move layout and dirty planning out of the frame implementation.

Implementation:

- Add `LayoutSystem` with constraints, measured size, arranged bounds, clip bounds, paint bounds, and layout tree output.
- Implement initial layout behaviors for `Column`, `Row`, `Padding`, `Align`, `SizedBox`, `ScrollView`, `Text`, `Button`, `Input`, `Image`, `Progress`, `Separator`, and `Spacer`.
- Add `DirtyTracker` with dirty node records, dirty reason separation, propagation, parent clipping, rect merge, and fallback thresholds.
- Keep full repaint fallback allowed.
- Require fallback reason in every full repaint decision.
- Emit dirty planner summary into `fiui-frame.json`.

Boundaries:

- LayoutSystem computes geometry only.
- DirtyTracker computes repaint planning only.
- Neither performs backend drawing.

Acceptance tests:

- Text change produces text/layout/paint dirty as appropriate.
- Style change produces style/paint dirty.
- Resize produces resize/layout/paint dirty.
- Attach/detach produces hierarchy/layout/paint dirty.
- Dirty rectangles are clipped through parent clip bounds.
- Dirty threshold fallback includes stable reason.
- Idle frame has no dirty rects and no unnecessary layout rebuild.

## Phase 5: StyleSystem and ResourceSystem

Status: scaffold implemented.

Goal:

- Expand visual style and resource ownership into explicit subsystems.

Implementation:

- Replace the thin token-only model with an internal `Theme` model:
  - color scheme
  - typography
  - spacing
  - radius and shape
  - shadow/elevation
  - motion
  - density
  - component themes
  - state styles
- Keep `modern.light` and `modern.dark` stable.
- Add component style scaffolds for Button, Input, Text, Container/Card-like surfaces, and Window background.
- Add ResourceSystem scaffold:
  - font resource id
  - image resource id
  - image metadata attached to image records
  - text layout cache entry
  - text metrics attached to text layout records
  - D3D texture resource id
  - texture metadata attached to image records
  - future external texture entry
- Add TextSystem scaffold:
  - DirectWrite factory
  - text measurement
  - deterministic fallback metrics
  - measurement diagnostics
  - text/style/theme/DPI cache key inputs
- Add ImageSystem scaffold:
  - WIC factory
  - image metadata query
  - deterministic fallback metadata
  - image diagnostics
- Emit resource create/cache/release events.

Boundaries:

- StyleSystem resolves visual policy.
- ResourceSystem owns resource identity, cache, and resource diagnostics.
- TextSystem owns text measurement.
- ImageSystem owns image metadata queries.
- StyleSystem does not load resources directly.
- ResourceSystem does not decide widget layout.
- TextSystem does not draw text or own resource ids.
- ImageSystem does not upload textures, draw images, or own resource ids.

Acceptance tests:

- Switching between `modern.light` and `modern.dark` changes resolved tokens without app raw colors.
- Button states resolve normal, hover, pressed, focused, disabled, and error style records.
- Resource creation and release appear in trace output.
- Text metric requests produce cached text layout metrics from DirectWrite or deterministic fallback.
- Image metadata requests produce cached image metadata from WIC or deterministic fallback.
- Image display commands produce backend texture metadata and fallback texture diagnostics when source decoding is unavailable.
- Render display commands consume theme state and resource records.
- Text layout resources are cached and repeated frame builds do not grow live resource count.

## Phase 6: RenderSystem Layering

Status: render tree, semantic layer tree, display list, backend summary, real D3D11 device/context creation, native window binding scaffold, swap chain scaffold, render target view scaffold, backend submit scaffold, rect draw path, rounded-rect surface draw path, basic shadow fallback draw path, opacity/transform identity command scaffold, basic DirectWrite/Direct2D text draw path, basic image textured-quad draw path, rectangular clip push/pop scissor path, rounded clip command scaffold with rectangular fallback, swap chain present path, image texture upload scaffold, unsupported draw diagnostics, and device recovery scaffold implemented; full layer-tree execution, true shadow blur/cache, true rounded masking, and real opacity/transform compositing are pending.

Goal:

- Create a render pipeline that can later map cleanly to D3D11 without changing public widgets.

Implementation:

- Add render tree data structures.
- Add layer tree data structures for clip, opacity, transform, text, image, rounded rect, shadow, and scroll layers.
- Link layer records to display command ranges and include layer summaries in frame JSON.
- Add serializable display list commands.
- Add batch list scaffold.
- Add D3D11 backend interface with placeholder implementation if real drawing is not ready.
- Submit display list and batch list through the backend scaffold.
- Create D3D11 device/context and record device availability in backend diagnostics.
- Bind native windows to backend swap chain and render target scaffolds when a platform window exists.
- Provide explicit device lost recovery without silent recovery in submit.
- Execute rect display commands through the D3D11 backend when a render target is available.
- Present the swap chain when a native window is bound.
- Execute basic rectangular clip display commands through D3D scissor state and Direct2D axis-aligned clips.
- Emit `clip` and `clip_end` display-list commands for scroll clip push/pop.
- Support nested rectangular clip stacks in the backend.
- Emit rounded clip command pairs for rounded scroll clip regions.
- Execute rounded clip commands as rectangular scissor fallback with stable diagnostics and counters.
- Keep advanced clipping, true rounded masking, real opacity/transform compositing, and true shadow blur/cache pending until the backend consumes layer-tree semantics directly.
- Emit opacity and transform display-list command pairs for the root visual scaffold.
- Execute opacity and transform commands in the backend as identity/pass-through scaffolds with stable diagnostics and counters.
- Draw text display commands through the backend when a render target is available.
- Keep text drawing scope limited to basic text, color, font size, and bounds clipping until font resources and richer layout policy exist.
- Draw rounded rectangle surface commands when display style radius is greater than zero.
- Keep rounded surface scope limited to surface fill until true rounded masking exists.
- Draw shadow display commands through a conservative rounded-surface fallback before the owning surface command.
- Record shadow draw and fallback counters; do not treat fallback shadows as complete blur, mask, cache, or layer-tree shadow execution.
- Upload image display resources into backend-owned textures before drawing image commands.
- Draw image display commands as basic textured quads when a render target is available.
- Keep image drawing scope limited to full-resource sampling into command bounds until layer-backed clipping and image fit policies exist.
- Keep backend texture objects private and expose only serializable texture metadata through resource/frame diagnostics.
- Update frame JSON to include widget tree, layout tree summary, render tree summary, display list summary, dirty planner summary, backend summary, backend device summary, and timings.

Boundaries:

- RenderSystem consumes NodeTree, LayoutSystem, StyleSystem, ResourceSystem, and DirtyTracker outputs.
- RenderSystem does not own event routing.
- Backend does not decide dirty propagation.

Acceptance tests:

- Display list contains stable command types for text, rect, border, image, clip, clip end, and rounded rect.
- Backend summary includes command count and batch count.
- Backend device summary includes submitted state, failure reason, consumed command count, device state, and submit counters.
- Device lost simulation emits diagnostics and causes the next submit to fail with explicit failure reason.
- Device recovery re-creates the D3D11 device/context and records recovery diagnostics.
- Hidden-window backend test clears a render target, draws rect/text/image commands, applies rectangular clip commands, and presents the swap chain.
- Full repaint and partial repaint candidates both produce display list diagnostics.
- Frame schema remains stable across examples.
- Display command style payload includes theme and control state.
- Display command resource payload includes resource id, kind, cache state, owner object id, owner path, and key.
- Image resource payload includes image metadata and texture metadata after backend submission.
- Backend device summary includes texture create/upload/fallback/release/failure/live counters.
- Backend device summary includes image draw and image pipeline counters.
- Backend device summary includes text draw, text pipeline, and text draw failure counters.
- Backend device summary includes rounded rectangle draw and rounded rectangle pipeline counters.
- Backend summary and backend device summary include shadow command, draw, and fallback counters.
- Backend summary and backend device summary include opacity/transform command and scaffold apply counters.
- Backend summary and backend device summary include rounded clip command, apply, and fallback counters.
- Backend device summary includes clip command, clip end, clip apply, clip stack depth, and scissor state counters.
- Layer tree includes root, rect, rounded rect, text, image, clip, scroll, opacity, transform, and shadow layer records.
- Layer records expose object id, path, bounds, style/resource payload, parent id, child count, and display command linkage.

## Phase 7: PlatformSystem

Status: scaffold implemented; pointer input, keyboard events, basic single-line `Input` text editing, internal plain-text clipboard paste/copy, and IME diagnostics are bridged to EventSystem/DiagnosticsHub.

Goal:

- Put Windows integration behind a dedicated boundary.

Implementation:

- Add `PlatformSystem` for HWND lifecycle, DPI, resize, input, IME, clipboard, timers, task queue, and device lost signals.
- Convert Win32 messages into internal events.
- Convert `WM_KEYDOWN`, `WM_KEYUP`, and `WM_CHAR` into structured keyboard/text input events.
- Route keyboard events to the current focus target.
- Support basic `Input` editing for text insert, Backspace, Delete, Left, and Right.
- Add internal plain-text clipboard buffer for copy/paste diagnostics.
- Route paste into the focused `Input` through EventSystem text insertion.
- Record IME start/composition/end diagnostics against the current focus target.
- Keep ordinary users on `fiui::run()`.
- Make platform failures visible to DiagnosticsHub.

Boundaries:

- PlatformSystem owns native message translation.
- PlatformSystem does not generate render tree data.
- PlatformSystem does not expose raw message handling as normal user code.
- Current text editing does not include selection, undo/redo, multiline editing, IME composition UI, real OS clipboard integration, or full Unicode editing policy.

Acceptance tests:

- Window create failure is diagnosed.
- Resize marks layout and paint dirty.
- DPI change marks layout/resource dirty.
- Keyboard and mouse messages can be converted into internal events.
- Mouse move/down/up routes through EventSystem.
- Keyboard events route to the current focus target.
- Basic `Input` text insert, Backspace, Delete, Left, and Right mutate text, dirty state, and diagnostics.
- Clipboard paste mutates focused `Input` text through EventSystem and emits clipboard diagnostics.
- IME start/composition/end events emit target-aware diagnostics.
- Platform pointer routes request frames through FrameScheduler.

## Phase 8: FrameScheduler

Status: scaffold implemented.

Goal:

- Centralize frame requests from API mutation, platform input, resize, DPI, theme change, resource change, and future animation/timer sources.

Implementation:

- Add `FrameScheduler`.
- Track pending state, request count, coalesced count, completed count, last request frame id, last completed frame id, last object id, last generation, last reason, last source, and last path.
- Request frames from `mutate_widget`, PlatformSystem events, and theme changes.
- Complete frames at the end of `render_frame`.
- Emit scheduler trace events and frame JSON summary.

Boundaries:

- FrameScheduler does not do layout.
- FrameScheduler does not render.
- FrameScheduler does not own the platform message loop.
- FrameScheduler does not replace DirtyTracker.

Acceptance tests:

- API mutation requests a frame.
- Multiple requests coalesce while pending.
- `render_frame` clears pending and records completed frame id.
- Platform pointer input requests a frame.
- Frame JSON includes `scheduler_summary`.

## Phase 9: Documentation Authority

Goal:

- Keep future AI and engineering work aligned.

Implementation:

- Maintain this implementation plan and the runtime architecture document as the primary sources.
- Keep the older design document as historical context.
- Add a note at the top of the older design document that the 2026-06-11 documents are authoritative.
- Do not add external UI framework names to new design documents.

Acceptance tests:

- Searching new documents for external UI framework names returns no matches.
- The older design document points to the new architecture and implementation plan.

## Public API Changes

Add or extend public APIs only when the internal contract is stable:

- `fiui::RuntimeConfig`
  - Diagnostics settings.
  - Theme name.
  - Performance and dirty threshold settings.
  - Diagnostics output directory.
- `fiui::Runtime`
  - Advanced entry point.
  - Default runtime remains available.
- `fiui::run(window, config)`
  - Keeps `run(window)`.
- `fiui::FrameReport`
  - Add layout, render, dirty, resource, diagnostics timings.
  - Add fallback reason and backend summary.
- `fiui::Theme`
  - Expand current token model.
  - Preserve compatibility with existing theme token lookup.

Do not add:

- Public `std::shared_ptr` ownership.
- Selector/cascade style engine.
- Script-based UI language.
- Full signal/slot syntax system.
- Virtualized list as a v0 blocker.
- Rich text editor as a v0 blocker.
- Visual designer as a v0 blocker.

## Verification Commands

Use these commands after implementation work:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Required Test Coverage

- Lifecycle:
  - copy/refcount
  - duplicate attach
  - detach
  - stale generation
  - destroy diagnostics
- Events:
  - click route
  - callback exception
  - focus target
  - capture target scaffold
  - pointer hit-test
  - pointer route
  - platform input bridge
  - input state dirty invalidation
- Dirty tracking:
  - text change
  - style change
  - resize
  - input
  - attach/detach
  - fallback reason
- Style:
  - theme switch
  - component state tokens
  - local override scaffold
- Resources:
  - font resource create/release
  - image resource create/release
  - text layout cache event
  - texture upload lifecycle event scaffold
  - fallback texture metadata
- Scheduler:
  - request frame
  - coalescing
  - complete frame
  - scheduler frame JSON summary
- Diagnostics:
  - trace schema
  - frame schema
  - leak schema
  - session id
  - frame id
  - object id
  - generation
  - path
  - category
  - action
- Examples:
  - `hello`
  - `settings_panel` as acceptance example
  - `control_center_demo` as dogfood example
  - `diagnostics_demo`

## Acceptance Example

`examples/settings_panel/main.cpp` is the focused v0 acceptance example.

`examples/control_center_demo/main.cpp` is the broader dogfood example. It should stay public-API-only and continue to cover navigation-like composition, multi-section layout, scroll, inputs, image resource replacement, progress, callbacks, theme switching, duplicate attach rejection, callback exception capture, frame reports, and diagnostics output.

The focused acceptance example must continue to cover:

- Modern theme setup.
- Layout across text, input, image, progress, and buttons.
- Image resource creation and replacement.
- Text layout resource creation.
- Successful callback execution.
- Callback exception capture.
- Duplicate attach diagnostics.
- Theme switch across frames.
- Dirty planner output.
- Scheduler request and complete diagnostics.
- Display list style/resource payloads.
- Frame JSON and trace JSONL output.

The examples are registered in CTest as `fiui_settings_panel_example` and `fiui_control_center_demo`.

## Next Work Queue

Implement these next, in order:

1. Layout sizing semantics:
   - Status: first-pass implementation completed. Row main-axis `Auto` no longer consumes remaining space; only `fill_width()` and `flex(grow)` participate in remaining-width allocation. `size(0, 0)` is covered as auto sizing, not flex sizing. Cross-axis behavior is explicit: Column children fill available width unless fixed width is set, and Row children fill available height unless fixed height is set. Frame JSON widget nodes now include layout diagnostics with width/height mode, overflow policy, explicit overflow flag, requested size, flex grow, padding, gap, final bounds, paint bounds, clip bounds, child union bounds, and overflow direction/amount fields. The control center demo includes layout mode buttons for balanced, dialog-wide, preview-tall, intentional overflow, and clip/visible overflow policy scenarios. Hit-testing respects `OverflowPolicy::Clip` vs `OverflowPolicy::Visible`. Ordinary container render clipping emits display-list clip commands when direct child layout bounds overflow, while explicit `.overflow(OverflowPolicy::Clip)` can force clipping. True layer-backed mask and rounded overflow clipping remain later backend work.
   - `size(width, height)` uses positive values as fixed size.
   - zero or negative size values are treated as auto, not as flex.
   - `fill_width()`, `fill_height()`, and `fill()` express parent-fill intent.
   - `flex(grow)` expresses remaining-space allocation in the active layout axis.
   - Window-like and viewport-like single-child containers fill their content area by default.
   - Row/Column resize behavior must stay covered by tests before expanding widgets.
2. Text pipeline:
   - font resource records.
   - text alignment and paragraph policy.
   - text overflow and ellipsis policy.
   - font fallback diagnostics.
   - richer text invalidation policy beyond cache-key separation.
3. Image pipeline:
   - cache eviction diagnostics.
   - image fit modes.
   - image clipping.
   - rounded image masking.
4. Layer-backed backend execution:
   - consume layer tree instead of only display list for rounded clip/shadow execution.
   - move rectangular clip push/pop from display-list commands into layer-tree execution.
   - replace rounded clip rectangular fallback with true rounded masking.
   - add shadow execution.
   - replace opacity and transform identity scaffolds with real layer-backed compositing.
   - map text/image layers to resource-backed drawing.
   - preserve display list diagnostics during transition.
5. Keyboard and text input:
   - richer focused input editing.
   - selection.
   - full Unicode text editing policy.
   - real OS clipboard integration.
   - IME composition text and candidate UI.
6. Object table:
   - richer object table reports in frame diagnostics.
   - object table compaction policy for destroyed tombstones.
   - target cleanup without long-term raw pointer dependency.
   - stale lookup tests for more platform/event paths.

## Implementation Assumptions

- Current code skeleton remains and is refactored in phases.
- Public value-handle API stays compatible.
- Full repaint fallback is allowed in v0 when reported.
- Missing dirty metadata is not allowed.
- Diagnostics must never be an afterthought.
- Modern UI quality comes from composition, theme, motion, and render tree quality before large widget count.
