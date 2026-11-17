#pragma once

// Arma líneas de un fd no bloqueante. Lo usan la HMI y el puente; la ESP32 no.

#include <cerrno>
#include <cstdio>
#include <string>

#include <unistd.h>

constexpr std::size_t kMaxDeliveredLine = 64;
constexpr int kReadChunk = 256;

// Un FIFO que leo por pedazos. pending guarda la línea a medias entre un read y el siguiente.
// onLine es qué hacer cuando ya llegó el \n: navegación o estado, según el canal.
struct Channel {
    int fd;
    const char* path;
    std::string pending;
    void (*onLine)(const std::string&);
};

inline void drain(Channel& ch) {
    char chunk[kReadChunk];
    ssize_t n = 0;
    // Sigue mientras el read traiga bytes. 0 o negativo: no hay más por ahora.
    while (true) {
        n = read(ch.fd, chunk, sizeof(chunk));
        if (n <= 0) {
            break;
        }
        for (ssize_t i = 0; i < n; i++) {
            char c = chunk[i];
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                // Línea vacía o de 64 o más: se tira entera, no se entrega el pedazo.
                if (!ch.pending.empty() && ch.pending.size() < kMaxDeliveredLine) {
                    ch.onLine(ch.pending);
                }
                ch.pending.clear();
                continue;
            }
            // Todavía no hay \n. Lo guardo para el próximo read.
            ch.pending.push_back(c);
        }
    }
    // EAGAIN y EINTR son normales: el poll vuelve a llamar. Otro error sí se avisa.
    if (n < 0 && errno != EAGAIN && errno != EINTR) {
        std::perror(ch.path);
    }
}
