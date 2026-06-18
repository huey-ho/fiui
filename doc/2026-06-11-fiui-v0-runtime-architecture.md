# fiui v0 Runtime Architecture

Date: 2026-06-11

This is the primary architecture document for future `fiui` work. Future AI agents and engineers should treat this document as the source of truth for module responsibilities, module boundaries, and implementation direction.

This document intentionally describes only `fiui` concepts. Do not infer that `fiui` should copy or recreate any external UI framework.

## Goals and Non-goals

### Goals

`fiui v0` is a lightweight Windows C++ UI runtime for AI-generated desktop tools. It must provide a small declarative C++ API, modern visual defaults, self-rendered D3D11 output, structured diagnostics, and a runtime architecture that can support high-performance partial repaint.

Required goals:

- Windows 10/11 x64.
- MSVC and CMake.
- Public C++ value-handle API.
- Native DLL distribution through `fiui.dll`, `fiui.lib`, and `include/fiui/*.h`.
- Self-rendered UI through D3D11, DXGI, DirectWrite, and WIC.
- Dirty node and dirty rectangle tracking from day 1.
- Structured AI-readable diagnostics from day 1.
- Theme tokens, component styles, state styles, and composition-based UI structure.
- Clear future path for D3D textures, shared handles, image/media surfaces, and resource diagnostics.

### Non-goals

`fiui v0` must stay narrow. It is not a full application platform or a full designer ecosystem.

Explicit non-goals:

- Cross-platform UI.
- Browser or web runtime embedding.
- Selector/cascade style engine.
- Full script-based UI language.
- Large first-release widget ecosystem.
- Full rich text editor.
- Virtualized list as a v0 requirement.
- Visual designer.
- Full media pipeline.
- Full zero-copy media implementation.
- Public ownership based on `std::shared_ptr`.
- Public API stability based on STL-heavy types crossing the DLL boundary.

## Core Architecture

`fiui` is built around a runtime center. Public value handles describe UI intent, while internal runtime systems own identity, layout, dirty tracking, rendering, resource management, event dispatch, diagnostics, and platform integration.

```text
App / AI-generated C++ Code
    |
    v
Public Declaration API
    |
    v
FiuiRuntime
    |
    v
NodeTree
    |
    v
LayoutSystem
    |
    v
RenderTree
    |
    v
DisplayList
    |
    v
DirtyRectPlanner
    |
    v
D3D11Backend
```

Cross-cutting systems:

```text
FiuiRuntime
    |
    +-- ObjectSystem
    +-- EventSystem
    +-- StyleSystem
    +-- ResourceSystem
    +-- TextSystem
    +-- ImageSystem
    +-- DiagnosticsHub
    +-- PlatformSystem
    +-- FrameScheduler
    +-- D3D11Backend
```

The runtime must keep these systems separate. A module may consume outputs from another module, but it must not take over that module's responsibility.

## Current Implementation Snapshot

As of the current v0 scaffold, the repository already contains these internal modules:

- `FiuiRuntime` with default runtime state, object id allocation, frame id allocation, dirty thresholds, runtime ObjectTable scaffold, and subsystem ownership.
- `ObjectHeader`, `NodeData`, `WidgetProperties`, and `DirtyState` inside `WidgetImpl`.
- `ObjectSystem` scaffold with object register/unregister, lookup by object id and generation, destroyed tombstones, stale generation diagnostics, and EventSystem target validation.
- `EventSystem` with click dispatch, hit-testing, pointer routing, focus target, capture target, hover target, event route diagnostics, and input-driven dirty invalidation.
- `LayoutSystem` with basic constraints-based arrangement and bounds, paint bounds, and clip bounds output.
- `DirtyTracker` with dirty reason classification, parent clipping, rectangle merging, full repaint fallback policy, and frame summary data.
- `StyleSystem` with active theme, theme tokens, typography, spacing, radius, motion, density, component themes, and button state styles.
- `ResourceSystem` with resource ids, cache state, image resources, image metadata storage, text layout cache scaffold, text metrics storage, resource diagnostics, and cache-hit diagnostics.
- `TextSystem` with DirectWrite factory creation, text measurement, fallback measurement, text diagnostics, and text system state.
- `ImageSystem` with WIC factory creation, image metadata queries, fallback image metadata, image diagnostics, and image system state.
- `RenderSystem` with render tree, layer tree, display commands, display style payloads, display resource payloads, backend batch summary, and frame JSON output.
- `D3D11Backend` with real D3D11 device/context creation, optional native window binding, DXGI swap chain scaffold, render target view scaffold, rect display command execution, rounded surface drawing, conservative shadow fallback drawing, opacity/transform identity command scaffolds, rectangular clip execution, rounded clip rectangular fallback, text drawing, image texture upload/draw, swap chain present, frame submit counters, device lost simulation, explicit recovery, backend failure reason, and backend diagnostics.
- `DiagnosticsHub` with session id, frame id, object id, generation, path, category, action, detail, frame JSON, trace JSONL, and leak JSON.
- `PlatformSystem` with HWND creation, DPI/resize/input/timer/device-lost scaffold, pointer input bridge, keyboard route bridge, basic single-line `Input` editing, internal plain-text clipboard scaffold, IME diagnostics scaffold, and platform diagnostics.
- `FrameScheduler` with frame request, coalescing, pending state, completion state, and scheduler diagnostics.

