cmake_minimum_required(VERSION 3.16)
project(cinta_logica LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_belt
    test_belt.cpp
    ../esp32/main/domain/belt.cpp
    ../esp32/main/domain/protocol.cpp)
target_include_directories(test_belt PRIVATE ../esp32/main/domain)
target_compile_options(test_belt PRIVATE -Wall -Wextra -UNDEBUG)
