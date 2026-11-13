# Entorno de desarrollo

Todo se compila dentro de la imagen `cinta-hmi-dev:v6.1`. La base es `espressif/idf:v6.1`, que ya trae ESP-IDF, CMake 4.0.3, `g++` y `esptool`. El Dockerfile le suma el cross-compiler `aarch64-linux-gnu-g++` y las cabeceras de X11.

La ventana de la HMI se ve en este escritorio: el contenedor usa el display de la sesión (`DISPLAY` y el socket de X11). El puerto de la ESP32 se le pasa cuando está enchufada.

## Una vez por máquina

Hace falta Docker, y el usuario en los grupos `docker` y `dialout`. Después de agregarlos hay que cerrar la sesión de Ubuntu y volver a entrar.

```bash
sudo usermod -aG docker,dialout "$USER"
```

La imagen oficial pesa unos 13 GB y ya está bajada. La nuestra se construye encima:

```bash
herramientas/construir-imagen.sh
```

## Día a día

| Acción | Comando |
|---|---|
| Compilar el firmware | `herramientas/compilar-fw.sh` |
| Flashear la ESP32 | `herramientas/flashear.sh` |
| Ver la consola serie | `herramientas/monitor.sh` |
| Otro `idf.py` | `herramientas/idf.sh menuconfig` |
| Compilar la HMI en la PC | `herramientas/compilar-hmi-pc.sh` |
| Abrir esa HMI | `herramientas/correr-hmi-pc.sh` |
| Binario estático para la Raspberry | `herramientas/compilar-hmi-rpi.sh` |
| Probar el ejemplo de la cátedra | `herramientas/probar-tablero.sh` |
| Verificar el entorno | `herramientas/verificar.sh` |
| Compilar la prueba de banco | `herramientas/banco.sh build` |
| Flashear esa prueba | `herramientas/banco.sh -p /dev/ttyACM0 flash` |
| Probar la lógica de la cinta | `cmake -S logica -B logica/build && cmake --build logica/build && ./logica/build/test_belt` |

El puerto por defecto es `/dev/ttyACM0` (el CH9102 de esta placa enumera así). Si aparece en otro nodo: `CINTA_PUERTO=/dev/ttyACM1 herramientas/flashear.sh`.

El firmware está en `esp32/` y la HMI en `rpi/`. `compilar-hmi-pc.sh` y `compilar-hmi-rpi.sh` entran al contenedor y compilan con los presets de CMake `pc` y `rpi`. Si falta `rpi/CMakeLists.txt`, avisan y salen. El preset `rpi` usa `cmake/toolchain-rpi.cmake` y deja el ejecutable estático.

## Qué no está en el git

`copiaTotalMateria/` es el volcado local de la cátedra. Ahí está el ejemplo `Tablero` (`main.cpp`, `mvc.cpp`, `GuiLite.h`, `Consolas_24B.cpp` y el binario `xWindow`). `probar-tablero.sh` lo compila y abre la ventana; no se sube al repo.

## Raspberry

En casa no hay Pi. `compilar-hmi-rpi.sh` deja el binario en `rpi/build/rpi/`. Ese binario estático se copia a la EOS para la defensa del lunes 16 y se corre contra `/dev/fb0`.

`correr-hmi-pc.sh` deja `/tmp/fifo_nav`, `/tmp/fifo_datos` y `/tmp/fifo_cmd` en esta máquina, no solo dentro del contenedor. Desde otra terminal: `echo adelante > /tmp/fifo_nav` o `echo '$1,2,2,4' > /tmp/fifo_datos`.
