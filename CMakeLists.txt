cmake_minimum_required(VERSION 3.25)

project(StrataLauncher VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

enable_testing()

add_subdirectory(cpp)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES"
    DESTINATION .
)
install(DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES"
    DESTINATION .
)

set(CPACK_PACKAGE_NAME "StrataLauncher")
set(CPACK_PACKAGE_VENDOR "StrataLauncher")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "StrataLauncher")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
string(TOLOWER "${CMAKE_SYSTEM_NAME}" CPACK_SYSTEM_NAME)
set(CPACK_PACKAGE_FILE_NAME
    "stratalauncher-${PROJECT_VERSION}-${CPACK_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
set(CPACK_NSIS_DISPLAY_NAME "StrataLauncher")
set(CPACK_NSIS_PACKAGE_NAME "StrataLauncher")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH OFF)
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut \\\"$SMPROGRAMS\\\\StrataLauncher.lnk\\\" \\\"$INSTDIR\\\\bin\\\\stratalauncher.exe\\\""
)
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete \\\"$SMPROGRAMS\\\\StrataLauncher.lnk\\\""
)

if(WIN32)
    set(CPACK_GENERATOR NSIS)
else()
    set(CPACK_GENERATOR TGZ)
endif()

include(CPack)
