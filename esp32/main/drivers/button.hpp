#pragma once

#include "driver/gpio.h"

// Pulsador a GND. Solo lee el nivel: el antirrebote y la interrupción los hace la task.
class Button {
public:
    explicit Button(gpio_num_t pin);
    void init();
    bool isPressed() const;

private:
    gpio_num_t pin_;
};
