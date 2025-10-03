cmake_minimum_required(VERSION 3.16)
project(CoreCDTL LANGUAGES C CXX)

# ————— LLVM Path and Configuration (Robust for Local and CI) —————

# If LLVM_HOME is not set (e.g., in CI environments), try to find llvm-config in PATH
if(NOT DEFINED LLVM_HOME)
    find_program(LLVM_CONFIG_EXE llvm-config)
    if(LLVM_CONFIG_EXE)
        get_filename_component(LLVM_HOME_TEMP "${LLVM_CONFIG_EXE}" DIRECTORY)
        # Move up one level to get the LLVM_HOME path (/usr/bin -> /usr, or /path/bin -> /path)
        get_filename_component(LLVM_HOME "${LLVM_HOME_TEMP}" DIRECTORY)
    endif()
    if(NOT LLVM_CONFIG_EXE)
        message(FATAL_ERROR "Could not find 'llvm-config' in PATH. Please install LLVM or set LLVM_HOME.")
    endif()
endif()

# ————— C and C++ Standards —————
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ————— Options —————
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_TOOLS "Build CLI tools" ON)
option(ENABLE_LOG_INFO  "Enable INFO log messages" ON)
option(ENABLE_LOG_WARN  "Enable WARN log messages" ON)

# ————— Warnings —————
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)

# ————— Log Definitions —————
add_compile_definitions(CORE_LOG_INFO CORE_LOG_WARN CORE_LOG_ERROR)

# ————— LLVM Config Flags —————
# 1. Get CXX Flags (Includes -std=c++17, -stdlib=libc++, and -I paths)
execute_process(
        COMMAND ${LLVM_HOME}/bin/llvm-config --cxxflags
        OUTPUT_VARIABLE LLVM_CXXFLAGS_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 2. Get Include Dir (Only the header path, safe for C files)
execute_process(
        COMMAND ${LLVM_HOME}/bin/llvm-config --includedir
        OUTPUT_VARIABLE LLVM_INCLUDE_DIR_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 3. Get Linker Flags (Libraries and system dependencies)
execute_process(
        COMMAND ${LLVM_HOME}/bin/llvm-config --ldflags --system-libs --libs orcjit native
        OUTPUT_VARIABLE LLVM_LDFLAGS_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

separate_arguments(LLVM_CXXFLAGS_LIST UNIX_COMMAND "${LLVM_CXXFLAGS_RAW}")
separate_arguments(LLVM_LDFLAGS_LIST UNIX_COMMAND "${LLVM_LDFLAGS_RAW}")

# Combine include path with -I flag
set(LLVM_INCLUDE_FLAG "-I${LLVM_INCLUDE_DIR_RAW}")

# ——————————————————————
# corecdtl (shared library - C/C++ mix, requires only LLVM includes)
# ——————————————————————
add_library(corecdtl SHARED
        src/core/cored.c
        src/core/log.c
        src/core/event_bus.c
        src/core/scheduler.c
        src/core/heapkit.c
        src/core/runtime.c
        src/plugin/plugin_loader.c
        src/plugin/plugin_manager.c
        src/plugin/plugin_stub.c
        src/utils/utils_string.c
        src/core/crash_recovery.c
        src/core/core_ctl.c
        src/gateway/gateway.c
        src/jit/llvm_jit.c
        src/gateway/gateway_dispatcher.c
        src/gateway/gateway_dispatcher.h
)

target_include_directories(corecdtl PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${LLVM_INCLUDE_FLAG} # Add LLVM headers without C++ flags
)

target_link_libraries(corecdtl PRIVATE Threads::Threads)
if(APPLE)
    target_link_options(corecdtl PRIVATE ${LLVM_LDFLAGS_LIST} -lc++)
else()
    target_link_options(corecdtl PRIVATE ${LLVM_LDFLAGS_LIST})
endif()

# ——————————————————————
# core_test (executable - C/C++ mix, requires only LLVM includes)
# ——————————————————————
add_executable(core_test
        src/core/core_test.c
        src/core/log.c
        src/core/event_bus.c
        src/core/scheduler.c
        src/core/heapkit.c
        src/core/runtime.c
        src/plugin/plugin_loader.c
        src/plugin/plugin_manager.c
        src/plugin/plugin_stub.c
        src/utils/utils_string.c
        src/core/crash_recovery.c
        src/core/core_ctl.c
        src/gateway/gateway.c
        src/jit/llvm_jit.c
        src/gateway/gateway_dispatcher.c
        src/gateway/gateway_dispatcher.h
        src/core/core_test.c
)

target_include_directories(core_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${LLVM_INCLUDE_FLAG} # Add LLVM headers without C++ flags
)

target_link_libraries(core_test PRIVATE corecdtl Threads::Threads)

# Platform-dependent link options
if(APPLE)
    target_link_options(corecdtl PRIVATE ${LLVM_LDFLAGS_LIST} -lc++)
else()
    target_link_options(corecdtl PRIVATE ${LLVM_LDFLAGS_LIST})
endif()

# ——————————————————————
# jitcore (shared library - C++ only, requires full CXX flags)
# ——————————————————————
add_library(jitcore SHARED
        libs/jit/jit_stub_generator.cpp
)

target_include_directories(jitcore PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        # LLVM includes are handled by LLVM_CXXFLAGS_LIST
)

# Apply all C++-specific flags (includes, standard, exceptions, etc.)
target_compile_options(jitcore PRIVATE ${LLVM_CXXFLAGS_LIST})

target_link_libraries(jitcore PUBLIC Threads::Threads)
# Platform-dependent link options
if(APPLE)
    target_link_options(jitcore PRIVATE ${LLVM_LDFLAGS_LIST} -lc++)
else()
    target_link_options(jitcore PRIVATE ${LLVM_LDFLAGS_LIST})
endif()

set_target_properties(jitcore PROPERTIES
        OUTPUT_NAME "jitcore"
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/jit-lib
)

# ——————————————————————
# CTL Tool
# ——————————————————————
if (BUILD_TOOLS)
    add_executable(corecdtl-ctl
            ctl/corecdtl-ctl.c
            ctl/art.c
    )
    target_include_directories(corecdtl-ctl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ctl)
    target_link_libraries(corecdtl-ctl PRIVATE Threads::Threads)
endif()

# ——————————————————————
# TEST
# ——————————————————————
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()