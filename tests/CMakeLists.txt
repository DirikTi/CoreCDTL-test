cmake_minimum_required(VERSION 3.16)

set(TEST_SOURCES
        test_main.c
        test_heapkit.c
        test_event_bus.c
        test_scheduler.c
        helpers/test_helpers.c
        unity/unity.c
)

add_executable(runTests ${TEST_SOURCES})

target_include_directories(runTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/unity
        ${CMAKE_CURRENT_SOURCE_DIR}/helpers
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils
)


target_link_libraries(runTests PRIVATE corecdtl Threads::Threads)

enable_testing()
add_test(NAME AllTests COMMAND runTests)
