cmake_minimum_required(VERSION 3.20)

project(sekaiemu_libretro_spike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SEKAIEMU_LIBRETRO_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/libretro/include")

if(NOT EXISTS "${SEKAIEMU_LIBRETRO_INCLUDE_DIR}/libretro.h")
  message(FATAL_ERROR
    "Missing vendored libretro API header at ${SEKAIEMU_LIBRETRO_INCLUDE_DIR}/libretro.h")
endif()

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(BZip2 REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
link_directories(${SDL2_LIBRARY_DIRS} ${SDL2_IMAGE_LIBRARY_DIRS} ${LIBARCHIVE_LIBRARY_DIRS})
if(NOT WIN32)
  find_program(BASH_EXECUTABLE bash REQUIRED)
endif()

set(SEKAIEMU_PLATFORM_LIBS)
if(WIN32)
  list(APPEND SEKAIEMU_PLATFORM_LIBS ws2_32)
else()
  list(APPEND SEKAIEMU_PLATFORM_LIBS dl)
endif()

set(SEKAIEMU_TRACKER_RUNTIME_SOURCES
  src/tracker_bundle_archive.cpp
  src/tracker_bundle_loader.cpp
  src/tracker_bundle_poptracker_legacy.cpp
  src/tracker_raster_image.cpp
  src/tracker_runtime_ingestion.cpp
  src/tracker_runtime.cpp
  src/tracker_runtime_persistence.cpp
  src/tracker_runtime_snapshot_values.cpp
  src/tracker_runtime_view_state.cpp
)

set(SEKAIEMU_TRACKER_RENDER_SOURCES
  src/tracker_pack_layout_document.cpp
  src/tracker_pack_layout_engine.cpp
  src/tracker_pack_layout_json.cpp
  src/tracker_pack_layout_resolution.cpp
  src/tracker_pack_layout_interaction.cpp
  src/tracker_pack_layout_renderer.cpp
  src/tracker_pack_layout_visual_state.cpp
  src/tracker_pack_layout_widgets.cpp
  ${SEKAIEMU_TRACKER_RUNTIME_SOURCES}
  src/tracker_overlay_detail_sections.cpp
  src/tracker_overlay_json.cpp
  src/tracker_map_context_menu.cpp
  src/tracker_pin_context_menu.cpp
  src/tracker_overlay_map_sections.cpp
  src/tracker_overlay_pack_metadata.cpp
  src/tracker_overlay_render_state.cpp
  src/tracker_overlay_session_state.cpp
  src/tracker_overlay_snapshot_helpers.cpp
  src/tracker_overlay_renderer.cpp
  src/tracker_overlay_style.cpp
)

add_executable(sekaiemu_libretro_spike
  src/main.cpp
  src/audio_output.cpp
  src/bridge_adapters/alttp_adapter.cpp
  src/bridge_adapters/firered_adapter.cpp
  src/bridge_adapters/ladx_adapter.cpp
  src/bridge_adapters/oot_adapter.cpp
  src/bridge_adapters/tloz_adapter.cpp
  src/bridge_ipc.cpp
  src/core_option_manager.cpp
  src/core_option_manager_utils.cpp
  src/host_io_utils.cpp
  src/input_script.cpp
  src/input_state.cpp
  src/input_state_config.cpp
  src/input_state_menu.cpp
  src/launch_options.cpp
  src/launcher.cpp
  src/legacy_bridge_commands.cpp
  src/legacy_bridge_watchers.cpp
  src/libretro_bridge_host_helpers.cpp
  src/libretro_environment.cpp
  src/bridge_terminal_presenter.cpp
  src/libretro_frame_automation.cpp
  src/libretro_frame_dump.cpp
  src/libretro_host_actions.cpp
  src/libretro_host_callbacks.cpp
  src/libretro_host_chat_overlay.cpp
  src/libretro_host_tracker_interactions.cpp
  src/libretro_host_frontend_settings.cpp
  src/libretro_host_lifecycle.cpp
  src/libretro_core_loader.cpp
  src/libretro_host.cpp
  src/libretro_io_callbacks.cpp
  src/libretro_menu_actions.cpp
  src/libretro_menu_presentation.cpp
  src/libretro_memory_tools.cpp
  src/libretro_host_runloop.cpp
  src/libretro_save_host_helpers.cpp
  src/libretro_tracker_host_state.cpp
  src/libretro_tracker_metadata_pump.cpp
  src/libretro_tracker_presentation.cpp
  src/libretro_video_host_helpers.cpp
  src/libretro_session.cpp
  src/libretro_state_slot_actions.cpp
  src/logger.cpp
  src/memory_domain_registry.cpp
  src/memory_profile.cpp
  src/overlay_canvas.cpp
  src/profile_bridge.cpp
  src/patch_materializer.cpp
  src/opengl_loader.cpp
  src/opengl_overlay_renderer.cpp
  src/opengl_video_utils.cpp
  src/runtime_chat_bridge.cpp
  src/runtime_memory_server.cpp
  src/runtime_chat_overlay.cpp
  src/runtime_frontend_settings.cpp
  src/runtime_loop.cpp
  src/runtime_menu.cpp
  src/runtime_menu_core_settings.cpp
  src/runtime_menu_controls.cpp
  src/runtime_menu_main_page.cpp
  src/runtime_menu_save_slots.cpp
  src/runtime_menu_settings.cpp
  src/runtime_menu_state.cpp
  src/runtime_menu_sync.cpp
  src/runtime_menu_tabs.cpp
  src/save_state_manager.cpp
  src/sklmi_bridge_registry.cpp
  src/sklmi_companion_runtime.cpp
  src/tracker_asset_resolver.cpp
  ${SEKAIEMU_TRACKER_RENDER_SOURCES}
  src/tracker_snapshot_io.cpp
  src/tracker_window_presenter.cpp
  src/software_video_backend.cpp
  src/opengl_video_backend.cpp
  src/vulkan_video_backend.cpp
)

target_include_directories(sekaiemu_libretro_spike
  PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${SEKAIEMU_LIBRETRO_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
)

target_compile_options(sekaiemu_libretro_spike PRIVATE ${SDL2_CFLAGS_OTHER} ${SDL2_IMAGE_CFLAGS_OTHER} ${LIBARCHIVE_CFLAGS_OTHER})

target_link_libraries(sekaiemu_libretro_spike
  PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    OpenSSL::Crypto
    BZip2::BZip2
    OpenGL::GL
    ${SEKAIEMU_PLATFORM_LIBS}
)

enable_testing()

if(BASH_EXECUTABLE)
  add_test(
    NAME sekaiemu_golden_earthbound
    COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/earthbound_golden.sh
            ${CMAKE_CURRENT_SOURCE_DIR}
  )

  add_test(
    NAME sekaiemu_golden_alttp
    COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/alttp_golden.sh
            ${CMAKE_CURRENT_SOURCE_DIR}
  )
endif()

add_executable(tracker_runtime_smoke
  src/tracker_runtime_smoke.cpp
  ${SEKAIEMU_TRACKER_RUNTIME_SOURCES}
)

target_include_directories(tracker_runtime_smoke
  PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${SEKAIEMU_LIBRETRO_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
)

target_compile_options(tracker_runtime_smoke PRIVATE ${SDL2_CFLAGS_OTHER} ${SDL2_IMAGE_CFLAGS_OTHER} ${LIBARCHIVE_CFLAGS_OTHER})

target_link_libraries(tracker_runtime_smoke
  PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    ${SEKAIEMU_PLATFORM_LIBS}
)

add_test(
  NAME sekaiemu_tracker_runtime_smoke
  COMMAND tracker_runtime_smoke
)

add_executable(tracker_overlay_renderer_smoke
  src/tracker_overlay_renderer_smoke.cpp
  ${SEKAIEMU_TRACKER_RENDER_SOURCES}
  src/overlay_canvas.cpp
)

target_include_directories(tracker_overlay_renderer_smoke
  PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${SEKAIEMU_LIBRETRO_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
)

target_compile_options(tracker_overlay_renderer_smoke PRIVATE ${SDL2_CFLAGS_OTHER} ${SDL2_IMAGE_CFLAGS_OTHER} ${LIBARCHIVE_CFLAGS_OTHER})

target_link_libraries(tracker_overlay_renderer_smoke
  PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    ${SEKAIEMU_PLATFORM_LIBS}
)

add_test(
  NAME sekaiemu_tracker_overlay_renderer_smoke
  COMMAND tracker_overlay_renderer_smoke
)

add_executable(tracker_alttp_bundle_smoke
  src/tracker_alttp_bundle_smoke.cpp
  ${SEKAIEMU_TRACKER_RENDER_SOURCES}
  src/overlay_canvas.cpp
)

target_include_directories(tracker_alttp_bundle_smoke
  PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${SEKAIEMU_LIBRETRO_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
)

target_compile_options(tracker_alttp_bundle_smoke PRIVATE ${SDL2_CFLAGS_OTHER} ${SDL2_IMAGE_CFLAGS_OTHER} ${LIBARCHIVE_CFLAGS_OTHER})

target_link_libraries(tracker_alttp_bundle_smoke
  PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    ${SEKAIEMU_PLATFORM_LIBS}
)

add_test(
  NAME sekaiemu_tracker_alttp_bundle_smoke
  COMMAND tracker_alttp_bundle_smoke ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(tracker_alttp_native_bundle_smoke
  src/tracker_alttp_native_bundle_smoke.cpp
  ${SEKAIEMU_TRACKER_RENDER_SOURCES}
  src/overlay_canvas.cpp
)

target_include_directories(tracker_alttp_native_bundle_smoke
  PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${SEKAIEMU_LIBRETRO_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
)

target_compile_options(tracker_alttp_native_bundle_smoke PRIVATE ${SDL2_CFLAGS_OTHER} ${SDL2_IMAGE_CFLAGS_OTHER} ${LIBARCHIVE_CFLAGS_OTHER})

target_link_libraries(tracker_alttp_native_bundle_smoke
  PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    ${SEKAIEMU_PLATFORM_LIBS}
)

add_test(
  NAME sekaiemu_tracker_alttp_native_bundle_smoke
  COMMAND tracker_alttp_native_bundle_smoke ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(tracker_preview_render
  src/tracker_preview_render.cpp
  ${SEKAIEMU_TRACKER_RENDER_SOURCES}
  src/overlay_canvas.cpp
)

target_include_directories(tracker_preview_render
  PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${SEKAIEMU_LIBRETRO_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
)

target_compile_options(tracker_preview_render PRIVATE ${SDL2_CFLAGS_OTHER} ${SDL2_IMAGE_CFLAGS_OTHER} ${LIBARCHIVE_CFLAGS_OTHER})

target_link_libraries(tracker_preview_render
  PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    ${SEKAIEMU_PLATFORM_LIBS}
)

add_test(
  NAME sekaiemu_tracker_preview_render_smoke
  COMMAND tracker_preview_render
          ${CMAKE_CURRENT_SOURCE_DIR}/tracker-bundles/alttp-default
          /tmp/sekaiemu-tracker-preview-smoke/alttp-default.ppm
          960
          540
          desert
          desert_palace
)

add_executable(tracker_snapshot_io_smoke
  src/tracker_snapshot_io_smoke.cpp
  src/tracker_snapshot_io.cpp
)

target_include_directories(tracker_snapshot_io_smoke
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
)

add_test(
  NAME sekaiemu_tracker_snapshot_io_smoke
  COMMAND tracker_snapshot_io_smoke
)

add_executable(save_state_manager_smoke
  src/save_state_manager_smoke.cpp
  src/save_state_manager.cpp
  src/host_io_utils.cpp
)

target_include_directories(save_state_manager_smoke
  PRIVATE
    ${SEKAIEMU_LIBRETRO_INCLUDE_DIR}
)

add_test(
  NAME sekaiemu_save_state_manager_smoke
  COMMAND save_state_manager_smoke
)

add_executable(runtime_frontend_settings_smoke
  src/runtime_frontend_settings_smoke.cpp
  src/runtime_frontend_settings.cpp
)

target_include_directories(runtime_frontend_settings_smoke
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
)

add_test(
  NAME sekaiemu_runtime_frontend_settings_smoke
  COMMAND runtime_frontend_settings_smoke
)

add_executable(input_script_smoke
  src/input_script_smoke.cpp
  src/input_script.cpp
)

target_include_directories(input_script_smoke
  PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SEKAIEMU_LIBRETRO_INCLUDE_DIR}
)

target_compile_options(input_script_smoke PRIVATE ${SDL2_CFLAGS_OTHER})

target_link_libraries(input_script_smoke
  PRIVATE
    ${SDL2_LIBRARIES}
)

add_test(
  NAME sekaiemu_input_script_smoke
  COMMAND input_script_smoke
)

add_executable(launch_options_smoke
  src/launch_options_smoke.cpp
  src/launch_options.cpp
)

target_include_directories(launch_options_smoke
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
)

add_test(
  NAME sekaiemu_launch_options_smoke
  COMMAND launch_options_smoke
)
