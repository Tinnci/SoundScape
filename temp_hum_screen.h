#ifndef TEMP_HUM_SCREEN_H
#define TEMP_HUM_SCREEN_H

#include "screen.h"

class TempHumScreen : public Screen {
public:
    // Constructor passes dependencies to the base Screen class
    TempHumScreen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef);

    // Override the draw method from the base class
    void draw(int yOffset = 0) override; // Add yOffset parameter
};

#endif // TEMP_HUM_SCREEN_H
