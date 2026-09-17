cmake_minimum_required(VERSION 3.14)
project(hazel VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)

# Optimize FLTK build
set(FLTK_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FLUID OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FLTK_OPTIONS OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FORMS OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_GL OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_HTML_DOCS OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_PDF_DOCS OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# SVGs are good for notebooks if we ever want graphics
set(FLTK_OPTION_SVG ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    fltk
    GIT_REPOSITORY https://github.com/fltk/fltk.git
    GIT_TAG release-1.4.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fltk)

add_library(hazel
    src/HazelApp.cpp 
    src/api.cpp src/TerminalPane.cpp
)

target_include_directories(hazel PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(hazel PUBLIC fltk::fltk fltk::images)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    add_executable(hazel_demo examples/demo.cpp)
    target_include_directories(hazel_demo PRIVATE third_party/ueval)
    target_link_libraries(hazel_demo PRIVATE hazel m)
endif()
