cmake_minimum_required(VERSION 3.16)
project(file_sorter VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

find_package(nlohmann_json REQUIRED)
find_package(exiv2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)

set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "" FORCE)
FetchContent_Declare(cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.5
)
FetchContent_MakeAvailable(cpr)

include_directories(include)
file(GLOB SOURCES "src/*.cc")

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    exiv2lib 
    cpr::cpr 
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    nlohmann_json::nlohmann_json
)