cmake_minimum_required(VERSION 3.20)

project(EChat LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# ===============================
# Dependencies
# ===============================

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JEMALLOC REQUIRED jemalloc)

# ===============================
# libsodium
# ===============================

set(SODIUM_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/libsodium/libsodium-stable)

set(SODIUM_INCLUDE_DIR
    ${SODIUM_ROOT}/src/libsodium/include
)

set(SODIUM_LIBRARY
    ${SODIUM_ROOT}/src/libsodium/.libs/libsodium.a
)

# ===============================
# Libraries
# ===============================

# MENU
add_library(CliMenu
    "cli_client/src/menu.c"
    "cli_client/src/terminal.c"
    "cli_client/include/menu.h"
    "cli_client/include/terminal.h"
)

target_include_directories(CliMenu
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/cli_client/include
)

# CLIENT CORE
add_library(ClientCore
    "client_core/src/client_core.c"
    "client_core/include/client_core.h"
    "client_core/include/cross_platform_api.h"
    "client_core/include/TLS.h"
)

target_include_directories(ClientCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/client_core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
        ${SODIUM_INCLUDE_DIR}
)

target_link_libraries(ClientCore
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        #${SODIUM_LIBRARY}
)

# ===============================
# SERVER (Linux only)
# ===============================

if(UNIX AND NOT WIN32)

    add_executable(server
        "server/server.c"
        "server/src/server_core.c"
        "server/src/event_loop.c"
    )

    target_include_directories(server
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/server/include
            ${CMAKE_CURRENT_SOURCE_DIR}/utils
            ${JEMALLOC_INCLUDE_DIRS}
    )
    target_link_directories(server PRIVATE ${JEMALLOC_LIBRARY_DIRS})

    target_link_libraries(server
        PRIVATE
            OpenSSL::SSL
            OpenSSL::Crypto
            ${JEMALLOC_LIBRARIES}
    )

endif()

# ===============================
# CLIENT (Windows + Linux)
# ===============================

add_executable(EChat_cli
    "cli_client/EChat_cli.c"
)

target_include_directories(EChat_cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_link_libraries(EChat_cli
    PRIVATE
        CliMenu
        ClientCore
)

# Windows-specific networking
if(WIN32)
    target_link_libraries(EChat_cli PRIVATE ws2_32)
endif()

# ===============================
# TESTS
# ===============================

option(ENABLE_TESTS "Build test executables" ON)

function(add_echat_test name)
    add_executable(${name} ${ARGN})

    target_include_directories(${name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/utils
    )

    target_link_libraries(${name}
        PRIVATE
            CliMenu
            #OpenSSL::SSL
            #OpenSSL::Crypto
    )

    if(WIN32)
        target_link_libraries(${name} PRIVATE ws2_32)
    endif()
endfunction()

if(ENABLE_TESTS)

    add_echat_test(test_menu
        tests/test_menu.c
    )

    add_echat_test(test_queue
        tests/test_queue.c
    )

endif()
