#pragma once

#include "driver/gpio.h"

// LED con la resistencia hacia el GPIO. En alto enciende.
class Led {
public:
    explicit Led(gpio_num_t pin);
    void init();
    void on();
    void off();

private:
    gpio_num_t pin_;
};
