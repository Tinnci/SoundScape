#ifndef SCREEN_H
#define SCREEN_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cmath> // For NAN
#include "EnvironmentData.h" // Include the environment data structure

// Forward declaration to avoid circular dependency
class UIManager;
class DataManager; // <<<<<<< ADDED Forward declaration for DataManager

class Screen {
public:
    // Constructor Declaration (Implementation moved to .cpp)
    Screen(TFT_eSPI& display, DataManager& dataMgr);

    virtual ~Screen() = default; // Virtual destructor

    // Called when this screen becomes active
    virtual void onEnter(UIManager* uiManager) { uiManagerPtr_ = uiManager; }

    // Called when this screen is about to become inactive
    virtual void onExit(UIManager* uiManager) {}

    // Draw the screen content
    virtual void draw(int yOffset = 0) = 0; // Pure virtual

    // Handle input events
    virtual bool handleInput(UIManager* uiManager, int buttonPin) { return false; }

protected:
    TFT_eSPI& tft;           // Reference to the display object
    DataManager& dataManager_; // Reference to the Data Manager
    UIManager* uiManagerPtr_;  // Pointer to the UI Manager (set in onEnter)

    // Helper function Declaration (Implementation moved to .cpp)
    const EnvironmentData& getLatestData() const;
}; 

#endif // SCREEN_H