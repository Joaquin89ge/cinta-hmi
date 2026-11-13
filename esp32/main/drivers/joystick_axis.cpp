#include "joystick_axis.hpp"

#include "esp_err.h"

JoystickAxis::JoystickAxis(adc_channel_t channel) : channel_(channel) {}

void JoystickAxis::init() {
    adc_oneshot_unit_init_cfg_t unitConfig = {};
    unitConfig.unit_id = ADC_UNIT_1;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unitConfig, &adc_));

    adc_oneshot_chan_cfg_t channelConfig = {};
    // 12 dB cubre el joystick alimentado a 3,3 V. Con menos, satura antes.
    channelConfig.atten = ADC_ATTEN_DB_12;
    channelConfig.bitwidth = ADC_BITWIDTH_DEFAULT;
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_, channel_, &channelConfig));
}

int JoystickAxis::readRaw() {
    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_, channel_, &raw));
    return raw;
}
