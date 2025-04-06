#ifndef NOISE_SCREEN_H
#define NOISE_SCREEN_H

#include "screen.h"

class NoiseScreen : public Screen {
public:
    // Constructor passes dependencies to the base Screen class
    NoiseScreen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef);

    // Override the draw method from the base class
    void draw(int yOffset = 0) override; // Add yOffset parameter
};

#endif // NOISE_SCREEN_H
