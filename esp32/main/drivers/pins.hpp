#pragma once

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"

// Cableado de la protoboard. Los drivers reciben estos valores: no los tienen adentro.

constexpr gpio_num_t kLedGreen = GPIO_NUM_23;
constexpr gpio_num_t kLedRed = GPIO_NUM_4;
constexpr gpio_num_t kLedBlue = GPIO_NUM_2;

constexpr gpio_num_t kButtonOk = GPIO_NUM_25;
constexpr gpio_num_t kButtonPiece = GPIO_NUM_26;
constexpr gpio_num_t kButtonEmergency = GPIO_NUM_27;

constexpr gpio_num_t kStepperIn1 = GPIO_NUM_18;
constexpr gpio_num_t kStepperIn2 = GPIO_NUM_19;
constexpr gpio_num_t kStepperIn3 = GPIO_NUM_21;
constexpr gpio_num_t kStepperIn4 = GPIO_NUM_22;

// GPIO34 es solo entrada y cae en el canal 6 del ADC1.
constexpr adc_channel_t kJoystickYChannel = ADC_CHANNEL_6;

// UART2: la consola de logs ya ocupa UART0 (el CH9102). Cambiar el cable es cambiar estas constantes.
constexpr uart_port_t kUartPort = UART_NUM_2;
constexpr gpio_num_t kUartTx = GPIO_NUM_17;
constexpr gpio_num_t kUartRx = GPIO_NUM_16;
constexpr int kUartBaud = 115200;
