#include "stepper.hpp"

#include "esp_err.h"

namespace {

constexpr int kHalfStepCount = 8;

// Medio paso, como quedó el hardware. Un paso completo se saltea la bobina intermedia.
constexpr int kHalfSteps[kHalfStepCount][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1},
};

}  // namespace

StepperDriver::StepperDriver(gpio_num_t in1, gpio_num_t in2, gpio_num_t in3, gpio_num_t in4)
    : pins_{in1, in2, in3, in4} {}

void StepperDriver::init() {
    gpio_config_t config = {};
    config.pin_bit_mask = 0;
    for (gpio_num_t pin : pins_) {
        config.pin_bit_mask |= 1ULL << pin;
    }
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
    release();
}

void StepperDriver::step() {
    writePhase(phase_);
    // Solo un sentido: la cinta no vuelve atrás.
    phase_ = (phase_ + 1) % kHalfStepCount;
}

void StepperDriver::release() {
    // No se toca phase_: el próximo step sigue la secuencia, no la reinicia.
    for (gpio_num_t pin : pins_) {
        gpio_set_level(pin, 0);
    }
}

void StepperDriver::writePhase(int phase) {
    for (int coil = 0; coil < 4; ++coil) {
        gpio_set_level(pins_[coil], kHalfSteps[phase][coil]);
    }
}
