cmake_minimum_required(VERSION 3.16)
project(http2-boost-server VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Find required packages
find_package(PkgConfig REQUIRED)

# Find Boost
find_package(Boost 1.70.0 REQUIRED COMPONENTS system)
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found. Install with: apt-get install libboost-dev libboost-system-dev")
endif()

# Find nghttp2
pkg_check_modules(NGHTTP2 REQUIRED libnghttp2)
if(NOT NGHTTP2_FOUND)
    message(FATAL_ERROR "libnghttp2 not found. Install with: apt-get install libnghttp2-dev")
endif()

message(STATUS "Found Boost version: ${Boost_VERSION}")
message(STATUS "Found nghttp2 version: ${NGHTTP2_VERSION}")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wextra -DDEBUG")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -DNDEBUG")
endif()

# nlohmann/json - header only (download if not found)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found, using FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/core/server.cpp
    src/transport/session.cpp
    src/transport/request_handler.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::system
    ${NGHTTP2_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${Boost_INCLUDE_DIRS}
    ${NGHTTP2_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
    ${NGHTTP2_CFLAGS_OTHER}
)

# Additional compiler settings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(${PROJECT_NAME} PRIVATE -march=native)
    endif()
endif()

# Install
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Testing
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_server
        tests/test_server.cpp
        src/core/server.cpp
        src/transport/session.cpp
        src/transport/request_handler.cpp
    )
    
    target_link_libraries(test_server PRIVATE
        Boost::system
        ${NGHTTP2_LIBRARIES}
        nlohmann_json::nlohmann_json
    )
    
    target_include_directories(test_server PRIVATE
        ${Boost_INCLUDE_DIRS}
        ${NGHTTP2_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src
    )
    
    add_test(NAME ServerTests COMMAND test_server)
endif()

# Print summary
message(STATUS "")
message(STATUS "Build Summary:")
message(STATUS "  Project: ${PROJECT_NAME}")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Boost version: ${Boost_VERSION}")
message(STATUS "  nghttp2 version: ${NGHTTP2_VERSION}")
message(STATUS "")