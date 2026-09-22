cmake_minimum_required(VERSION 3.16)
project(sget LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SGET_SOURCES
    log.c
    win2linux.c
    common_socket.c
    sget_thread.c
    metadata.c
    download.c
)

function(configure_sget target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W3)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
    if(WIN32)
        target_compile_definitions(${target} PRIVATE WIN32 _CRT_SECURE_NO_WARNINGS)
        target_link_libraries(${target} PRIVATE ws2_32)
    else()
        find_package(Threads REQUIRED)
        target_compile_definitions(${target} PRIVATE _POSIX_C_SOURCE=200809L _FILE_OFFSET_BITS=64)
        target_link_libraries(${target} PRIVATE Threads::Threads)
    endif()
endfunction()

add_executable(sget ${SGET_SOURCES} utest.c)
configure_sget(sget)

include(CTest)
if(BUILD_TESTING)
    add_executable(sget_core_tests ${SGET_SOURCES} tests/test_core.c)
    target_include_directories(sget_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    configure_sget(sget_core_tests)
    add_test(NAME sget_core COMMAND sget_core_tests)
endif()
