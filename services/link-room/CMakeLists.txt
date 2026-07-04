cmake_minimum_required(VERSION 3.22)

project(sekailink_link_room LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED IMPORTED_TARGET libmariadb)

set(SEKAILINK_THIRD_PARTY_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../sekailink-worlds/third_party/sekailink_runtime/third_party"
    CACHE PATH "Path containing vendored third-party headers such as nlohmann/json.hpp")

add_library(sekailink_link_room_core STATIC
  src/room_audit_store.cpp
  src/room_linkedworld.cpp
  src/room_projection.cpp
  src/room_projection_factory.cpp
  src/room_projection_mysql.cpp
  src/room_projection_restore.cpp
  src/room_projection_sql.cpp
  src/room_projection_sqlite.cpp
  src/room_registry.cpp
  src/room_restore.cpp
  src/room_retention.cpp
  src/room_seed_package_dispatch.cpp
  src/room_server_config.cpp
  src/room_server_http.cpp
  src/room_server_node.cpp
  src/room_server_protocol.cpp
  src/room_server_tcp.cpp
  src/room_session.cpp
  src/room_state.cpp
)

target_include_directories(sekailink_link_room_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SEKAILINK_THIRD_PARTY_DIR}
  ${MARIADB_INCLUDE_DIRS}
)

target_link_libraries(sekailink_link_room_core PUBLIC
  SQLite::SQLite3
  PkgConfig::MARIADB
)

add_executable(sekailink_room_server_service
  src/room_server_service_main.cpp
)

target_link_libraries(sekailink_room_server_service PRIVATE
  sekailink_link_room_core
)

add_executable(sekailink_room_server_tcp_cli
  src/room_server_tcp_cli_main.cpp
)

target_link_libraries(sekailink_room_server_tcp_cli PRIVATE
  sekailink_link_room_core
)

add_executable(sekailink_room_linkedworld_surface_smoke
  src/room_linkedworld_surface_smoke_main.cpp
)

target_link_libraries(sekailink_room_linkedworld_surface_smoke PRIVATE
  sekailink_link_room_core
)

add_executable(sekailink_room_seed_package_dispatch
  src/room_seed_package_dispatch_main.cpp
)

target_link_libraries(sekailink_room_seed_package_dispatch PRIVATE
  sekailink_link_room_core
)

add_executable(sekailink_room_seed_package_dispatch_smoke
  src/room_seed_package_dispatch_smoke_main.cpp
)

target_link_libraries(sekailink_room_seed_package_dispatch_smoke PRIVATE
  sekailink_link_room_core
)

add_executable(sekailink_room_seed_package_dispatch_tcp_smoke
  src/room_seed_package_dispatch_tcp_smoke_main.cpp
)

target_link_libraries(sekailink_room_seed_package_dispatch_tcp_smoke PRIVATE
  sekailink_link_room_core
)
