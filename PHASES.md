# StrataLauncher: C++/Qt Migration Phases

Status legend: [ ] todo, [~] in progress, [x] done. One phase per session. Stop after each phase and report.
Java code is the parity reference. See `AGENTS.md` for all rules.
Authentication remains offline-only through M6; Microsoft sign-in and credential storage begin in M7.

## Dependency justifications
- QtKeychain: native secret storage for Microsoft and launcher tokens without exposing them in logs or config files.
- libarchive: handles ZIP/TAR extraction for runtime and mod archives with path traversal and symlink checks.
- zlib: required for NBT gzip compatibility and compressed binary blobs in the launcher data model.
- Qt Quick Controls 2: the Qt-native replacement for JavaFX styling and component behavior in the launcher shell and flyouts.

## M0: Inventory and plan [x]
Goal: map the Java code to the C++ plan, with no code changes.
- With grep/wc, inventory Java packages and classes: LOC, responsibility, third-party dependencies and their Qt/C++ replacements, and which Java tests cover what.
- Create `docs/PARITY.md` (every Java class mapped to a planned C++ class, status "todo").
- Move the old Java phase file to `docs/legacy-java/PHASES-java.md` if it still exists at the repo root.
- Fill in "Dependency justifications" above and add per-phase file lists to M2 to M13 (each under ~100 words).
- Acceptance: PARITY.md covers every main-source Java class; no source files changed. Verify: `git status` shows only docs changes.

## M1: Skeleton [x]
Files: `cpp/CMakeLists.txt`, `CMakePresets.json` (debug+ASan/UBSan, release, offscreen-test), `vcpkg.json`, `.clang-format`, clang-tidy config, `core/ui/app/tests` scaffolding, assets wiring.
Acceptance:
- `cmake --preset debug && cmake --build --preset debug && ctest --preset debug` passes with no warnings.
- The app opens a rounded, themed main window with a navigation shell (Home, Instances, Servers, Settings placeholders), a dark/light toggle persisted across restarts, and the hobby-warning banner.
- Fonts (DM Sans, Outfit, JetBrains Mono) and a theme-aware icon component load; verify by logging the loaded family names.
- `Motion.qml` singleton with the three curves and `reduceMotion`; a Reduce-motion toggle.
- Startup log lines: Qt version, platform plugin, graphics backend, effective theme.
- Debug frame-time overlay (off by default, toggled by a flag or hidden key).
- Headless tests pass: theme settings round-trip, Motion curve constants, one QML smoke test loading the main window offscreen.
GUI check: state what you need me to verify before launching.

## M2: Instances screen prototype (mock data) + DECISION GATE [x]
Files: `cpp/ui/instances/*`, `cpp/ui/navigation/*`, `cpp/ui/flyouts/*`, `cpp/tests/benchmarks/*`, plus the instance mock data and `--bench` runner. This phase validates list selection, launch buttons, flyouts, startup animation, and status-light behavior before the expensive core feature work begins.

## M3: Core foundations [x]
Files: `cpp/core/settings/*`, `cpp/core/instance/*`, `cpp/core/logging/*`, `cpp/tests/core/*`. This phase adds path helpers, secure logging, atomic config writes, instance creation/deletion, and compatibility fixtures for existing instance data layouts.

## M4: Mojang manifest, download manager, installing screen [x]
Files: `cpp/core/mojang/*`, `cpp/core/download/*`, `cpp/core/instance/*` integration, `cpp/ui/install/*`, app/QML integration, and focused Qt tests. Fetch and validate Mojang's version manifest, cache it atomically for offline startup, and retain cached versions when refresh fails. Implement bounded concurrent HTTP(S) downloads with retries, cancellation on failure, SHA-1 verification, and atomic destination replacement. Creating an instance must perform real version/client downloads and report progress in the centered installing view; failures remain visible and do not leave partial files. No Microsoft auth or credentials in M4.

Acceptance: headless tests cover manifest parsing/cache fallback, concurrency/retry/checksum failure, atomic cleanup, instance install progress/error, and offline-only startup behavior. Run the full debug build and tests once at phase end. GUI verification is required before M5.

**Completed:** Added validated Mojang manifest parsing with atomic cache and offline fallback, concurrent size-bounded HTTP(S) downloads with retries, cancellation, SHA-1 checks, and atomic replacement, plus asynchronous instance creation/install progress UI. The selected version's metadata and client JAR are downloaded; authentication remains offline-only. Qt 6.11.1 debug build and all 3 test suites pass. The user visually confirmed that the version selector loads and includes `1.41.2 Pre-Release 4`.

