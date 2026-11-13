#include "belt.hpp"
#include "button.hpp"
#include "joystick_axis.hpp"
#include "led.hpp"
#include "pins.hpp"
#include "protocol.hpp"
#include "stepper.hpp"
#include "uart_link.hpp"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static_assert(kUartLineCapacity == kLineCapacity);

namespace {

constexpr int kLowMs = 8;
constexpr int kMediumMs = 4;
constexpr int kHighMs = 2;
constexpr int kIdleMs = 20;
constexpr int kStatePeriodMs = 100;
constexpr int kHeartbeatMs = 400;
constexpr int kInputPeriodMs = 20;
constexpr int kPiecePeriodMs = 10;
constexpr int kPieceStableMs = 30;
// TODO(fase2): si el ruido del ADC repite la navegación cerca de un umbral,
// agregar umbrales de entrada y de salida distintos.
constexpr int kJoyUpBelow = 1200;
constexpr int kJoyDownAbove = 2800;
constexpr int kEventQueueLength = 8;
constexpr int kNavQueueLength = 4;
constexpr int kCore = 1;
constexpr int kStackSmall = 2048;
constexpr int kStackWithLog = 3072;
constexpr int kQueueWaitMs = 20;
constexpr const char* kTag = "cinta";

// NOTE: estáticos, no locales de app_main. Las tasks siguen vivas si app_main retorna.
Led green(kLedGreen);
Led red(kLedRed);
Led blue(kLedBlue);
Button okButton(kButtonOk);
Button pieceButton(kButtonPiece);
Button emergencyButton(kButtonEmergency);
StepperDriver stepper(kStepperIn1, kStepperIn2, kStepperIn3, kStepperIn4);
JoystickAxis joystick(kJoystickYChannel);
UartLink link({kUartPort, kUartTx, kUartRx, kUartBaud});

Belt belt;
QueueHandle_t eventQueue;
QueueHandle_t navQueue;
SemaphoreHandle_t beltMutex;
// NOTE: motor y seguridad escriben los mismos 4 GPIO. Sin este mutex, una
// emergencia a mitad de step() podía volver a energizar las bobinas.
// Nunca se toma junto con beltMutex.
SemaphoreHandle_t coilMutex;
SemaphoreHandle_t emergencySem;

static void IRAM_ATTR emergencyIsr(void*) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(emergencySem, &woken);
    if (woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void safetyTask(void*) {
    while (true) {
        xSemaphoreTake(emergencySem, portMAX_DELAY);
        // NOTE: motor y seguridad escriben los mismos 4 GPIO. Sin este mutex, una
        // emergencia a mitad de step() podía volver a energizar las bobinas.
        // Nunca se toma junto con beltMutex.
        xSemaphoreTake(coilMutex, portMAX_DELAY);
        stepper.release();
        xSemaphoreGive(coilMutex);

        BeltEvent event = BeltEvent::Emergency;
        // NOTE: va al final de la cola. Adelantarla dejaría un Start ya encolado
        // para después, y ese Start saldría de emergencia. El corte de las
        // bobinas no depende de la cola.
        xQueueSend(eventQueue, &event, portMAX_DELAY);
    }
}

static void beltTask(void*) {
    while (true) {
        BeltEvent event;
        xQueueReceive(eventQueue, &event, portMAX_DELAY);
        xSemaphoreTake(beltMutex, portMAX_DELAY);
        belt = apply(belt, event);
        xSemaphoreGive(beltMutex);
    }
}

static int stepPeriodMs(Speed speed) {
    switch (speed) {
    case Speed::Low:
        return kLowMs;
    case Speed::Medium:
        return kMediumMs;
    case Speed::High:
        return kHighMs;
    }
    return kLowMs;
}

static void motorTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    while (true) {
        xSemaphoreTake(beltMutex, portMAX_DELAY);
        Belt snapshot = belt;
        xSemaphoreGive(beltMutex);

        int periodMs = kIdleMs;
        xSemaphoreTake(coilMutex, portMAX_DELAY);
        if (snapshot.state == BeltState::Running) {
            // WARNING: si la emergencia sigue accionada (o ya lo está al encender),
            // ARRANCAR vuelve a marcha igual. No hay enclavamiento en este prototipo.
            stepper.step();
            periodMs = stepPeriodMs(snapshot.speed);
        } else {
            stepper.release();
        }
        xSemaphoreGive(coilMutex);

        if (snapshot.state == BeltState::Running) {
            green.on();
            red.off();
        } else if (snapshot.state == BeltState::Emergency) {
            green.off();
            red.on();
        } else {
            green.off();
            red.off();
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(periodMs));
    }
}

static void joystickTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    int previousZone = 0;
    bool okSeen = false;
    bool okArmed = false;
    while (true) {
        int raw = joystick.readRaw();
        int zone = 0;
        if (raw < kJoyUpBelow) {
            zone = -1;
        } else if (raw > kJoyDownAbove) {
            zone = 1;
        }
        // Solo el flanco al salir del centro. Quedarse arriba no repite el $2.
        if (zone != previousZone && zone != 0) {
            NavCode nav = zone < 0 ? NavCode::Up : NavCode::Down;
            if (xQueueSend(navQueue, &nav, 0) != pdTRUE) {
                ESP_LOGW(kTag, "cola de navegación llena, se pierde un movimiento");
            }
        }
        previousZone = zone;

        // Dos muestras seguidas: el período de 20 ms filtra el rebote del OK.
        bool pressed = okButton.isPressed();
        if (pressed && okSeen && !okArmed) {
            NavCode nav = NavCode::Enter;
            if (xQueueSend(navQueue, &nav, 0) != pdTRUE) {
                ESP_LOGW(kTag, "cola de navegación llena, se pierde un movimiento");
            }
            okArmed = true;
        }
        if (!pressed) {
            okArmed = false;
        }
        okSeen = pressed;
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(kInputPeriodMs));
    }
}

