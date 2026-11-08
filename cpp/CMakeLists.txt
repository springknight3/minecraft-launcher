cmake_minimum_required(VERSION 3.25)

project(StrataLauncher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test Network)
qt_policy(SET QTP0001 NEW)
include(CTest)

add_subdirectory(core)
add_subdirectory(ui)
add_subdirectory(app)
add_subdirectory(tests)
