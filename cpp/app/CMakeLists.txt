qt_add_executable(stratalauncher
    main.cpp
    resources.qrc
)

target_link_libraries(stratalauncher
    PRIVATE
        strata_core
        strata_ui
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
