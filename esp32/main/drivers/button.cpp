#include "button.hpp"

#include "esp_err.h"

Button::Button(gpio_num_t pin) : pin_(pin) {}

void Button::init() {
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin_;
    config.mode = GPIO_MODE_INPUT;
    // Pull-up interno: suelto queda en alto y no flota. Apretado cierra a GND.
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
}

// Apretado es nivel bajo: el pulsador cierra el pin a GND.
bool Button::isPressed() const {
    return gpio_get_level(pin_) == 0;
}
