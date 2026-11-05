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
| `net.stratalauncher.core.auth.Account` | `cpp/core/auth/Account` | todo | persisted account record and Microsoft/offline identity state |
| `net.stratalauncher.core.auth.AccountManager` | `cpp/core/auth/AccountManager` | todo | account load/save, active account selection, refresh |
| `net.stratalauncher.core.auth.CredentialStore` | `cpp/core/auth/CredentialStore` | todo | secret abstraction with a QtKeychain-backed implementation |
| `net.stratalauncher.core.auth.LauncherSettings` | `cpp/core/settings/LauncherSettings` | todo | global launcher preferences and persisted theme choices |
| `net.stratalauncher.core.auth.MicrosoftAuthService` | `cpp/core/auth/MicrosoftAuthService` | todo | Microsoft device-code flow and token acquisition |
| `net.stratalauncher.core.auth.NativeCredentialStore` | `cpp/core/auth/NativeCredentialStore` | todo | platform credential storage adapter |
| `net.stratalauncher.core.auth.VSyncConfiguration` | `cpp/core/settings/VSyncConfiguration` | todo | VSync preference parsing and persistence |
| `net.stratalauncher.core.download.DownloadManager` | `cpp/core/download/DownloadManager` | done | concurrent file downloads, retry, checksum, and progress |
| `net.stratalauncher.core.download.DownloadTask` | `cpp/core/download/DownloadTask` | done | per-file download source, destination, SHA, and state |
| `net.stratalauncher.core.instance.Instance` | `cpp/core/instance/Instance` | done | instance metadata, path, version, and runtime binding |
| `net.stratalauncher.core.instance.InstanceManager` | `cpp/core/instance/InstanceManager` | done | list management, create/duplicate/delete, and disk layout |
| `net.stratalauncher.core.launch.GameLauncher` | `cpp/core/launch/GameLauncher`, `cpp/core/launch/GameProcess` | done | version rules, offline launch args, native extraction, process/readiness/crash tracking |
| `net.stratalauncher.core.loader.FabricInstaller` | `cpp/core/loader/FabricInstaller` | todo | Fabric loader installation and metadata |
| `net.stratalauncher.core.loader.ForgeInstaller` | `cpp/core/loader/ForgeInstaller` | todo | Forge setup/install pipeline |
| `net.stratalauncher.core.loader.LoaderInstaller` | `cpp/core/loader/LoaderInstaller` | todo | common loader installation orchestration |
| `net.stratalauncher.core.loader.LoaderMetadataClient` | `cpp/core/loader/LoaderMetadataClient` | todo | metadata lookup for loader versions |
| `net.stratalauncher.core.loader.LoaderProfile` | `cpp/core/loader/LoaderProfile` | todo | parsed loader profile and compatibility data |
| `net.stratalauncher.core.loader.LoaderSelection` | `cpp/core/loader/LoaderSelection` | todo | user-selected loader and version combination |
| `net.stratalauncher.core.loader.LoaderType` | `cpp/core/loader/LoaderType` | todo | enum-like loader identifiers |
| `net.stratalauncher.core.loader.MavenVersions` | `cpp/core/loader/MavenVersions` | todo | Maven metadata resolution and version filtering |
| `net.stratalauncher.core.loader.NeoForgeInstaller` | `cpp/core/loader/NeoForgeInstaller` | todo | NeoForge installer pipeline |
| `net.stratalauncher.core.loader.ProfileBasedInstaller` | `cpp/core/loader/ProfileBasedInstaller` | todo | profile-based installation logic |
| `net.stratalauncher.core.loader.QuiltInstaller` | `cpp/core/loader/QuiltInstaller` | todo | Quilt installer and compatibility handling |
| `net.stratalauncher.core.logging.LauncherLogging` | `cpp/core/logging/LauncherLogging` | done | central logger and redaction safeguards |
| `net.stratalauncher.core.modrinth.ContentManager` | `cpp/core/modrinth/ContentManager` | todo | browse/install/enable content for mods and packs |
| `net.stratalauncher.core.modrinth.ContentType` | `cpp/core/modrinth/ContentType` | todo | enum for content categories |
| `net.stratalauncher.core.modrinth.ModrinthClient` | `cpp/core/modrinth/ModrinthClient` | todo | Modrinth API client and metadata parsing |
| `net.stratalauncher.core.modrinth.ServerContentType` | `cpp/core/modrinth/ServerContentType` | todo | server-side content categories |
| `net.stratalauncher.core.mojang.MojangManifestClient` | `cpp/core/mojang/MojangManifestClient` | done | Mojang version manifest parsing, cache, and offline fallback |
| `net.stratalauncher.core.runtime.ExternalJavaRuntimeInstaller` | `cpp/core/runtime/JavaRuntimeManager` | in progress | Temurin/Zulu package discovery, checksum verification where published, staged extraction |
| `net.stratalauncher.core.runtime.JavaRuntimeManager` | `cpp/core/runtime/JavaRuntimeManager` | in progress | Mojang runtime metadata, installation, inventory, selection, and deletion safeguards |
| `net.stratalauncher.core.server.DevServerManager` | `cpp/core/server/DevServerManager` | todo | local server lifecycle for dev/test server management |
| `net.stratalauncher.core.server.ServerDistribution` | `cpp/core/server/ServerDistribution` | todo | server distribution metadata and installation data |
| `net.stratalauncher.core.server.ServerModrinthManager` | `cpp/core/server/ServerModrinthManager` | todo | mod/plugin management for server content |
| `net.stratalauncher.core.settings.InstanceSettings` | `cpp/core/settings/InstanceSettings` | done | instance settings and legacy JSON compatibility |
| `net.stratalauncher.core.settings.MinecraftControls` | `cpp/core/settings/MinecraftControls` | todo | keybind and control mapping encoders |
| `net.stratalauncher.core.settings.MinecraftOptionValues` | `cpp/core/settings/MinecraftOptionValues` | todo | default/allowed values for game options |
| `net.stratalauncher.core.settings.NbtFile` | `cpp/core/settings/NbtFile` | todo | NBT file read/write for `servers.dat` and friends |
| `net.stratalauncher.core.settings.OptionsParser` | `cpp/core/settings/OptionsParser` | todo | `options.txt` parser and serializer |
| `net.stratalauncher.core.settings.ServerListStore` | `cpp/core/settings/ServerListStore` | todo | server list persistence and compatibility |
| `net.stratalauncher.ui.AnimationBenchmark` | `cpp/ui/AnimationBenchmark` | todo | UI benchmark harness used for frame cost measurements |
| `net.stratalauncher.ui.ButtonMotion` | `cpp/ui/animations/ButtonMotion` | todo | shared button/hover motion shims |
| `net.stratalauncher.ui.PageTransition` | `cpp/ui/animations/PageTransition` | todo | page-switch animation orchestration |
| `net.stratalauncher.ui.FlyoutManager` | `cpp/ui/FlyoutManager` | todo | reusable overlay/flyout manager and toast queue |
| `net.stratalauncher.ui.FrameTimeOverlay` | `cpp/ui/FrameTimeOverlay` | todo | render-time overlay for debug profiling |
| `net.stratalauncher.ui.IconAssets` | `cpp/ui/IconAssets` | todo | theme-aware icon selection and SVG asset mapping |
| `net.stratalauncher.ui.MainLayout` | `cpp/ui/MainLayout` | todo | shell layout, navigation, and top-level window state |
| `net.stratalauncher.ui.MotionConstants` | `cpp/ui/MotionConstants` | todo | centralized motion timing constants |
| `net.stratalauncher.ui.StartupOverlay` | `cpp/ui/StartupOverlay` | todo | splash and initialization overlay |
| `net.stratalauncher.ui.ThemeManager` | `cpp/ui/ThemeManager` | todo | theme persistence and runtime palette toggling |
| `net.stratalauncher.ui.ToggleSwitchStyle` | `cpp/ui/ToggleSwitchStyle` | todo | custom switch styling and motion cadence |
| `net.stratalauncher.ui.UiTypography` | `cpp/ui/UiTypography` | todo | font family and typography scale setup |
| `net.stratalauncher.ui.auth.AccountSelectorView` | `cpp/ui/auth/AccountSelectorView` | todo | account picker and sign-in flyout |
| `net.stratalauncher.ui.components.StatusIndicator` | `cpp/ui/components/StatusIndicator` | todo | green/blue/grey/red process state indicator |
| `net.stratalauncher.ui.content.ContentView` | `cpp/ui/content/ContentView` | todo | content browser and install workflow |
| `net.stratalauncher.ui.install.InstallingView` | `cpp/ui/install/InstallingView` | todo | install progress overlay and per-item download message |
| `net.stratalauncher.ui.instances.InstanceListView` | `cpp/ui/instances/InstanceListView` | todo | list view, selection, launch, and instance cards |
| `net.stratalauncher.ui.instances.RunningInstanceView` | `cpp/ui/instances/RunningInstanceView` | todo | running game view and console/logs panel |
| `net.stratalauncher.ui.server.ServerConsoleView` | `cpp/ui/server/ServerConsoleView` | todo | server console and command pane |
| `net.stratalauncher.ui.server.ServerPanelView` | `cpp/ui/server/ServerPanelView` | todo | server management and status pane |
| `net.stratalauncher.ui.settings.InstanceSettingsView` | `cpp/ui/settings/InstanceSettingsView` | todo | instance settings flyout and editor pages |

## Coverage notes

The old Java suite covers the critical behaviors in `core/*` and the UI shell in `ui/instances`. The port should preserve the Java tests in equivalent Qt Test / Qt Quick Test suites before moving to later phases.
