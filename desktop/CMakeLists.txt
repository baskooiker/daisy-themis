cmake_minimum_required(VERSION 3.16)
project(ThemisDesktop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(THEMIS_ENABLE_MIDI "Enable MIDI output support" ON)

# Find required packages
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# RtMidi (optional)
if(THEMIS_ENABLE_MIDI)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(RTMIDI rtmidi)
    endif()
    if(NOT RTMIDI_FOUND)
        message(WARNING "RtMidi not found - MIDI output will be disabled")
        set(THEMIS_ENABLE_MIDI OFF)
    endif()
endif()

# ImGui - fetch from GitHub
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)

# Create ImGui library
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC
    SDL2::SDL2
    OpenGL::GL
)

# Core library (shared algorithms)
add_library(themis_core STATIC
    ../core/themis_platform.cpp
    ../core/themis_data.cpp
    ../core/themis_patterns.cpp
    ../core/themis_melody.cpp
    ../core/themis_sequencer.cpp
)
target_include_directories(themis_core PUBLIC ../core)

# Desktop application
set(DESKTOP_SOURCES
    main.cpp
    platform_desktop.cpp
    audio.cpp
    ui.cpp
)

if(THEMIS_ENABLE_MIDI)
    list(APPEND DESKTOP_SOURCES midi_out.cpp)
endif()

add_executable(themis_desktop ${DESKTOP_SOURCES})

target_include_directories(themis_desktop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
)

target_link_libraries(themis_desktop PRIVATE
    themis_core
    imgui
    SDL2::SDL2
    OpenGL::GL
)

if(THEMIS_ENABLE_MIDI)
    target_compile_definitions(themis_desktop PRIVATE THEMIS_ENABLE_MIDI)
    target_include_directories(themis_desktop PRIVATE ${RTMIDI_INCLUDE_DIRS})
    target_link_libraries(themis_desktop PRIVATE ${RTMIDI_LIBRARIES})
endif()

# Platform-specific settings
if(APPLE)
    target_link_libraries(themis_desktop PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(themis_desktop PRIVATE
        pthread
        dl
    )
endif()

# Install
install(TARGETS themis_desktop RUNTIME DESTINATION bin)
