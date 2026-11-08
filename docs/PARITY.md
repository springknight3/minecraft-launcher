# StrataLauncher parity map: Java -> C++/Qt

## M0 inventory

- Java main-source classes: 63
- Java test classes: 24
- Main Java source LOC: 13,932
- Main Java test LOC: not counted in the parity map; map is by lifecycle and responsibility

### Package LOC summary

| Package | LOC |
|---|---:|
| `net.stratalauncher` | 224 |
| `net.stratalauncher.core.auth` | 948 |
| `net.stratalauncher.core.download` | 263 |
| `net.stratalauncher.core.instance` | 258 |
| `net.stratalauncher.core.launch` | 696 |
| `net.stratalauncher.core.loader` | 916 |
| `net.stratalauncher.core.logging` | 26 |
| `net.stratalauncher.core.modrinth` | 690 |
| `net.stratalauncher.core/mojang` | 102 |
| `net.stratalauncher.core.runtime` | 1319 |
| `net.stratalauncher.core.server` | 1242 |
| `net.stratalauncher.core.settings` | 865 |
| `net.stratalauncher.ui` | 2025 |
| `net.stratalauncher.ui.animations` | 148 |
| `net.stratalauncher.ui.auth` | 229 |
| `net.stratalauncher.ui.components` | 67 |
| `net.stratalauncher.ui.content` | 566 |
| `net.stratalauncher.ui.install` | 44 |
| `net.stratalauncher.ui.instances` | 867 |
| `net.stratalauncher.ui.server` | 1408 |
| `net.stratalauncher.ui.settings` | 1029 |

### Third-party dependencies and Qt/C++ replacements

- `atlantafx-base` / `atlantafx-styles` -> Qt Quick Controls 2 + custom QML styling; no JavaFX dependency remains.
- `gson` -> `QJsonDocument` / `QJsonObject` / `QJsonArray` in Qt Core with strict validation.
- `java-keyring` / platform credential stores -> QtKeychain for token storage and secret retrieval.
- `commons-compress` -> libarchive for ZIP/TAR extraction with integrity and path validation.
- `zlib` / gzip handling -> Qt's `QByteArray` gzip helpers or explicit zlib integration for NBT content.
- JUnit 5 -> Qt Test / Qt Quick Test in `cpp/tests` for headless coverage.

## Java class -> planned C++ class map

Status is `todo`, `in progress`, or `done`; mark a class done only after its phase verification.

