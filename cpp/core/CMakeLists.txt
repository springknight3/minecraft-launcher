add_library(strata_core STATIC
    auth/OfflineAccountStore.cpp
    launch/GameLauncher.cpp
    launch/GameProcess.cpp
    Paths.cpp
    ThemeSettings.cpp
    mojang/MojangManifestClient.cpp
    mojang/MinecraftInstaller.cpp
    download/DownloadManager.cpp
    runtime/JavaRuntimeManager.cpp
    instance/Instance.cpp
    instance/InstanceManager.cpp
    settings/InstanceSettings.cpp
    logging/LauncherLogging.cpp
)

find_package(LibArchive REQUIRED)

target_include_directories(strata_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(strata_core
    PUBLIC
        Qt6::Core
        Qt6::Network
        LibArchive::LibArchive
)
