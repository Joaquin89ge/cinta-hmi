#pragma once

#include "driver/gpio.h"

#include <array>

// Driver del motor paso a paso 28BYJ-48 con placa ULN2003.
// Solo sabe dar pasos y apagar las bobinas; la velocidad la decide quien lo llama.
class StepperDriver {
public:
    StepperDriver(gpio_num_t in1, gpio_num_t in2, gpio_num_t in3, gpio_num_t in4);
    void init();
    void step();
    void release();

private:
    void writePhase(int phase);

    std::array<gpio_num_t, 4> pins_;
    int phase_ = 0;
};
