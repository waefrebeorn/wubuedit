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

# ── Proxy clip generation (pure C11, invokes FFmpeg CLI) ───────
add_library(wedit_proxy STATIC
    src/wedit_proxy.c
)
target_include_directories(wedit_proxy PUBLIC src)
target_link_libraries(wedit_proxy PUBLIC wedit_timeline)

# ── EDL export (pure C11, generates CMX 3600 EDL for NLE round-trip) ─
add_library(wedit_edl STATIC
    src/wedit_edl.c
)
target_include_directories(wedit_edl PUBLIC src)
target_link_libraries(wedit_edl PUBLIC wedit_timeline)

# ── Render pipeline (FFmpeg deps linked after detection below) ────────────

# ── GPU renderer (SDL2 + OpenGL, pure C11) ─────────────────────────────
# SDL2 + OpenGL are SYSTEM LIBRARIES, linked like -lz.
# Shaders are original GLSL code written by WuBu — the YUV→RGB decoder
# is adapted from Diffusion Studio's decompiled shader architecture.
find_package(OpenGL QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
    pkg_check_modules(GLEW QUIET glew)
endif()

# Also check GLEW directly (pkg_check_modules sometimes doesn't set _FOUND correctly)
find_path(GLEW_INCLUDE_DIR GL/glew.h)
find_library(GLEW_LIBRARY NAMES GLEW glew)
if(GLEW_INCLUDE_DIR AND GLEW_LIBRARY)
    set(GLEW_FOUND TRUE)
    set(GLEW_INCLUDE_DIRS ${GLEW_INCLUDE_DIR})
    set(GLEW_LIBRARIES ${GLEW_LIBRARY})
endif()

if(SDL2_FOUND)
    add_library(wugui_renderer STATIC
        src/wugui_renderer.c
    )
    target_include_directories(wugui_renderer PUBLIC src)
    target_include_directories(wugui_renderer PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(wugui_renderer PUBLIC
        wedit_timeline
        ${SDL2_LIBRARIES}
        ${SDL2_LDFLAGS}
    )
    if(OPENGL_FOUND)
        target_link_libraries(wugui_renderer PUBLIC OpenGL::GL)
    endif()
    if(GLEW_FOUND)
        if(GLEW_INCLUDE_DIRS)
            target_include_directories(wugui_renderer PRIVATE ${GLEW_INCLUDE_DIRS})
        endif()
        if(GLEW_LIBRARIES)
            target_link_libraries(wugui_renderer PUBLIC ${GLEW_LIBRARIES})
        endif()
        target_compile_definitions(wugui_renderer PUBLIC WUGUI_HAS_GLEW=1)
    else()
        target_compile_definitions(wugui_renderer PUBLIC WUGUI_HAS_SDL_LOADER=1)
    endif()
    target_compile_definitions(wugui_renderer PUBLIC WUGUI_HAS_SDL2=1)
    message(STATUS "SDL2 + OpenGL found — GPU renderer enabled")
else()
    message(STATUS "SDL2 not found — GUI renderer disabled (CLI-only mode)")
endif()

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

# ── Filter chain (native C11 pixel processing, AGI path) ──────────────────
add_library(wedit_filter STATIC
    src/wedit_filter.c
)
target_include_directories(wedit_filter PUBLIC src)
target_link_libraries(wedit_filter PUBLIC wedit_timeline m)

# ── Preview engine (GPU-accelerated real-time preview) ──────────────────
add_library(wedit_preview STATIC
    src/wedit_preview.c
)
target_include_directories(wedit_preview PUBLIC src)
target_link_libraries(wedit_preview PUBLIC wedit_timeline wedit_render wedit_filter m)
if(TARGET wugui_renderer)
    target_link_libraries(wedit_preview PUBLIC wugui_renderer)
endif()
if(WEDIT_AV_FOUND)
    target_compile_definitions(wedit_preview PUBLIC WEDIT_HAS_FFMPEG=1)
endif()

# ── Render pipeline (pure C11 core, FFmpeg for encode/mux plumbing) ────────
# Frame-by-frame compositing: timeline → clip lookup → decode → composite → encode
# Uses CPU blend modes (matching Diffusion Studio's Gf table) + color grading.
# GPU path hooks into wugui_renderer when SDL2 is available.
add_library(wedit_render STATIC
    src/wedit_render.c
    src/wedit_clip_source.c
)
target_include_directories(wedit_render PUBLIC src)
target_link_libraries(wedit_render PUBLIC wedit_timeline wedit_effects wedit_filter m)
if(WEDIT_AV_FOUND)
    target_link_libraries(wedit_render PUBLIC wedit_media)
    target_compile_definitions(wedit_render PUBLIC WEDIT_HAS_FFMPEG=1)
endif()
# Link wugui_renderer for GPU compositing path (when available)
if(TARGET wugui_renderer)
    target_link_libraries(wedit_render PUBLIC wugui_renderer)
endif()

# ── CLI entry point ───────────────────────────────────────────────────────
add_executable(wubuedit
    src/wedit_main.c
)
target_link_libraries(wubuedit PRIVATE wedit_timeline)
if(WEDIT_AV_FOUND)
    target_link_libraries(wubuedit PRIVATE wedit_media)
endif()
target_link_libraries(wubuedit PRIVATE wedit_project wedit_effects wedit_proxy wedit_edl wedit_render wedit_preview wedit_filter m)

# ── Tests ─────────────────────────────────────────────────────────────────
enable_testing()
add_executable(test_timeline tests/test_timeline.c)
target_link_libraries(test_timeline PRIVATE wedit_timeline wedit_project wedit_effects wedit_proxy wedit_edl wedit_filter m)
add_test(NAME timeline_tests COMMAND test_timeline)

# ── Render pipeline tests (requires FFmpeg for media decode) ──────────────
if(WEDIT_AV_FOUND)
    add_executable(test_render tests/test_render.c)
    target_link_libraries(test_render PRIVATE wedit_render wedit_timeline wedit_project m)
    add_test(NAME render_tests COMMAND test_render)
endif()
