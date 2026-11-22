cmake_minimum_required(VERSION 3.16)

project(tp_thread_pool
    VERSION 1.1.0
    DESCRIPTION "Configurable C++17 thread pool"
    LANGUAGES CXX)

# ---- Options ---------------------------------------------------------------

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(TP_IS_TOP_LEVEL ON)
else()
    set(TP_IS_TOP_LEVEL OFF)
endif()

option(TP_BUILD_TESTS        "Build the tp unit tests"         ${TP_IS_TOP_LEVEL})
option(TP_BUILD_EXAMPLES     "Build the tp examples"           ${TP_IS_TOP_LEVEL})
option(TP_BUILD_BENCHMARKS   "Build the tp benchmarks"         ${TP_IS_TOP_LEVEL})
option(TP_INSTALL            "Generate the tp install target"  ${TP_IS_TOP_LEVEL})
option(TP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(TP_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(BUILD_SHARED_LIBS AND WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

find_package(Threads REQUIRED)

# Warnings for our own targets only (PRIVATE, so they never leak to consumers).
function(tp_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(TP_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(TP_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---- Library ---------------------------------------------------------------

add_library(tp_thread_pool
    src/config.cpp
    src/thread_pool.cpp
    src/platform.cpp)
add_library(tp::thread_pool ALIAS tp_thread_pool)

target_compile_features(tp_thread_pool PUBLIC cxx_std_17)
target_include_directories(tp_thread_pool
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tp_thread_pool PUBLIC Threads::Threads)
set_target_properties(tp_thread_pool PROPERTIES
    EXPORT_NAME thread_pool
    OUTPUT_NAME tp_thread_pool
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_EXTENSIONS OFF)
tp_set_warnings(tp_thread_pool)

# ---- Tests -----------------------------------------------------------------

if(TP_BUILD_TESTS)
    enable_testing()
    foreach(test_name IN ITEMS test_config test_thread_pool)
        add_executable(tp_${test_name} tests/${test_name}.cpp)
        target_link_libraries(tp_${test_name} PRIVATE tp::thread_pool)
        target_include_directories(tp_${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        set_target_properties(tp_${test_name} PROPERTIES CXX_EXTENSIONS OFF)
        tp_set_warnings(tp_${test_name})
        add_test(NAME ${test_name} COMMAND tp_${test_name})
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 120)
    endforeach()
endif()

# ---- Examples --------------------------------------------------------------

if(TP_BUILD_EXAMPLES)
    add_executable(tp_example_basic examples/basic/main.cpp)
    target_link_libraries(tp_example_basic PRIVATE tp::thread_pool)
    tp_set_warnings(tp_example_basic)

    add_executable(tp_pusher_demo
        examples/pusher_demo/main.cpp
        examples/pusher_demo/common_task.cpp)
    target_include_directories(tp_pusher_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples/pusher_demo)
    target_link_libraries(tp_pusher_demo PRIVATE tp::thread_pool)
    tp_set_warnings(tp_pusher_demo)
    # Ship a ready-to-use config next to the demo binary (works for multi-config too).
    add_custom_command(TARGET tp_pusher_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/config/threadpool.ini.example
                $<TARGET_FILE_DIR:tp_pusher_demo>/threadpool.ini
        VERBATIM)
endif()

# ---- Benchmarks ------------------------------------------------------------

if(TP_BUILD_BENCHMARKS)
    add_executable(tp_bench_throughput benchmarks/bench_throughput.cpp)
    target_link_libraries(tp_bench_throughput PRIVATE tp::thread_pool)
    tp_set_warnings(tp_bench_throughput)
endif()

# ---- Install / package -----------------------------------------------------

if(TP_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(TP_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/tp)

    install(TARGETS tp_thread_pool
        EXPORT tpTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY include/tp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp")
    install(EXPORT tpTargets
        FILE tpTargets.cmake
        NAMESPACE tp::
        DESTINATION ${TP_INSTALL_CMAKEDIR})

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tpConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/tpConfig.cmake
        INSTALL_DESTINATION ${TP_INSTALL_CMAKEDIR})
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/tpConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/tpConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/tpConfigVersion.cmake
        DESTINATION ${TP_INSTALL_CMAKEDIR})

    install(FILES config/threadpool.ini.example
        DESTINATION ${CMAKE_INSTALL_DATADIR}/tp)
endif()
