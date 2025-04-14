#ifndef NOISE_SCREEN_H
#define NOISE_SCREEN_H

#include "screen.h"

// Forward declaration
class DataManager;

class NoiseScreen : public Screen {
public:
    // Constructor now takes DataManager reference
    NoiseScreen(TFT_eSPI& display, DataManager& dataMgr);

    void draw(int yOffset = 0) override;
};

#endif // NOISE_SCREEN_H
