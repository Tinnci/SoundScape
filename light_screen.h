#ifndef LIGHT_SCREEN_H
#define LIGHT_SCREEN_H

#include "screen.h"

// Forward declaration
class DataManager;

class LightScreen : public Screen {
public:
    // Constructor now takes DataManager reference
    LightScreen(TFT_eSPI& display, DataManager& dataMgr);

    void draw(int yOffset = 0) override;
};

#endif // LIGHT_SCREEN_H
