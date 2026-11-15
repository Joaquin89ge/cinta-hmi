// Prueba de banco: los tres LED, un tramo de motor y los pulsadores.
// No es la cinta. Sirve para ver cables antes de flashear esp32/.

#include "button.hpp"
#include "led.hpp"
#include "pins.hpp"
#include "stepper.hpp"
#include "uart_link.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr int kBlinkMs = 400;
constexpr int kMotorSteps = 400;
constexpr int kMotorStepMs = 6;
constexpr int kSampleMs = 50;
constexpr int kLogPeriodMs = 250;
constexpr const char* kTag = "banco";

void showLed(Led& led) {
    led.on();
    vTaskDelay(pdMS_TO_TICKS(kBlinkMs));
    led.off();
}

}  // namespace

extern "C" void app_main(void) {
    Led green(kLedGreen);
    Led red(kLedRed);
    Led blue(kLedBlue);
    Button up(kButtonUp);
    Button down(kButtonDown);
    Button ok(kButtonOk);
    Button piece(kButtonPiece);
    Button emergency(kButtonEmergency);
    StepperDriver stepper(kStepperIn1, kStepperIn2, kStepperIn3, kStepperIn4);
    // El mismo cable que la cinta. Hoy es el USB; ver kUart en pins.hpp.
    UartLink link({kUart.port, kUart.tx, kUart.rx, kUartBaud});

    green.init();
    red.init();
    blue.init();
    up.init();
    down.init();
    ok.init();
    piece.init();
    emergency.init();
    stepper.init();
    link.init();

    ESP_LOGI(kTag, "leds: verde, rojo, azul");
    showLed(green);
    showLed(red);
    showLed(blue);

    ESP_LOGI(kTag, "motor: giro y suelto");
    for (int step = 0; step < kMotorSteps; ++step) {
        stepper.step();
        vTaskDelay(pdMS_TO_TICKS(kMotorStepMs));
    }
    stepper.release();

    ESP_LOGI(kTag, "entradas: 1 es apretado. Apretá los pulsadores.");
    int elapsedMs = 0;
    while (true) {
        if (piece.isPressed()) {
            green.on();
        } else {
            green.off();
        }
        if (emergency.isPressed()) {
            red.on();
        } else {
            red.off();
        }
        if (ok.isPressed()) {
            blue.on();
        } else {
            blue.off();
        }

        elapsedMs += kSampleMs;
        if (elapsedMs >= kLogPeriodMs) {
            elapsedMs = 0;
            ESP_LOGI(
                kTag,
                "arriba=%d abajo=%d ok=%d pieza=%d emer=%d",
                up.isPressed() ? 1 : 0,
                down.isPressed() ? 1 : 0,
                ok.isPressed() ? 1 : 0,
                piece.isPressed() ? 1 : 0,
                emergency.isPressed() ? 1 : 0);
        }
        vTaskDelay(pdMS_TO_TICKS(kSampleMs));
    }
}