| Java class | Planned C++ class | Status | Responsibility |
|---|---|---|---|
| `net.stratalauncher.App` | `cpp/app/AppBootstrap` | todo | app lifecycle, startup, and window bootstrap |
| `net.stratalauncher.LauncherBootstrap` | `cpp/app/LauncherBootstrap` | todo | Java entrypoint replacement and CLI bootstrap |
| `net.stratalauncher.core.auth.Account` | `cpp/core/auth/Account` | done | persisted Microsoft/offline account records and launch profiles |
| `net.stratalauncher.core.auth.AccountManager` | `cpp/core/auth/AccountManager` | done | atomic account metadata, active selection, and token refresh |
| `net.stratalauncher.core.auth.CredentialStore` | `cpp/core/auth/CredentialStore` | done | credential abstraction and QtKeychain-backed secret storage |
| `net.stratalauncher.core.auth.LauncherSettings` | `cpp/core/settings/LauncherSettings` | done | Java-compatible global preferences with atomic persistence |
| `net.stratalauncher.core.auth.MicrosoftAuthService` | `cpp/core/auth/MicrosoftAuth` | done | device-code OAuth, Xbox/XSTS exchange, Minecraft profile and refresh |
| `net.stratalauncher.core.auth.NativeCredentialStore` | `cpp/core/auth/NativeCredentialStore` | done | QtKeychain adapter; tokens are never written to account metadata |
| `net.stratalauncher.core.auth.VSyncConfiguration` | `cpp/core/settings/LauncherSettings`, `cpp/app/main.cpp` | done | VSync preference persistence and startup application |
| `net.stratalauncher.core.download.DownloadManager` | `cpp/core/download/DownloadManager` | done | concurrent file downloads, retry, checksum, and progress |
| `net.stratalauncher.core.download.DownloadTask` | `cpp/core/download/DownloadTask` | done | per-file download source, destination, SHA, and state |
| `net.stratalauncher.core.instance.Instance` | `cpp/core/instance/Instance` | done | instance metadata, path, version, and runtime binding |
| `net.stratalauncher.core.instance.InstanceManager` | `cpp/core/instance/InstanceManager` | done | list management, create/duplicate/delete, and disk layout |
| `net.stratalauncher.core.launch.GameLauncher` | `cpp/core/launch/GameLauncher`, `cpp/core/loader/LoaderInstaller` | done | version rules, loader-profile merge and launch args, native extraction, process/readiness/crash tracking |
| `net.stratalauncher.core.loader.FabricInstaller` | `cpp/core/loader/LoaderInstaller::installProfileBased` | done | Fabric metadata, profile validation, library installation |
| `net.stratalauncher.core.loader.ForgeInstaller` | `cpp/core/loader/LoaderInstaller::installOfficial` | done | SHA-1 verified Forge installer and client profile discovery |
| `net.stratalauncher.core.loader.LoaderInstaller` | `cpp/core/loader/LoaderInstaller` | done | common loader install orchestration and atomic per-instance state |
| `net.stratalauncher.core.loader.LoaderMetadataClient` | `cpp/core/loader/LoaderInstaller::availableVersions` | done | bounded HTTPS metadata lookup and compatible version filtering |
| `net.stratalauncher.core.loader.LoaderProfile` | `cpp/core/loader/LoaderInstaller.h::LoaderProfile` | done | validated profile metadata and Minecraft inheritance |
| `net.stratalauncher.core.loader.LoaderSelection` | `cpp/core/loader/LoaderInstaller.h::LoaderSelection` | done | selected loader type and compatible version |
| `net.stratalauncher.core.loader.LoaderType` | `cpp/core/loader/LoaderInstaller.h::LoaderType` | done | supported loader identifiers |
| `net.stratalauncher.core.loader.MavenVersions` | `cpp/core/loader/LoaderInstaller::parseMavenVersions` | done | DTD-rejecting Maven metadata parsing and newest-first filtering |
| `net.stratalauncher.core.loader.NeoForgeInstaller` | `cpp/core/loader/LoaderInstaller::installOfficial` | done | compatible NeoForge installer and client profile discovery |
| `net.stratalauncher.core.loader.ProfileBasedInstaller` | `cpp/core/loader/LoaderInstaller::installProfileBased` | done | shared Fabric/Quilt profile-based installation |
| `net.stratalauncher.core.loader.QuiltInstaller` | `cpp/core/loader/LoaderInstaller::installProfileBased` | done | Quilt metadata, profile validation, library installation |
| `net.stratalauncher.core.logging.LauncherLogging` | `cpp/core/logging/LauncherLogging` | done | central logger and redaction safeguards |
| `net.stratalauncher.core.modrinth.ContentManager` | `cpp/core/modrinth/ContentManager` | done | tracked install, enable/disable, removal, world lookup, and dependency checks |
| `net.stratalauncher.core.modrinth.ContentType` | `cpp/core/modrinth/ContentType` | done | Modrinth project types and game-directory mapping |
| `net.stratalauncher.core.modrinth.ModrinthClient` | `cpp/core/modrinth/ModrinthClient` | done | bounded Modrinth search/version API and strict metadata/download validation |
| `net.stratalauncher.core.modrinth.ServerContentType` | `cpp/core/modrinth/ContentType::ServerContentType` | done | server-side Modrinth project categories |
| `net.stratalauncher.core.mojang.MojangManifestClient` | `cpp/core/mojang/MojangManifestClient` | done | Mojang version manifest parsing, cache, and offline fallback |
| `net.stratalauncher.core.runtime.ExternalJavaRuntimeInstaller` | `cpp/core/runtime/JavaRuntimeManager` | in progress | Temurin/Zulu package discovery, checksum verification where published, staged extraction |
| `net.stratalauncher.core.runtime.JavaRuntimeManager` | `cpp/core/runtime/JavaRuntimeManager` | in progress | Mojang runtime metadata, installation, inventory, selection, and deletion safeguards |
| `net.stratalauncher.core.server.DevServerManager` | `cpp/core/server/DevServerManager` | done | server creation, installation, launch, inventory, and lifecycle |
| `net.stratalauncher.core.server.ServerDistribution` | `cpp/core/server/ServerDistribution` | done | Vanilla, Paper, and Fabric distribution metadata and install plans |
| `net.stratalauncher.core.server.ServerModrinthManager` | `cpp/core/server/ServerModrinthManager` | todo | mod/plugin management for server content |
| `net.stratalauncher.core.settings.InstanceSettings` | `cpp/core/settings/InstanceSettings` | done | instance settings and legacy JSON compatibility |
| `net.stratalauncher.core.settings.MinecraftControls` | `cpp/core/settings/MinecraftControls` | done | default bindings, key labels, and conflict detection; interactive key capture remains a known gap |
| `net.stratalauncher.core.settings.MinecraftOptionValues` | `cpp/core/settings/MinecraftOptionValues` | done | version-aware FOV display/storage conversion |
| `net.stratalauncher.core.settings.NbtFile` | `cpp/core/settings/NbtFile` | done | bounded NBT read/write, gzip compatibility, atomic replacement |
| `net.stratalauncher.core.settings.OptionsParser` | `cpp/core/settings/OptionsFile` | done | lossless `options.txt` parsing and atomic updates |
| `net.stratalauncher.core.settings.ServerListStore` | `cpp/core/settings/ServerListStore` | done | `servers.dat` CRUD, unknown-tag preservation, and compression migration |
| `net.stratalauncher.ui.AnimationBenchmark` | `cpp/ui/AnimationBenchmark` | todo | UI benchmark harness used for frame cost measurements |
| `net.stratalauncher.ui.ButtonMotion` | `cpp/app/MainWindow.qml` | done | button and instance-card hover/click motion |
| `net.stratalauncher.ui.PageTransition` | `cpp/app/MainWindow.qml`, `cpp/app/Motion.qml` | done | page-switch fade and horizontal movement |
| `net.stratalauncher.ui.FlyoutManager` | `cpp/ui/FlyoutManager` | todo | reusable overlay/flyout manager and toast queue |
| `net.stratalauncher.ui.FrameTimeOverlay` | `cpp/app/FrameTimeOverlay.qml` | done | frame-swap timing statistics and debug overlay |
| `net.stratalauncher.ui.IconAssets` | `cpp/app/ThemeAwareIcon.qml`, `cpp/app/resources.qrc` | done | bundled SVG lookup and correct theme/color pairing |
| `net.stratalauncher.ui.MainLayout` | `cpp/app/MainWindow.qml` | done | shell, navigation, Home/About, and window state |
| `net.stratalauncher.ui.MotionConstants` | `cpp/app/Motion.qml` | done | centralized motion durations, curves, and reduce-motion behavior |
| `net.stratalauncher.ui.StartupOverlay` | `cpp/app/MainWindow.qml` | done | animated splash, warning, copyright, and initialization wait |
| `net.stratalauncher.ui.ThemeManager` | `cpp/core/ThemeSettings.*`, `cpp/app/Theme.qml` | done | persisted appearance settings and shared radius tokens |
| `net.stratalauncher.ui.ToggleSwitchStyle` | `cpp/app/StrataSwitch.qml` | done | custom switch track, thumb, label placement, and motion |
| `net.stratalauncher.ui.UiTypography` | `cpp/app/main.cpp`, `cpp/app/assets/fonts/` | done | bundled font loading and application typography |
| `net.stratalauncher.ui.auth.AccountSelectorView` | `cpp/app/MainWindow.qml` | done | account picker, offline profile controls, and Microsoft sign-in flyout |
| `net.stratalauncher.ui.components.StatusIndicator` | `cpp/ui/components/StatusIndicator` | todo | green/blue/grey/red process state indicator |
| `net.stratalauncher.ui.content.ContentView` | `cpp/app/MainWindow.qml` + `cpp/ui/content/ContentController` | done | asynchronous content browser and install workflow |
| `net.stratalauncher.ui.install.InstallingView` | `cpp/ui/install/InstallingView` | todo | install progress overlay and per-item download message |
| `net.stratalauncher.ui.instances.InstanceListView` | `cpp/ui/instances/InstanceListView` | todo | list view, selection, launch, and instance cards |
| `net.stratalauncher.ui.instances.RunningInstanceView` | `cpp/ui/instances/RunningInstanceView` | todo | running game view and console/logs panel |
| `net.stratalauncher.ui.server.ServerConsoleView` | `cpp/app/MainWindow.qml`, `cpp/core/server/ServerProcess` | done | live server console, commands, and output history |
| `net.stratalauncher.ui.server.ServerPanelView` | `cpp/app/MainWindow.qml`, `cpp/core/server/DevServerManager` | done | server inventory, lifecycle, settings, and resource state |
| `net.stratalauncher.ui.settings.InstanceSettingsView` | `cpp/ui/settings/InstanceSettingsController` and `cpp/app/MainWindow.qml` | done | asynchronous per-instance options, servers, Java flags, and runtime editing; interactive key capture remains a known gap |