static void pieceTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    int stableMs = 0;
    bool counted = false;
    while (true) {
        if (pieceButton.isPressed()) {
            stableMs += kPiecePeriodMs;
            if (!counted && stableMs >= kPieceStableMs) {
                BeltEvent event = BeltEvent::Piece;
                if (xQueueSend(eventQueue, &event, pdMS_TO_TICKS(kQueueWaitMs)) != pdTRUE) {
                    ESP_LOGW(kTag, "cola de eventos llena, se pierde una pieza");
                }
                counted = true;
            }
        } else {
            stableMs = 0;
            counted = false;
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(kPiecePeriodMs));
    }
}

static void uartRxTask(void*) {
    char line[kLineCapacity];
    while (true) {
        // false: la línea no entraba en 64 caracteres. No es un comando.
        if (!link.readLine(line, kLineCapacity)) {
            continue;
        }
        BeltEvent event;
        if (!interpret(line, event)) {
            continue;
        }
        if (xQueueSend(eventQueue, &event, pdMS_TO_TICKS(kQueueWaitMs)) != pdTRUE) {
            ESP_LOGW(kTag, "cola de eventos llena, se pierde un comando");
        }
    }
}

static void uartTxTask(void*) {
    TickType_t nextState = xTaskGetTickCount() + pdMS_TO_TICKS(kStatePeriodMs);
    char line[kLineCapacity];
    while (true) {
        TickType_t now = xTaskGetTickCount();
        // NOTE: comparar ticks con > falla cuando el contador da la vuelta
        // (~49 días a 1000 Hz). Para el prototipo no importa.
        TickType_t wait = nextState > now ? nextState - now : 0;
        NavCode nav;
        if (xQueueReceive(navQueue, &nav, wait) == pdTRUE) {
            if (writeNavLine(line, kLineCapacity, nav)) {
                link.write(line);
            }
        }
        now = xTaskGetTickCount();
        if (now >= nextState) {
            xSemaphoreTake(beltMutex, portMAX_DELAY);
            Belt snapshot = belt;
            xSemaphoreGive(beltMutex);
            if (writeStateLine(line, kLineCapacity, snapshot)) {
                link.write(line);
            }
            nextState += pdMS_TO_TICKS(kStatePeriodMs);
        }
    }
}

