#define GUILITE_ON
#include "pantalla.hpp"

#include "fifos.hpp"
#include "protocol.hpp"

#include <cstdio>

extern const LATTICE_FONT_INFO Consolas_24B;

namespace {

// Medidas del menú.
constexpr short kMargin = 16;
constexpr short kRowHeight = 48;
constexpr short kGap = 8;
constexpr short kButtonWidth = 280;
// El estado en emergencia va en rojo.
constexpr unsigned int kEmergencyColor = GL_RGB(220, 40, 40);

// IDs de esta ventana. Las flechas del spin (0x1111 y 0x2222) no se reutilizan.
enum {
    ID_MAIN = 1,
    ID_TITLE,
    ID_STATE,
    ID_SPEED,
    ID_PIECES,
    ID_START,
    ID_STOP,
    ID_SPIN,
};

c_surface* surface = nullptr;
c_display* display = nullptr;
// Último estado que llegó. Antes del primer mensaje la cinta figura parada.
Belt storedBelt;
// Ventana que usan los FIFO para mover el foco. Queda en null hasta que open terminó.
c_wnd* connected = nullptr;
MainScreen mainScreen;

const char* stateName(BeltState state) {
    switch (state) {
    case BeltState::Stopped:
        return "PARADA";
    case BeltState::Running:
        return "MARCHA";
    case BeltState::Emergency:
        return "EMERGENCIA";
    }
    return "PARADA";
}

const char* speedName(Speed speed) {
    switch (speed) {
    case Speed::Low:
        return "BAJA";
    case Speed::Medium:
        return "MEDIA";
    case Speed::High:
        return "ALTA";
    }
    return "BAJA";
}

void send(CommandCode command) {
    char line[kLineCapacity];
    if (!writeCommandLine(line, kLineCapacity, command)) {
        std::fprintf(stderr, "No entra el comando en la línea\n");
        return;
    }
    writeOrder(line);
}

void sendSpeed(Speed speed) {
    char line[kLineCapacity];
    if (!writeSpeedLine(line, kLineCapacity, speed)) {
        std::fprintf(stderr, "No entra la velocidad en la línea\n");
        return;
    }
    writeOrder(line);
}

// Letra y colores de botones y etiquetas, una sola vez antes de dibujar.
void loadTheme() {
    c_theme::add_font(FONT_DEFAULT, &Consolas_24B);
    c_theme::add_color(COLOR_WND_FONT, GL_RGB(255, 255, 243));
    c_theme::add_color(COLOR_WND_NORMAL, GL_RGB(59, 75, 94));
    c_theme::add_color(COLOR_WND_PUSHED, GL_RGB(33, 42, 53));
    c_theme::add_color(COLOR_WND_FOCUS, GL_RGB(78, 198, 76));
    c_theme::add_color(COLOR_WND_BORDER, GL_RGB(46, 59, 73));
}

}  // namespace

void mainLoop(void* phy_fb, int width, int height, int color_bytes) {
    // Tema, después la superficie y recién ahí el menú.
    loadTheme();
    surface = new c_surface(kScreenWidth, kScreenHeight, static_cast<unsigned int>(color_bytes), Z_ORDER_LEVEL_2);
    // Vive todo el proceso: cada pixel sale por acá.
    display = new c_display(phy_fb, width, height, surface);
    openFifos();
    mainScreen.open(surface);
    connected = &mainScreen;
    // Espera líneas del puente; el enter lo atiende el botón.
    while (true) {
        pollFifos();
    }
}

// Guardo el estado y, si el menú ya está abierto, lo redibujo.
void showState(Belt belt) {
    storedBelt = belt;
    if (connected == &mainScreen) {
        mainScreen.paintBelt(storedBelt);
    }
}

// Los FIFO la piden para llamar on_navigate.
c_wnd* connectedScreen() {
    return connected;
}

// Armo la lista de controles, los conecto a la ventana y los muestro.
void MainScreen::open(c_surface* next) {
    // Dónde dibuja esta ventana (el lienzo 512×512 de mainLoop).
    set_surface(next);
    // Solo llena tree_ con posiciones; los widgets aún no están en el árbol GuiLite.
    fillTree();
    // connect (c_wnd, de GuiLite): registra MainScreen como raíz (parent nullptr,
    // pantalla entera) y crea cada hijo leyendo tree_. Ahí corre pre_create_wnd
    // y on_init_children (callbacks de botones, foco en ARRANCAR).
    connect(nullptr, ID_MAIN, nullptr, 0, 0, kScreenWidth, kScreenHeight, tree_);
    // Primer dibujado: fondo y recorrido de hijos con show_window.
    show_window();
}

void MainScreen::pre_create_wnd() {
    // disconnect deja m_attr en 0.
    // ATTR_VISIBLE: show_window dibuja. ATTR_FOCUS: on_navigate mueve el foco.
    m_attr = static_cast<WND_ATTRIBUTION>(ATTR_VISIBLE | ATTR_FOCUS);
    m_bg_color = c_theme::get_color(COLOR_WND_NORMAL);
}

