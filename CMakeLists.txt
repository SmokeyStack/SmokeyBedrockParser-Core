cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0079 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIB_NAME SmokeyBedrockParser-Core)

project(SmokeyBedrockParser-Core VERSION 0.1)

set(LEVELDB_BUILD_TESTS OFF)
set(LEVELDB_BUILD_BENCHMARKS OFF)
set(NBT_BUILD_TESTS OFF CACHE INTERNAL "Don't build nbt++ tests")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.c include/*cpp)
file(GLOB_RECURSE HEADER_FILES include/*.h include/*.hpp)
if (NOT TARGET ${LIB_NAME})
    add_library(${LIB_NAME} STATIC ${SRC_FILES})
endif()

if (NOT TARGET leveldb)
    add_subdirectory(third-party/leveldb)
endif()
if (NOT TARGET spdlog)
    add_subdirectory(third-party/spdlog)
endif()
if (NOT TARGET nbt++)
    add_subdirectory(third-party/libnbtplusplus)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

# GOOGLE disables RTTI for leveldb so we will do the same
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Disable RTTI.
  string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
else()
  # Disable RTTI.
  string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

target_include_directories(${LIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(${LIB_NAME}
  leveldb spdlog nbt++
)