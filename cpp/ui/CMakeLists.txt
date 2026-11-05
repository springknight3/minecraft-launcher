add_library(strata_ui STATIC
    LauncherController.cpp
    LauncherController.h
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
