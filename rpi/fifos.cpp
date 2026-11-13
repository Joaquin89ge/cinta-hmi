#include "fifos.hpp"

#include "pantalla.hpp"
#include "protocol.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace {

constexpr const char* kNavPath = "/tmp/fifo_nav";
constexpr const char* kDataPath = "/tmp/fifo_datos";
constexpr const char* kCmdPath = "/tmp/fifo_cmd";

struct LineBuffer {
    const char* path;
    int fd;
    char data[kLineCapacity];
    int length;
    bool discarding;
};

int navFd = -1;
int dataFd = -1;
int cmdFd = -1;
LineBuffer navLine;
LineBuffer dataLine;

int openFifo(const char* path) {
    // O_RDWR: open no espera al otro proceso, y el read no ve EOF si ese cierra.
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        std::perror(path);
        std::exit(EXIT_FAILURE);
    }
    return fd;
}

void takeLine(LineBuffer& line, void (*onLine)(const char*)) {
    if (line.length > 0 && line.data[line.length - 1] == '\r') {
        line.length -= 1;
    }
    line.data[line.length] = '\0';
    line.length = 0;
    if (line.data[0] != '\0') {
        onLine(line.data);
    }
}

void pushBytes(LineBuffer& line, const char* bytes, int count, void (*onLine)(const char*)) {
    for (int i = 0; i < count; i++) {
        char c = bytes[i];
        if (line.discarding) {
            if (c == '\n') {
                line.discarding = false;
            }
            continue;
        }
        if (c == '\n') {
            takeLine(line, onLine);
            continue;
        }
        // Sin '\n' no es una línea: se tira hasta el próximo salto.
        if (line.length >= kLineCapacity - 1) {
            line.discarding = true;
            line.length = 0;
            continue;
        }
        line.data[line.length] = c;
        line.length += 1;
    }
}

void drain(LineBuffer& line, void (*onLine)(const char*)) {
    char chunk[256];
    while (true) {
        ssize_t count = read(line.fd, chunk, sizeof(chunk));
        if (count < 0) {
            if (errno == EAGAIN) {
                return;
            }
            std::perror(line.path);
            return;
        }
        if (count == 0) {
            return;
        }
        pushBytes(line, chunk, static_cast<int>(count), onLine);
    }
}

void onNavLine(const char* line) {
    c_wnd* screen = connectedScreen();
    if (screen == nullptr) {
        return;
    }
    if (std::strcmp(line, "adelante") == 0) {
        screen->on_navigate(NAV_FORWARD);
        return;
    }
    if (std::strcmp(line, "atras") == 0) {
        screen->on_navigate(NAV_BACKWARD);
        return;
    }
    if (std::strcmp(line, "enter") == 0) {
        screen->on_navigate(NAV_ENTER);
    }
}

void onDataLine(const char* line) {
    Belt belt;
    if (!readStateLine(line, belt)) {
        return;
    }
    showState(belt);
}

}  // namespace

void openFifos() {
    navFd = openFifo(kNavPath);
    dataFd = openFifo(kDataPath);
    cmdFd = openFifo(kCmdPath);
    navLine.path = kNavPath;
    navLine.fd = navFd;
    dataLine.path = kDataPath;
    dataLine.fd = dataFd;
}

void pollFifos() {
    struct pollfd fds[2];
    fds[0].fd = navFd;
    fds[0].events = POLLIN;
    fds[1].fd = dataFd;
    fds[1].events = POLLIN;

    int ready = poll(fds, 2, -1);
    if (ready < 0) {
        if (errno == EINTR) {
            return;
        }
        std::perror("poll");
        return;
    }
    if ((fds[0].revents & POLLIN) != 0) {
        drain(navLine, onNavLine);
    }
    if ((fds[1].revents & POLLIN) != 0) {
        drain(dataLine, onDataLine);
    }
}

void writeOrder(const char* line) {
    if (line == nullptr || line[0] == '\0') {
        return;
    }
    int length = 0;
    while (line[length] != '\0') {
        length += 1;
    }
    // Menos de PIPE_BUF y un solo write no se mezclan. Si el kernel corta igual, se sigue.
    int sent = 0;
    while (sent < length) {
        ssize_t written = write(cmdFd, line + sent, static_cast<size_t>(length - sent));
        if (written < 0) {
            if (errno == EAGAIN) {
                std::fprintf(stderr, "%s: el FIFO está lleno, se pierde la orden\n", kCmdPath);
                return;
            }
            std::perror(kCmdPath);
            return;
        }
        if (written == 0) {
            std::fprintf(stderr, "%s: no se escribió la orden\n", kCmdPath);
            return;
        }
        sent += static_cast<int>(written);
    }
}
