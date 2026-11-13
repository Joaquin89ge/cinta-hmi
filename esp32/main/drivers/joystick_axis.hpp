#pragma once

#include "esp_adc/adc_oneshot.h"

// Eje del joystick. Devuelve la cuenta del ADC; arriba o abajo lo decide quien lo llama.
class JoystickAxis {
public:
    explicit JoystickAxis(adc_channel_t channel);
    void init();
    int readRaw();

private:
    adc_channel_t channel_;
    adc_oneshot_unit_handle_t adc_ = nullptr;
};
