# cinta-hmi

HMI de una cinta transportadora simulada. Trabajo práctico de Sistemas Operativos en Tiempo Real (UNLaM): una ESP32 con FreeRTOS y una pantalla en una Raspberry Pi con el Linux mínimo de la cátedra (EOS).

| Documento | Qué explica |
|---|---|
| [docs/contexto.md](docs/contexto.md) | Qué se construye, el cableado y las decisiones |
| [docs/entorno.md](docs/entorno.md) | Compilar, flashear y probar en esta PC |
| [docs/raspberry.md](docs/raspberry.md) | La SD, el EOS y el arranque en la Pi |
| [docs/protocolo.md](docs/protocolo.md) | Las líneas `$` entre la ESP32, el puente y la HMI |
| [docs/freertos.md](docs/freertos.md) | Tasks, colas, semáforo y mutex |
| [docs/drivers.md](docs/drivers.md) | LED, pulsadores, motor y UART |
| [docs/maquina-estados.md](docs/maquina-estados.md) | `apply`: estados, velocidad y piezas |
