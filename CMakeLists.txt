cmake_minimum_required(VERSION 3.20)
project(KingsAndCadence_Engine VERSION 1.0 LANGUAGES CXX)

# Force C++20 standard for hardware bit-manipulation libraries
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Aggressive optimizations for AI speed
if(MSVC)
    add_compile_options(/O2 /arch:AVX2 /fp:fast)
else()
    add_compile_options(-O3 -march=native -ffast-math -flto)
endif()

# --- NEW: Fetch and Link Pybind11 ---
include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# Keep your old engine target for debugging
add_executable(engine src/main.cpp)
target_include_directories(engine PRIVATE include)

# --- NEW: Build the Python Native Module ---
# Note: "chess_engine" MUST match the PYBIND11_MODULE name in module.cpp!
pybind11_add_module(chess_engine src/module.cpp)
target_include_directories(chess_engine PRIVATE include)