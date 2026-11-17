// Une la UART con los tres FIFO de la HMI.
// En esta PC, con el firmware en kUartUsb: ./puente /dev/ttyACM0
// En la Raspberry, con kUartGpio:          ./puente /dev/ttyS0
// La HMI no cambia. El dispositivo es el único argumento.

#include "linea.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

namespace {

constexpr const char* kNavPath = "/tmp/fifo_nav";
constexpr const char* kDataPath = "/tmp/fifo_datos";
constexpr const char* kCmdPath = "/tmp/fifo_cmd";
constexpr speed_t kBaud = B115200;

int serialFd = -1;
int navFd = -1;
int dataFd = -1;
int cmdFd = -1;

Channel serialChannel;
Channel cmdChannel;

void writeLine(int fd, const char* path, const std::string& text) {
    std::string out = text + "\n";
    if (write(fd, out.data(), out.size()) < 0) {
        if (errno == EAGAIN) {
            std::fprintf(stderr, "%s: lleno, se pierde la línea\n", path);
            return;
        }
        std::perror(path);
    }
}

int openFifo(const char* path) {
    if (mkfifo(path, 0666) < 0 && errno != EEXIST) {
        std::perror(path);
        std::exit(EXIT_FAILURE);
    }
    // O_RDWR: no se bloquea si la HMI todavía no abrió el otro extremo.
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        std::perror(path);
        std::exit(EXIT_FAILURE);
    }
    return fd;
}

int openSerial(const char* path) {
    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        std::perror(path);
        std::exit(EXIT_FAILURE);
    }

    termios config {};
    if (tcgetattr(fd, &config) < 0) {
        std::perror(path);
        std::exit(EXIT_FAILURE);
    }
    // Sin cocción del tty: llegan los bytes del protocolo, no líneas editadas.
    cfmakeraw(&config);
    cfsetispeed(&config, kBaud);
    cfsetospeed(&config, kBaud);
    config.c_cflag |= CLOCAL | CREAD;
    config.c_cflag &= ~CSTOPB;
    config.c_cflag &= ~PARENB;
    config.c_cflag &= ~CRTSCTS;
    // El read no espera: quien espera es poll.
    config.c_cc[VMIN] = 0;
    config.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &config) < 0) {
        std::perror(path);
        std::exit(EXIT_FAILURE);
    }
    tcflush(fd, TCIOFLUSH);
    return fd;
}

void onSerialLine(const std::string& line) {
    // Lo que no empieza con $ es un log. No va a la HMI.
    // $1 se copia tal cual. $2 se traduce: arriba es atras, abajo es adelante, OK es enter.
    if (line.rfind("$1,", 0) == 0) {
        writeLine(dataFd, kDataPath, line);
        return;
    }
    if (line == "$2,1") {
        std::fprintf(stderr, "nav atras\n");
        writeLine(navFd, kNavPath, "atras");
    } else if (line == "$2,2") {
        std::fprintf(stderr, "nav adelante\n");
        writeLine(navFd, kNavPath, "adelante");
    } else if (line == "$2,3") {
        std::fprintf(stderr, "nav enter\n");
        writeLine(navFd, kNavPath, "enter");
    }
}

void onCommandLine(const std::string& line) {
    // La HMI ya mandó $3, $4 o $5. Se le agrega el salto y se va a la placa.
    std::fprintf(stderr, "cmd %s\n", line.c_str());
    writeLine(serialFd, "serie", line);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Uso: puente <dispositivo>\n");
        std::fprintf(stderr, "  PC, firmware en USB: puente /dev/ttyACM0\n");
        std::fprintf(stderr, "  Raspberry:           puente /dev/ttyS0\n");
        return 1;
    }

    serialFd = openSerial(argv[1]);
    navFd = openFifo(kNavPath);
    dataFd = openFifo(kDataPath);
    cmdFd = openFifo(kCmdPath);

    serialChannel.path = argv[1];
    serialChannel.fd = serialFd;
    serialChannel.onLine = onSerialLine;
    cmdChannel.path = kCmdPath;
    cmdChannel.fd = cmdFd;
    cmdChannel.onLine = onCommandLine;

    std::fprintf(stderr, "puente en %s, 115200. No abras el monitor.\n", argv[1]);

    // La serie va hacia la HMI; fifo_cmd, hacia la ESP32.
    // -1: este lazo espera a que alguno de los dos tenga bytes. EINTR es una señal.
    while (true) {
        pollfd fds[2];
        fds[0].fd = serialFd;
        fds[0].events = POLLIN;
        fds[1].fd = cmdFd;
        fds[1].events = POLLIN;

        int ready = poll(fds, 2, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("poll");
            return 1;
        }
        if ((fds[0].revents & POLLIN) != 0) {
            drain(serialChannel);
        }
        if ((fds[1].revents & POLLIN) != 0) {
            drain(cmdChannel);
        }
    }
}
