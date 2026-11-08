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

## M6: Java runtime management [x]
Files: `cpp/core/runtime/*`, `cpp/core/settings/InstanceSettings.*`, `cpp/core/launch/*`, `cpp/ui/LauncherController.*`, `cpp/app/MainWindow.qml`, and `cpp/tests/M6RuntimeTests.*`. Manage Mojang runtimes from version metadata, Temurin/Zulu package discovery and installation, per-instance stable runtime selection, shared-by-default versus redundant instance runtimes, installed-runtime inventory/ownership, deletion protection for in-use runtimes, and a runtime-manager UI.

Acceptance: validate runtime manifests and archive paths; stage and verify downloads before replacing an installation; select the Mojang Java component matching the Minecraft metadata/version (including legacy fallback), or install a selected external vendor; persist launcher vendor/redundancy preferences and per-instance mode/component/storage choice without changing existing instances when preferences later change; launch with the selected managed Java executable and explain missing runtime state. Tests cover metadata/version mappings, platform/runtime selection, unsafe archive manifests, checksums/atomic install behavior, stable/shared/redundant paths, persistence, inventory, and in-use deletion. Run the full debug build/tests and request GUI verification before M7.

Known provider limitation: the Azul Zulu metadata response used by the Java parity implementation does not publish an archive checksum; Zulu downloads use HTTPS host validation and a persisted executable SHA-1 integrity marker. Temurin package SHA-256 and Mojang per-file SHA-1 are verified.

## M7: Auth [x]
Files: `cpp/core/auth/*`, `cpp/ui/LauncherController.*`, `cpp/app/MainWindow.qml`, and `cpp/tests/M7AuthTests.*`. This phase adds offline and Microsoft device-code login flows, QtKeychain-backed credentials, multi-account handling, and the account flyout.

**Completed:** Added validated offline and Microsoft account persistence, native secure credential storage, device-code OAuth with Xbox/XSTS/Minecraft profile exchange and refresh, cancellable sign-in, and active-account token substitution at launch. Account metadata remains atomic and contains no access or refresh tokens. Added an account flyout for multiple offline/Microsoft identities and the required public-client ID; existing offline profiles migrate into the account list. Removing accounts requires typed `DELETE`; its prompt autofocuses and rejects paste. Debug build and focused auth/launch/UI tests pass. The Microsoft flow needs a maintainer-provided Entra public-client ID and interactive sign-in for end-to-end verification.

## M8: Loaders [x]
Files: `cpp/core/loader/*`, `cpp/core/instance/*` integration, `cpp/ui/LauncherController.*`, `cpp/app/MainWindow.qml`, and `cpp/tests/M8LoaderTests.*`. This phase installs Fabric, Quilt, Forge, and NeoForge, filters compatible versions, and preserves loader-specific JVM flags.

**Completed:** Added compatible loader-version lookup, bounded HTTPS JSON/XML parsing with DTD rejection, Fabric/Quilt profile validation and SHA-1-verified library downloads, and Forge/NeoForge installer execution using the managed runtime. Loader state is persisted atomically as pending/installed; incomplete or mismatched profiles block launch. The create-instance dialog selects a loader/version, and launch metadata merges loader main class, client JAR, libraries, game args, and JVM args. Qt tests cover version filtering, profile compatibility, path validation, state, and final launch arguments. Full Debug build and all 7 CTest suites pass. The user confirmed the GUI selection flow; live online loader installs remain unverified.

## M9: Modrinth and content tab [x]
Files: `cpp/core/modrinth/*`, `cpp/ui/content/*`, `cpp/app/MainWindow.qml`, and `cpp/tests/M9ModrinthTests.*`. This phase adds API clients, content browser tabs, install/enable/remove flows, loader/version filters, and dependency validation.

**Completed:** Added bounded Modrinth search/version lookups with Minecraft and loader facets, compatible content installation with SHA-1 validation, per-instance tracked state, enable/disable, removal, world-specific data packs, and required/incompatible dependency checks. Missing required dependencies are resolved recursively, listed for confirmation, and installed before the selected project. The Content page is wired to the asynchronous controller, and the dependency confirmation popup uses a centered modal layout. Focused Qt tests cover request facets/pagination, dependency resolution order, response parsing, unsafe downloads and world paths, legacy state, enable/remove, and required/incompatible dependencies. The user confirmed M9 complete; the Debug build, QML lint, and all 8 CTest suites pass. Live online Modrinth installs were not exercised. Dependency installation and `.disabled` file renaming extend the Java implementation and are recorded as parity differences.

## M10: Per-instance settings editors [x]
Files: `cpp/core/settings/*`, `cpp/ui/settings/*`, `cpp/tests/M10SettingsTests.*`. This phase preserves `options.txt` and `servers.dat` formats, handles per-instance overrides, and keeps Java flags aligned with the managed runtime.

