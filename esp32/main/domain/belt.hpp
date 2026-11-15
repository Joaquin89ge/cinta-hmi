#pragma once

// Lógica de la cinta, sin pines ni FreeRTOS.
// apply devuelve el valor siguiente para que ninguna task comparta una variable global.

// Los números son los del cable. No hay una segunda tabla de códigos.
enum class BeltState : int {
    Stopped = 1,
    Running = 2,
    Emergency = 3,
};

enum class Speed : int {
    Low = 1,
    Medium = 2,
    High = 3,
};

enum class BeltEvent {
    Start,
    Stop,
    Emergency,
    SetLow,
    SetMedium,
    SetHigh,
    Piece,
    ResetPieces,
};

struct Belt {
    // Al alimentar la placa no hay comando todavía: quieta, lenta y en cero.
    BeltState state = BeltState::Stopped;
    Speed speed = Speed::Low;
    int pieces = 0;
};

Belt apply(Belt belt, BeltEvent event);
