#include "led.hpp"

#include "esp_err.h"

Led::Led(gpio_num_t pin) : pin_(pin) {}

void Led::init() {
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin_;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
    // Arranca apagado. Quien lo prenda es la task.
    off();
}

void Led::on() {
    gpio_set_level(pin_, 1);
}

void Led::off() {
    gpio_set_level(pin_, 0);
}