The current implementation is still a scaffold in several places. It can build a semantic layer tree, measure text with DirectWrite, query image metadata with WIC, store text and image metadata, clear a render target, draw rect and rounded surface display commands, draw conservative shadow fallback commands, execute opacity/transform identity command scaffolds, apply rectangular clip commands, apply rounded clip commands through rectangular fallback, draw basic text and image commands, upload backend-owned image textures, route pointer and keyboard input, edit single-line `Input` text for basic key/text events, paste/copy through an internal plain-text clipboard scaffold, record target-aware IME diagnostics, and present a bound swap chain. It does not yet provide full layer-tree execution, true shadow blur/cache, true rounded masking, real opacity/transform compositing, selection, rich text editing, IME composition UI, real OS clipboard integration, or accessibility behavior.

## Module Functions and Boundaries

### FiuiRuntime

Function:

- Acts as the single runtime center for one UI runtime session.
- Holds the root node, object table, subsystem instances, frame clock, dirty thresholds, diagnostics session, and platform connection.
- Coordinates frame processing: events, state changes, layout, dirty planning, render tree update, display list generation, backend submission, and diagnostics.
- Provides a default runtime for simple applications and a configurable runtime for advanced applications.

Boundary:

- Does not implement concrete control drawing.
- Does not own business state from the application.
- Does not expose internal subsystem pointers as required public API.
- Does not bypass subsystem boundaries for convenience.

### ObjectSystem

Function:

- Owns stable object identity through `object_id` and `generation`.
- Tracks intrusive reference counts for internal implementations.
- Tracks lifecycle states such as created, attached, detached, destroying, and destroyed.
- Tracks parent/child ownership relationships.
- Generates stable debug paths from explicit `debug_id` values or fallback ids.
- Registers objects in the runtime ObjectTable at creation.
- Marks destroyed tombstones at final release so stale lookups can be diagnosed.
- Provides internal lookup by object id and generation for event/platform target validation.
- Detects duplicate attach, stale handle use, invalid parent/child operations, and suspicious lifetime patterns.

Boundary:

- Does not perform layout.
- Does not draw.
- Does not resolve styles.
- Does not route input.
- Does not load resources.
- Does not expose a public object registry API in v0.
- Does not provide cross-thread object access in v0.

### Public Declaration API

Function:

- Provides the user-facing C++ API for AI and developers.
- Exposes small value handles such as `Window`, `Button`, `Column`, `Row`, `Text`, and `Input`.
- Allows controls to be created, named, configured, and then attached to a parent.
- Keeps public examples simple and explicit.

Boundary:

- Does not expose `WidgetImpl`.
- Does not expose internal containers.
- Does not expose renderer resources or GPU handles as normal widget ownership.
- Does not expose `std::shared_ptr` as public ownership.
- Does not perform actual layout, rendering, event routing, or resource loading.

### NodeTree

Function:

