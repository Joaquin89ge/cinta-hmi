#define GUILITE_ON
#include "pantalla.hpp"

#include "fifos.hpp"
#include "protocol.hpp"

#include <cstdio>

extern const LATTICE_FONT_INFO Consolas_24B;

namespace {

constexpr short kMargin = 16;
constexpr short kRowHeight = 48;
constexpr short kGap = 8;
constexpr short kButtonWidth = 280;
constexpr short kSpinWidth = 300;
constexpr unsigned int kEmergencyColor = GL_RGB(220, 40, 40);

enum {
    ID_MAIN = 1,
    ID_SETTINGS_ROOT,
    ID_TITLE,
    ID_STATE,
    ID_SPEED,
    ID_PIECES,
    ID_START,
    ID_STOP,
    ID_OPEN_SETTINGS,
    ID_SPEED_LABEL,
    ID_SPIN,
    ID_RESET,
    ID_BACK,
};

c_surface* surface = nullptr;
c_display* display = nullptr;
Belt storedBelt;
c_wnd* connected = nullptr;
MainScreen mainScreen;
SettingsScreen settingsScreen;

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

// Speed::Low es 0. El spin y el protocolo cuentan desde SpeedCode::Low, que es 1.
unsigned short speedLevel(Speed speed) {
    switch (speed) {
    case Speed::Low:
        return static_cast<unsigned short>(SpeedCode::Low);
    case Speed::Medium:
        return static_cast<unsigned short>(SpeedCode::Medium);
    case Speed::High:
        return static_cast<unsigned short>(SpeedCode::High);
    }
    return static_cast<unsigned short>(SpeedCode::Low);
}

void send(CommandCode command) {
    char line[kLineCapacity];
    if (!writeCommandLine(line, kLineCapacity, command)) {
        std::fprintf(stderr, "No entra el comando en la línea\n");
        return;
    }
    writeOrder(line);
}

void sendSpeed(SpeedCode speed) {
    char line[kLineCapacity];
    if (!writeSpeedLine(line, kLineCapacity, speed)) {
        std::fprintf(stderr, "No entra la velocidad en la línea\n");
        return;
    }
    writeOrder(line);
}

void sendReset() {
    char line[kLineCapacity];
    if (!writeResetLine(line, kLineCapacity)) {
        std::fprintf(stderr, "No entra el reset en la línea\n");
        return;
    }
    writeOrder(line);
}

void showMain() {
    if (connected == &settingsScreen) {
        connected = nullptr;
        settingsScreen.disconnect();
    }
    mainScreen.open(surface);
    connected = &mainScreen;
}

void showSettings() {
    if (connected == &mainScreen) {
        connected = nullptr;
        mainScreen.disconnect();
    }
    settingsScreen.open(surface);
    connected = &settingsScreen;
}

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
    loadTheme();
    surface = new c_surface(kScreenWidth, kScreenHeight, static_cast<unsigned int>(color_bytes), Z_ORDER_LEVEL_2);
    // Vive todo el proceso: cada pixel de la superficie sale por acá.
    display = new c_display(phy_fb, width, height, surface);
    openFifos();
    showMain();
    while (true) {
        pollFifos();
    }
}

void showState(Belt belt) {
    storedBelt = belt;
    if (connected == &mainScreen) {
        mainScreen.paintBelt(storedBelt);
    }
}

c_wnd* connectedScreen() {
    return connected;
}

void MainScreen::open(c_surface* next) {
    set_surface(next);
    fillTree();
    connect(nullptr, ID_MAIN, nullptr, 0, 0, kScreenWidth, kScreenHeight, tree_);
    show_window();
}

void MainScreen::pre_create_wnd() {
    // disconnect deja m_attr en 0. Sin ATTR_VISIBLE, show_window no dibuja.
    // Sin ATTR_FOCUS, on_navigate no mueve el foco.
    m_attr = static_cast<WND_ATTRIBUTION>(ATTR_VISIBLE | ATTR_FOCUS);
    m_bg_color = c_theme::get_color(COLOR_WND_NORMAL);
}

void MainScreen::on_init_children() {
    // pre_create_wnd del botón pone on_click en cero: el callback va después de connect.
    start_.set_on_click(static_cast<WND_CALLBACK>(&MainScreen::onStart));
    stop_.set_on_click(static_cast<WND_CALLBACK>(&MainScreen::onStop));
    settings_.set_on_click(static_cast<WND_CALLBACK>(&MainScreen::onSettings));
    paintBelt(storedBelt);
    focusFirst();
}

void MainScreen::on_paint() {
    c_rect rect;
    get_screen_rect(rect);
    m_surface->fill_rect(rect, c_theme::get_color(COLOR_WND_NORMAL), static_cast<unsigned int>(m_z_order));
}

