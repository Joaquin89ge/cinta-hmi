#pragma once

#include "belt.hpp"

// Acuerdo con la Raspberry: el mismo header se incluye de los dos lados.
// El número empieza en 1. El 0 no es un mensaje: un buffer en cero no es una acción.

constexpr int kLineCapacity = 64;

enum class MessageType : int {
    State = 1,
    Nav = 2,
    Command = 3,
    Speed = 4,
    ResetPieces = 5,
};

enum class StateCode : int {
    Stopped = 1,
    Running = 2,
    Emergency = 3,
};

enum class SpeedCode : int {
    Low = 1,
    Medium = 2,
    High = 3,
};

enum class NavCode : int {
    Up = 1,
    Down = 2,
    Enter = 3,
};

enum class CommandCode : int {
    Start = 1,
    Stop = 2,
};

// Arma una línea con el número del enum. false si no entra en el buffer.
bool writeStateLine(char* buffer, int capacity, Belt belt);
bool writeNavLine(char* buffer, int capacity, NavCode nav);
bool writeCommandLine(char* buffer, int capacity, CommandCode command);
bool writeSpeedLine(char* buffer, int capacity, SpeedCode speed);
bool writeResetLine(char* buffer, int capacity);

// Lee un $1 y llena belt. Si la línea no es un estado válido, devuelve false
// y deja belt como estaba.
bool readStateLine(const char* line, Belt& belt);

// Único lugar que convierte una acción recibida en BeltEvent.
// La información (estado y navegación) y lo que no empieza con $ devuelven false
// y dejan event como estaba.
bool interpret(const char* line, BeltEvent& event);