- Represents stable UI identity after public declaration handles create internal nodes.
- Stores node state, widget properties, subscriptions, parent/child links, dirty source metadata, and resolved debug path.
- Bridges public value handles and internal systems.
- Provides traversal data for layout, event hit-testing, render tree generation, diagnostics, and lifecycle checks.

Boundary:

- Does not generate GPU commands.
- Does not directly access Win32 messages.
- Does not load images, fonts, or textures.
- Does not compute final draw batches.

### EventSystem

Function:

- Owns event queue and event dispatch.
- Converts platform input into structured internal events.
- Tracks hit-test result, focus target, capture target, hover target, and active event path.
- Supports event filters and subscriptions.
- Ensures event-caused state changes are recorded through NodeTree and DirtyTracker.
- Wraps callback failures and reports them through DiagnosticsHub.

Boundary:

- Does not implement business logic inside controls.
- Does not directly mutate layout results.
- Does not draw.
- Does not load resources.
- Does not skip diagnostics when callbacks fail.

### LayoutSystem

Function:

- Computes layout using a constraints-based model.
- Parent nodes pass constraints down.
- Child nodes return measured sizes.
- Parents assign final positions.
- Outputs layout tree data, including bounds, clip bounds, paint bounds, scroll extents, and hit-test bounds.
- Maintains layout cache and layout invalidation boundaries.

Boundary:

- Does not draw.
- Does not submit GPU commands.
- Does not load resources, except for consuming metrics made available by ResourceSystem.
- Does not dispatch events.
- Does not own widget lifecycle.

### StyleSystem

Function:

- Owns theme data and visual decisions.
- Provides color scheme, typography, spacing, radius, shape, shadow/elevation, motion, density, component themes, and state styles.
- Resolves control state styles such as normal, hover, pressed, focused, disabled, selected, and error.
- Supports global themes and future local overrides.
- Encourages AI-generated code to express structure and intent instead of raw colors and pixel values.

Boundary:

- Does not implement selector/cascade style matching.
- Does not generate display list commands.
- Does not own widget lifecycle.
- Does not decide event routing.
- Does not load renderer textures directly.

### ResourceSystem

Function:

- Owns resource identity, cache state, and lifetime diagnostics.
- Manages fonts, images, text layout cache, decoded image data, D3D textures, and future external texture entries.
- Stores text layout metrics produced by TextSystem.
- Stores image metadata produced by ImageSystem.
- Provides resource metrics to LayoutSystem.
- Provides renderer-ready resources to RenderSystem.
- Tracks resource creation, cache hits, cache eviction, external ownership, and release.

Boundary:

- Does not decide widget layout.
- Does not measure text; TextSystem owns text measurement.
- Does not decode image metadata; ImageSystem owns image metadata queries.
- Does not dispatch input.
- Does not own user-visible widget state.
- Does not make external texture ownership a required v0 public API.
- Does not hide resource leaks from DiagnosticsHub.

### TextSystem

Function:

- Owns text measurement services.
- Creates and tracks the DirectWrite factory.
- Measures text using text content, font size, layout bounds, and DPI.
- Falls back to deterministic estimated metrics if DirectWrite measurement fails.
- Emits diagnostics for factory creation, measurement, and fallback measurement.
- Returns metrics for ResourceSystem to cache and RenderSystem to attach to text layers and display commands.

Boundary:

- Does not own widget lifecycle.
- Does not decide layout constraints.
- Does not own resource ids or cache entries.
- Does not draw text glyphs yet.
- Does not route input or edit text state.
- Does not expose DirectWrite interfaces through public APIs.

### ImageSystem

Function:

- Owns image metadata query services.
- Creates and tracks the WIC imaging factory.
- Reads image width, height, DPI, frame count, and pixel format when a resource path resolves to a decodable image.
- Falls back to deterministic metadata from widget bounds or defaults when the image is missing or undecodable.
- Emits diagnostics for factory creation, metadata queries, and fallback metadata.
- Returns metadata for ResourceSystem to cache and RenderSystem to attach to image layers and display commands.

Boundary:

- Does not own widget lifecycle.
- Does not own resource ids or cache entries.
- Does not upload textures yet.
- Does not draw images yet.
- Does not choose layout constraints.
- Does not expose WIC interfaces through public APIs.

### RenderSystem

Function:

