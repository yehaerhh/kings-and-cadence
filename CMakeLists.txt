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

# Core executable (Later this will be switched to a shared library for Python)
add_executable(engine src/main.cpp)
target_include_directories(engine PRIVATE include)