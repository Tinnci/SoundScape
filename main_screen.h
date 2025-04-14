#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#include "screen.h"

// Forward declaration
class DataManager;

class MainScreen : public Screen {
public:
    // Constructor now takes DataManager reference
    MainScreen(TFT_eSPI& display, DataManager& dataMgr);

    void draw(int yOffset = 0) override;
};

#endif // MAIN_SCREEN_H
