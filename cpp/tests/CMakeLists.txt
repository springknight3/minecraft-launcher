add_executable(strata_tests
    LauncherQtTests.cpp
    ../app/resources.qrc
)

target_link_libraries(strata_tests
    PRIVATE
        strata_core
        strata_ui
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Test
)

add_test(NAME strata_tests COMMAND strata_tests)

add_executable(strata_m3_tests
    M3CoreTests.cpp
)

target_link_libraries(strata_m3_tests
    PRIVATE
        strata_core
        Qt6::Core
        Qt6::Test
)

add_test(NAME strata_m3_tests COMMAND strata_m3_tests)

add_executable(strata_m4_tests
    M4InstallTests.cpp
    M4InstallTests.h
)

target_link_libraries(strata_m4_tests
    PRIVATE
        strata_core
        Qt6::Core
        Qt6::Network
        Qt6::Test
)

add_test(NAME strata_m4_tests COMMAND strata_m4_tests)

add_executable(strata_process_fixture
    GameProcessFixture.cpp
)

add_executable(strata_m5_tests
    M5LaunchTests.cpp
    M5LaunchTests.h
)

target_compile_definitions(strata_m5_tests
    PRIVATE
        STRATA_PROCESS_FIXTURE="$<TARGET_FILE:strata_process_fixture>"
)

target_link_libraries(strata_m5_tests
    PRIVATE
        strata_core
        Qt6::Core
        Qt6::Test
)

add_dependencies(strata_m5_tests strata_process_fixture)
add_test(NAME strata_m5_tests COMMAND strata_m5_tests)

add_executable(strata_m6_tests
    M6RuntimeTests.cpp
    M6RuntimeTests.h
)

target_link_libraries(strata_m6_tests
    PRIVATE
        strata_core
        Qt6::Core
        Qt6::Test
)

add_test(NAME strata_m6_tests COMMAND strata_m6_tests)
