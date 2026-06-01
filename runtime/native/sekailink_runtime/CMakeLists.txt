cmake_minimum_required(VERSION 3.24)

project(sekailink_runtime LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(BUILD_QT OFF CACHE BOOL "" FORCE)
set(BUILD_SDL OFF CACHE BOOL "" FORCE)
set(BUILD_LIBRETRO OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENABLE_SCRIPTING OFF CACHE BOOL "" FORCE)
set(USE_LUA OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/submodules/mgba ${CMAKE_CURRENT_BINARY_DIR}/mgba)

file(GLOB_RECURSE GAMBATTE_CORE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/submodules/gambatte/libgambatte/src/*.cpp
)
list(FILTER GAMBATTE_CORE_SOURCES EXCLUDE REGEX "/cinterface\\.cpp$")
list(FILTER GAMBATTE_CORE_SOURCES EXCLUDE REGEX "/file/file_zip\\.cpp$")

add_library(sekaiemu_gba_core STATIC
  src/gba_runtime.cpp
)

add_library(sekaiemu_gambatte_core STATIC
  src/gambatte_runtime.cpp
  ${GAMBATTE_CORE_SOURCES}
)

add_library(sekaiemu_nes_core STATIC
  src/nes_runtime.cpp
  ../../../../third_party/bizhawk-src/quicknes/bizinterface.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/apu.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/fme7/apu_fme7.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/namco/apu_namco.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/vrc6/apu_vrc6.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/vrc7/apu_vrc7.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/vrc7/emu2413.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/vrc7/emu2413_state.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/effectsBuffer.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/blipBuffer.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/NESEffectsBuffer.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/oscs.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/buffer.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/apu/multiBuffer.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/emu.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/mappers/mapper.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/cpu.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/ppu/ppu.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/ppu/ppuRendering.cpp
  ../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core/ppu/ppuImpl.cpp
)

add_library(sekaiemu_snes_core STATIC
  src/snes_runtime.cpp
  ../../../../third_party/bizhawk-src/waterbox/emulibc/emulibc.c
  ../../../../third_party/bizhawk-src/waterbox/libco/amd64.c
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/emulator/emulator.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/processor/spc700/spc700.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/processor/wdc65816/wdc65816.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/processor/arm7tdmi/arm7tdmi.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/interface/interface.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/system/system.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/controller/controller.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/cartridge/cartridge.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/memory/memory.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/cpu/cpu.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/smp/smp.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/dsp/dsp.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/ppu/ppu.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/ppu-fast/ppu.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/expansion/expansion.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/coprocessor/coprocessor.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/sfc/slot/slot.cpp
  ../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes/target-bsnescore/bsnescore.cpp
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/apu.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/camera.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/rumble.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/display.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/gb.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/joypad.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/mbc.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/memory.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/printer.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/random.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/save_state.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/sgb.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/sm83_cpu.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/timing.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/BizInterface.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/blip_buf.c
)

target_include_directories(sekaiemu_gba_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_include_directories(sekaiemu_nes_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/quicknes/core/source/quickerNES/core
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/quicknes/core/extern/jaffarCommon/include
)

target_include_directories(sekaiemu_snes_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/waterbox/emulibc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/waterbox/libco
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/waterbox/bsnescore/bsnes
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core
)

target_include_directories(sekaiemu_gambatte_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/submodules/gambatte/libgambatte/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/submodules/gambatte/libgambatte/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/submodules/gambatte/common
)

target_compile_definitions(sekaiemu_nes_core PRIVATE
  _GNU_SOURCE
  __INLINE__=inline
  _QUICKERNES_DETECT_JOYPAD_READS
  _QUICKERNES_ENABLE_TRACEBACK_SUPPORT
  _QUICKERNES_SUPPORT_ARKANOID_INPUTS
  _QUICKERNES_PRINT_CART_INFO
)

target_compile_definitions(sekaiemu_snes_core PRIVATE
  PROFILE_COMPATIBILITY
  _GNU_SOURCE
  GB_INTERNAL
  GB_DISABLE_DEBUGGER
  GB_DISABLE_CHEATS
  GB_DISABLE_TIMEKEEPING
  GB_DISABLE_REWIND
  GB_VERSION=
)

target_compile_definitions(sekaiemu_gba_core PRIVATE
  $<TARGET_PROPERTY:mgba,COMPILE_DEFINITIONS>
)

target_compile_definitions(sekaiemu_gambatte_core PRIVATE
  HAVE_STDINT_H
)

set_source_files_properties(
  ../../../../third_party/bizhawk-src/waterbox/emulibc/emulibc.c
  ../../../../third_party/bizhawk-src/waterbox/libco/amd64.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/apu.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/camera.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/rumble.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/display.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/gb.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/joypad.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/mbc.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/memory.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/printer.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/random.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/save_state.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/sgb.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/sm83_cpu.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/libsameboy/Core/timing.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/BizInterface.c
  ../../../../third_party/bizhawk-src/submodules/sameboy/blip_buf.c
  PROPERTIES LANGUAGE C
)

target_link_libraries(sekaiemu_gba_core PUBLIC mgba)
target_link_libraries(sekaiemu_gambatte_core PUBLIC z)

add_executable(sekailink_runtime_gba
  src/main.cpp
)

add_executable(sekailink_runtime_snes
  src/main_snes_runtime.cpp
)

add_executable(sekaiemu_gba_sdl
  src/main_sdl.cpp
  src/ui_sdl.cpp
)

add_executable(sekaiemu_gb_sdl
  src/main_gb_sdl.cpp
)

add_executable(sekaiemu_nes_sdl
  src/main_nes_sdl.cpp
)

add_library(sekaiemu_snes_libsnes_core STATIC
  ../../../../third_party/bizhawk-src/waterbox/libsnes/bsnes/target-libsnes/libsnes.cpp
  ../../../../third_party/bizhawk-src/waterbox/libsnes/bsnes/snes/system/system.cpp
  ../../../../third_party/bizhawk-src/waterbox/libsnes/bsnes/gameboy/system/system.cpp
  ../../../../third_party/bizhawk-src/waterbox/emulibc/emulibc.c
  ../../../../third_party/bizhawk-src/waterbox/libco/amd64.c
)

target_include_directories(sekaiemu_snes_libsnes_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/waterbox/libsnes/bsnes
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/waterbox/emulibc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/bizhawk-src/waterbox/libco
)

target_compile_definitions(sekaiemu_snes_libsnes_core PRIVATE
  PROFILE_COMPATIBILITY
  _GNU_SOURCE
  GAMEBOY
)

set_source_files_properties(
  ../../../../third_party/bizhawk-src/waterbox/emulibc/emulibc.c
  ../../../../third_party/bizhawk-src/waterbox/libco/amd64.c
  PROPERTIES LANGUAGE C
)

add_executable(sekaiemu_snes_sdl
  src/main_snes_sdl.cpp
)

add_executable(sekaiemu_gba_audio_probe
  src/audio_probe.cpp
)

target_include_directories(sekailink_runtime_gba PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_include_directories(sekailink_runtime_snes PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_definitions(sekailink_runtime_gba PRIVATE
  $<TARGET_PROPERTY:mgba,COMPILE_DEFINITIONS>
)

target_link_libraries(sekailink_runtime_gba PRIVATE sekaiemu_gba_core)
target_link_libraries(sekailink_runtime_snes PRIVATE sekaiemu_snes_core)

target_include_directories(sekaiemu_gba_sdl PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${SDL2_INCLUDE_DIRS}
)

target_compile_options(sekaiemu_gba_sdl PRIVATE ${SDL2_CFLAGS_OTHER})
target_link_directories(sekaiemu_gba_sdl PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(sekaiemu_gba_sdl PRIVATE sekaiemu_gba_core ${SDL2_LIBRARIES})

target_include_directories(sekaiemu_gb_sdl PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SDL2_INCLUDE_DIRS}
)
target_compile_options(sekaiemu_gb_sdl PRIVATE ${SDL2_CFLAGS_OTHER})
target_link_directories(sekaiemu_gb_sdl PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(sekaiemu_gb_sdl PRIVATE sekaiemu_gambatte_core ${SDL2_LIBRARIES})

target_include_directories(sekaiemu_nes_sdl PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SDL2_INCLUDE_DIRS}
)
target_compile_options(sekaiemu_nes_sdl PRIVATE ${SDL2_CFLAGS_OTHER})
target_link_directories(sekaiemu_nes_sdl PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(sekaiemu_nes_sdl PRIVATE sekaiemu_nes_core ${SDL2_LIBRARIES})

target_include_directories(sekaiemu_snes_sdl PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SDL2_INCLUDE_DIRS}
)
target_compile_options(sekaiemu_snes_sdl PRIVATE ${SDL2_CFLAGS_OTHER})
target_link_directories(sekaiemu_snes_sdl PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(sekaiemu_snes_sdl PRIVATE sekaiemu_snes_libsnes_core ${SDL2_LIBRARIES})

target_include_directories(sekaiemu_gba_audio_probe PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_definitions(sekaiemu_gba_audio_probe PRIVATE
  $<TARGET_PROPERTY:mgba,COMPILE_DEFINITIONS>
)
target_link_libraries(sekaiemu_gba_audio_probe PRIVATE sekaiemu_gba_core)

if(WIN32)
  target_link_libraries(sekailink_runtime_gba PRIVATE ws2_32)
  target_link_libraries(sekailink_runtime_snes PRIVATE ws2_32)
endif()