**Completed:** Added lossless atomic `options.txt` updates, version-aware FOV conversion, gzip NBT read/write with unknown-tag preservation and Minecraft-version compression migration, and per-instance server list management. The asynchronous settings flyout edits common and advanced game options, servers, Java flag overrides, and runtime selection without blocking the GUI. Qt tests cover the legacy option parsing, FOV conversion, control mapping/conflicts, NBT preservation/compression, server edits, and asynchronous settings persistence. The user confirmed M10 complete. Known gap: interactive keyboard/mouse rebinding is not ported; keybind data remains editable through advanced `options.txt` entries.

## M11: Server manager [x]
Files: `cpp/core/server/*`, `cpp/ui/server/*`, `cpp/tests/server/*`. This phase adds server installation, lifecycle management, console/history, `server.properties` editing, and resource monitoring.

**Completed:** Added Vanilla, Paper, and Fabric server installation and launch plans, managed Java selection, server lifecycle/readiness, bounded live console, EULA persistence, server settings and `server.properties` editing, and process resource monitoring. Server-owned managed runtimes are included in inventory and deletion protection. Both server and Minecraft consoles follow new output; Linux RSS sampling reads `/proc` to EOF rather than relying on its reported size. Focused M11 tests cover installation/launch, lifecycle, runtime ownership, EULA persistence, console autoscroll, and memory sampling. Debug and Release builds pass, all 10 CTest suites pass, and the user confirmed the M11 GUI checks.

## M12: Remaining UI and polish [x]
Files: `cpp/ui/*`, `cpp/app/assets/*`, `cpp/tests/ui/*`. This phase covers the remaining launcher settings, About view, enhanced motion polish, accessibility, and final theme/styling pass. It should look exactly like the original Java app UI.

**Completed:** Added the launcher home and recent-launch controls, global settings and About views, themed controls, startup/page/tab motion, status indicators, and polished instance/server screens with settings flyouts, console/content tabs, and dangerous actions. Fixed the startup backdrop capture so it hides the shell only while the launcher-settings flyout is open, and sized the collapsed sidebar to fit its padded icons. The user confirmed the GUI; the Debug build, QML lint, and all 10 CTest suites pass. Known gaps: live server-side Modrinth installs and the frame-time overlay were not separately verified in the GUI; macOS GUI validation remains unavailable.

## M13: Packaging, CI, final audit [~]

**Goal:** Produce reproducible release artifacts for all three platforms, automate them in CI, verify them (macOS via CI, Windows via manual VM test by me), and complete the final parity review. After this phase the C++ launcher is the only launcher in the repo.

**Progress:** Added install/deployment rules, CPack metadata, QML deployment scanning, and headless `--version`/`--smoke-test` modes. `scripts/package.sh` stages Linux AppDir packages and macOS/Windows artifacts with SHA-256 sidecars. Linux AppImage and tarball creation succeeds locally; the script selects Qt 6 `qmake`, includes Wayland/offscreen plugins, and disables the old bundled `strip` that cannot handle RELR sections. The tarball and unpacked AppDir pass QML smoke checks. Added a pinned Qt6Keychain source-build helper and staged the Windows smoke script/checklist in the ignored `windows-disk/` folder. CI, legal notices, README, clean-distro verification, cross-platform checks, and the final parity audit remain.

**Files in scope:** root `CMakeLists.txt` packaging config (CPack / install rules), `.github/workflows/{ci,verify,release}.yml`, `scripts/package.sh`, `scripts/ci-verify.sh`, `scripts/windows-smoke.ps1`, `README.md`, `LICENSE`, `docs/PARITY.md`, `docs/WINDOWS_MANUAL_TEST.md`, `THIRD_PARTY_NOTICES` / LGPL notices, release runtime bundles.

### 1. Release artifacts
| Platform | Artifact | Notes |
|---|---|---|
| Linux | `.AppImage` | Bundle Qt/runtime deps; must run on a clean distro container |
| Linux | `.tar.gz` | Relocatable layout: `bin/`, `lib/`, `share/`, `LICENSES/` |
| macOS | `.app` (zipped or `.dmg`) | Bundle frameworks (macdeployqt or equivalent); universal or per-arch, state which |
| Windows | `.exe` installer | NSIS or Inno Setup; bundle DLLs; Start Menu shortcut + uninstaller |

Artifact names follow `<app>-<version>-<os>-<arch>.<ext>`. Version comes from a single source (CMake project version / git tag).

### 2. Manual packaging script
- `scripts/package.sh [linux|macos|windows|all]`, `set -euo pipefail`, prints usage on bad args, fails clearly on missing tools, outputs to `dist/`.
- All workflows call this same script (or the same CMake/CPack targets) so local and CI builds never diverge.
- Windows: script runs under Git Bash/MSYS2, or document the limitation.

