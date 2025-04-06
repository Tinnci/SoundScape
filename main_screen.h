#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#include "screen.h"

class MainScreen : public Screen {
public:
    // Constructor passes dependencies to the base Screen class
    MainScreen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef);

    // Override the draw method from the base class
    void draw(int yOffset = 0) override; // Add yOffset parameter

    // Override onEnter if needed for specific setup
    // void onEnter(UIManager* uiManager) override;

    // Override handleInput if this screen needs specific button handling
    // bool handleInput(UIManager* uiManager, int buttonPin) override;
};

#endif // MAIN_SCREEN_H
