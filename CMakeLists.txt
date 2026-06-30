cmake_minimum_required(VERSION 3.16)
project(gened_firmware LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(gened_core
    firmware/hal/sim/hal_sim.cpp
    firmware/event_runtime/event_queue.cpp
    firmware/connectivity/connectivity.cpp
    firmware/diagnostics/diagnostics.cpp
    firmware/ota/ota.cpp
    firmware/power/power_manager.cpp
    firmware/rules_engine/rules_engine.cpp
)
target_include_directories(gened_core PUBLIC firmware)
target_link_libraries(gened_core PUBLIC Threads::Threads)

add_executable(gened_firmware firmware/app_runtime/main.cpp)
target_link_libraries(gened_firmware PRIVATE gened_core)

enable_testing()
add_executable(test_event_queue tests/test_event_queue.cpp)
target_link_libraries(test_event_queue PRIVATE gened_core)
add_test(NAME event_queue COMMAND test_event_queue)

add_executable(test_modules tests/test_modules.cpp)
target_link_libraries(test_modules PRIVATE gened_core)
add_test(NAME modules COMMAND test_modules)

add_executable(test_scenarios tests/test_scenarios.cpp)
target_link_libraries(test_scenarios PRIVATE gened_core)
add_test(NAME scenario_connectivity
         COMMAND test_scenarios connectivity_loss)
add_test(NAME scenario_partial_ack
         COMMAND test_scenarios partial_ack)
add_test(NAME scenario_flash_corruption
         COMMAND test_scenarios flash_corruption)
add_test(NAME scenario_low_battery
         COMMAND test_scenarios low_battery)
add_test(NAME scenario_bad_signature
         COMMAND test_scenarios bad_ota_signature)