### 3. GitHub Actions
- `ci.yml`: build + tests on pull requests.
- `verify.yml`: **manual verification workflow**, `on: workflow_dispatch` only.
  - Input `platform`: `all | linux | macos | windows` (default `linux,windows`; macOS runs only when requested, since private-repo macOS minutes bill at 10x).
  - Matrix: `ubuntu-latest` (AppImage + tar.gz), `macos-latest` (.app), `windows-latest` (.exe).
  - Each job: build via `scripts/package.sh`, run the automated smoke test (section 4), upload artifacts with `actions/upload-artifact`.
  - The Windows job's artifact is what I test on my VM, so it must upload the installer, its `SHA256` file, and `scripts/windows-smoke.ps1`.
  - Never creates tags or releases.
- `release.yml`: triggers on version tags (`v*`) plus `workflow_dispatch`. Reuses the same build steps (reusable workflow or shared composite action, not copy-paste). On tag, attaches all artifacts to a GitHub Release with `SHA256SUMS`, plus a source archive (see section 6).
- Cache dependencies in all workflows.
- Code signing/notarization: out of scope this phase. Leave secrets-gated hook steps and note unsigned status in README.
- Bootstrap: a workflow must exist on the default branch before it can be dispatched by name. Tell me when a first version of `verify.yml` needs merging to the default branch; do not push to the default branch yourself.

### 4. Automated smoke-launch checks (in CI)
- Launch the packaged binary headless (`--version` or `--smoke-test`, or `xvfb-run` on Linux); assert exit code 0 and expected version string.
- AppImage/tar.gz: run in a clean container without dev packages to catch missing libraries.
- macOS: launch the `.app` binary from inside the bundle, assert exit code and version string, and check with `otool -L` that no libraries resolve outside the bundle or system paths.
- Windows (CI runner): silent-install, run `--version`, uninstall. This is a sanity check only. It does NOT count as Windows sign-off; section 5 does.

### 5. Windows manual verification (I do this on my Windows VM)
You cannot run Windows. I test the installer by hand on a Windows VM. Your job is to make that fast and unambiguous, then stop and wait.

Deliverables from you:
- `scripts/windows-smoke.ps1`: PowerShell script I run on the VM. It silently installs the installer, verifies expected files exist (exe, DLLs, `LICENSE`, `LICENSES/`, `THIRD_PARTY_NOTICES`), checks the Start Menu shortcut, runs `--version` and asserts the version string, runs the uninstaller silently, and verifies the install directory and shortcut are removed. It prints clear PASS/FAIL per step and exits nonzero on any failure. Parameters: installer path, expected version.
- `docs/WINDOWS_MANUAL_TEST.md`: a short checklist for the parts a script can't cover:
  1. Verify the installer's SHA256 against the published value.
  2. Run the installer interactively; confirm the license page shows GPLv3, the install-directory choice works, and the Start Menu shortcut is created.
  3. Expect a SmartScreen "unknown publisher" warning (unsigned build); note it, do not treat it as a failure.
  4. Launch the app from the shortcut; confirm the main window renders, then perform the core flows (log in or offline mode, version list loads, a game launch reaches the main menu). List the exact flows to try.
  5. Check for missing-DLL errors, console flashes, and wrong icon or name.
  6. Uninstall through Settings > Apps; confirm it leaves no leftover shortcut or install directory.
  7. Record the Windows version, VM type, and anything odd.
- Fetch the artifact for me with `gh run download <id> --name <windows-artifact> --dir dist-ci/windows/`, then print the full path, the SHA256, and the exact commands/steps I should run on the VM (including how to copy the files in, e.g. shared folder or `scp`).
- Then STOP and ask me to test. Do not mark Windows verified, and do not proceed to the final done-check on your own.

My reporting:
- I will reply with PASS or FAIL per checklist item. Record the result in `docs/PARITY.md` (or a `docs/VERIFICATION.md` section) with the date, the installer's SHA256, and the VM details.
- On FAIL: fix, push, re-dispatch `verify.yml` for Windows, fetch the new artifact, and ask me to re-test. Cap at 5 failed attempts on the same error, then report what you tried.
- If the installer, bundled DLLs, or installer scripts change after my last PASS, my sign-off is stale: ask me to re-test the final artifact.
- Until I confirm, Windows status is **PENDING MANUAL VERIFICATION**, not UNVERIFIED and not PASS.

