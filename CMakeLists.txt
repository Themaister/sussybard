cmake_minimum_required(VERSION 3.6)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)
project(SussyBard LANGUAGES CXX C)

add_library(fmsynth STATIC fmsynth/src/fmsynth.c)
target_include_directories(fmsynth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/fmsynth/include)
# Don't bother with SIMD here.

if (CMAKE_COMPILER_IS_GNUCXX OR (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang"))
    set(SUSSYBARD_CXX_FLAGS -Wshadow -Wall -Wextra -Wno-comment -Wno-missing-field-initializers -Wno-empty-body -ffast-math)
    if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
        set(SUSSYBARD_CXX_FLAGS ${SUSSYBARD_CXX_FLAGS} -Wno-backslash-newline-escape)
    endif()
    if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Release"))
        message("Disabling frame pointer for profiling/debug.")
        set(SUSSYBARD_CXX_FLAGS ${SUSSYBARD_CXX_FLAGS} -fno-omit-frame-pointer)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
        message("Enabling SSE3 support.")
        set(SUSSYBARD_CXX_FLAGS ${SUSSYBARD_CXX_FLAGS} -msse3)
    endif()
elseif (MSVC)
    set(SUSSYBARD_CXX_FLAGS /D_CRT_SECURE_NO_WARNINGS /wd4267 /wd4244 /wd4309 /wd4005 /MP /DNOMINMAX)
endif()

include(GNUInstallDirs)

if (NOT WIN32)
    find_package(ALSA REQUIRED)
    include(FindPkgConfig)
    pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb xcb-xtest xcb-keysyms)
    pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)
endif()

add_executable(sussybard
        sussybard.cpp
        dsp.hpp
        simd_headers.hpp
        midi_source.hpp
        cli_parser.hpp cli_parser.cpp
        midi_source_udp.hpp midi_source_udp.cpp
        udp_common.hpp
        udp_sink.hpp udp_sink.cpp
        synth.cpp synth.hpp)

target_link_libraries(sussybard PRIVATE fmsynth)

if (NOT WIN32)
    target_sources(sussybard PRIVATE
            audio_pulse.cpp audio_pulse.hpp
            midi_source_alsa.cpp midi_source_alsa.hpp
            key_sink_xcb.cpp key_sink_xcb.hpp)
    target_link_libraries(sussybard PRIVATE ${ALSA_LIBRARIES})
    target_link_libraries(sussybard PRIVATE PkgConfig::XCB PkgConfig::PULSE)
    target_include_directories(sussybard PRIVATE ${ALSA_INCLUDE_DIRS})
else()
    target_sources(sussybard PRIVATE
            audio_wasapi.cpp audio_wasapi.hpp
            midi_source_win32.cpp midi_source_win32.hpp
            key_sink_win32.cpp key_sink_win32.hpp)
    target_link_libraries(sussybard PRIVATE winmm avrt ws2_32)
endif()

target_compile_options(sussybard PRIVATE ${SUSSYBARD_CXX_FLAGS})

