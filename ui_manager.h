#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "screen.h" // Needs Screen base class definition
#include "memory_utils.h"

// Define button pins used by UIManager for navigation
// (Moved from sketch_mar18b.ino)
#define BTN1_PIN 2
#define BTN3_PIN 41

#define BTN_UP_PIN   BTN1_PIN // Assuming BTN1 is Up
#define BTN_DOWN_PIN BTN3_PIN // Assuming BTN3 is Down
// Other button pins can be handled by individual screens or passed to handleInput

class UIManager {
public:
    UIManager();

    // Add a screen to the manager. Screens are navigated in the order they are added.
    void addScreen(Screen* screen);

    // Set the initial screen (usually the first one added)
    void setInitialScreen();

    // Handle button input. Checks for global navigation first.
    void handleInput(int buttonPin);

    // Update the UI. Should be called in the main loop. Checks if redraw is needed.
    void update();

    // Force a redraw of the current screen
    void forceRedraw();

    // Getters for shared state (alternative to using extern in screens)
    // bool isWifiConnected() const;
    // bool isSdCardInitialized() const;
    // bool isTimeInitialized() const;
    // uint8_t getCurrentLedMode() const;
    // void setWifiStatus(bool connected); // Methods to update state
    // --- State Management ---
    // Setters (called from main sketch logic)
    void setWifiStatus(bool connected);
    void setSdCardStatus(bool initialized);
    void setTimeStatus(bool initialized);
    void setLedMode(uint8_t mode);

    // Getters (called by screens)
    bool isWifiConnected() const;
    bool isSdCardInitialized() const;
    bool isTimeInitialized() const;
    uint8_t getCurrentLedMode() const;
    // --- End State Management ---

private:
    std::vector<Screen*> screens; // Stores pointers to all managed screens
    int activeScreenIndex;        // Index of the currently active screen
    Screen* activeScreen;         // Pointer to the active screen object
    bool redrawNeeded;            // Flag indicating if the screen needs redrawing

    // Private members for shared state
    bool wifiConnected_;
    bool sdCardInitialized_;
    bool timeInitialized_;
    uint8_t currentLedMode_;

    // --- Transition State ---
    bool isTransitioning_;
    unsigned long transitionStartTime_;
    uint32_t transitionDuration_; // Duration in ms
    Screen* outgoingScreen_;
    // activeScreen serves as the incoming screen during transition
    // --- End Transition State ---

    // Internal method to change the active screen
    void setActiveScreenByIndex(int index);

    // Internal method to start a transition
    void startTransition(int nextIndex);
};

#endif // UI_MANAGER_H
