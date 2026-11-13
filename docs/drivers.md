# Drivers de la protoboard

Un driver configura un pedazo de hardware y nada más. El constructor guarda pines o el canal. `init()` escribe los registros. Ninguno conoce `Belt`, `apply` ni FreeRTOS.

Los números de pin están en [esp32/main/drivers/pins.hpp](../esp32/main/drivers/pins.hpp). El driver los recibe: no los tiene escritos adentro.

[esp32/main/app_main.cpp](../esp32/main/app_main.cpp) crea las tasks de la cinta. La máquina de estados está en [esp32/main/domain](../esp32/main/domain) y la llama la task `cinta`. La prueba de cables (LED, motor, entradas) está en [test/](../test/).

## Led

[esp32/main/drivers/led.hpp](../esp32/main/drivers/led.hpp). `init`, `on`, `off`.

La resistencia va del GPIO al LED. En alto enciende, y `init` lo deja apagado. Hay tres objetos: verde GPIO23, rojo GPIO4 y azul GPIO2.

No parpadea solo. Quién lo prende y cada cuánto es de la task que lo use.

## Button

[esp32/main/drivers/button.hpp](../esp32/main/drivers/button.hpp). `init`, `isPressed`.

Entrada con pull-up interno. Suelto queda en alto. Apretado cierra a GND, así que `isPressed` es nivel bajo. Hay tres objetos: OK GPIO25, pieza GPIO26 y emergencia GPIO27.

No hace antirrebote y no instala interrupción. El rebote lo va a filtrar la task de piezas. La interrupción de emergencia la va a registrar la task de seguridad: el driver solo sabe leer el nivel.

## StepperDriver

[esp32/main/drivers/stepper.hpp](../esp32/main/drivers/stepper.hpp). `init`, `step`, `release`.

Motor 28BYJ-48 con la placa ULN2003, en GPIO18, 19, 21 y 22. `step` avanza un medio paso, en un solo sentido: la cinta no vuelve atrás. `release` pone las cuatro bobinas en bajo para que no calienten, y no reinicia la fase. El próximo `step` sigue la secuencia.

No espera entre pasos. El tiempo (~8, ~4 o ~2 ms) lo decide quien lo llama, según la velocidad.

## JoystickAxis

[esp32/main/drivers/joystick_axis.hpp](../esp32/main/drivers/joystick_axis.hpp). `init`, `readRaw`.

Lee el eje Y, GPIO34, canal 6 del ADC1. La atenuación es de 12 dB porque el joystick está alimentado a 3,3 V. `readRaw` devuelve la cuenta del conversor.

No decide arriba ni abajo. Esos umbrales son de la task de joystick.

## UartLink

[esp32/main/drivers/uart_link.hpp](../esp32/main/drivers/uart_link.hpp). `init`, `write`, `readLine`.

`UartConfig` lleva puerto, TX, RX y baud. En el banco es UART2, TX GPIO17, RX GPIO16, 115200 8N1. UART0 es la consola del CH9102 (`/dev/ttyACM0`): instalar el protocolo ahí la pisaría. Cambiar el cable es cambiar esas constantes.

`write` manda los bytes que le pasan, sin agregar el salto de línea. `readLine` bloquea hasta el `\n` y lo devuelve sin el salto. Si la línea no entra en 64 caracteres, descarta hasta el próximo `\n` y devuelve `false`.

No conoce `$1` ni `$3`. Esos números están en [protocolo.md](protocolo.md). Quién llama a `interpret` y quién escribe el puerto está en [freertos.md](freertos.md).
