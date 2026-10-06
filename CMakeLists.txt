# PC ("host") build: the shared C code, its tests, and PC tools.
# The firmware is a separate CMake project in firmware/ (made by STM32CubeMX).
cmake_minimum_required(VERSION 3.20)
project(vigil_host LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(VIGIL_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
if(VIGIL_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# Strict warnings for our own code. -Wdouble-promotion matters on the STM32:
# its FPU only does float, so an accidental double makes math ~10x slower.
add_library(vigil_warnings INTERFACE)
target_compile_options(vigil_warnings INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion)

# ---- Shared C code: the exact same files are compiled into the firmware ----
add_library(vigil_core
  core/src/frames.c
)
target_include_directories(vigil_core PUBLIC core/include)
target_link_libraries(vigil_core PRIVATE vigil_warnings)

# ---- Tests (run on the PC) ----
include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

enable_testing()
add_executable(vigil_tests
  tests/test_frames.cpp
)
target_link_libraries(vigil_tests PRIVATE vigil_core vigil_warnings Catch2::Catch2WithMain)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(vigil_tests)