void MainScreen::on_init_children() {
    // Ya están creados: les digo qué llamar al apretarlos y dejo el foco en ARRANCAR.
    // El click nace en cero, por eso el callback va acá.
    start_.set_on_click(static_cast<WND_CALLBACK>(&MainScreen::onStart));
    stop_.set_on_click(static_cast<WND_CALLBACK>(&MainScreen::onStop));
    speedSpin_.set_on_change(static_cast<WND_CALLBACK>(&MainScreen::onSpeed));
    speedSpin_.set_max_min(static_cast<short>(Speed::High), static_cast<short>(Speed::Low));
    speedSpin_.set_step(1);
    paintBelt(storedBelt);
    focusFirst();
}

// Fondo de la ventana. Las etiquetas se pintan solas encima.
void MainScreen::on_paint() {
    c_rect rect;
    get_screen_rect(rect);
    m_surface->fill_rect(rect, c_theme::get_color(COLOR_WND_NORMAL), static_cast<unsigned int>(m_z_order));
}

void MainScreen::paintBelt(const Belt& belt) {
    // Armo los tres textos.
    std::snprintf(stateText_, kTextCapacity, "Estado: %s", stateName(belt.state));
    std::snprintf(speedText_, kTextCapacity, "Velocidad: %s", speedName(belt.speed));
    std::snprintf(piecesText_, kTextCapacity, "Piezas: %d", belt.pieces);
    // set_str guarda el puntero: el texto vive en estos miembros.
    state_.set_str(stateText_);
    speed_.set_str(speedText_);
    pieces_.set_str(piecesText_);
    // En emergencia el estado va en rojo.
    unsigned int color = c_theme::get_color(COLOR_WND_FONT);
    if (belt.state == BeltState::Emergency) {
        color = kEmergencyColor;
    }
    state_.set_font_color(color);
    // set_value no llama a on_change: cambio el número sin mandar comando.
    speedSpin_.set_value(static_cast<unsigned short>(belt.speed));
    // connected es this recién al terminar open. Antes, show_window no dibuja.
    if (connected != this) {
        return;
    }
    state_.show_window();
    speed_.show_window();
    pieces_.show_window();
    speedSpin_.show_window();
}

// Mando arrancar.
void MainScreen::onStart(int, int) {
    send(CommandCode::Start);
}

// Mando parar.
void MainScreen::onStop(int, int) {
    send(CommandCode::Stop);
}

// Mando la velocidad nueva.
void MainScreen::onSpeed(int, int value) {
    if (value < static_cast<int>(Speed::Low) || value > static_cast<int>(Speed::High)) {
        return;
    }
    sendSpeed(static_cast<Speed>(value));
}

void MainScreen::fillTree() {
    const short labelWidth = static_cast<short>(kScreenWidth - 2 * kMargin);
    const short buttonX = static_cast<short>((kScreenWidth - kButtonWidth) / 2);
    short y = kMargin;

    // De arriba a abajo: título, datos, botones y el spin.
    tree_[0] = {&title_, ID_TITLE, "CINTA L1", kMargin, y, labelWidth, kRowHeight, nullptr};
    y = static_cast<short>(y + kRowHeight);
    tree_[1] = {&state_, ID_STATE, stateText_, kMargin, y, labelWidth, kRowHeight, nullptr};
    y = static_cast<short>(y + kRowHeight);
    tree_[2] = {&speed_, ID_SPEED, speedText_, kMargin, y, labelWidth, kRowHeight, nullptr};
    y = static_cast<short>(y + kRowHeight);
    tree_[3] = {&pieces_, ID_PIECES, piecesText_, kMargin, y, labelWidth, kRowHeight, nullptr};
    y = static_cast<short>(y + kRowHeight + kMargin);
    tree_[4] = {&start_, ID_START, "ARRANCAR", buttonX, y, kButtonWidth, kRowHeight, nullptr};
    y = static_cast<short>(y + kRowHeight + kGap);
    tree_[5] = {&stop_, ID_STOP, "PARAR", buttonX, y, kButtonWidth, kRowHeight, nullptr};
    y = static_cast<short>(y + kRowHeight + kGap);
    // Va último: + y − se cuelgan del padre al crearlo y quedan al final.
    // El número no tiene foco, así que el orden es ARRANCAR, PARAR, +, −.
    tree_[6] = {&speedSpin_, ID_SPIN, nullptr, buttonX, y, kButtonWidth, static_cast<short>(kRowHeight * 2), nullptr};
    // El {} final marca que no hay más controles.
    tree_[7] = {};
}

// Las etiquetas no tienen foco: el primero es ARRANCAR.
void MainScreen::focusFirst() {
    c_wnd* child = m_top_child;
    while (child != nullptr) {
        if (child->is_focus_wnd()) {
            set_child_focus(child);
            return;
        }
        child = child->get_next_sibling();
    }
}
