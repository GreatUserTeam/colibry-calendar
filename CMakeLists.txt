cmake_minimum_required(VERSION 3.21)

project(colibry-calendar
    VERSION 0.2.0
    DESCRIPTION "Calendar for the Colibry"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(Colibry_BUILD_TESTS "Build unit tests" ON)
option(Colibry_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(_qt_components Widgets)
if(Colibry_BUILD_TESTS)
    list(APPEND _qt_components Test)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS ${_qt_components})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(ColibryApps)

if(Colibry_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(common)
add_library(gus_calendar_core STATIC
    src/core/EventStore.h
    src/core/EventStore.cpp
)
target_include_directories(gus_calendar_core PUBLIC src)
target_link_libraries(gus_calendar_core PUBLIC Qt6::Widgets)
colibry_target_defaults(gus_calendar_core)

add_library(gus_calendar_ui STATIC
    src/CalendarWindow.h
    src/CalendarWindow.cpp
    src/EventDialog.h
    src/EventDialog.cpp
)
target_include_directories(gus_calendar_ui PUBLIC src)
target_link_libraries(gus_calendar_ui PUBLIC gus_calendar_core colibry_common Qt6::Widgets)
colibry_target_defaults(gus_calendar_ui)

add_executable(colibry-calendar src/main.cpp)
target_link_libraries(colibry-calendar PRIVATE gus_calendar_ui)
colibry_target_defaults(colibry-calendar)

colibry_install_app(colibry-calendar
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-calendar.desktop
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-calendar.svg)

if(Colibry_BUILD_TESTS)
    colibry_add_test(calendar_core SOURCES tests/test_core.cpp LINK gus_calendar_core)
    colibry_add_test(calendar_ui SOURCES tests/test_ui.cpp LINK gus_calendar_ui)
endif()
