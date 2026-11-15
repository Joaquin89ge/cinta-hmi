#include "belt.hpp"
#include "protocol.hpp"

#include <cassert>
#include <cstring>

int main() {
    Belt stopped;

    Belt running = apply(stopped, BeltEvent::Start);
    assert(running.state == BeltState::Running);
    assert(running.speed == Speed::Low);
    assert(running.pieces == 0);

    assert(apply(stopped, BeltEvent::Stop).state == BeltState::Stopped);
    assert(apply(stopped, BeltEvent::Emergency).state == BeltState::Emergency);

    assert(apply(running, BeltEvent::Start).state == BeltState::Running);
    assert(apply(running, BeltEvent::Stop).state == BeltState::Stopped);
    assert(apply(running, BeltEvent::Emergency).state == BeltState::Emergency);

    Belt emergency;
    emergency.state = BeltState::Emergency;
    assert(apply(emergency, BeltEvent::Start).state == BeltState::Running);
    assert(apply(emergency, BeltEvent::Stop).state == BeltState::Emergency);
    assert(apply(emergency, BeltEvent::Emergency).state == BeltState::Emergency);

    Belt withSpeed = running;
    withSpeed = apply(withSpeed, BeltEvent::SetMedium);
    assert(withSpeed.state == BeltState::Running);
    assert(withSpeed.speed == Speed::Medium);
    withSpeed = apply(withSpeed, BeltEvent::SetHigh);
    assert(withSpeed.speed == Speed::High);
    withSpeed = apply(withSpeed, BeltEvent::SetLow);
    assert(withSpeed.speed == Speed::Low);
    assert(withSpeed.state == BeltState::Running);

    Belt emergencySpeed = emergency;
    emergencySpeed = apply(emergencySpeed, BeltEvent::SetHigh);
    assert(emergencySpeed.state == BeltState::Emergency);
    assert(emergencySpeed.speed == Speed::High);

    Belt counted = apply(running, BeltEvent::Piece);
    assert(counted.pieces == 1);
    assert(counted.state == BeltState::Running);
    counted = apply(counted, BeltEvent::Piece);
    assert(counted.pieces == 2);

    Belt countedStopped = apply(emergency, BeltEvent::Piece);
    assert(countedStopped.pieces == 1);
    assert(countedStopped.state == BeltState::Emergency);

    counted = apply(counted, BeltEvent::ResetPieces);
    assert(counted.pieces == 0);
    assert(counted.state == BeltState::Running);
    assert(counted.speed == Speed::Low);

    char line[kLineCapacity];
    Belt reported;
    reported.state = BeltState::Running;
    reported.speed = Speed::Medium;
    reported.pieces = 4;
    assert(writeStateLine(line, kLineCapacity, reported));
    assert(std::strcmp(line, "$1,2,2,4\n") == 0);

    Belt parsed;
    parsed.state = BeltState::Emergency;
    parsed.speed = Speed::High;
    parsed.pieces = 9;
    assert(readStateLine(line, parsed));
    assert(parsed.state == BeltState::Running);
    assert(parsed.speed == Speed::Medium);
    assert(parsed.pieces == 4);

    Belt idle;
    assert(writeStateLine(line, kLineCapacity, idle));
    assert(std::strcmp(line, "$1,1,1,0\n") == 0);
    assert(readStateLine(line, parsed));
    assert(parsed.state == BeltState::Stopped);
    assert(parsed.speed == Speed::Low);
    assert(parsed.pieces == 0);
    assert(!writeStateLine(line, 4, idle));

    Belt kept;
    kept.state = BeltState::Emergency;
    kept.speed = Speed::High;
    kept.pieces = 7;
    assert(!readStateLine("$1,9,2,4", kept));
    assert(kept.state == BeltState::Emergency);
    assert(kept.speed == Speed::High);
    assert(kept.pieces == 7);
    assert(!readStateLine("$1,2,2", kept));
    assert(kept.state == BeltState::Emergency);
    assert(kept.speed == Speed::High);
    assert(kept.pieces == 7);
    assert(!readStateLine("$1,2,2,4,1", kept));
    assert(kept.state == BeltState::Emergency);
    assert(kept.speed == Speed::High);
    assert(kept.pieces == 7);

    assert(writeNavLine(line, kLineCapacity, NavCode::Up));
    assert(std::strcmp(line, "$2,1\n") == 0);
    assert(writeNavLine(line, kLineCapacity, NavCode::Down));
    assert(std::strcmp(line, "$2,2\n") == 0);
    assert(writeNavLine(line, kLineCapacity, NavCode::Enter));
    assert(std::strcmp(line, "$2,3\n") == 0);

    assert(writeCommandLine(line, kLineCapacity, CommandCode::Start));
    assert(std::strcmp(line, "$3,1\n") == 0);
    assert(writeSpeedLine(line, kLineCapacity, Speed::Medium));
    assert(std::strcmp(line, "$4,2\n") == 0);
    assert(writeResetLine(line, kLineCapacity));
    assert(std::strcmp(line, "$5\n") == 0);

    BeltEvent event = BeltEvent::Piece;
    assert(interpret("$3,1", event));
    assert(event == BeltEvent::Start);
    assert(interpret("$3,2\r\n", event));
    assert(event == BeltEvent::Stop);
    assert(interpret("$4,1", event));
    assert(event == BeltEvent::SetLow);
    assert(interpret("$4,2", event));
    assert(event == BeltEvent::SetMedium);
    assert(interpret("$4,3", event));
    assert(event == BeltEvent::SetHigh);
    assert(interpret("$5", event));
    assert(event == BeltEvent::ResetPieces);

    event = BeltEvent::Piece;
    assert(!interpret("I (1) banco: adc=0", event));
    assert(event == BeltEvent::Piece);
    assert(!interpret("$1,2,2,4", event));
    assert(!interpret("$2,1", event));
    assert(!interpret("$3,9", event));
    assert(!interpret("$9,1", event));
    assert(!interpret("$3", event));
    assert(event == BeltEvent::Piece);

    return 0;
}
