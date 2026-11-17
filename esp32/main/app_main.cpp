#include "belt.hpp"
#include "button.hpp"
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

namespace {

// Tiempo entre medios pasos, según la velocidad. Parada o en emergencia
// el motor igual despierta cada 20 ms, para ver si ya hay que arrancar.
// El $1 hacia la HMI sale cada 100 ms. El LED azul cambia cada 400 ms.
constexpr int kLowMs = 8;
constexpr int kMediumMs = 4;
constexpr int kHighMs = 2;
constexpr int kIdleMs = 20;
constexpr int kStatePeriodMs = 100;
constexpr int kHeartbeatMs = 400;
constexpr int kInputPeriodMs = 20;
constexpr int kPiecePeriodMs = 10;
constexpr int kPieceStableMs = 30;
constexpr int kEventQueueLength = 8;
constexpr int kNavQueueLength = 4;
constexpr int kCore = 1;
constexpr int kStackSmall = 2048;
constexpr int kStackWithLog = 3072;
// sscanf de newlib entra en uart_rx.
constexpr int kUartRxStack = 4096;
constexpr int kQueueWaitMs = 20;
constexpr const char* kTag = "cinta";

// NOTE: estáticos, no locales de app_main. Las tasks siguen vivas si app_main retorna.
Led green(kLedGreen);
Led red(kLedRed);
Led blue(kLedBlue);
Button upButton(kButtonUp);
Button downButton(kButtonDown);
Button okButton(kButtonOk);
Button pieceButton(kButtonPiece);
Button emergencyButton(kButtonEmergency);
StepperDriver stepper(kStepperIn1, kStepperIn2, kStepperIn3, kStepperIn4);
// El cable se elige en pins.hpp: kUartUsb en la PC, kUartGpio en la Raspberry.
UartLink link({kUart.port, kUart.tx, kUart.rx, kUartBaud});

Belt belt;
// eventQueue cambia la cinta: órdenes de la HMI, piezas y emergencia.
// navQueue solo se reenvía: arriba, abajo y OK no pasan por apply.
QueueHandle_t eventQueue;
QueueHandle_t navQueue;
// beltMutex protege la copia de belt. Se toma, se copia y se suelta:
// el paso del motor no lo retiene.
SemaphoreHandle_t beltMutex;
// NOTE: motor y seguridad escriben los mismos 4 GPIO. Sin este mutex, una
// emergencia a mitad de step() podía volver a energizar las bobinas.
// Nunca se toma junto con beltMutex.
SemaphoreHandle_t coilMutex;
// Binario: la ISR lo da y safetyTask lo espera. No guarda cuántas veces se apretó.
SemaphoreHandle_t emergencySem;

// Solo entrega el semáforo. El corte de las bobinas lo hace safetyTask.
// Si esa task estaba dormida, el yield la despierta ahora.
static void IRAM_ATTR emergencyIsr(void*) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(emergencySem, &woken);
    if (woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

// Corta las bobinas y encola la emergencia.
static void safetyTask(void*) {
    while (true) {
        xSemaphoreTake(emergencySem, portMAX_DELAY);
        // Mismo mutex que el motor.
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

// Aplica el evento y publica el Belt nuevo.
static void beltTask(void*) {
    while (true) {
        BeltEvent event;
        xQueueReceive(eventQueue, &event, portMAX_DELAY);
        xSemaphoreTake(beltMutex, portMAX_DELAY);
        belt = apply(belt, event);
        xSemaphoreGive(beltMutex);
    }
}

// Baja 8 ms, media 4, alta 2. Si la velocidad no cierra, queda la lenta.
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

// Da un paso o suelta las bobinas, y prende el LED del estado.
// La copia sale del mutex antes de tocar los GPIO: beltTask tiene que
// poder publicar aunque el paso tarde.
static void motorTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    while (true) {
        xSemaphoreTake(beltMutex, portMAX_DELAY);
        Belt snapshot = belt;
        xSemaphoreGive(beltMutex);

        int periodMs = kIdleMs;
        xSemaphoreTake(coilMutex, portMAX_DELAY);
        if (snapshot.state == BeltState::Running) {
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
        // DelayUntil: el período se mide desde el despertar anterior, no desde ahora.
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(periodMs));
    }
}

// Dos muestras seguidas, separadas 20 ms, filtran el rebote.
// upArmed, downArmed y okArmed evitan un segundo $2 mientras sigue apretado.
// El mismo flanco está escrito tres veces: cada pulsador se explica solo.
static void navTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    bool upSeen = false;
    bool upArmed = false;
    bool downSeen = false;
    bool downArmed = false;
    bool okSeen = false;
    bool okArmed = false;
    while (true) {
        // seen es la muestra anterior. armed corta la repetición mientras sigue apretado.
        bool pressed = upButton.isPressed();
        if (pressed && upSeen && !upArmed) {
            NavCode nav = NavCode::Up;
            // 0: si la cola está llena no espero. Se pierde este toque.
            if (xQueueSend(navQueue, &nav, 0) != pdTRUE) {
                ESP_LOGW(kTag, "cola de navegación llena, se pierde un movimiento");
            }
            upArmed = true;
        }
        if (!pressed) {
            upArmed = false;
        }
        upSeen = pressed;

        pressed = downButton.isPressed();
        if (pressed && downSeen && !downArmed) {
            NavCode nav = NavCode::Down;
            if (xQueueSend(navQueue, &nav, 0) != pdTRUE) {
                ESP_LOGW(kTag, "cola de navegación llena, se pierde un movimiento");
            }
            downArmed = true;
        }
        if (!pressed) {
            downArmed = false;
        }
        downSeen = pressed;

        pressed = okButton.isPressed();
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

// Una pulsación, una pieza. counted no se suelta hasta que el botón vuelve
// a arriba: dejarlo apretado no suma de a una cada 10 ms.
static void pieceTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    int stableMs = 0;
    bool counted = false;
    while (true) {
        if (pieceButton.isPressed()) {
            // Hace falta verlo apretado ~30 ms seguidos. Un rebote corto no cuenta.
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

// interpret descarta logs, $1 y $2. Acá solo entran órdenes de la HMI
// ($3 arrancar/parar, $4 velocidad, $5 reset) y se encolan para beltTask.
static void uartRxTask(void*) {
    char line[kLineCapacity];
    while (true) {
        // false: la línea no entraba en 64 caracteres. No es un comando.
        if (!link.readLine(line, kLineCapacity)) {
            continue;
        }
        BeltEvent event;
        // false: era un log, un $1, un $2 o un número que no existe.
        if (!interpret(line, event)) {
            continue;
        }
        if (xQueueSend(eventQueue, &event, pdMS_TO_TICKS(kQueueWaitMs)) != pdTRUE) {
            ESP_LOGW(kTag, "cola de eventos llena, se pierde un comando");
        }
    }
}

// Espera en la cola de navegación hasta el próximo $1. Si llega un pulsador
// antes, el $2 sale enseguida y el $1 sigue en su período de 100 ms.
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
            // Copio y suelto: el $1 no se arma con el mutex tomado.
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

// LED azul: el firmware está vivo.
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
    // Pines, motor y UART. Las tasks arrancan después, con esto ya listo.
    green.init();
    red.init();
    blue.init();
    upButton.init();
    downButton.init();
    okButton.init();
    pieceButton.init();
    emergencyButton.init();
    stepper.init();
    link.init();

    // Colas y semáforos.
    eventQueue = xQueueCreate(kEventQueueLength, sizeof(BeltEvent));
    navQueue = xQueueCreate(kNavQueueLength, sizeof(NavCode));
    beltMutex = xSemaphoreCreateMutex();
    coilMutex = xSemaphoreCreateMutex();
    emergencySem = xSemaphoreCreateBinary();
    // Si falló un create, no sigo: sin cola o sin mutex la cinta no tiene sentido.
    configASSERT(eventQueue != nullptr);
    configASSERT(navQueue != nullptr);
    configASSERT(beltMutex != nullptr);
    configASSERT(coilMutex != nullptr);
    configASSERT(emergencySem != nullptr);

    // NOTE: núcleo 1 en todas. En el otro núcleo la prioridad no ordena al motor
    // respecto de seguridad, y step() podría pisar el release().
    // Stacks en bytes: 4096 en uart_rx; 3072 donde hay snprintf o ESP_LOGW; 2048 en el resto.
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
        navTask, "nav", kStackWithLog, nullptr, 2, nullptr, kCore);
    configASSERT(created == pdPASS);
    created = xTaskCreatePinnedToCore(
        pieceTask, "piezas", kStackWithLog, nullptr, 2, nullptr, kCore);
    configASSERT(created == pdPASS);
    created = xTaskCreatePinnedToCore(
        uartRxTask, "uart_rx", kUartRxStack, nullptr, 2, nullptr, kCore);
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
