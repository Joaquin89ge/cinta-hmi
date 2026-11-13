#pragma once

// Lógica de la cinta, sin pines ni FreeRTOS.
// apply devuelve el valor siguiente para que ninguna task comparta una variable global.

enum class BeltState {
    Stopped,
    Running,
    Emergency,
};

enum class Speed {
    Low,
    Medium,
    High,
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
