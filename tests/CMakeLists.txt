cmake_minimum_required(VERSION 3.16)
project(drivesense_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/include
)

# Unit tests executable (no driver required)
add_executable(unit_tests
    unit_tests.cpp
    ../app/src/AlertManager.cpp
    ../app/src/Logger.cpp
    ../app/src/SensorData.cpp
)
target_link_libraries(unit_tests PRIVATE Threads::Threads)

# Integration tests executable (requires loaded driver)
add_executable(integration_test
    integration_test.cpp
    ../app/src/SensorDevice.cpp
    ../app/src/SensorData.cpp
)
target_link_libraries(integration_test PRIVATE Threads::Threads)
