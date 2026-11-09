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
  joystick, pulsadores, motor, LEDs
        │ UART (USB en desarrollo; GPIO como plan B)
        ▼
proceso puente (Linux) ──► /tmp/fifo_nav   ──► proceso HMI (GuiLite) ──► framebuffer ──► monitor
                       ──► /tmp/fifo_datos ──►
                       ◄── /tmp/fifo_cmd   ◄──
```

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
| Joystick eje Y | GPIO34 (ADC1) | Alimentado a 3,3 V. Arriba/abajo navegan el menú |
| Joystick botón (OK) | GPIO25 | Pull-up interno |
| Pulsador de pieza | GPIO26 | Pull-up interno; cada pulsación suma una pieza |
| Pulsador de emergencia | GPIO27 | Pull-up interno + interrupción |
| LED verde (marcha) | GPIO23 | Con resistencia de 220 Ω |
| LED rojo (emergencia) | GPIO4 | Con resistencia de 220 Ω |
| LED azul de la placa | GPIO2 | Latido: indica que el programa corre |
| UART a la RPi (plan B) | TX GPIO17, RX GPIO16 | A RX/TX del GPIO de la RPi, con GND común |

Evitar GPIO6 a 11 (flash) y los strapping pins (0, 2, 12, 15) como entradas.

## Comportamiento

- Estados: PARADA ↔ MARCHA. Desde cualquiera, el pulsador de emergencia lleva a EMERGENCIA.
  Desde EMERGENCIA se sale con ARRANCAR desde el menú.
- Velocidad en 3 niveles: BAJA, MEDIA y ALTA (tiempo entre medios pasos de ~8, ~4 y ~2 ms).
- FreeRTOS con tick a 1000 Hz (`configTICK_RATE_HZ` en menuconfig) para poder dar pasos de milisegundos.
- La máquina de estados es una función pura (`enum` + `switch`) sin acceso a hardware.

## Tasks previstas

| Task / ISR | Prioridad | Rol |
|---|---|---|
| ISR emergencia | interrupción | Da un semáforo a la task de seguridad |
| seguridad | 5 | Frena el motor y pasa a EMERGENCIA |
| motor | 4 | Da pasos según el estado y la velocidad |
| joystick | 2 | Lee el ADC y el botón cada 20 ms y manda navegación |
| piezas | 2 | Lee el pulsador con antirrebote y cuenta |
| uart_rx | 2 | Recibe comandos de la HMI y los pone en una cola |
| uart_tx | 1 | Manda el estado periódicamente |

Datos compartidos protegidos con un mutex.

## Protocolo UART (texto, una línea por mensaje)

| Dirección | Mensaje |
|---|---|
| ESP32 → RPi | `$EST,<estado>,<velocidad>,<piezas>` |
| ESP32 → RPi | `$NAV,ARRIBA` / `$NAV,ABAJO` / `$NAV,ENTER` |
| RPi → ESP32 | `$CMD,START` / `$CMD,STOP` |
| RPi → ESP32 | `$VEL,BAJA` / `$VEL,MEDIA` / `$VEL,ALTA` |
| RPi → ESP32 | `$RST_PZ` |

El puente ignora las líneas que no empiezan con `$` (así los logs de depuración no molestan).

## Pantallas

1. **Principal**: estado, velocidad, piezas; botones ARRANCAR, PARAR y AJUSTES.
2. **Ajustes**: velocidad (spin box), RESET PIEZAS y VOLVER.

En emergencia, el estado se muestra en rojo.

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
| Conexión ESP32–RPi por USB o por GPIO | abierta (depende de que EOS reconozca el CH9102). En la PC de desarrollo ese USB aparece como `/dev/ttyACM0` |

## Fuera de alcance por ahora

Control PID, encoder, gráficos de respuesta, log a archivo, más pantallas.
Pueden venir después del MVP; no adelantarlos.

## Fechas

- Martes 10/11: consulta en el laboratorio (probar en la RPi real).
- Lunes 16/11, 13:00: entrega por MIeL y defensa.