## M5: Game launch [x]
Files: `cpp/core/auth/OfflineAccountStore.*`, `cpp/core/launch/*`, `cpp/core/settings/*`, `cpp/ui/LauncherController.*`, `cpp/app/MainWindow.qml`, and `cpp/tests/M5LaunchTests.*`. This phase implements offline identity, version rules, launch args, readiness/crash tracking, asynchronous process logging, and the live console panel. The installer also downloads the libraries and assets required by the launch plan; runtime selection/management remains M6.

Acceptance: implement offline account identity only (no token or Microsoft flow), validate Mojang argument rules and substitutions, launch with `QProcess` argument lists, stream stdout/stderr without blocking the GUI, and track starting/ready/stopped/crashed state from process events and log readiness markers. Tests cover rule evaluation, argument boundaries, launch status/readiness/crash, and offline account behavior. Run full debug build/tests and request GUI verification before M6.

**Completed:** Added offline player profile persistence with Java-compatible UUIDs, Mojang launch-rule and argument handling, bounded safe native extraction, process lifecycle/readiness/crash tracking, asynchronous console/log streaming, and launch controls. Extended installation to fetch libraries/assets needed by vanilla launches. Full Qt 6.11.1 debug build and all 4 test suites pass; user visually confirmed successful Minecraft launch and M5 behavior.

## M6: Java runtime management [~]
Files: `cpp/core/runtime/*`, `cpp/core/settings/InstanceSettings.*`, `cpp/core/launch/*`, `cpp/ui/LauncherController.*`, `cpp/app/MainWindow.qml`, and `cpp/tests/M6RuntimeTests.*`. Manage Mojang runtimes from version metadata, Temurin/Zulu package discovery and installation, per-instance stable runtime selection, shared-by-default versus redundant instance runtimes, installed-runtime inventory/ownership, deletion protection for in-use runtimes, and a runtime-manager UI.

Acceptance: validate runtime manifests and archive paths; stage and verify downloads before replacing an installation; select the Mojang Java component matching the Minecraft metadata/version (including legacy fallback), or install a selected external vendor; persist launcher vendor/redundancy preferences and per-instance mode/component/storage choice without changing existing instances when preferences later change; launch with the selected managed Java executable and explain missing runtime state. Tests cover metadata/version mappings, platform/runtime selection, unsafe archive manifests, checksums/atomic install behavior, stable/shared/redundant paths, persistence, inventory, and in-use deletion. Run the full debug build/tests and request GUI verification before M7.

Known provider limitation: the Azul Zulu metadata response used by the Java parity implementation does not publish an archive checksum; Zulu downloads use HTTPS host validation and a persisted executable SHA-1 integrity marker. Temurin package SHA-256 and Mojang per-file SHA-1 are verified.

## M7: Auth [ ]
Files: `cpp/core/auth/*`, `cpp/ui/auth/*`, `cpp/tests/auth/*`. This phase adds offline and Microsoft device-code login flows, QtKeychain-backed credentials, multi-account handling, and the account flyout.

## M8: Loaders [ ]
Files: `cpp/core/loader/*`, `cpp/core/instance/*`, `cpp/tests/loader/*`. This phase installs Fabric, Quilt, Forge, and NeoForge, filters compatible versions, and preserves loader-specific JVM flags.

## M9: Modrinth and content tab [ ]
Files: `cpp/core/modrinth/*`, `cpp/ui/content/*`, `cpp/tests/modrinth/*`. This phase adds API clients, content browser tabs, install/enable/remove flows, loader/version filters, and dependency validation.

## M10: Per-instance settings editors [ ]
Files: `cpp/core/settings/*`, `cpp/ui/settings/*`, `cpp/tests/settings/*`. This phase preserves `options.txt` and `servers.dat` formats, handles per-instance overrides, and keeps Java flags aligned with the managed runtime.

## M11: Server manager [ ]
Files: `cpp/core/server/*`, `cpp/ui/server/*`, `cpp/tests/server/*`. This phase adds server installation, lifecycle management, console/history, `server.properties` editing, and resource monitoring.

## M12: Remaining UI and polish [ ]
Files: `cpp/ui/*`, `cpp/app/assets/*`, `cpp/tests/ui/*`. This phase covers the remaining launcher settings, About view, enhanced motion polish, accessibility, and final theme/styling pass. It should look exactly like the original Java app UI.

## M13: Packaging, CI, final audit [ ]
Files: root CMake packaging config, GitHub Actions, README, final `docs/PARITY.md` audit, and release runtime bundles. This phase validates packaging, smoke-launch checks, LGPL notices, and the final parity review. Do not remove the Java tree, move it into ../minecraft-launcher-java-backup/.