void MainScreen::paintBelt(const Belt& belt) {
    std::snprintf(stateText_, kTextCapacity, "Estado: %s", stateName(belt.state));
    std::snprintf(speedText_, kTextCapacity, "Velocidad: %s", speedName(belt.speed));
    std::snprintf(piecesText_, kTextCapacity, "Piezas: %d", belt.pieces);
    // set_str guarda el puntero: el texto vive en estos miembros.
    state_.set_str(stateText_);
    speed_.set_str(speedText_);
    pieces_.set_str(piecesText_);
    unsigned int color = c_theme::get_color(COLOR_WND_FONT);
    if (belt.state == BeltState::Emergency) {
        color = kEmergencyColor;
    }
    state_.set_font_color(color);
    if (connected != this) {
        return;
    }
    state_.show_window();
    speed_.show_window();
    pieces_.show_window();
}

void MainScreen::onStart(int, int) {
    send(CommandCode::Start);
}

void MainScreen::onStop(int, int) {
    send(CommandCode::Stop);
}

void MainScreen::onSettings(int, int) {
    showSettings();
}

void MainScreen::fillTree() {
    const short labelWidth = static_cast<short>(kScreenWidth - 2 * kMargin);
    const short buttonX = static_cast<short>((kScreenWidth - kButtonWidth) / 2);
    short y = kMargin;

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
    tree_[6] = {&settings_, ID_OPEN_SETTINGS, "AJUSTES", buttonX, y, kButtonWidth, kRowHeight, nullptr};
    tree_[7] = {};
}

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

void SettingsScreen::open(c_surface* next) {
    set_surface(next);
    fillTree();
    connect(nullptr, ID_SETTINGS_ROOT, nullptr, 0, 0, kScreenWidth, kScreenHeight, tree_);
    show_window();
}

void SettingsScreen::pre_create_wnd() {
    // Igual que la principal: disconnect apaga ATTR_VISIBLE y ATTR_FOCUS.
    m_attr = static_cast<WND_ATTRIBUTION>(ATTR_VISIBLE | ATTR_FOCUS);
    m_bg_color = c_theme::get_color(COLOR_WND_NORMAL);
}

void SettingsScreen::on_init_children() {
    speed_.set_on_change(static_cast<WND_CALLBACK>(&SettingsScreen::onSpeed));
    speed_.set_max_min(static_cast<short>(SpeedCode::High), static_cast<short>(SpeedCode::Low));
    speed_.set_step(1);
    // set_value no llama a on_change: abrir Ajustes no manda $4.
    speed_.set_value(speedLevel(storedBelt.speed));
    reset_.set_on_click(static_cast<WND_CALLBACK>(&SettingsScreen::onReset));
    back_.set_on_click(static_cast<WND_CALLBACK>(&SettingsScreen::onBack));
    focusFirst();
}

void SettingsScreen::on_paint() {
    c_rect rect;
    get_screen_rect(rect);
    m_surface->fill_rect(rect, c_theme::get_color(COLOR_WND_NORMAL), static_cast<unsigned int>(m_z_order));
}

void SettingsScreen::onSpeed(int, int value) {
    switch (value) {
    case static_cast<int>(SpeedCode::Low):
        sendSpeed(SpeedCode::Low);
        break;
    case static_cast<int>(SpeedCode::Medium):
        sendSpeed(SpeedCode::Medium);
        break;
    case static_cast<int>(SpeedCode::High):
        sendSpeed(SpeedCode::High);
        break;
    default:
        break;
    }
}

void SettingsScreen::onReset(int, int) {
    sendReset();
}

void SettingsScreen::onBack(int, int) {
    showMain();
}

void SettingsScreen::fillTree() {
    const short labelWidth = static_cast<short>(kScreenWidth - 2 * kMargin);
    const short buttonX = static_cast<short>((kScreenWidth - kButtonWidth) / 2);
    const short spinX = static_cast<short>((kScreenWidth - kSpinWidth) / 2);
    short y = kMargin;

    tree_[0] = {&title_, ID_TITLE, "AJUSTES", kMargin, y, labelWidth, kRowHeight, nullptr};
    y = static_cast<short>(y + kRowHeight);
    tree_[1] = {&speedLabel_, ID_SPEED_LABEL, "Velocidad", kMargin, y, labelWidth, kRowHeight, nullptr};
    y = static_cast<short>(y + kRowHeight);
    // Antes que los botones: pre_create_wnd del spin inserta + y - en el padre,
    // y esos dos (ATTR_FOCUS) son los que reciben el foco, no el número.
    tree_[2] = {&speed_, ID_SPIN, nullptr, spinX, y, kSpinWidth, static_cast<short>(kRowHeight * 2), nullptr};
    y = static_cast<short>(y + kRowHeight * 2 + kMargin);
    tree_[3] = {&reset_, ID_RESET, "RESET PIEZAS", buttonX, y, kButtonWidth, kRowHeight, nullptr};
    y = static_cast<short>(y + kRowHeight + kGap);
    tree_[4] = {&back_, ID_BACK, "VOLVER", buttonX, y, kButtonWidth, kRowHeight, nullptr};
    tree_[5] = {};
}

void SettingsScreen::focusFirst() {
    c_wnd* child = m_top_child;
    while (child != nullptr) {
        if (child->is_focus_wnd()) {
            set_child_focus(child);
            return;
        }
        child = child->get_next_sibling();
    }
}
