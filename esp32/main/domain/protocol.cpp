#include "protocol.hpp"

#include <cstddef>
#include <cstdio>

namespace {

// at es donde terminó sscanf. Si después sobra texto que no es \r o \n, no es esta línea.
bool tailOk(const char* line, int got, int fields, char tail, int at) {
    if (got < fields || at < 0) {
        return false;
    }
    if (line[at] == '\0') {
        return got == fields;
    }
    if (got != fields + 1) {
        return false;
    }
    if (tail == '\n') {
        return line[at + 1] == '\0';
    }
    if (tail == '\r') {
        return line[at + 1] == '\0' || (line[at + 1] == '\n' && line[at + 2] == '\0');
    }
    return false;
}

bool inRange(int value, int low, int high) {
    return value >= low && value <= high;
}

}  // namespace

// Arman la línea con el número del enum. false si no entró en el buffer.
bool writeStateLine(char* buffer, int capacity, Belt belt) {
    int written = std::snprintf(
        buffer,
        static_cast<std::size_t>(capacity),
        "$%d,%d,%d,%d\n",
        static_cast<int>(MessageType::State),
        static_cast<int>(belt.state),
        static_cast<int>(belt.speed),
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

bool writeSpeedLine(char* buffer, int capacity, Speed speed) {
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
    // $1,estado,velocidad,piezas. El Belt se escribe recién si todo cierra.
    int type = 0;
    int state = 0;
    int speed = 0;
    int pieces = 0;
    char tail = '\0';
    int at = -1;
    int got = std::sscanf(line, "$%d,%d,%d,%d%n%c", &type, &state, &speed, &pieces, &at, &tail);
    if (!tailOk(line, got, 4, tail, at)) {
        return false;
    }
    if (type != static_cast<int>(MessageType::State)) {
        return false;
    }
    if (!inRange(state, static_cast<int>(BeltState::Stopped), static_cast<int>(BeltState::Emergency))) {
        return false;
    }
    if (!inRange(speed, static_cast<int>(Speed::Low), static_cast<int>(Speed::High))) {
        return false;
    }

    belt.state = static_cast<BeltState>(state);
    belt.speed = static_cast<Speed>(speed);
    belt.pieces = pieces;
    return true;
}

bool interpret(const char* line, BeltEvent& event) {
    // Primero $3 y $4, que traen un número. $5 no lleva coma y se lee más abajo.
    int type = 0;
    int code = 0;
    char tail = '\0';
    int at = -1;
    int got = std::sscanf(line, "$%d,%d%n%c", &type, &code, &at, &tail);
    if (got >= 2 && tailOk(line, got, 2, tail, at)) {
        if (type == static_cast<int>(MessageType::Command)) {
            if (code == static_cast<int>(CommandCode::Start)) {
                event = BeltEvent::Start;
                return true;
            }
            if (code == static_cast<int>(CommandCode::Stop)) {
                event = BeltEvent::Stop;
                return true;
            }
            return false;
        }
        if (type == static_cast<int>(MessageType::Speed)) {
            if (!inRange(code, static_cast<int>(Speed::Low), static_cast<int>(Speed::High))) {
                return false;
            }
            switch (static_cast<Speed>(code)) {
            case Speed::Low:
                event = BeltEvent::SetLow;
                return true;
            case Speed::Medium:
                event = BeltEvent::SetMedium;
                return true;
            case Speed::High:
                event = BeltEvent::SetHigh;
                return true;
            }
            return false;
        }
        return false;
    }

    // $5, sin coma. $3 solo, o un tipo desconocido, cae acá y se rechaza.
    tail = '\0';
    at = -1;
    got = std::sscanf(line, "$%d%n%c", &type, &at, &tail);
    if (!tailOk(line, got, 1, tail, at)) {
        return false;
    }
    if (type != static_cast<int>(MessageType::ResetPieces)) {
        return false;
    }
    event = BeltEvent::ResetPieces;
    return true;
}
