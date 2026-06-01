cmake_minimum_required(VERSION 3.22)

project(sekailink_server_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_check_modules(MARIADB REQUIRED IMPORTED_TARGET libmariadb)
find_library(ARGON2_LIBRARY argon2 REQUIRED)

set(SEKAILINK_RUNTIME_THIRD_PARTY_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/sekailink_runtime/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../sekailink-core/source-snapshots/runtime-reference/native/sekailink_runtime/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../servers/worlds/third_party/sekailink_runtime/third_party
)

set(SEKAILINK_RUNTIME_THIRD_PARTY_INCLUDE_DIRS)
foreach(candidate ${SEKAILINK_RUNTIME_THIRD_PARTY_DIRS})
  if(EXISTS ${candidate}/nlohmann/json.hpp)
    list(APPEND SEKAILINK_RUNTIME_THIRD_PARTY_INCLUDE_DIRS ${candidate})
  endif()
endforeach()

if(NOT SEKAILINK_RUNTIME_THIRD_PARTY_INCLUDE_DIRS)
  message(FATAL_ERROR "nlohmann/json.hpp was not found. Vendor sekailink_runtime/third_party into sekailink-worlds/third_party or provide the clean-room runtime reference.")
endif()

add_library(sekailink_server_core STATIC
  src/admin_agent.cpp
  src/evolution_room_query_service.cpp
  src/game_server_http.cpp
  src/game_room_projection.cpp
  src/game_save_state.cpp
  src/game_server_protocol.cpp
  src/game_session.cpp
  src/game_server_tcp.cpp
  src/game_world.cpp
  src/generic_generation.cpp
  src/generic_generation_checks.cpp
  src/generic_generation_internal.cpp
  src/generic_generation_logic_rules.cpp
  src/generic_generation_patch_contracts.cpp
  src/generic_generation_placement.cpp
  src/generic_generation_region_analysis.cpp
  src/generic_generation_region_graph.cpp
  src/generic_generation_surface_contracts.cpp
  src/generation_service.cpp
  src/generation_server.cpp
  src/identity_service.cpp
  src/lobby_admin_service.cpp
  src/lobby_runtime_service.cpp
  src/room_projection_query.cpp
  src/room_state.cpp
  src/room_session.cpp
  src/room_registry.cpp
  src/room_audit_store.cpp
  src/room_server_protocol.cpp
  src/multiserver_semantics.cpp
  src/room_server_tcp.cpp
  src/room_server_http.cpp
  src/room_server_config.cpp
  src/room_server_node.cpp
  src/room_restore.cpp
  src/room_retention.cpp
  src/room_projection.cpp
  src/room_projection_restore.cpp
  src/room_projection_sqlite.cpp
  src/room_projection_mysql.cpp
  src/room_projection_factory.cpp
  src/room_projection_sql.cpp
)

target_include_directories(sekailink_server_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SEKAILINK_RUNTIME_THIRD_PARTY_INCLUDE_DIRS}
  ${SQLite3_INCLUDE_DIRS}
  ${MARIADB_INCLUDE_DIRS}
)

target_link_libraries(sekailink_server_core PUBLIC
  SQLite::SQLite3
  OpenSSL::Crypto
  OpenSSL::SSL
  PkgConfig::MARIADB
  ${ARGON2_LIBRARY}
)

add_executable(sekailink_server_room_state_smoke
  src/room_state_smoke_main.cpp
)

target_link_libraries(sekailink_server_room_state_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_server_room_state_perf_smoke
  src/room_state_perf_smoke_main.cpp
)

target_link_libraries(sekailink_server_room_state_perf_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_server_room_session_smoke
  src/room_session_smoke_main.cpp
)

target_link_libraries(sekailink_server_room_session_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_daemon
  src/room_server_main.cpp
)

target_link_libraries(sekailink_room_server_daemon PRIVATE
  sekailink_server_core
)

add_executable(sekailink_game_server_daemon
  src/game_server_main.cpp
)

