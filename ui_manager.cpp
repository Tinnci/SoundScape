#include "ui_manager.h"
#include "ui.h" // Include for LED_MODE definitions
#include <Arduino.h> // For Serial, maybe millis() if needed later

// 声明外部函数，用于检查内存状态
extern bool isLowMemory(size_t threshold);

// Constructor
UIManager::UIManager() :
    activeScreenIndex(-1),
    activeScreen(nullptr),
    redrawNeeded(true), // Start with a redraw needed
    // Initialize state variables
    wifiConnected_(false),
    sdCardInitialized_(false),
    timeInitialized_(false),
    currentLedMode_(LED_MODE_NOISE), // Default LED mode from original code
    // Initialize transition state
    isTransitioning_(false),
    transitionStartTime_(0),
    transitionDuration_(300), // Default duration 300ms
    outgoingScreen_(nullptr)
{} // End of constructor initialization list

// Add a screen to the manager
void UIManager::addScreen(Screen* screen) {
    screens.push_back(screen);
}

// Set the initial screen
void UIManager::setInitialScreen() {
    if (!screens.empty()) {
        setActiveScreenByIndex(0);
        activeScreen->onEnter(this); // Call onEnter for the initial screen
        redrawNeeded = true; // Ensure initial draw
    } else {
        Serial.println("[UIManager] Error: No screens added!");
    }
}

// Handle button input
void UIManager::handleInput(int buttonPin) {
    if (screens.empty() || activeScreen == nullptr) {
        return; // No screens or active screen to handle input
    }

    // Disable input during transition
    if (isTransitioning_) {
        return;
    }

    bool handled = false; // Declare handled ONCE here at the beginning of the effective scope

    // Check for global navigation first
    if (buttonPin == BTN_UP_PIN) { // Up button navigates to previous screen
        int prevIndex = (activeScreenIndex - 1 + screens.size()) % screens.size();
        startTransition(prevIndex); // Start transition instead of direct switch
        handled = true;
        Serial.printf("[UIManager] Starting transition UP to screen index %d\n", prevIndex);
    } else if (buttonPin == BTN_DOWN_PIN) { // Down button navigates to next screen
        int nextIndex = (activeScreenIndex + 1) % screens.size();
        startTransition(nextIndex); // Start transition instead of direct switch
        handled = true;
        Serial.printf("[UIManager] Starting transition DOWN to screen index %d\n", nextIndex);
    }

    // If not handled by global navigation, pass to the active screen
    if (!handled) {
        // Note: The result of activeScreen->handleInput might overwrite the 'handled' flag
        // if the screen handles the input. This is the intended behavior.
        handled = activeScreen->handleInput(this, buttonPin);
        if (handled) {
             Serial.printf("[UIManager] Input handled by screen index %d\n", activeScreenIndex);
        }
    }

    // If still not handled, could add default behavior here if needed
    // if (!handled) { ... }
}

// Update the UI (call this in loop())
void UIManager::update() {
    extern TFT_eSPI tft;

    if (isTransitioning_) {
        // 检查是否有足够内存进行动画
        if (isLowMemory(20000)) { // 使用20KB作为安全阈值
            // 内存不足，立即结束过渡动画
            isTransitioning_ = false;
            outgoingScreen_ = nullptr;
            tft.fillScreen(TFT_BLACK);
            activeScreen->draw(0);
            redrawNeeded = false;
            Serial.println("内存不足，跳过过渡动画");
            return;
        }

        unsigned long currentTime = millis();
        float progress = (float)(currentTime - transitionStartTime_) / transitionDuration_;
        
        // Add easing function for smoother animation
        float easedProgress = progress * progress * (3 - 2 * progress); // Smooth step function

        if (progress >= 1.0f) {
            // Transition finished
            isTransitioning_ = false;
            outgoingScreen_ = nullptr;
            tft.fillScreen(TFT_BLACK); // Clear screen once at the end
            activeScreen->draw(0); // Draw final screen
            redrawNeeded = false;
            Serial.printf("[UIManager] Transition finished. Active screen: %d\n", activeScreenIndex);
        } else {
            // During transition, calculate positions with eased progress
            int screenHeight = tft.height();
            int outgoingY = (int)(easedProgress * -screenHeight);
            int incomingY = screenHeight + (int)(easedProgress * -screenHeight);

            tft.fillScreen(TFT_BLACK); // Clear once per frame

            // Draw both screens at their calculated positions
            if (outgoingScreen_) {
                outgoingScreen_->draw(outgoingY);
            }
            if (activeScreen) {
                activeScreen->draw(incomingY);
            }
            redrawNeeded = false;
        }
    } else if (redrawNeeded && activeScreen != nullptr) {
        tft.fillScreen(TFT_BLACK); // Clear screen before normal redraw
        activeScreen->draw();
        redrawNeeded = false;
    }
}

// Force a redraw on the next update cycle
void UIManager::forceRedraw() {
    redrawNeeded = true;
}

// Internal method to change the active screen (called by startTransition or setInitialScreen)
// Does not handle onExit/onEnter directly anymore, startTransition does.
void UIManager::setActiveScreenByIndex(int index) {
     if (index >= 0 && index < screens.size()) {
        activeScreenIndex = index;
        activeScreen = screens[activeScreenIndex];
        // onEnter is called by startTransition or setInitialScreen
        // redrawNeeded is set by startTransition or setInitialScreen
    } else {
        Serial.printf("[UIManager] Error: Invalid screen index %d\n", index);
        activeScreenIndex = -1; // Mark as invalid
        activeScreen = nullptr;
    }
}

// Internal method to start a transition
void UIManager::startTransition(int nextIndex) {
    if (isTransitioning_ || nextIndex == activeScreenIndex || nextIndex < 0 || nextIndex >= screens.size()) {
        return; // Already transitioning, same screen, or invalid index
    }

    if (activeScreen) {
        activeScreen->onExit(this); // Call onExit for the current screen
        outgoingScreen_ = activeScreen; // Set current as outgoing
    } else {
        outgoingScreen_ = nullptr; // No previous screen if called initially
    }

    // Set the new active screen (incoming screen)
    activeScreenIndex = nextIndex;
    activeScreen = screens[activeScreenIndex];
    activeScreen->onEnter(this); // Call onEnter for the new screen

    // Start the transition timer
    isTransitioning_ = true;
    transitionStartTime_ = millis();
    redrawNeeded = false; // Drawing is handled by the transition logic in update()
}

// --- State Management Implementation ---
void UIManager::setWifiStatus(bool connected) {
    if (wifiConnected_ != connected) {
        wifiConnected_ = connected;
        redrawNeeded = true; // Redraw if status changes
    }
}

void UIManager::setSdCardStatus(bool initialized) {
    if (sdCardInitialized_ != initialized) {
        sdCardInitialized_ = initialized;
        redrawNeeded = true; // Redraw if status changes
    }
}

void UIManager::setTimeStatus(bool initialized) {
     if (timeInitialized_ != initialized) {
        timeInitialized_ = initialized;
        redrawNeeded = true; // Redraw if status changes
    }
}

void UIManager::setLedMode(uint8_t mode) {
    if (currentLedMode_ != mode) {
        currentLedMode_ = mode;
        redrawNeeded = true; // Redraw if status changes
    }
}

bool UIManager::isWifiConnected() const { return wifiConnected_; }
bool UIManager::isSdCardInitialized() const { return sdCardInitialized_; }
bool UIManager::isTimeInitialized() const { return timeInitialized_; }
uint8_t UIManager::getCurrentLedMode() const { return currentLedMode_; }
// --- End State Management ---
