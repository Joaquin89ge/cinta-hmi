cmake_minimum_required(VERSION 3.16)
project(cinta_hmi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# belt.cpp no entra: la HMI no corre apply. Muestra el estado que llega
# y manda órdenes; la máquina de estados queda en la ESP32.
add_executable(cinta_hmi
    main.cpp
    pantalla.cpp
    fifos.cpp
    terceros/Consolas_24B.cpp
    ../esp32/main/domain/protocol.cpp
)

target_include_directories(cinta_hmi PRIVATE
    terceros
    ../esp32/main/domain
    ../comun
)

target_compile_options(cinta_hmi PRIVATE -pthread)
target_link_options(cinta_hmi PRIVATE -pthread)
