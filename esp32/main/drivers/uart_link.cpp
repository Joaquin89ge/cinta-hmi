#include "uart_link.hpp"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

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

    // Solo en la prueba por USB. La consola de ESP-IDF ya instaló el driver del UART0
    // (el CH9102). Hay que soltarlo para usar ese puerto con el protocolo.
    // Con kUartGpio el puerto es el UART2 y este bloque no corre. Se puede dejar.
    if (config_.port == UART_NUM_0) {
        esp_err_t deleted = uart_driver_delete(UART_NUM_0);
        if (deleted != ESP_OK && deleted != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(deleted);
        }
    }

    // Recepción chica: las líneas son cortas. Sin cola de transmisión, el write espera.
    ESP_ERROR_CHECK(uart_driver_install(config_.port, kUartRxBufferBytes, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(config_.port, &uart));
    ESP_ERROR_CHECK(uart_set_pin(
        config_.port, config_.tx, config_.rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

// Manda los bytes que le pasan. El salto de línea ya viene en el texto.
void UartLink::write(const char* text) {
    int written = uart_write_bytes(config_.port, text, std::strlen(text));
    ESP_ERROR_CHECK(written < 0 ? ESP_FAIL : ESP_OK);
}

bool UartLink::readLine(char* buffer, int capacity) {
    uint8_t byte = 0;
    int length = 0;
    // NOTE: bloquea hasta el próximo byte. uart_rx es la única que llama,
    // y ya no comparte el lazo con el motor.
    while (uart_read_bytes(config_.port, &byte, 1, portMAX_DELAY) == 1) {
        if (byte == '\r') {
            continue;
        }
        if (byte == '\n') {
            // Vacía o más larga que el buffer: se tira. No dejo un pedazo como comando.
            if (length == 0 || length >= capacity) {
                return false;
            }
            buffer[length] = '\0';
            return true;
        }
        if (length >= capacity - 1) {
            length = capacity;
            continue;
        }
        buffer[length] = static_cast<char>(byte);
        length += 1;
    }
    return false;
}