target_link_libraries(sekailink_game_server_daemon PRIVATE
  sekailink_server_core
)

add_executable(sekailink_game_server_service
  src/game_server_service_main.cpp
)

target_link_libraries(sekailink_game_server_service PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_service
  src/room_server_service_main.cpp
)

target_link_libraries(sekailink_room_server_service PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_tcp_cli
  src/room_server_tcp_cli_main.cpp
)

target_link_libraries(sekailink_room_server_tcp_cli PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_projection_seed
  src/room_projection_seed_main.cpp
)

target_link_libraries(sekailink_room_projection_seed PRIVATE
  sekailink_server_core
)

add_executable(sekailink_admin_agent_service
  src/admin_agent_service_main.cpp
)

target_link_libraries(sekailink_admin_agent_service PRIVATE
  sekailink_server_core
)

add_executable(sekailink_generation_server_service
  src/generation_server_service_main.cpp
)

target_link_libraries(sekailink_generation_server_service PRIVATE
  sekailink_server_core
)

add_executable(sekailink_evolution_room_query_service
  src/evolution_room_query_service_main.cpp
)

target_link_libraries(sekailink_evolution_room_query_service PRIVATE
  sekailink_server_core
)

add_executable(sekailink_identity_service
  src/identity_service_main.cpp
)

target_link_libraries(sekailink_identity_service PRIVATE
  sekailink_server_core
)

add_executable(sekailink_lobby_admin_service
  src/lobby_admin_service_main.cpp
)

target_link_libraries(sekailink_lobby_admin_service PRIVATE
  sekailink_server_core
)

add_executable(sekailink_lobby_runtime_service
  src/lobby_runtime_service_main.cpp
)

target_link_libraries(sekailink_lobby_runtime_service PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_restore_daemon_smoke
  src/room_server_restore_daemon_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_restore_daemon_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_projection_restore_daemon_smoke
  src/room_server_projection_restore_daemon_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_projection_restore_daemon_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_projection_restore_mysql_daemon_smoke
  src/room_server_projection_restore_mysql_daemon_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_projection_restore_mysql_daemon_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_retention_smoke
  src/room_retention_smoke_main.cpp
)

target_link_libraries(sekailink_room_retention_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_server_room_audit_smoke
  src/room_audit_smoke_main.cpp
)

target_link_libraries(sekailink_server_room_audit_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_audit_daemon_smoke
  src/room_server_audit_daemon_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_audit_daemon_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_protocol_smoke
  src/room_server_protocol_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_protocol_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_protocol_auth_smoke
  src/room_server_protocol_auth_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_protocol_auth_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_multiserver_semantics_smoke
  src/multiserver_semantics_smoke_main.cpp
)

target_link_libraries(sekailink_multiserver_semantics_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_game_world_package_smoke
  src/game_world_package_smoke_main.cpp
)

target_link_libraries(sekailink_game_world_package_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_game_save_state_smoke
  src/game_save_state_smoke_main.cpp
)

target_link_libraries(sekailink_game_save_state_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_game_session_smoke
  src/game_session_smoke_main.cpp
)

target_link_libraries(sekailink_game_session_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_game_server_protocol_smoke
  src/game_server_protocol_smoke_main.cpp
)

target_link_libraries(sekailink_game_server_protocol_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_game_server_http_smoke
  src/game_server_http_smoke_main.cpp
)

target_link_libraries(sekailink_game_server_http_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_game_server_tcp_smoke
  src/game_server_tcp_smoke_main.cpp
)

target_link_libraries(sekailink_game_server_tcp_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_game_room_projection_smoke
  src/game_room_projection_smoke_main.cpp
)

