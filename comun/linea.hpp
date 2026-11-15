#pragma once

// Arma líneas de un fd no bloqueante. Lo usan la HMI y el puente; la ESP32 no.

#include <cerrno>
#include <cstdio>
#include <string>

#include <unistd.h>

constexpr std::size_t kMaxDeliveredLine = 64;
constexpr int kReadChunk = 256;

struct Channel {
    int fd;
    const char* path;
    std::string pending;
    void (*onLine)(const std::string&);
};

inline void drain(Channel& ch) {
    char chunk[kReadChunk];
    ssize_t n = 0;
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
                if (!ch.pending.empty() && ch.pending.size() < kMaxDeliveredLine) {
                    ch.onLine(ch.pending);
                }
                ch.pending.clear();
                continue;
            }
            // Una línea sin \n puede crecer; el puente siempre manda \n.
            ch.pending.push_back(c);
        }
    }
    if (n < 0 && errno != EAGAIN && errno != EINTR) {
        std::perror(ch.path);
    }
}