- Converts NodeTree, LayoutSystem output, StyleSystem output, and ResourceSystem output into render tree data.
- Builds layer tree data for clips, opacity, transforms, text, images, rounded rectangles, shadows, and scroll regions.
- Links layers to display command ranges for diagnostics and later backend consumption.
- Builds a serializable display list.
- Builds backend batch lists suitable for D3D11.
- Records rendering timing and rendering fallback decisions.

Boundary:

- Does not route platform input.
- Does not own business state.
- Does not bypass DirtyTracker to repaint.
- Does not create widget identity.
- Does not decide theme tokens.

### D3D11Backend

Function:

- Owns backend device state for the retained display-list pipeline.
- Creates and owns the D3D11 device and immediate context.
- Binds a native window when available and creates DXGI swap chain and render target view scaffolds.
- Records device creation, swap chain creation, render target creation, resize, recovery, and failure attempts.
- Consumes display lists and backend batch lists produced by RenderSystem.
- Executes the minimal draw path: clear active render target, draw rect and rounded surface commands, draw conservative shadow fallback commands, execute opacity/transform identity command scaffolds, draw text and image commands, apply rectangular clip commands, apply rounded clip commands through rectangular fallback, record unsupported draw commands, and present the swap chain when a native window is bound.
- Reports submit success, consumed command count, rect draw count, rounded surface draw count, shadow draw/fallback count, opacity/transform command/apply counts, rounded clip command/apply/fallback counts, text draw count, image draw count, clip counters, unsupported command count, unsupported draw command count, present state, device availability, context availability, swap chain availability, render target availability, device lost state, frame submit count, and failure reason.
- Emits backend diagnostics for initialize, swap chain creation, render target creation, pipeline creation, recovery, submit begin, submit end, submit failure, headless submit, draw rects, draw rounded rect, draw shadow, shadow fallback, opacity push/pop, transform push/pop, rounded clip command/apply/fallback/pop, draw text, draw image, clip push/pop/apply, unsupported draw command, present, unsupported command, and device lost.
- Provides a scaffold for later layer-backed command execution, richer resource uploads, true shadow blur/cache, true rounded masking, real opacity/transform compositing, and device recovery policy.

Boundary:

- Does not build display commands.
- Does not decide layout, style, dirty propagation, or widget lifecycle.
- Does not own business state.
- Does not route platform input.
- Does not silently recover from device lost without explicit recovery policy.
- Does not expose GPU resource ownership through public v0 APIs.
- Does not provide true shadow blur/cache, true rounded masking, real opacity/transform compositing, advanced text shaping policy, image fit policy, or full layer-tree execution yet.

### DirtyTracker

Function:

- Tracks dirty nodes and dirty reasons.
- Separates layout dirty, paint dirty, resource dirty, input dirty, and theme dirty.
- Expands dirty paint bounds for shadows, antialiasing, rounded corners, clips, transforms, and text.
- Clips dirty rectangles through parent clip bounds.
- Merges dirty rectangles.
- Decides when a frame should fall back to full repaint.
- Reports dirty source, original rectangles, merged rectangles, fallback reason, thresholds, and repaint cost.

Boundary:

- Does not perform actual drawing.
- Does not own layout measurement.
- Does not load resources.
- Does not suppress diagnostics when full repaint is chosen.

### DiagnosticsHub

Function:

- Owns structured diagnostics output.
- Emits trace, frame, crash, leak, and object table reports.
- Uses stable schemas with session id, frame id, object id, generation, path, category, action, detail, and timing.
- Records API mutations, object lifecycle, attach/detach, event dispatch, layout invalidation, paint invalidation, resource creation/release, render fallback, and platform failures.
- Keeps diagnostics useful for AI analysis.

Boundary:

- Does not control business flow.
- Does not make normal UI operation fail just because diagnostics writing fails.
- Does not expose private implementation pointers.
- Does not store unbounded logs without retention policy.

### PlatformSystem

Function:

