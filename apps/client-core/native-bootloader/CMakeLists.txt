cmake_minimum_required(VERSION 3.20)

project(sekailink_native_bootloader LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32 AND EXISTS "/mnt/windows/msys64/ucrt64")
  set(SEKAILINK_MSYS2_UCRT_ROOT "/mnt/windows/msys64/ucrt64" CACHE PATH "MSYS2 UCRT root used for Windows bootloader cross-builds")
  set(BOOTLOADER_PLATFORM_INCLUDES
    ${SEKAILINK_MSYS2_UCRT_ROOT}/include
    ${SEKAILINK_MSYS2_UCRT_ROOT}/include/SDL2
  )
  set(BOOTLOADER_PLATFORM_LIB_DIRS ${SEKAILINK_MSYS2_UCRT_ROOT}/lib)
  set(BOOTLOADER_PLATFORM_LIBS
    mingw32 SDL2main SDL2 curl ssl crypto
    ws2_32 crypt32 bcrypt wldap32 secur32 iphlpapi
    winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32
  )
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
  find_package(OpenSSL REQUIRED)
  set(BOOTLOADER_PLATFORM_LIBS PkgConfig::SDL2 PkgConfig::CURL OpenSSL::Crypto)
endif()

add_library(miniz STATIC
  third_party/miniz/miniz.c
  third_party/miniz/miniz_tdef.c
  third_party/miniz/miniz_tinfl.c
  third_party/miniz/miniz_zip.c
)
target_include_directories(miniz PUBLIC third_party/miniz)
target_compile_definitions(miniz PRIVATE _LARGEFILE64_SOURCE=1)

set(SEKAILINK_BOOTLOADER_SOURCES src/main.cpp)

if(WIN32)
  enable_language(RC)
  list(APPEND SEKAILINK_BOOTLOADER_SOURCES resources/sekailink.rc)
endif()

add_executable(sekailink_bootloader
  ${SEKAILINK_BOOTLOADER_SOURCES}
)

target_include_directories(sekailink_bootloader PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../runtime/native/sekailink_runtime/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/font8x8
  ${BOOTLOADER_PLATFORM_INCLUDES}
)

target_link_directories(sekailink_bootloader PRIVATE ${BOOTLOADER_PLATFORM_LIB_DIRS})

target_link_libraries(sekailink_bootloader PRIVATE
  miniz
  ${BOOTLOADER_PLATFORM_LIBS}
)

if(WIN32)
  set_target_properties(sekailink_bootloader PROPERTIES
    OUTPUT_NAME "SekaiLink Bootloader"
    WIN32_EXECUTABLE TRUE
  )
else()
  set_target_properties(sekailink_bootloader PROPERTIES OUTPUT_NAME "sekailink-bootloader")
endif()
