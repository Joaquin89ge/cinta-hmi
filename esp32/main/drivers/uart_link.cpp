#include "uart_link.hpp"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include <cstddef>
#include <cstring>

namespace {

constexpr int kUartRxBufferBytes = 256;

}  // namespace

UartLink::UartLink(UartConfig config) : config_(config) {}

void UartLink::init() {
    uart_config_t uart = {};
    uart.baud_rate = config_.baud;
    uart.data_bits = UART_DATA_8_BITS;
    uart.parity = UART_PARITY_DISABLE;
    uart.stop_bits = UART_STOP_BITS_1;
    uart.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(config_.port, kUartRxBufferBytes, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(config_.port, &uart));
    ESP_ERROR_CHECK(uart_set_pin(
        config_.port, config_.tx, config_.rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void UartLink::write(const char* text) {
    int written = uart_write_bytes(config_.port, text, std::strlen(text));
    ESP_ERROR_CHECK(written < 0 ? ESP_FAIL : ESP_OK);
}

bool UartLink::readLine(char* buffer, int capacity) {
    uint8_t byte = 0;
    // NOTE: bloquea hasta el próximo byte. uart_rx es la única que llama,
    // y ya no comparte el lazo con el motor.
    while (uart_read_bytes(config_.port, &byte, 1, portMAX_DELAY) == 1) {
        if (byte == '\r') {
            continue;
        }
        if (byte == '\n') {
            pending_[pendingLength_] = '\0';
            bool fits = !discarding_ && pendingLength_ > 0 && pendingLength_ < capacity;
            if (fits) {
                std::memcpy(buffer, pending_, static_cast<std::size_t>(pendingLength_) + 1);
            }
            pendingLength_ = 0;
            discarding_ = false;
            return fits;
        }
        if (discarding_ || pendingLength_ >= kUartLineCapacity - 1) {
            discarding_ = true;
            continue;
        }
        pending_[pendingLength_] = static_cast<char>(byte);
        pendingLength_ += 1;
    }
    return false;
}
