#include "protocol.hpp"

#include <climits>
#include <cstddef>
#include <cstdio>

namespace {

StateCode stateCode(BeltState state) {
    switch (state) {
    case BeltState::Stopped:
        return StateCode::Stopped;
    case BeltState::Running:
        return StateCode::Running;
    case BeltState::Emergency:
        return StateCode::Emergency;
    }
    return StateCode::Stopped;
}

SpeedCode speedCode(Speed speed) {
    switch (speed) {
    case Speed::Low:
        return SpeedCode::Low;
    case Speed::Medium:
        return SpeedCode::Medium;
    case Speed::High:
        return SpeedCode::High;
    }
    return SpeedCode::Low;
}

bool beltState(int code, BeltState& state) {
    switch (static_cast<StateCode>(code)) {
    case StateCode::Stopped:
        state = BeltState::Stopped;
        return true;
    case StateCode::Running:
        state = BeltState::Running;
        return true;
    case StateCode::Emergency:
        state = BeltState::Emergency;
        return true;
    }
    return false;
}

bool beltSpeed(int code, Speed& speed) {
    switch (static_cast<SpeedCode>(code)) {
    case SpeedCode::Low:
        speed = Speed::Low;
        return true;
    case SpeedCode::Medium:
        speed = Speed::Medium;
        return true;
    case SpeedCode::High:
        speed = Speed::High;
        return true;
    }
    return false;
}

bool copyLine(const char* line, char* dest, int capacity) {
    int i = 0;
    while (line[i] != '\0' && line[i] != '\n' && line[i] != '\r') {
        if (i >= capacity - 1) {
            return false;
        }
        dest[i] = line[i];
        i += 1;
    }
    dest[i] = '\0';
    return i > 0;
}

bool readInt(const char* text, int& index, int& value) {
    if (text[index] < '0' || text[index] > '9') {
        return false;
    }
    value = 0;
    while (text[index] >= '0' && text[index] <= '9') {
        int digit = text[index] - '0';
        // Un int desbordado se leería como otro número. Se corta antes de multiplicar.
        if (value > (INT_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
        index += 1;
    }
    return true;
}

}  // namespace

bool writeStateLine(char* buffer, int capacity, Belt belt) {
    // El número que sale es el del enum. No hay una segunda tabla de códigos.
    int written = std::snprintf(
        buffer,
        static_cast<std::size_t>(capacity),
        "$%d,%d,%d,%d\n",
        static_cast<int>(MessageType::State),
        static_cast<int>(stateCode(belt.state)),
        static_cast<int>(speedCode(belt.speed)),
        belt.pieces);
    return written > 0 && written < capacity;
}

bool writeNavLine(char* buffer, int capacity, NavCode nav) {
    int written = std::snprintf(
        buffer,
        static_cast<std::size_t>(capacity),
        "$%d,%d\n",
        static_cast<int>(MessageType::Nav),
        static_cast<int>(nav));
    return written > 0 && written < capacity;
}

bool writeCommandLine(char* buffer, int capacity, CommandCode command) {
    int written = std::snprintf(
        buffer,
        static_cast<std::size_t>(capacity),
        "$%d,%d\n",
        static_cast<int>(MessageType::Command),
        static_cast<int>(command));
    return written > 0 && written < capacity;
}

bool writeSpeedLine(char* buffer, int capacity, SpeedCode speed) {
    int written = std::snprintf(
        buffer,
        static_cast<std::size_t>(capacity),
        "$%d,%d\n",
        static_cast<int>(MessageType::Speed),
        static_cast<int>(speed));
    return written > 0 && written < capacity;
}

bool writeResetLine(char* buffer, int capacity) {
    int written = std::snprintf(
        buffer,
        static_cast<std::size_t>(capacity),
        "$%d\n",
        static_cast<int>(MessageType::ResetPieces));
    return written > 0 && written < capacity;
}

bool readStateLine(const char* line, Belt& belt) {
    // El $1 informa cómo está la cinta. No es un evento: no pasa por apply.
    char text[kLineCapacity];
    if (!copyLine(line, text, kLineCapacity)) {
        return false;
    }
    if (text[0] != '$') {
        return false;
    }

    int index = 1;
    int type = 0;
    if (!readInt(text, index, type) || type != static_cast<int>(MessageType::State)) {
        return false;
    }

    int stateCodeValue = 0;
    int speedCodeValue = 0;
    int pieces = 0;
    if (text[index] != ',') {
        return false;
    }
    index += 1;
    if (!readInt(text, index, stateCodeValue) || text[index] != ',') {
        return false;
    }
    index += 1;
    if (!readInt(text, index, speedCodeValue) || text[index] != ',') {
        return false;
    }
    index += 1;
    if (!readInt(text, index, pieces) || text[index] != '\0') {
        return false;
    }

    BeltState state = BeltState::Stopped;
    Speed speed = Speed::Low;
    if (!beltState(stateCodeValue, state) || !beltSpeed(speedCodeValue, speed)) {
        return false;
    }

    belt.state = state;
    belt.speed = speed;
    belt.pieces = pieces;
    return true;
}

bool interpret(const char* line, BeltEvent& event) {
    char text[kLineCapacity];
    if (!copyLine(line, text, kLineCapacity)) {
        return false;
    }
    if (text[0] != '$') {
        return false;
    }

    int index = 1;
    int type = 0;
    if (!readInt(text, index, type)) {
        return false;
    }

    int code = 0;
    bool hasCode = false;
    if (text[index] == ',') {
        index += 1;
        if (!readInt(text, index, code)) {
            return false;
        }
        hasCode = true;
    }
    // Una acción no lleva campos de más. El estado que llega por error cae abajo.
    if (text[index] != '\0') {
        return false;
    }

    // Único lugar donde un número de acción pasa a ser un BeltEvent.
    switch (static_cast<MessageType>(type)) {
    case MessageType::Command:
        if (!hasCode) {
            return false;
        }
        switch (static_cast<CommandCode>(code)) {
        case CommandCode::Start:
            event = BeltEvent::Start;
            return true;
        case CommandCode::Stop:
            event = BeltEvent::Stop;
            return true;
        }
        return false;
    case MessageType::Speed:
        if (!hasCode) {
            return false;
        }
        switch (static_cast<SpeedCode>(code)) {
        case SpeedCode::Low:
            event = BeltEvent::SetLow;
            return true;
        case SpeedCode::Medium:
            event = BeltEvent::SetMedium;
            return true;
        case SpeedCode::High:
            event = BeltEvent::SetHigh;
            return true;
        }
        return false;
    case MessageType::ResetPieces:
        if (hasCode) {
            return false;
        }
        event = BeltEvent::ResetPieces;
        return true;
    case MessageType::State:
    case MessageType::Nav:
        return false;
    }
    return false;
}
