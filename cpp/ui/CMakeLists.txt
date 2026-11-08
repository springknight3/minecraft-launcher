add_library(strata_ui STATIC
    content/ContentController.cpp
    content/ContentController.h
    LauncherController.cpp
    LauncherController.h
    settings/InstanceSettingsController.cpp
    settings/InstanceSettingsController.h
)

target_include_directories(strata_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(strata_ui
    PUBLIC
        strata_core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
