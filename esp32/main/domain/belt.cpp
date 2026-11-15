#include "belt.hpp"

Belt apply(Belt belt, BeltEvent event) {
    // La velocidad y la cuenta no son estados de la máquina: el menú las cambia
    // parada, en marcha o en emergencia. Dar pasos o no lo decide después quien mueve el motor.
    switch (event) {
    case BeltEvent::SetLow:
        belt.speed = Speed::Low;
        return belt;
    case BeltEvent::SetMedium:
        belt.speed = Speed::Medium;
        return belt;
    case BeltEvent::SetHigh:
        belt.speed = Speed::High;
        return belt;
    case BeltEvent::Piece:
        belt.pieces += 1;
        return belt;
    case BeltEvent::ResetPieces:
        belt.pieces = 0;
        return belt;
    case BeltEvent::Start:
    case BeltEvent::Stop:
    case BeltEvent::Emergency:
        break;
    }

    // Start, Stop y Emergency. En cada estado, lo que no está escrito no cambia nada:
    // PARAR estando parada, o ARRANCAR ya en marcha, se ignoran.
    switch (belt.state) {
    case BeltState::Stopped:
        switch (event) {
        case BeltEvent::Start:
            belt.state = BeltState::Running;
            break;
        case BeltEvent::Emergency:
            belt.state = BeltState::Emergency;
            break;
        case BeltEvent::Stop:
            break;
        default:
            break;
        }
        break;

    case BeltState::Running:
        switch (event) {
        case BeltEvent::Stop:
            belt.state = BeltState::Stopped;
            break;
        case BeltEvent::Emergency:
            belt.state = BeltState::Emergency;
            break;
        case BeltEvent::Start:
            break;
        default:
            break;
        }
        break;

    case BeltState::Emergency:
        switch (event) {
        case BeltEvent::Start:
            // ARRANCAR es la única salida, y ese botón pone la cinta en marcha.
            belt.state = BeltState::Running;
            break;
        case BeltEvent::Stop:
            // PARAR no alcanza: si no, el menú soltaría la emergencia sin un arranque explícito.
            break;
        case BeltEvent::Emergency:
            break;
        default:
            break;
        }
        break;
    }

    return belt;
}
