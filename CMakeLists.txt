cmake_minimum_required(VERSION 3.16)
project(rom C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O3)
endif()

# Per-target backend gates. Defaults match the Windows reference build (matches
# the upstream la-dx-recompiled binary the user runs). The libxenon/wasm/etc.
# toolchain files will flip LA_MULTIPLAYER off where the network stack isn't
# realistic to port.
option(LA_MULTIPLAYER "Build with ENet + multiplayer overlay (mp_*.cpp)" ON)

# Runtime lives in-tree (vendored from sp00nznet/gb-recompiled).
set(GBRT_DIR "${CMAKE_SOURCE_DIR}/runtime")

# SDL2 backend is currently the only platform implementation. The libxenon /
# wasm / android backends will replace platform_sdl.cpp with a sibling
# platform_<target>.cpp that satisfies the same runtime/include/platform_sdl.h
# contract (gb_platform_init / poll_events / render_frame / vsync / ...).

# --- Runtime library (gbrt) -------------------------------------------------
add_library(gbrt STATIC
    ${GBRT_DIR}/src/gbrt.c
    ${GBRT_DIR}/src/ppu.c
    ${GBRT_DIR}/src/audio.c
    ${GBRT_DIR}/src/interpreter.c
    ${GBRT_DIR}/src/hwtrace.c
    ${GBRT_DIR}/src/platform_3ds.c          # Usar backend de 3DS
    ${GBRT_DIR}/src/menu_gui_stubs.c        # Usar stubs para no pedir ImGui
    ${GBRT_DIR}/src/asset_viewer_stubs.c    # Usar stubs para no pedir ImGui
    ${GBRT_DIR}/src/default_shbin.s
)

target_include_directories(gbrt PUBLIC
    ${GBRT_DIR}/include
    ${GBRT_DIR}/src
)

target_include_directories(gbrt PRIVATE 
    ${DEVKITPRO}/libctru/include
)

# --- Main executable --------------------------------------------------------
file(GLOB ROM_ROOT_SOURCES "${CMAKE_SOURCE_DIR}/rom*.c")
file(GLOB ROM_FUNC_SOURCES "${CMAKE_SOURCE_DIR}/functions/*.c")

add_executable(rom
    rom_main.c
    ${ROM_ROOT_SOURCES}
    ${ROM_FUNC_SOURCES}
)

target_include_directories(rom PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/functions
    ${GBRT_DIR}/include
)

# Enlazar con las librerías nativas de 3DS (libctru) y matemáticas (libm)
target_link_libraries(rom gbrt citro3d ctru m)