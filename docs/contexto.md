# Contexto del proyecto: cinta-hmi

> Este archivo es contexto, no una regla. Explica qué se construye y por qué.
> Las decisiones marcadas como **abierta** se pueden discutir: si ves algo mejor, proponelo.
> Si una decisión cambia, actualizá este archivo.

## Qué es

TP del segundo parcial de Sistemas Operativos en Tiempo Real (UNLaM).
Una HMI para una **cinta transportadora simulada**: una ESP32 con FreeRTOS maneja la "cinta"
y una Raspberry Pi 4 con EOS (Linux mínimo de la cátedra) muestra un menú en un monitor por framebuffer.

Es un MVP: lo mínimo que cumple la consigna, bien hecho y fácil de defender.

## Qué evalúa la cátedra

- Temas: scripts en Linux, C/C++, IPC, acceso a hardware, memoria y filesystem, framebuffer, FreeRTOS.
- Rúbrica: cantidad de componentes distintos de GuiLite, mecanismos de IPC usados,
  opciones del script de compilación, y modificaciones en vivo pedidas por el docente.
- La GUI se navega con "adelante", "atrás" y "enter" recibidos por named pipe.
- En la defensa tiene que correr en la RPi real con EOS, conectada a la ESP32 y a un monitor.

## Arquitectura

```
ESP32 (FreeRTOS)
  pulsadores, motor, LEDs
        │ UART2, GPIO17/16 (en la Pi: /dev/ttyS0)
        ▼
proceso puente (Linux) ──► /tmp/fifo_nav   ──► proceso HMI (GuiLite) ──► framebuffer ──► monitor
                       ──► /tmp/fifo_datos ──►
                       ◄── /tmp/fifo_cmd   ◄──
```

Para revisar el diseño antes de seguir: [drivers.md](drivers.md), [protocolo.md](protocolo.md) y [freertos.md](freertos.md).

## Stack

| Parte | Tecnología |
|---|---|
| Firmware | ESP-IDF, C++ liviano, FreeRTOS sin envoltorios |
| HMI | C++ con GuiLite, partiendo del ejemplo de la cátedra (`main.cpp`, `mvc.cpp`, `xWindow`) |
| Puente serie | C++, POSIX (`termios`, `poll`) |
| Build | CMake en todo (ESP-IDF ya lo usa); presets `pc` y `rpi` para la HMI |
| Simulación en PC | Ubuntu + `xWindow` (framebuffer simulado de la cátedra) |
| RPi | EOS en arm64; cross-compilado con `aarch64-linux-gnu-g++`, linkeo estático |
| Entorno | Contenedor `cinta-hmi-dev:v6.1` (ESP-IDF v6.1). Ver `docs/entorno.md` |

## Hardware (todo en protoboard)

| Elemento | Pin ESP32 | Notas |
|---|---|---|
| Motor 28BYJ-48 + placa ULN2003 | GPIO18, 19, 21, 22 | Medio paso; placa alimentada desde VIN (5 V) |
| Pulsador arriba | GPIO32 | A GND, pull-up interno. Sube en el menú |
| Pulsador abajo | GPIO33 | A GND, pull-up interno. Baja en el menú |
| Pulsador OK | GPIO25 | A GND, pull-up interno |
| Pulsador de pieza | GPIO26 | Pull-up interno; cada pulsación suma una pieza |
| Pulsador de emergencia | GPIO27 | Pull-up interno + interrupción |
| LED verde (marcha) | GPIO23 | Con resistencia de 220 Ω |
| LED rojo (emergencia) | GPIO4 | Con resistencia de 220 Ω |
| LED azul de la placa | GPIO2 | Latido: indica que el programa corre |
| UART a la RPi | TX GPIO17, RX GPIO16 | Al RX/TX de la Pi (pin 10 y pin 8), con GND. En la Pi es `/dev/ttyS0` |

Evitar GPIO6 a 11 (flash) y los strapping pins (0, 2, 12, 15) como entradas.

## Comportamiento

- Estados: PARADA ↔ MARCHA. Desde cualquiera, el pulsador de emergencia lleva a EMERGENCIA.
  Desde EMERGENCIA se sale con ARRANCAR desde el menú.
