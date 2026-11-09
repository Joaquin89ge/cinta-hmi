# Toolchain del proyecto. La base ya trae ESP-IDF, CMake, g++ y esptool.
# Acá se suman el cross de la Raspberry y X11 para la ventana de prueba en la PC.
FROM espressif/idf:v6.1

ARG HOST_UID=1000
ARG HOST_GID=1000

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        g++-aarch64-linux-gnu \
        libc6-dev-arm64-cross \
        libx11-dev \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

# La imagen de Espressif ya trae el usuario ubuntu (uid 1000) en el grupo dialout.
# Si el uid de quien construye es otro, se crea hostuser con ese uid.
RUN set -eux; \
    if id -u "${HOST_UID}" >/dev/null 2>&1; then \
        usermod -aG dialout "$(id -nu "${HOST_UID}")"; \
    else \
        if ! getent group "${HOST_GID}" >/dev/null; then \
            groupadd -g "${HOST_GID}" hostuser; \
        fi; \
        host_group="$(getent group "${HOST_GID}" | cut -d: -f1)"; \
        useradd -m -u "${HOST_UID}" -g "${host_group}" -G dialout --no-log-init hostuser; \
    fi
