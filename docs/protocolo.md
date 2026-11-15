# Protocolo de la cinta

Este texto queda entre los drivers y FreeRTOS. [UartLink](drivers.md) solo mueve líneas: no conoce estos números. Quién llama a `interpret` y quién escribe el puerto está en [freertos.md](freertos.md).

El contrato está en [esp32/main/domain/protocol.hpp](../esp32/main/domain/protocol.hpp). La ESP32 y la Raspberry incluyen ese header. El número empieza en 1: el 0 no es un mensaje, así un buffer vacío no se lee como una acción.

La línea es texto, una por mensaje, empieza con `$` y termina en `\n`. No tiene checksum. Entran como máximo 64 caracteres (`kLineCapacity`). Lo que no empieza con `$` se ignora: así un log del monitor no es un comando.

Hay dos clases. **Información**, la manda la ESP32. **Acción**, la manda la Raspberry. Solo una acción puede volverse un `BeltEvent`, y eso ocurre en un único `switch`, `interpret`, en [esp32/main/domain/protocol.cpp](../esp32/main/domain/protocol.cpp).

## Números

| Tipo | Número | Dirección | Línea | Significado |
|---|---|---|---|---|
| `MessageType::State` | 1 | ESP32 → RPi | `$1,<estado>,<velocidad>,<piezas>` | Información. No es una orden |
| `MessageType::Nav` | 2 | ESP32 → RPi | `$2,<nav>` | Información. No es una orden |
| `MessageType::Command` | 3 | RPi → ESP32 | `$3,<comando>` | Acción |
| `MessageType::Speed` | 4 | RPi → ESP32 | `$4,<velocidad>` | Acción |
| `MessageType::ResetPieces` | 5 | RPi → ESP32 | `$5` | Acción |

| Código | Número | En el mensaje |
|---|---|---|
| `BeltState::Stopped` | 1 | estado, parada |
| `BeltState::Running` | 2 | estado, marcha |
| `BeltState::Emergency` | 3 | estado, emergencia |
| `Speed::Low` | 1 | velocidad baja |
| `Speed::Medium` | 2 | velocidad media |
| `Speed::High` | 3 | velocidad alta |
| `NavCode::Up` | 1 | arriba |
| `NavCode::Down` | 2 | abajo |
| `NavCode::Enter` | 3 | enter |
| `CommandCode::Start` | 1 | arrancar |
| `CommandCode::Stop` | 2 | parar |

Las piezas no son un código: viajan como la cuenta. `$1,2,2,4` es marcha, velocidad media, 4 piezas.

## Qué hace cada acción

| Línea | `BeltEvent` |
|---|---|
| `$3,1` | `Start` |
| `$3,2` | `Stop` |
| `$4,1` | `SetLow` |
| `$4,2` | `SetMedium` |
| `$4,3` | `SetHigh` |
| `$5` | `ResetPieces` |

`$1,...` y `$2,...` recibidos en la ESP32 no cambian el evento: son información que sale, no una orden. Un código que no está en el enum tampoco.

Al armar una línea se escribe el valor del mismo enum. No hay una segunda tabla de números. `writeStateLine` y `writeNavLine` arman la información. `writeCommandLine`, `writeSpeedLine` y `writeResetLine` arman `$3,<comando>`, `$4,<velocidad>` y `$5`. Terminan en `\n` y devuelven false si no entran en el buffer.

`readStateLine` hace el camino inverso de `$1`: exige `$`, tipo estado y exactamente tres campos, y llena el `Belt`. El estado y la velocidad tienen que ser un valor de `BeltState` y `Speed` (1, 2 o 3). Si la línea no es un `$1` válido, devuelve false y el `Belt` queda como estaba. No llama a `apply`.

## Cable del banco

115200 8N1. El protocolo sale por UART2, TX GPIO17 y RX GPIO16 (`kUartGpio` en [pins.hpp](../esp32/main/drivers/pins.hpp)). En la Pi el puente abre `/dev/ttyS0`. Para probar por USB en la PC se cambia `kUart` a `kUartUsb` (UART0, `/dev/ttyACM0`) y se flashea. Con el USB no se abre el monitor, porque el puerto lo usa el puente.
