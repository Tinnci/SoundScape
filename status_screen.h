#ifndef STATUS_SCREEN_H
#define STATUS_SCREEN_H

#include "screen.h"

class StatusScreen : public Screen {
public:
    // Constructor passes dependencies to the base Screen class
    StatusScreen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef);

    // Override the draw method from the base class
    void draw(int yOffset = 0) override; // Add yOffset parameter
};

#endif // STATUS_SCREEN_H
