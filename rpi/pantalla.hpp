#pragma once

#include "GuiLite.h"
#include "belt.hpp"

constexpr int kScreenWidth = 512;
constexpr int kScreenHeight = 512;
constexpr int kTextCapacity = 32;

class MainScreen : public c_wnd {
public:
    void open(c_surface* surface);
    void paintBelt(const Belt& belt);

private:
    void pre_create_wnd() override;
    void on_init_children() override;
    void on_paint() override;
    void onStart(int id, int param);
    void onStop(int id, int param);
    void onSpeed(int id, int value);
    void fillTree();
    void focusFirst();

    c_label title_;
    c_label state_;
    c_label speed_;
    c_label pieces_;
    c_button start_;
    c_button stop_;
    c_spin_box speedSpin_;
    WND_TREE tree_[8]{};
    char stateText_[kTextCapacity]{};
    char speedText_[kTextCapacity]{};
    char piecesText_[kTextCapacity]{};
};

void mainLoop(void* phy_fb, int width, int height, int color_bytes);
void showState(Belt belt);
c_wnd* connectedScreen();
