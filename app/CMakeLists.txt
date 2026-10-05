cmake_minimum_required(VERSION 3.16)
project(drivesense_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURSES_INCLUDE_DIRS}
)

set(APP_SOURCES
    src/SensorData.cpp
    src/SensorDevice.cpp
    src/AlertManager.cpp
    src/Logger.cpp
    src/Dashboard.cpp
    src/App.cpp
    src/main.cpp
)

add_executable(drivesense_app ${APP_SOURCES})
target_link_libraries(drivesense_app PRIVATE ${CURSES_LIBRARIES} Threads::Threads)

set(WEB_SOURCES
    src/SensorData.cpp
    src/SensorDevice.cpp
    src/AlertManager.cpp
    src/web_main.cpp
)

add_executable(drivesense_web ${WEB_SOURCES})
target_link_libraries(drivesense_web PRIVATE Threads::Threads)
