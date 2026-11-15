// Une la UART con los tres FIFO de la HMI.
// En esta PC, con el firmware en kUartUsb: ./puente /dev/ttyACM0
// En la Raspberry, con kUartGpio:          ./puente /dev/ttyS0
// La HMI no cambia. El dispositivo es el único argumento.

#include "linea.hpp"
#include "protocol.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

void writeAll(int fd, const char* path, const char* data, int length) {
    ssize_t written = write(fd, data, static_cast<size_t>(length));
    if (written < 0) {
        if (errno == EAGAIN) {
            std::fprintf(stderr, "%s: lleno, se pierde la línea\n", path);
            return;
        }
        std::perror(path);
    }
}

void writeText(int fd, const char* path, const char* text) {
    writeAll(fd, path, text, static_cast<int>(std::strlen(text)));
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

// Fin, \n, \r, o \r\n. Un carácter de más no es esta línea.
bool tailOk(const char* line, int got, int fields, char tail, int at) {
    if (got < fields || at < 0) {
        return false;
    }
    if (line[at] == '\0') {
        return got == fields;
    }
    if (got != fields + 1) {
        return false;
    }
    if (tail == '\n') {
        return line[at + 1] == '\0';
    }
    if (tail == '\r') {
        return line[at + 1] == '\0' || (line[at + 1] == '\n' && line[at + 2] == '\0');
    }
    return false;
}

void onSerialLine(const std::string& line) {
    // Los logs de la consola comparten el USB en esta prueba. No empiezan con $.
    // El $1 se copia igual a fifo_datos. El $2 se traduce a las palabras de la HMI.
    if (line.empty() || line[0] != '$') {
        return;
    }
    const char* text = line.c_str();
    int type = 0;
    int state = 0;
    int speed = 0;
    int pieces = 0;
    char tail = '\0';
    int at = -1;
    int got = std::sscanf(text, "$%d,%d,%d,%d%n%c", &type, &state, &speed, &pieces, &at, &tail);
    if (tailOk(text, got, 4, tail, at)
        && type == static_cast<int>(MessageType::State)
        && state >= static_cast<int>(BeltState::Stopped)
        && state <= static_cast<int>(BeltState::Emergency)
        && speed >= static_cast<int>(Speed::Low)
        && speed <= static_cast<int>(Speed::High)) {
        char out[kLineCapacity];
        int written = std::snprintf(out, sizeof(out), "%s\n", text);
        if (written > 0 && written < static_cast<int>(sizeof(out))) {
            writeAll(dataFd, kDataPath, out, written);
        }
        return;
    }

    // La HMI no entiende $2. Arriba es el ítem anterior (atras), abajo el siguiente
    // (adelante) y el pulsador OK es enter.
    int nav = 0;
    tail = '\0';
    at = -1;
    got = std::sscanf(text, "$%d,%d%n%c", &type, &nav, &at, &tail);
    if (!tailOk(text, got, 2, tail, at) || type != static_cast<int>(MessageType::Nav)) {
        return;
    }
    const char* word = nullptr;
    if (nav == static_cast<int>(NavCode::Up)) {
        word = "atras\n";
    } else if (nav == static_cast<int>(NavCode::Down)) {
        word = "adelante\n";
    } else if (nav == static_cast<int>(NavCode::Enter)) {
        word = "enter\n";
    }
    if (word != nullptr) {
        std::fprintf(stderr, "nav %s", word);
        writeText(navFd, kNavPath, word);
    }
}

// fifo_cmd ya trae un $3, $4 o $5. Se le agrega el salto y se manda a la ESP32.
void onCommandLine(const std::string& line) {
    char out[kLineCapacity];
    int written = std::snprintf(out, sizeof(out), "%s\n", line.c_str());
    if (written <= 0 || written >= static_cast<int>(sizeof(out))) {
        return;
    }
    std::fprintf(stderr, "cmd %s", out);
    writeAll(serialFd, "serie", out, written);
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
