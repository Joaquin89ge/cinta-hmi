#pragma once

#include "driver/gpio.h"
#include "driver/uart.h"

// Cableado de la protoboard. Los drivers reciben estos valores: no los tienen adentro.

constexpr gpio_num_t kLedGreen = GPIO_NUM_23;
constexpr gpio_num_t kLedRed = GPIO_NUM_4;
constexpr gpio_num_t kLedBlue = GPIO_NUM_2;

constexpr gpio_num_t kButtonOk = GPIO_NUM_25;
constexpr gpio_num_t kButtonPiece = GPIO_NUM_26;
constexpr gpio_num_t kButtonEmergency = GPIO_NUM_27;
// A GND, pull-up interno, igual que los otros.
constexpr gpio_num_t kButtonUp = GPIO_NUM_32;
constexpr gpio_num_t kButtonDown = GPIO_NUM_33;

constexpr gpio_num_t kStepperIn1 = GPIO_NUM_18;
constexpr gpio_num_t kStepperIn2 = GPIO_NUM_19;
constexpr gpio_num_t kStepperIn3 = GPIO_NUM_21;
constexpr gpio_num_t kStepperIn4 = GPIO_NUM_22;

struct UartPins {
    uart_port_t port;
    gpio_num_t tx;
    gpio_num_t rx;
};

// Prueba en esta PC. El CH9102 del USB está soldado al UART0: TX GPIO1, RX GPIO3.
// La placa aparece como /dev/ttyACM0. No abrir el monitor mientras corre el puente.
constexpr UartPins kUartUsb = {UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3};

// Raspberry. UART aparte de la consola de logs: TX GPIO17, RX GPIO16.
// En la Pi el apunte la ve como /dev/ttyS0, con enable_uart=1.
constexpr UartPins kUartGpio = {UART_NUM_2, GPIO_NUM_17, GPIO_NUM_16};

// Cable activo. Para volver a probar por USB en la PC: kUartUsb y flashear.
constexpr UartPins kUart = kUartUsb;

constexpr int kUartBaud = 115200;
