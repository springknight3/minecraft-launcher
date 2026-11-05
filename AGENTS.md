# AGENTS.md - Project Instructions for GitHub Copilot CLI

## Project
StrataLauncher: a cross-platform (Windows, macOS, Linux) custom Minecraft: Java Edition launcher.
The project is being ported from Java/JavaFX to C++20/Qt 6 (Qt Quick). Read `PHASES.md` for the plan and `CURVES.md` for motion curves.

## Legacy Java tree (parity reference)
- The Java code (`src/`, `build.gradle.kts`, ...) is LEGACY. Do not modify it. Do not delete it until the maintainer approves.
- It is the behavior specification. Port from its source and tests. Record every deviation in `docs/PARITY.md`.
- Old Java phase history is in `docs/legacy-java/PHASES-java.md`.

## Stack (C++)
- C++20, CMake 3.25+ with presets, Ninja. Warnings: `-Wall -Wextra -Wpedantic`, treated as errors in CI. `.clang-format` and a clang-tidy config (bugprone-*, modernize-*, selected cppcoreguidelines-*).
- Qt >= 6.8 as installed on this machine. Check the version first. Never download Qt.
- Modules: Core, Network, Gui, Qml, Quick, QuickControls2 (Basic style, fully custom-styled), Test, QuickTest. No QWidget UI. No GPL-only Qt modules (stay LGPL-compatible, dynamic linking).
- Non-Qt dependencies only where Qt has no equivalent: QtKeychain (credentials), libarchive (tar.gz/zip), zlib (NBT gzip). `find_package` first, `vcpkg.json` fallback. Each needs a one-line justification in `PHASES.md`.
- Namespace `strata`. PascalCase classes, camelCase methods, `m_` members.

## Layout (all under `cpp/`)
- `core/` -> static lib `strata_core`: QtCore + QtNetwork ONLY, no Gui/Quick. Mirrors Java packages: instance, mojang, launch, download, auth, loader, modrinth, settings, server, runtime, logging.
- `ui/` -> QML module (`qt_add_qml_module`) + C++ view models (QObject, Q_PROPERTY, QAbstractListModel) = `strata_ui`.
- `app/` -> executable, `main()`, bootstrap. `assets/` -> fonts, icons, logo. `tests/` -> Qt Test (core), QtQuickTest (UI).
- `docs/PARITY.md`: Java class -> C++ class -> test status -> known differences.

## Build and test commands
- Configure/build: `cmake --preset debug && cmake --build --preset debug`
- Test: `ctest --preset debug --output-on-failure` (headless: `QT_QPA_PLATFORM=offscreen`)
- Release: `cmake --preset release && cmake --build --preset release`
- Presets: `debug` (ASan + UBSan), `release`, `offscreen-test`.

## Rules: parity and data
- Preserve user-visible behavior and text, including:
  - Hobby warning: "This is a hobby project and not meant to replace anything! May contain security issues and flaws! Not affiliated with Mojang Studios, Microsoft, ..." (startup splash and About page, plus a copyright line).
  - Installing screen: white centered header "Installing <instance-name>...", centered animated progress bar, small gray centered "Downloading: <item>".
  - Typed `DELETE` confirmation for destructive actions.
- Use the same data directories (`~/.local/share/StrataLauncher`, `%APPDATA%`, `~/Library/Application Support`). Keep instance.json, settings, options.txt, and servers.dat formats compatible. Stored tokens do not migrate (users sign in again); document it.
- Translate each Java test suite into Qt Test first, then write C++ until it passes.

## Rules: memory safety and security
- RAII only. No raw owning pointers, no manual new/delete. Use Qt parent ownership, `std::unique_ptr`, `QPointer`. Bounds-checked access (`.at()`).
- The debug preset builds with AddressSanitizer + UBSan. All tests must pass under it.
- Preserve every Java security behavior:
  - Tokens only through the credential store. Never log tokens, keys, or full launch args that contain them.
  - SHA verification on every download.
  - Archive extraction rejects absolute paths, `..`, and symlink escapes, and verifies integrity.
  - Processes via `QProcess` with argument lists, never a shell string.
  - Access token substituted into game args only at launch.
  - Atomic config writes (`QSaveFile`). TLS defaults stay on. Bounded reads of network responses.

