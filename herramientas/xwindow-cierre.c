// xWindow de la cátedra no mira el clic de cerrar: solo el mouse y el teclado.
// Esta biblioteca se carga encima y hace que esa X termine el proceso.
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdlib.h>

#include <X11/Xlib.h>

static Atom delete_atom;
static int atom_ready;

static void arm(Display* display, Window window) {
    delete_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &delete_atom, 1);
    atom_ready = 1;
}

Window XCreateSimpleWindow(Display* display, Window parent, int x, int y,
                           unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border,
                           unsigned long background) {
    static Window (*real_create)(Display*, Window, int, int, unsigned int,
                                 unsigned int, unsigned int, unsigned long,
                                 unsigned long) = 0;
    if (real_create == 0) {
        real_create = dlsym(RTLD_NEXT, "XCreateSimpleWindow");
    }
    Window window = real_create(display, parent, x, y, width, height,
                                border_width, border, background);
    arm(display, window);
    return window;
}

int XNextEvent(Display* display, XEvent* event) {
    static int (*real_next)(Display*, XEvent*) = 0;
    if (real_next == 0) {
        real_next = dlsym(RTLD_NEXT, "XNextEvent");
    }
    int result = real_next(display, event);
    if (atom_ready && event->type == ClientMessage &&
        (Atom)event->xclient.data.l[0] == delete_atom) {
        exit(0);
    }
    return result;
}
