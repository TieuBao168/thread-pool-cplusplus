# Minimal external consumer used by CI to check that tp can be integrated via
#   -DTP_CONSUME_MODE=find_package  (+ CMAKE_PREFIX_PATH=<install prefix>)
#   -DTP_CONSUME_MODE=subdirectory  -DTP_SOURCE_DIR=<repo root>
#   -DTP_CONSUME_MODE=fetchcontent  -DTP_SOURCE_DIR=<repo root>
cmake_minimum_required(VERSION 3.16)
project(tp_consumer LANGUAGES CXX)

set(TP_CONSUME_MODE "find_package" CACHE STRING "find_package | subdirectory | fetchcontent")
set_property(CACHE TP_CONSUME_MODE PROPERTY STRINGS find_package subdirectory fetchcontent)
set(TP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "tp repository root")

if(TP_CONSUME_MODE STREQUAL "find_package")
    find_package(tp 1.0 CONFIG REQUIRED)
elseif(TP_CONSUME_MODE STREQUAL "subdirectory")
    add_subdirectory(${TP_SOURCE_DIR} ${CMAKE_BINARY_DIR}/tp-build EXCLUDE_FROM_ALL)
elseif(TP_CONSUME_MODE STREQUAL "fetchcontent")
    include(FetchContent)
    FetchContent_Declare(tp SOURCE_DIR ${TP_SOURCE_DIR})
    FetchContent_MakeAvailable(tp)
else()
    message(FATAL_ERROR "Unknown TP_CONSUME_MODE '${TP_CONSUME_MODE}'")
endif()

if(NOT TARGET tp::thread_pool)
    message(FATAL_ERROR "tp::thread_pool target not available")
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE tp::thread_pool)

enable_testing()
add_test(NAME consumer COMMAND consumer)