static void heartbeatTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    bool on = false;
    while (true) {
        on = !on;
        if (on) {
            blue.on();
        } else {
            blue.off();
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(kHeartbeatMs));
    }
}

}  // namespace

extern "C" void app_main(void) {
    green.init();
    red.init();
    blue.init();
    okButton.init();
    pieceButton.init();
    emergencyButton.init();
    stepper.init();
    joystick.init();
    link.init();

    eventQueue = xQueueCreate(kEventQueueLength, sizeof(BeltEvent));
    navQueue = xQueueCreate(kNavQueueLength, sizeof(NavCode));
    beltMutex = xSemaphoreCreateMutex();
    coilMutex = xSemaphoreCreateMutex();
    emergencySem = xSemaphoreCreateBinary();
    configASSERT(eventQueue != nullptr);
    configASSERT(navQueue != nullptr);
    configASSERT(beltMutex != nullptr);
    configASSERT(coilMutex != nullptr);
    configASSERT(emergencySem != nullptr);

    // NOTE: núcleo 1 en todas. En el otro núcleo la prioridad no ordena al motor
    // respecto de seguridad, y step() podría pisar el release().
    // Stacks en bytes: 3072 donde hay snprintf o ESP_LOGW; 2048 en el resto.
    // Prioridad 5: suelta las bobinas antes que cualquier otro trabajo.
    BaseType_t created = xTaskCreatePinnedToCore(
        safetyTask, "seguridad", kStackSmall, nullptr, 5, nullptr, kCore);
    configASSERT(created == pdPASS);
    // Prioridad 4: publica el estado antes de que el motor dé el próximo paso.
    created = xTaskCreatePinnedToCore(
        beltTask, "cinta", kStackSmall, nullptr, 4, nullptr, kCore);
    configASSERT(created == pdPASS);
    // Prioridad 3: lee el estado ya publicado y recién ahí da un paso.
    created = xTaskCreatePinnedToCore(
        motorTask, "motor", kStackSmall, nullptr, 3, nullptr, kCore);
    configASSERT(created == pdPASS);
    // Prioridad 2: entradas. Ninguna tiene que ganarle a las otras.
    created = xTaskCreatePinnedToCore(
        joystickTask, "joystick", kStackWithLog, nullptr, 2, nullptr, kCore);
    configASSERT(created == pdPASS);
    created = xTaskCreatePinnedToCore(
        pieceTask, "piezas", kStackWithLog, nullptr, 2, nullptr, kCore);
    configASSERT(created == pdPASS);
    created = xTaskCreatePinnedToCore(
        uartRxTask, "uart_rx", kStackWithLog, nullptr, 2, nullptr, kCore);
    configASSERT(created == pdPASS);
    // Prioridad 1: el estado y el latido pueden esperar.
    created = xTaskCreatePinnedToCore(
        uartTxTask, "uart_tx", kStackWithLog, nullptr, 1, nullptr, kCore);
    configASSERT(created == pdPASS);
    created = xTaskCreatePinnedToCore(
        heartbeatTask, "latido", kStackSmall, nullptr, 1, nullptr, kCore);
    configASSERT(created == pdPASS);

    // NOTE: Button::init deja la interrupción apagada. El flanco se configura
    // acá y se habilita después de registrar el handler.
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_set_intr_type(kButtonEmergency, GPIO_INTR_NEGEDGE));
    ESP_ERROR_CHECK(gpio_isr_handler_add(kButtonEmergency, emergencyIsr, nullptr));
    ESP_ERROR_CHECK(gpio_intr_enable(kButtonEmergency));
}
