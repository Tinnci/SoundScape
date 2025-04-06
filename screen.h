#ifndef SCREEN_H
#define SCREEN_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h> // Needed for time_t

// Forward declaration of UIManager to avoid circular dependency
class UIManager;

// Define environment data structure here (moved from ui.h)
struct EnvironmentData {
    time_t timestamp;    // 时间戳
    float temperature;   // 温度
    float humidity;      // 湿度
    float decibels;      // 分贝值
    float lux;          // 光照强度
};

class Screen {
public:
    // Constructor can take dependencies if needed (like TFT object, shared data)
    Screen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef) :
        tft(display), envDataPtr(data), dataIndex(dataIdxRef) {}

    virtual ~Screen() = default; // Virtual destructor for base class

    // Called when this screen becomes active
    // Store the UIManager pointer for later use (e.g., in draw)
    virtual void onEnter(UIManager* uiManager) { uiManagerPtr_ = uiManager; }

    // Called when this screen is about to become inactive
    virtual void onExit(UIManager* uiManager) {}

    // Called periodically by the UIManager to draw the screen content
    // Added yOffset parameter for transition animations
    virtual void draw(int yOffset = 0) = 0; // Pure virtual, must be implemented by subclasses

    // Called by the UIManager when an input event occurs for this screen
    // Returns true if the input was handled, false otherwise (e.g., for global navigation)
    virtual bool handleInput(UIManager* uiManager, int buttonPin) { return false; } // Default: does not handle input

protected:
    TFT_eSPI& tft; // Reference to the display object
    EnvironmentData* envDataPtr; // Pointer to the shared environment data array
    int& dataIndex; // Reference to the current data index
    UIManager* uiManagerPtr_; // Pointer to the UI Manager (set in onEnter)

    // Helper to get the latest data safely
    const EnvironmentData& getLatestData() const {
        if (dataIndex > 0) {
            return envDataPtr[dataIndex - 1];
        }
        // Return a default/dummy object if no data yet
        static EnvironmentData dummyData = {0, -999.0f, -1.0f, 0.0f, -1.0f};
        return dummyData;
    }
};

#endif // SCREEN_H
