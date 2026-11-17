# Raspberry y la SD

La Pi no compila. En esta PC se cruzan dos binarios estáticos y se copian, junto con el sistema de la cátedra, a la tarjeta. El detalle de cómo se compilan está en [entorno.md](entorno.md). El cable y el protocolo están en [contexto.md](contexto.md) y [protocolo.md](protocolo.md).

## El sistema

EOS es el Linux mínimo de la cátedra, armado con [RPIBusybox](https://github.com/edgardogho/RPIBusybox). No hace falta correr `buildEOS.sh`: el árbol ya armado se descarga del Drive que indica ese repositorio. Al descomprimirlo aparecen `part/bootfs` y `part/rootfs`.

La Raspberry solo arranca desde una partición FAT. El `cmdline.txt` de la cátedra pide la raíz en la segunda partición, ext4: `root=/dev/mmcblk0p2 rootfstype=ext4`, con la consola en el monitor (`console=tty1`).

| Partición | Tamaño | Tipo | Etiqueta | Qué hay |
|---|---|---|---|---|
| 1 | 512 MB | FAT32 | `BOOTFS` | `start4.elf`, `kernel8.img` (Pi 3 y 4), `kernel_2712.img` (Pi 5), `config.txt` |
| 2 | el resto | ext4 | `ROOTFS` | BusyBox, módulos del kernel y `/root` |

`config.txt` lleva `arm_64bit=1` y `enable_uart=1`. Esa segunda línea deja los pines físicos 8 y 10 como `/dev/ttyS0`. La raíz tiene que ser ext4: BusyBox es un binario y `sh`, `ls` y `mkfifo` son enlaces a él. En una FAT esos enlaces no existen.

Al encender, `/etc/init.d/rcS` monta `/tmp` en memoria, `/proc` y `/sys`, y deja la raíz escribible. La consola espera Enter. El puerto serie no se usa como consola, así que queda libre para el puente.

## Qué se copia a `/root`

| Archivo en la Pi | De dónde sale |
|---|---|
| `/root/cinta_hmi` | `rpi/build/rpi/cinta_hmi` |
| `/root/puente` | `puente/build/rpi/puente` |
| `/root/correr` | `herramientas/correr/correr` |

Los tres van con permiso de ejecución. En la Pi, después de Enter:

```sh
/root/correr
```

Esa ruta se puede escribir desde cualquier carpeta. `./correr` solo funciona después de `cd /root`.

El script crea tres named pipes, abre el puente en segundo plano sobre `/dev/ttyS0` y deja la HMI en primer plano sobre `/dev/fb0`. Ctrl+C cierra los dos. Los pipes no se guardan en la tarjeta: `/tmp` se vacía al apagar.

| Pipe | Quién escribe | Qué lleva |
|---|---|---|
| `/tmp/fifo_nav` | puente | `adelante`, `atras`, `enter` |
| `/tmp/fifo_datos` | puente | el estado `$1,...` |
| `/tmp/fifo_cmd` | HMI | las órdenes `$3` y `$4` |

La HMI elige la pantalla con el argumento. `shared-fb` es la ventana de la PC. `/dev/fb0` es el monitor de la Pi. El mismo binario ARM tiene los dos caminos. El script le pasa `/dev/fb0`.

## Cable

TX GPIO17 de la ESP32 al pin 10 de la Pi (RX). RX GPIO16 al pin 8 (TX). GND con GND. Los dos lados son 3,3 V.

El firmware tiene que estar flasheado con `kUart = kUartGpio` en `esp32/main/drivers/pins.hpp`. Ese es el valor del fuente. Si la placa se flasheó antes para probar por USB, hay que volver a flashear: el cambio del fuente no llega solo a la ESP32.

## Volver a armar la tarjeta

Esto borra la SD entera. El disco tiene que ser el lector de la tarjeta, no un disco interno.

```bash
sudo umount /dev/mmcblk0p1 /dev/mmcblk0p2
sudo parted /dev/mmcblk0 --script mklabel msdos \
    mkpart primary fat32 1MiB 513MiB \
    mkpart primary ext4 513MiB 100% \
    set 1 boot on
sudo mkfs.vfat -F 32 -n BOOTFS /dev/mmcblk0p1
sudo mkfs.ext4 -L ROOTFS /dev/mmcblk0p2
sudo mount /dev/mmcblk0p1 /mnt/boot
sudo mount /dev/mmcblk0p2 /mnt/root
sudo tar -xzf eos.tgz -C /mnt/boot --strip-components=2 --no-same-owner part/bootfs
sudo tar -xzf eos.tgz -C /mnt/root --strip-components=2 part/rootfs
printf '\nenable_uart=1\n' | sudo tee -a /mnt/boot/config.txt
sudo cp rpi/build/rpi/cinta_hmi puente/build/rpi/puente herramientas/correr/correr /mnt/root/root/
sudo chmod 755 /mnt/root/root/cinta_hmi /mnt/root/root/puente /mnt/root/root/correr
sync
sudo umount /mnt/boot /mnt/root
```

`parted` deja la primera partición como FAT32 de arranque. El `tar` de la raíz se copia preservando los enlaces de BusyBox. El de la boot usa `--no-same-owner` porque una FAT no guarda el dueño. `eos.tgz` no está en el git.
