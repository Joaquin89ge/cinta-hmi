cmake_minimum_required(VERSION 3.16)
project(cinta_puente LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Solo el header del protocolo, por los números. No se linkea belt.cpp.
add_executable(puente main.cpp)
target_include_directories(puente PRIVATE
    ../esp32/main/domain
    ../comun
)
target_compile_options(puente PRIVATE -Wall -Wextra)
