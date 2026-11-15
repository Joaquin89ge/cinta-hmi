#include "fifos.hpp"

#include "linea.hpp"
#include "pantalla.hpp"
#include "protocol.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace {

constexpr const char* kNavPath = "/tmp/fifo_nav";
constexpr const char* kDataPath = "/tmp/fifo_datos";
constexpr const char* kCmdPath = "/tmp/fifo_cmd";

int navFd = -1;
int dataFd = -1;
int cmdFd = -1;
Channel navChannel;
Channel dataChannel;

int openFifo(const char* path) {
    // O_RDWR: open no espera al otro proceso, y el read no ve EOF si ese cierra.
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        std::perror(path);
        std::exit(EXIT_FAILURE);
    }
    return fd;
}

// Palabras del puente a las teclas de GuiLite.
// adelante baja el foco, atras lo sube, enter aprieta el botón enfocado.
void onNavLine(const std::string& line) {
    c_wnd* screen = connectedScreen();
    if (screen == nullptr) {
        return;
    }
    if (line == "adelante") {
        screen->on_navigate(NAV_FORWARD);
        return;
    }
    if (line == "atras") {
        screen->on_navigate(NAV_BACKWARD);
        return;
    }
    if (line == "enter") {
        screen->on_navigate(NAV_ENTER);
    }
}

void onDataLine(const std::string& line) {
    Belt belt;
    if (!readStateLine(line.c_str(), belt)) {
        return;
    }
    showState(belt);
}

}  // namespace

void openFifos() {
    navFd = openFifo(kNavPath);
    dataFd = openFifo(kDataPath);
    cmdFd = openFifo(kCmdPath);
    navChannel.path = kNavPath;
    navChannel.fd = navFd;
    navChannel.onLine = onNavLine;
    dataChannel.path = kDataPath;
    dataChannel.fd = dataFd;
    dataChannel.onLine = onDataLine;
}

// Solo se leen navegación y estado. Las órdenes salen por writeOrder.
// -1 frena a la HMI hasta que llegue una línea: la pantalla no tiene otro reloj.
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
        drain(navChannel);
    }
    if ((fds[1].revents & POLLIN) != 0) {
        drain(dataChannel);
    }
}

void writeOrder(const char* line) {
    if (line == nullptr || line[0] == '\0') {
        return;
    }
    // La orden es corta: un write alcanza. Si el FIFO está lleno, se pierde.
    ssize_t written = write(cmdFd, line, std::strlen(line));
    if (written < 0) {
        if (errno == EAGAIN) {
            std::fprintf(stderr, "%s: el FIFO está lleno, se pierde la orden\n", kCmdPath);
            return;
        }
        std::perror(kCmdPath);
    }
}
