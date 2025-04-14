#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>

// Forward declarations to avoid circular includes
class UIManager;
class DataManager;
class CommunicationManager; // Forward declare if needed for WiFi reconnect

class InputManager {
public:
    // Constructor takes pointers/references to other managers it needs to interact with
    InputManager(UIManager& uiMgr, DataManager& dataMgr, CommunicationManager& commMgr);

    void begin(); // Initialize button pins
    void update(); // Call this in the main loop to check buttons

private:
    // Define button pins internally
    static const uint8_t BTN1_PIN = 2;  // Up
    static const uint8_t BTN2_PIN = 1;  // Right (LED Mode)
    static const uint8_t BTN3_PIN = 41; // Down
    static const uint8_t BTN4_PIN = 40; // Left (Save / WiFi Reconnect)
    static const uint8_t BTN5_PIN = 42; // Center (Manual Refresh / Restart)

    // Button states
    bool lastBtnState[5]; // Store last state for edge detection (0-4 corresponds to BTN1-5)
    unsigned long btnPressTime[5]; // Store press start time for long press detection
    bool btnLongPressTriggered[5]; // Flag to prevent multiple triggers

    static const unsigned long LONG_PRESS_MS = 1000; // 1 second

    // References to other managers
    UIManager& uiManager_;
    DataManager& dataManager_;
    CommunicationManager& commManager_; // Store reference to CommManager

    // Helper function to reconnect WiFi (might need access to SSID/Pass or a method in CommManager)
    void reconnectWiFi();
};

#endif // INPUT_MANAGER_H 