target_link_libraries(sekailink_game_room_projection_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_tcp_smoke
  src/room_server_tcp_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_tcp_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_tcp_loop_smoke
  src/room_server_tcp_loop_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_tcp_loop_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_tcp_timeout_smoke
  src/room_server_tcp_timeout_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_tcp_timeout_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_tcp_auth_smoke
  src/room_server_tcp_auth_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_tcp_auth_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_http_smoke
  src/room_server_http_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_http_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_http_auth_smoke
  src/room_server_http_auth_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_http_auth_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_node_smoke
  src/room_server_node_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_node_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_service_smoke
  src/room_server_service_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_service_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_admin_agent_smoke
  src/admin_agent_smoke_main.cpp
)

target_link_libraries(sekailink_admin_agent_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_admin_agent_service_smoke
  src/admin_agent_service_smoke_main.cpp
)

target_link_libraries(sekailink_admin_agent_service_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_generation_service_smoke
  src/generation_service_smoke_main.cpp
)

target_link_libraries(sekailink_generation_service_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_generation_server_smoke
  src/generation_server_smoke_main.cpp
)

target_link_libraries(sekailink_generation_server_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_generation_server_service_smoke
  src/generation_server_service_smoke_main.cpp
)

target_link_libraries(sekailink_generation_server_service_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_generic_generation_smoke
  src/generic_generation_smoke_main.cpp
)

target_link_libraries(sekailink_generic_generation_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_generic_generation_probe
  src/generic_generation_probe_main.cpp
)

target_link_libraries(sekailink_generic_generation_probe PRIVATE
  sekailink_server_core
)

add_executable(sekailink_generic_generation_package
  src/generic_generation_package_main.cpp
)

target_link_libraries(sekailink_generic_generation_package PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_projection_query_smoke
  src/room_projection_query_smoke_main.cpp
)

target_link_libraries(sekailink_room_projection_query_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_evolution_room_query_smoke
  src/evolution_room_query_smoke_main.cpp
)

target_link_libraries(sekailink_evolution_room_query_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_evolution_room_query_service_smoke
  src/evolution_room_query_service_smoke_main.cpp
)

target_link_libraries(sekailink_evolution_room_query_service_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_identity_service_smoke
  src/identity_service_smoke_main.cpp
)

target_link_libraries(sekailink_identity_service_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_lobby_admin_service_smoke
  src/lobby_admin_service_smoke_main.cpp
)

target_link_libraries(sekailink_lobby_admin_service_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_lobby_runtime_service_smoke
  src/lobby_runtime_service_smoke_main.cpp
)

target_link_libraries(sekailink_lobby_runtime_service_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_server_http_tcp_smoke
  src/room_server_http_tcp_smoke_main.cpp
)

target_link_libraries(sekailink_room_server_http_tcp_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_restore_smoke
  src/room_restore_smoke_main.cpp
)

target_link_libraries(sekailink_room_restore_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_restore_manifest_smoke
  src/room_restore_manifest_smoke_main.cpp
)

target_link_libraries(sekailink_room_restore_manifest_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_projection_smoke
  src/room_projection_smoke_main.cpp
)

target_link_libraries(sekailink_room_projection_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_projection_restore_smoke
  src/room_projection_restore_smoke_main.cpp
)

target_link_libraries(sekailink_room_projection_restore_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_projection_sqlite_smoke
  src/room_projection_sqlite_smoke_main.cpp
)

target_link_libraries(sekailink_room_projection_sqlite_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_projection_factory_smoke
  src/room_projection_factory_smoke_main.cpp
)

target_link_libraries(sekailink_room_projection_factory_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_projection_sql_smoke
  src/room_projection_sql_smoke_main.cpp
)

target_link_libraries(sekailink_room_projection_sql_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_projection_mysql_smoke
  src/room_projection_mysql_smoke_main.cpp
)

target_link_libraries(sekailink_room_projection_mysql_smoke PRIVATE
  sekailink_server_core
)

add_executable(sekailink_room_projection_restore_mysql_smoke
  src/room_projection_restore_mysql_smoke_main.cpp
)

target_link_libraries(sekailink_room_projection_restore_mysql_smoke PRIVATE
  sekailink_server_core
)

if(WIN32)
  target_link_libraries(sekailink_server_core PUBLIC ws2_32)
endif()
