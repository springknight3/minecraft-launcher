qt_add_executable(stratalauncher
    main.cpp
    resources.qrc
)

set_source_files_properties(
    Motion.qml
    Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(stratalauncher
    URI StrataLauncher
    VERSION 1.0
    QML_FILES
        MainWindow.qml
        Motion.qml
        ContentSelectionBox.qml
        StrataButton.qml
        StrataCheckBox.qml
        StrataComboBox.qml
        StrataSpinBox.qml
        StatusLight.qml
        StrataSwitch.qml
        StrataTextArea.qml
        StrataTextField.qml
        StrataTabButton.qml
        StaggeredFade.qml
        Theme.qml
        ThemeAwareIcon.qml
        FrameTimeOverlay.qml
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

target_compile_definitions(stratalauncher
    PRIVATE
        STRATA_VERSION="${PROJECT_VERSION}"
)

set_target_properties(stratalauncher PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "StrataLauncher"
    MACOSX_BUNDLE_GUI_IDENTIFIER "net.stratalauncher.app"
)

include(GNUInstallDirs)
install(TARGETS stratalauncher
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(APPLE OR WIN32)
    qt_generate_deploy_qml_app_script(
        TARGET stratalauncher
        OUTPUT_SCRIPT deploy_script
    )
    install(SCRIPT ${deploy_script})
endif()
