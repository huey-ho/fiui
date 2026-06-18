# fiui v0 Release Checklist

Date: 2026-06-17

Use this checklist before treating a build as a v0 stabilization candidate.

## Required Checks

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --install build --config Debug --prefix out/fiui-debug
```

The same sequence can be run with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/verify.ps1 -Config Debug
powershell -ExecutionPolicy Bypass -File scripts/verify.ps1 -Config Release
```

Manual visible-window smoke:

```powershell
build/Debug/fiui_settings_panel.exe --run
build/Debug/fiui_control_center_demo.exe --run
```

Use `--run-ai` only when investigating diagnostics. It enables high-detail frame
and trace output in the visible window and can feel slow in Debug builds.

Validate these paths after the run:

- `fiui-test-diagnostics/stability/fiui-frame.json`
- `fiui-test-diagnostics/stability/fiui-trace.jsonl`
- `fiui-test-diagnostics/stability/fiui-leaks.json`
- `fiui-diagnostics/settings_panel/fiui-frame.json`
- `fiui-diagnostics/settings_panel/self-test.txt`
- `fiui-diagnostics/control_center_demo/fiui-frame.json`

## Release Criteria

- Debug and Release builds are green.
- CTest is green for Debug and Release.
- The installed package is consumable by `tests/package_smoke` through
  `find_package(fiui CONFIG REQUIRED)` and `fiui::fiui`.
- `fiui_stability_tests` covers layout, input, diagnostics schema, display list
  shape, hidden-window backend submit, and device lost/recover.
- `settings_panel` and `control_center_demo` remain public API-only examples.
- README and stabilization docs match the actual supported v0 behavior.
- Install output contains `bin/fiui.dll`, `lib/fiui.lib`, headers, docs, and
  CMake target exports.

## Repository Hygiene

Keep generated files out of commits unless explicitly reviewing artifacts:

- `build/`
- `out/`
- `fiui-diagnostics/`
- `fiui-test-diagnostics/`
- `*.user`, `*.suo`, `*.vcxproj.user`

Known local review-only files should either be deleted before release or moved
into a documented fixture path:

- `tmp_split.cpp`
- root-level ad hoc bitmaps such as `fiui-test-wide.bmp`

Do not revert unrelated working tree changes while preparing a stabilization
candidate.

## Current Verification Record

Last checked on 2026-06-18:

- `scripts/verify.ps1 -Config Debug -BuildDir build-verify-debug -InstallPrefix out/fiui-debug`:
  passed build, 4/4 CTest, install, and package smoke.
- `scripts/verify.ps1 -Config Release -BuildDir build-verify-release -InstallPrefix out/fiui-release`:
  passed build, 4/4 CTest, install, and package smoke.
- `tests/package_smoke`: passed as an installed-package consumer in Debug and
  Release.
- `fiui_settings_panel` default acceptance run writes
  `fiui-diagnostics/settings_panel/self-test.txt`.
- Manual visible-window smoke with `fiui_settings_panel --run` and
  `fiui_control_center_demo --run` is still an explicit release-signoff step.