### 6. Remote verification with `gh`
You have the `gh` CLI, already authenticated. You cannot run macOS locally, so verify it in CI. `scripts/ci-verify.sh <branch> [platform]` wraps the loop:

    git push origin <branch>
    gh workflow run verify.yml --ref <branch> -f platform=<platform>
    sleep 10
    RUN_ID=$(gh run list --workflow=verify.yml --branch <branch> --limit 1 --json databaseId --jq '.[0].databaseId')
    gh run watch "$RUN_ID" --exit-status
    # on failure:
    gh run view "$RUN_ID" --log-failed
    # on success:
    gh run download "$RUN_ID" --dir dist-ci/
    gh run view "$RUN_ID" --json url,jobs --jq '{url, jobs: [.jobs[] | {name, conclusion}]}'

Rules:
- On failure: read `--log-failed`, fix, push, re-dispatch. Stop after 5 failed attempts on the same error and report what you tried.
- Run the Linux jobs locally in a clean container first; use `gh` for macOS and for building the Windows installer. Run macOS only when macOS-relevant files changed, and always for the final verification.
- Inspect downloaded artifacts locally (names, checksums, `LICENSE`, `LICENSES/` and `THIRD_PARTY_NOTICES` present).
- **Forbidden:** pushing tags, creating or editing releases, force-pushing, pushing to the default branch, changing repo secrets or settings, or running `release.yml` against a real tag. Only I do those.
- If `gh auth status` fails or the push is rejected, mark macOS UNVERIFIED and Windows UNBUILT, and stop. Do not assume they pass.

### 7. Licensing
**Project license: GPL-3.0-or-later** (SPDX: `GPL-3.0-or-later`).

Project license:
- Root `LICENSE` contains the full GPLv3 text. Every source file carries an `SPDX-License-Identifier: GPL-3.0-or-later` header (add missing ones; do not alter files that carry a different third-party license).
- Ship `LICENSE` in every artifact: tar.gz root, AppImage (`usr/share/doc/<app>/`), macOS bundle `Resources/`, and the Windows installer (installed directory, and shown on the installer's license page).
- Before packaging, confirm GPLv3 is compatible with the original Java launcher's license and with every copied or ported file and asset. If the original is under an incompatible license, STOP and report it; do not resolve it silently.

Bundled third-party code:
- Ship license texts and notices for all bundled libraries (Qt, etc.) in `LICENSES/` and `THIRD_PARTY_NOTICES`; verify both are present in every package.
- Verify every bundled dependency is GPLv3-compatible (Qt LGPLv3 and GPLv3 modules are; Apache-2.0 is; GPLv2-only and proprietary code are not). List each dependency with its license identifier in `THIRD_PARTY_NOTICES`.
- Link Qt dynamically and keep the libraries replaceable. Document how users can relink or swap them in the README.

Corresponding source (GPLv3 §6):
- Every release must make the exact corresponding source available: `release.yml` attaches a source archive (`git archive` of the tag) to the GitHub Release. The About dialog or README states where the source is.
- Include build instructions sufficient to rebuild the shipped binaries.

Flag any unresolved license question in `docs/PARITY.md` or a tracked follow-up.

### 8. Final parity audit
- Go through `docs/PARITY.md` line by line against the original Java behavior. Mark each item Done / Intentionally different (with rationale) / Missing.
- Any Missing item blocks completion or must be filed as a tracked follow-up.

### 9. Java tree retirement (do this LAST, after the parity audit passes)
- Move the Java tree to `../minecraft-launcher-java-backup/` (outside the repo). Use `git mv` or copy-then-verify-then-remove; never `rm -rf` before confirming the backup is complete (diff file counts/hashes).
- Remove Java references from build files, CI, and README.
- Re-run `verify.yml` with `platform=all` afterward to confirm nothing depended on the Java tree.
- Then fetch the final Windows installer and ask me to re-run the manual Windows test (section 5) against it.

### 10. README
Build instructions, install instructions per platform, packaging script usage, how to run `verify.yml`, license section (GPL-3.0-or-later, where to get the source, how to relink Qt), and known limitations (e.g. unsigned binaries, SmartScreen warning on Windows).

### Definition of done
- [ ] Linux artifacts build locally via `scripts/package.sh`; all four artifacts build in CI
- [ ] `verify.yml` run with `platform=all` is green; run URL and per-job conclusions reported
- [ ] Automated smoke tests pass on Linux, macOS, and the Windows CI runner
- [ ] **Windows manual test on my VM: I have reported PASS against the final installer** (agent cannot check this box itself)
- [ ] `LICENSE` (GPLv3), `LICENSES/`, and `THIRD_PARTY_NOTICES` present in every artifact
- [ ] SPDX headers present; dependency license compatibility confirmed and listed
- [ ] Source archive attached by `release.yml`
- [ ] `docs/PARITY.md` fully resolved
- [ ] Java tree moved to backup, with no dangling references
- [ ] README updated

Do not mark this phase complete until every box is checked. Report any item you could not verify as UNVERIFIED (macOS) or PENDING MANUAL VERIFICATION (Windows), with the reason.