## Rules: threading and performance
- The GUI thread never blocks. Use async QNetworkAccessManager, QtConcurrent/QThreadPool, and queued signals for progress. No file I/O, large JSON parsing, or hashing on the GUI thread after startup.
- Target: steady 60 FPS on the maintainer's machine (Intel HD 4600, Mesa, Wayland). Run natively on Wayland. Log `QGuiApplication::platformName()` and the Qt Quick graphics backend at startup.
- Debug-only frame-time overlay (off by default) based on `QQuickWindow::frameSwapped`.
- Build views and flyouts once and reuse them (hidden or translated). Never construct them at transition time. Pre-load fonts and icons at startup.

## Rules: platform
- The maintainer cannot test macOS. Keep OS-specific logic (paths, osx/arm64 rule evaluation, runtime selection, `-XstartOnFirstThread`) in small injectable functions with unit tests simulating Windows/macOS/Linux and x64/arm64.
- The launcher has no JVM of its own but still downloads and manages Java runtimes (Mojang, Temurin, Zulu) for Minecraft and servers.

## UI and animation spec
Modern UI, dark/light toggle, sleek gradient style, rounded corners from one central radius token. Base the look on the official Minecraft launcher and SKLauncher; do not copy their branding or assets.
- Fonts (bundled, loaded at startup): DM Sans (UI), Outfit (headers/titles), JetBrains Mono (consoles).
- Icons: reuse the existing SVGs. `*-dark.svg` = fill #000000 (light-mode backgrounds), `*-light.svg` = fill #ffffff (dark-mode backgrounds). Check how the Java `IconAssets` handles pairs and mirror it. Icons recolor with the theme at runtime.
- Motion: one `Motion.qml` singleton holds all durations and easings, plus a `reduceMotion` property that disables non-essential animation. Curves from `CURVES.md` map to `Easing.BezierSpline` with `bezierCurve: [x0, y0, x1, y1, 1, 1]`:
  - standard `[0.25, 0.1, 0.25, 1, 1, 1]`
  - emphasis `[0.22, 1, 0.36, 1, 1, 1]`
  - express `[0.2, 0.8, 0.2, 1, 1, 1]`
- Animations:
  - Startup: logo, warning, copyright. Minimum 3 seconds, longer if initialization is still running. All refreshes begin only after the window and animation are visible.
  - Button click/hover; instance card hover.
  - Page/tab switch: crossfade + slight horizontal translation, staggered element fade + move in.
  - Progress bar movement is interpolated.
  - Status light: grey (no animation) when not running; blue after process start and before readiness; green only after an in-game readiness signal (never a timer), with a slight in/out fade blink plus a growing overlay dot; red on crash.
  - Live console: fade in + vertical slide in.
  - Flyouts (instance settings, launcher settings, accounts): slide + fade.
  - Toasts: centered at the bottom, sleek fade in; pulse + size increase for warn/error.
- All dialogs are in-window overlays, never separate OS windows. Document unavoidable native file dialogs as known gaps.

## Working rules
- Use grep to locate code before opening files. Never print whole files or whole build logs (errors and the last ~30 lines only). Do not re-read files already read this session. Use targeted edits.
- Pick the simplest approach that meets the acceptance criteria. Compare at most two options. State a one-line assumption for ambiguity and continue. If the same fix fails twice, stop and report.
- Do not refactor unrelated code or add features outside the phase.
- Run only the tests relevant to the change while working; run the full build and tests once at the end of a phase.
- Every phase compiles cleanly with no TODO stubs for core features. Deferred work goes in a "Known gaps" list.
- Before launching the GUI, state exactly what you need me to check. I will close it.
- Inside /TEMPORARY ASSETS/, all assets (icons, fonts) are there raw. Use those.
- At the end of a phase, run its verification commands, report (what changed, test results, startup log lines, known gaps), update the status in `PHASES.md`, then STOP. Do not start the next phase until confirmed.
