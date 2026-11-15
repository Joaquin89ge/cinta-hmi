#pragma once

#include "driver/gpio.h"
#include "driver/uart.h"

struct UartConfig {
    uart_port_t port;
    gpio_num_t tx;
    gpio_num_t rx;
    int baud;
};

// Manda y recibe líneas. No conoce los números del protocolo ni la cinta.
class UartLink {
public:
    explicit UartLink(UartConfig config);
    void init();
    void write(const char* text);
    bool readLine(char* buffer, int capacity);

private:
    UartConfig config_;
};
