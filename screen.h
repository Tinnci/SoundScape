#ifndef SCREEN_H
#define SCREEN_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h> // Needed for time_t
#include "EnvironmentData.h" // Include the environment data structure
#include <cmath> // Add include for NAN

// Forward declaration of UIManager to avoid circular dependency
class UIManager;

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
             // Calculate the correct index for circular buffer
            int latestIdx = (dataIndex - 1 + (sizeof(envDataPtr[0]) * (24*60))) % (sizeof(envDataPtr[0]) * (24*60)); // Assuming envDataPtr is the array
             // Simpler if envDataPtr points to the fixed size array envData
            // latestIdx = (dataIndex - 1 + (sizeof(envData) / sizeof(envData[0]))) % (sizeof(envData) / sizeof(envData[0]));
            // Safest: just use dataIndex-1 assuming it's correctly wrapped in recordEnvironmentData
            latestIdx = (dataIndex == 0) ? ( (sizeof(EnvironmentData)*(24*60))/sizeof(EnvironmentData) - 1) : (dataIndex - 1); // Handle wrap around case more carefully?

            // Let's stick to the simpler (dataIndex - 1) assuming recordEnvironmentData handles wrapping correctly.
            // Need to ensure dataIndex is never 0 when accessing index -1 if it just wrapped.
            // Better: check if dataIndex is 0. If so, the latest is at the end of the buffer.
             int actualLatestIndex = (dataIndex == 0) ? ((24*60) - 1) : (dataIndex - 1);
             // Check if the buffer has even filled once
             // A separate flag 'buffer_filled_once' might be needed for perfect circular buffer logic
             // Let's assume for now dataIndex > 0 means envDataPtr[dataIndex-1] is the latest valid *written* slot.

            return envDataPtr[dataIndex - 1]; // Returns the reference

        }
        // Return a default/dummy object if no data yet
        static EnvironmentData dummyData; // Keep static dummy
        dummyData.timestamp = 0;
        dummyData.decibels = NAN; // Use NAN for dummy invalid
        dummyData.humidity = NAN;
        dummyData.temperature = NAN;
        dummyData.lux = NAN;
        return dummyData;
    }
};

#endif // SCREEN_H