- Encapsulates Windows platform integration.
- Owns HWND creation/destruction, DPI, monitor changes, resize, mouse, keyboard, touch, wheel, IME, clipboard, timers, task queue, window visibility, and device lost signals.
- Converts platform messages into internal structured events.
- Routes keyboard events to the focused widget.
- Supports basic single-line `Input` editing for text insert, Backspace, Delete, Left, and Right.
- Provides an internal plain-text clipboard scaffold for focused `Input` copy/paste diagnostics.
- Records IME start/composition/end diagnostics with the current focus target.
- Provides platform diagnostics for failures.

Boundary:

- Does not leak raw Win32 message dispatch into normal application code.
- Does not generate render tree data.
- Does not decide component styles.
- Does not own widget hierarchy.
- Does not implement selection, undo/redo, multiline editing, full Unicode editing policy, IME composition UI, or real OS clipboard integration in the current scaffold.

### FrameScheduler

Function:

- Owns frame request state.
- Coalesces repeated requests before a frame is rendered.
- Records why a frame was requested, which subsystem requested it, and which object/path was involved.
- Tracks pending state, request count, coalesced count, completed count, last request frame id, and last completed frame id.
- Provides scheduler data for frame diagnostics.

Boundary:

- Does not perform layout.
- Does not render.
- Does not decide dirty rectangle policy.
- Does not own the platform message loop.
- Does not run timers or threads in the current scaffold.

## Data Flow

### UI Creation

```text
Public Declaration API
    |
    v
ObjectSystem creates object identity
    |
    v
NodeTree stores node data and properties
    |
    v
DirtyTracker marks layout and paint dirty
    |
    v
DiagnosticsHub records API mutation
```

### Event Processing

```text
PlatformSystem
    |
    v
EventSystem
    |
    v
NodeTree target path
    |
    v
Application callback / subscription
    |
    v
DirtyTracker
    |
    v
FrameScheduler
    |
    v
DiagnosticsHub
```

### Frame Processing

```text
FrameScheduler pending request
    |
    v
Dirty nodes
    |
    v
LayoutSystem
    |
    v
RenderSystem
    |
    v
DirtyRectPlanner
    |
    v
D3D11Backend
    |
    v
DiagnosticsHub frame report
    |
    v
FrameScheduler complete frame
```

## Public API Policy

The public API should stay simple, explicit, and AI-friendly.

Required policy:

- Use direct control names.
- Prefer block-based construction in official examples.
- Avoid operator-heavy DSLs.
- Avoid deep expression nesting.
- Allow explicit `debug_id()` on every public UI object.
- Generate stable fallback paths when no debug id is provided.
- Use public value handles with internal intrusive reference-counted implementations.
- Keep advanced runtime controls optional.

Forbidden public API direction:

- Public ownership through `std::shared_ptr`.
- Public exposure of internal implementation types.
- Public exposure of normal renderer resources as widget ownership.
- Selector/cascade style engine.
- Full script-based UI language as v0 requirement.

## v0 Acceptance Criteria

The v0 runtime architecture is acceptable when:

- Existing examples still build and run.
- Public value-handle behavior remains stable.
- Duplicate attach is rejected and diagnosed.
- Dirty metadata exists for every node.
- Frame reports include dirty source, fallback reason, timing, and tree snapshots.
- Theme data can switch modern light/dark styles without raw app colors.
- Diagnostics files have stable schemas.
- The architecture has clear module boundaries for future rendering and input expansion.
- `settings_panel` runs as a CTest example and exercises layout, callbacks, lifecycle diagnostics, theme switching, resources, dirty tracking, frame scheduling, display list output, and diagnostics output.
- `control_center_demo` runs as a CTest example and exercises a broader public-API-only UI with navigation-like composition, multiple sections, scroll, inputs, image replacement, progress, callbacks, theme switching, lifecycle diagnostics, frame reports, and diagnostics output.
- Platform pointer input can route through EventSystem, mutate target state, mark dirty, request a frame, and affect the next display command state style.

## Immediate Remaining Work

The next implementation work should focus on these areas:

- Text rendering: glyph drawing, font collection policy, and text layer execution through the backend.
- Image rendering: texture upload, image layer execution, cache eviction policy, and image sizing behavior.
- Keyboard and text input: key route, focus editing scaffold, IME composition diagnostics, and clipboard integration.
- Object table: lookup by object id and generation, stale generation detection, safer target cleanup, and object table diagnostics.
