#ifndef LIGHT_SCREEN_H
#define LIGHT_SCREEN_H

#include "screen.h"

class LightScreen : public Screen {
public:
    // Constructor passes dependencies to the base Screen class
    LightScreen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef);

    // Override the draw method from the base class
    void draw(int yOffset = 0) override; // Add yOffset parameter
};

#endif // LIGHT_SCREEN_H