## Coverage notes

The old Java suite covers the critical behaviors in `core/*` and the UI shell in `ui/instances`. The port should preserve the Java tests in equivalent Qt Test / Qt Quick Test suites before moving to later phases.

The M8 loader Java classes are consolidated into `LoaderInstaller` and its data types rather than separate installer/client classes. Forge and NeoForge retain their upstream installer processes; the launcher verifies the installer artifact checksum and supplies the selected managed Java runtime.

M9 preserves tracked-only removal, Modrinth SHA-1 verification, loader/Minecraft compatibility filters, and per-world data-pack placement. C++ additionally resolves and installs missing required dependencies after user confirmation, validates incompatible dependencies, and toggles content by appending `.disabled` to its filename; these differences are covered by M9 tests. The user confirmed M9 complete; live Modrinth installs were not exercised.

M11 implements Vanilla, Paper, and Fabric server installation and lifecycle, with EULA persistence, server properties editing, asynchronous console output, runtime ownership protection, and resource monitoring. M12 adds server-side Modrinth plugin/mod browsing and installation; live online installs were not independently exercised.

M12 ports global preferences to Java-compatible `settings.json`; stored account tokens are intentionally not migrated and users must sign in again. VSync changes apply after restart. The Qt startup entrance uses a centered scale-only animation; unlike Java, it omits the 10px vertical translation to keep the splash content centered throughout. The user confirmed M12 complete. The frame-time overlay was not separately verified in the GUI.