- Velocidad en 3 niveles: BAJA, MEDIA y ALTA (tiempo entre medios pasos de ~8, ~4 y ~2 ms).
- FreeRTOS con tick a 1000 Hz (`configTICK_RATE_HZ` en menuconfig) para poder dar pasos de milisegundos.
- La máquina de estados es una función pura (`enum` + `switch`) sin acceso a hardware.
  Transiciones, diagrama y prueba en la PC: [maquina-estados.md](maquina-estados.md).

## Tasks

El detalle de colas, semáforos y mutex está en [freertos.md](freertos.md). El código está en `esp32/main/app_main.cpp`. Comparten el núcleo 1: en dos núcleos la prioridad no ordena al motor respecto de seguridad, y un paso podría pisar el corte de las bobinas.

| Task / ISR | Prioridad | Rol |
|---|---|---|
| ISR emergencia | interrupción | Da un semáforo a la task de seguridad |
| seguridad | 5 | Suelta las bobinas y encola EMERGENCIA |
| cinta | 4 | Única que llama a `apply`; publica el estado |
| motor | 3 | Da pasos según el estado y la velocidad |
| nav | 2 | Lee los tres pulsadores cada 20 ms y manda navegación |
| piezas | 2 | Lee el pulsador con antirrebote y cuenta |
| uart_rx | 2 | Recibe comandos de la HMI y los pone en una cola |
| uart_tx | 1 | Manda el estado cada 100 ms y la navegación |
| latido | 1 | Conmuta el LED azul |

El `Belt` va bajo un mutex. Otro mutex, `coilMutex`, protege los cuatro GPIO del motor: lo usan la task de seguridad y la del motor, y no se toma junto con el del `Belt`.

## Protocolo UART (texto, una línea por mensaje)

Los números son los enum de [protocolo.md](protocolo.md). El 0 no se usa.

| Dirección | Mensaje |
|---|---|
| ESP32 → RPi | `$1,<estado>,<velocidad>,<piezas>` |
| ESP32 → RPi | `$2,<nav>` |
| RPi → ESP32 | `$3,<comando>` |
| RPi → ESP32 | `$4,<velocidad>` |
| RPi → ESP32 | `$5` |

El puente ignora las líneas que no empiezan con `$` (así los logs de depuración no molestan). La ESP32 manda el `$1` cada 100 ms. El enlace es UART2, TX GPIO17 y RX GPIO16, para no pisar la consola de UART0. En la Pi se enciende con `enable_uart=1` y el puente abre `/dev/ttyS0`. El USB (`kUartUsb`, `/dev/ttyACM0`) queda solo para probar en la PC, y hay que flashear para volver.

## Pantallas

Una sola pantalla. Arriba la información y abajo las acciones, en este orden visual y de foco:

1. Etiquetas: estado, velocidad y piezas. En emergencia el estado va en rojo.
2. ARRANCAR (`$3,1`) y PARAR (`$3,2`).
3. Spin de velocidad. El número es el nivel (1 baja, 2 media, 3 alta) y no toma foco. «+» sube un nivel y «−» lo baja, sin pasar de 3 ni de 1, y mandan `$4`.

El foco baja así: ARRANCAR, PARAR, «+», «−».

## Decisiones

| Decisión | Estado |
|---|---|
| ESP32 + RPi con EOS, GuiLite, navegación por named pipe | firme (consigna) |
| FIFOs como mecanismo de IPC | firme |
| FreeRTOS llamado directo, sin envoltorios | firme |
| CMake en todo | firme |
| Toolchain en el contenedor `cinta-hmi-dev` (ESP-IDF v6.1) | firme |
| C++ liviano en la ESP32 | firme (confirmar con la cátedra que se acepta en lugar de .c) |
| Motor paso a paso 28BYJ-48 | firme |
| Protocolo de texto sin checksum | abierta |
| Velocidad en 3 niveles en vez de continua | abierta |
| Conexión ESP32–RPi por UART2 (GPIO17/16, `/dev/ttyS0`) | firme. El USB no: el EOS no trae el driver del CH9102 |

## Fuera de alcance por ahora

Control PID, encoder, gráficos de respuesta, log a archivo, más pantallas.
Tampoco el enclavamiento de la emergencia (ARRANCAR sale aunque el pulsador siga apretado).
Pueden venir después del MVP; no adelantarlos.

## Fechas

- Martes 10/11: consulta en el laboratorio (probar en la RPi real).
- Lunes 16/11, 13:00: entrega por MIeL y defensa.
