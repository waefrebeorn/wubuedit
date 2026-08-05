# wubuedit/CMakeLists.txt — SLERM ground-up video editor, native C11 core
cmake_minimum_required(VERSION 3.16)
project(wubuedit C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ── Native-only guard (slerm-reimplementation SOP) ────────────────────────
# This editor must NOT link a web runtime or JVM.
if(DEFINED ENV{ELECTRON})
    message(FATAL_ERROR "wubuedit is native-only: ELECTRON is not allowed.")
endif()

# ── Compiler flags ────────────────────────────────────────────────────────
add_compile_options(-Wall -Wextra -Wpedantic -g)

# ── Core library: timeline model (zero FFmpeg deps) ───────────────────────
add_library(wedit_timeline STATIC
    src/wedit_timeline.c
)
target_include_directories(wedit_timeline PUBLIC src)

# ── Project serialization (pure C11, depends on timeline) ────────────────
add_library(wedit_project STATIC
    src/wedit_project.c
)
target_include_directories(wedit_project PUBLIC src)
target_link_libraries(wedit_project PUBLIC wedit_timeline)

# ── Effects pipeline (pure C11, no FFmpeg deps) ────────────────
add_library(wedit_effects STATIC
    src/wedit_effects.c
)
target_include_directories(wedit_effects PUBLIC src)
target_link_libraries(wedit_effects PUBLIC wedit_timeline)

# ── FFmpeg integration (optional, for demux/decode/encode/mux) ───────────
# FFmpeg/libav is a SYSTEM LIBRARY we link against, like -lz.
# It provides codec/container plumbing. Our timeline model is from-scratch C11.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(AVFORMAT   QUIET libavformat)
    pkg_check_modules(AVCODEC    QUIET libavcodec)
    pkg_check_modules(AVUTIL     QUIET libavutil)
    pkg_check_modules(SWSCALE    QUIET libswscale)
    pkg_check_modules(SWRESAMPLE QUIET libswresample)
    pkg_check_modules(AVFILTER   QUIET libavfilter)

    set(WEDIT_AV_FOUND FALSE)
    if(AVFORMAT_FOUND AND AVCODEC_FOUND AND AVUTIL_FOUND AND AVFILTER_FOUND)
        set(WEDIT_AV_FOUND TRUE)
    endif()
endif()

if(WEDIT_AV_FOUND)
    add_library(wedit_media STATIC
        src/wedit_demux.c
        src/wedit_decode.c
        src/wedit_encode.c
        src/wedit_mux.c
        src/wedit_audio.c
        src/wedit_filter_graph.c
    )
    target_include_directories(wedit_media PUBLIC src)
    target_include_directories(wedit_media PRIVATE
        ${AVFORMAT_INCLUDE_DIRS}
        ${AVCODEC_INCLUDE_DIRS}
        ${AVUTIL_INCLUDE_DIRS}
        ${SWSCALE_INCLUDE_DIRS}
        ${AVFILTER_INCLUDE_DIRS}
    )
    target_link_libraries(wedit_media PUBLIC
        ${AVFORMAT_LIBRARIES}
        ${AVCODEC_LIBRARIES}
        ${AVUTIL_LIBRARIES}
        ${SWSCALE_LIBRARIES}
        ${SWRESAMPLE_LIBRARIES}
        ${AVFILTER_LIBRARIES}
    )
    target_compile_definitions(wedit_media PUBLIC WEDIT_HAS_FFMPEG=1)
    message(STATUS "FFmpeg/libav found — media pipeline + filter graph enabled")
else()
    message(STATUS "FFmpeg/libav not found — timeline-only mode")
endif()

# ── CLI entry point ───────────────────────────────────────────────────────
add_executable(wubuedit
    src/wedit_main.c
)
target_link_libraries(wubuedit PRIVATE wedit_timeline)
if(WEDIT_AV_FOUND)
    target_link_libraries(wubuedit PRIVATE wedit_media)
endif()
target_link_libraries(wubuedit PRIVATE wedit_project wedit_effects)

# ── Tests ─────────────────────────────────────────────────────────────────
enable_testing()
add_executable(test_timeline tests/test_timeline.c)
target_link_libraries(test_timeline PRIVATE wedit_timeline wedit_project wedit_effects)
add_test(NAME timeline_tests COMMAND test_timeline)
