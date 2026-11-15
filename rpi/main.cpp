#include "pantalla.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <unistd.h>

namespace {

enum FramebufferMode {
    FB_NULL_MODE,
    FB_APP_MODE,
    FB_DEV_MODE,
};

const char* kHelp =
    "Uso:\n"
    "  cinta_hmi shared-fb\n"
    "  cinta_hmi /dev/fb0\n";

// xWindow crea la memoria compartida. Tamaño 0 engancha la que ya existe,
// no pide otra. Se reintenta porque este proceso puede arrancar antes que la ventana.
void* getSharedFramebuffer(int sharedId) {
    while (true) {
        int shmid = shmget(sharedId, 0, SHM_R | SHM_W);
        if (shmid >= 0) {
            void* attached = shmat(shmid, nullptr, 0);
            // shmat devuelve -1, no null, cuando falla.
            if (attached != reinterpret_cast<void*>(-1)) {
                return attached;
            }
            std::perror("shmat shared-fb");
        } else {
            std::perror("shmget shared-fb");
        }
        sleep(1);
    }
}

void* getDeviceFramebuffer(const char* path, int& width, int& height, int& colorBytes) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        std::perror(path);
        _exit(EXIT_FAILURE);
    }

    struct fb_var_screeninfo info;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) < 0) {
        std::perror(path);
        _exit(EXIT_FAILURE);
    }

    width = static_cast<int>(info.xres);
    height = static_cast<int>(info.yres);
    colorBytes = static_cast<int>(info.bits_per_pixel) / 8;
    if ((width & 0x3) != 0) {
        std::printf("Advertencia: xres=%d no es múltiplo de 4\n", width);
    }
    std::printf("xres=%u yres=%u bits_per_pixel=%u\n", info.xres, info.yres, info.bits_per_pixel);

    std::size_t bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(colorBytes);
    void* fb = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        std::perror(path);
        _exit(EXIT_FAILURE);
    }
    std::memset(fb, 0, bytes);
    return fb;
}

}  // namespace

int main(int argc, char** argv) {
    std::printf("%s", kHelp);

    // 2 bytes por pixel: el RGB565 de xWindow y del framebuffer de la cátedra.
    int colorBytes = 2;
    int screenWidth = kScreenWidth;
    int screenHeight = kScreenHeight;
    FramebufferMode mode = FB_NULL_MODE;
    const char* fbPath = nullptr;
    // El mismo id que usa xWindow.
    const int sharedId = 1;

    if (argc == 2) {
        if (std::strcmp(argv[1], "shared-fb") == 0) {
            mode = FB_APP_MODE;
        } else if (std::strncmp(argv[1], "/dev", 4) == 0) {
            fbPath = argv[1];
            mode = FB_DEV_MODE;
        } else {
            std::printf("Argumento inválido: shared-fb o una ruta /dev/fb*\n");
            return 1;
        }
    }

    // shared-fb: ventana de la PC. /dev/fb0: monitor de la Raspberry.
    // Sin argumento se reserva memoria y no se ve nada.
    void* fb = nullptr;
    switch (mode) {
    case FB_APP_MODE:
        fb = getSharedFramebuffer(sharedId);
        break;
    case FB_DEV_MODE:
        fb = getDeviceFramebuffer(fbPath, screenWidth, screenHeight, colorBytes);
        break;
    default:
        fb = std::calloc(static_cast<std::size_t>(screenWidth) * static_cast<std::size_t>(screenHeight), static_cast<std::size_t>(colorBytes));
        break;
    }

    // La superficie queda en 512x512. screenWidth solo describe el framebuffer.
    mainLoop(fb, screenWidth, screenHeight, colorBytes);
    return 0;
}
