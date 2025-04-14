#include "input_manager.h"
#include "ui_manager.h"
#include "data_manager.h"
#include "communication_manager.h" // Include full header for CommManager methods
#include <WiFi.h> // Needed for WiFi.status() - Can be removed now
#include <time.h> // Needed for initTime() simulation - Can be removed now

// Constructor
InputManager::InputManager(UIManager& uiMgr, DataManager& dataMgr, CommunicationManager& commMgr) :
    uiManager_(uiMgr),
    dataManager_(dataMgr),
    commManager_(commMgr) // Store reference
{
    // Initialize button states
    for (int i = 0; i < 5; ++i) {
        lastBtnState[i] = HIGH; // Assume buttons are pull-up
        btnPressTime[i] = 0;
        btnLongPressTriggered[i] = false;
    }
}

// Initialize button pins
void InputManager::begin() {
    pinMode(BTN1_PIN, INPUT_PULLUP);
    pinMode(BTN2_PIN, INPUT_PULLUP);
    pinMode(BTN3_PIN, INPUT_PULLUP);
    pinMode(BTN4_PIN, INPUT_PULLUP);
    pinMode(BTN5_PIN, INPUT_PULLUP);
    Serial.println("[InputManager] Button pins initialized.");
}

// Update function - checks button states and handles events
void InputManager::update() {
    unsigned long currentTime = millis();
    int btnPins[5] = {BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, BTN5_PIN};

    for (int i = 0; i < 5; ++i) {
        bool currentState = digitalRead(btnPins[i]);

        // --- Edge Detection and Action --- //

        // Button Pressed (Falling Edge: HIGH -> LOW)
        if (currentState == LOW && lastBtnState[i] == HIGH) {
            btnPressTime[i] = currentTime;
            btnLongPressTriggered[i] = false;
            // Serial.printf("[InputManager] Button %d Down\n", i + 1);
        }
        // Button Released (Rising Edge: LOW -> HIGH)
        else if (currentState == HIGH && lastBtnState[i] == LOW) {
            // Check if it was a short press (long press didn't trigger)
            if (!btnLongPressTriggered[i]) {
                // --- Short Press Actions --- //
                // Serial.printf("[InputManager] Button %d Short Press\n", i + 1);
                switch (i) {
                    case 0: // BTN1 (Up)
                        uiManager_.handleInput(BTN1_PIN); // Pass original pin ID to UIManager
                        break;
                    case 1: // BTN2 (Right - LED Mode)
                        {
                            uint8_t nextMode = (uiManager_.getCurrentLedMode() + 1) % 4;
                            uiManager_.setLedMode(nextMode);
                            // updateLEDs(); // LED update should happen elsewhere based on UIManager state
                            Serial.printf("[InputManager] Button 2: Set LED Mode to %d\n", nextMode);
                        }
                        break;
                    case 2: // BTN3 (Down)
                        uiManager_.handleInput(BTN3_PIN); // Pass original pin ID to UIManager
                        break;
                    case 3: // BTN4 (Left - Save Data)
                        dataManager_.saveDataToSd(); // Call DataManager method
                        // uiManager_.forceRedraw(); // Let DataManager/UIManager handle redraw if needed
                        break;
                    case 4: // BTN5 (Center - Manual Refresh)
                        dataManager_.recordCurrentData(); // Call DataManager method
                        // uiManager_.forceRedraw(); // Let DataManager/UIManager handle redraw if needed
                        break;
                }
            }
            btnPressTime[i] = 0; // Reset press time
            // Serial.printf("[InputManager] Button %d Up\n", i + 1);
        }
        // Button Held Down (LOW -> LOW)
        else if (currentState == LOW && lastBtnState[i] == LOW) {
            // Check for long press trigger
            if (!btnLongPressTriggered[i] && btnPressTime[i] != 0 && (currentTime - btnPressTime[i] >= LONG_PRESS_MS)) {
                 // --- Long Press Actions --- //
                 Serial.printf("[InputManager] Button %d Long Press\n", i + 1);
                 btnLongPressTriggered[i] = true; // Mark as triggered

                 switch (i) {
                     case 0: // BTN1 (Up) - No long press action defined
                         break;
                     case 1: // BTN2 (Right) - No long press action defined
                         break;
                     case 2: // BTN3 (Down) - No long press action defined
                         break;
                     case 3: // BTN4 (Left - Reconnect WiFi)
                         // Call CommunicationManager method
                         commManager_.reconnectWiFi();
                         break;
                     case 4: // BTN5 (Center - Restart)
                         Serial.println("[InputManager] Restarting via Button 5 long press...");
                         Serial.flush();
                         delay(100);
                         ESP.restart();
                         break;
                 }
            }
        }

        // Update last state
        lastBtnState[i] = currentState;
    }
}

// Remove the implementation of reconnectWiFi
/*
void InputManager::reconnectWiFi() {
    // This function needs the actual logic to connect to WiFi.
    // It might involve calling methods in CommunicationManager or directly using WiFi library.
    // For now, simulate the process and update UIManager status.
    Serial.println("[InputManager] Attempting to reconnect WiFi...");

    // --- Placeholder for actual WiFi connection logic ---
    // Example (replace with actual implementation):
    WiFi.disconnect(true); // Optional: disconnect first
    delay(100);
    WiFi.begin("501_2.4G", "12340000"); // Use actual SSID/Password (Ideally from config)

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    // --- End Placeholder ---

    bool wifiStatus = (WiFi.status() == WL_CONNECTED);
    if (wifiStatus) {
        Serial.println("[InputManager] WiFi reconnected successfully.");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        // Attempt to re-sync time
        // Placeholder for initTime() logic
        Serial.println("Attempting to re-sync NTP time...");
        configTime(28800, 0, "pool.ntp.org"); // Use actual offsets/server
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            Serial.println("NTP time re-synced.");
            uiManager_.setTimeStatus(true); // Update UIManager state
        } else {
            Serial.println("NTP time re-sync failed.");
             uiManager_.setTimeStatus(false);
        }
    } else {
        Serial.println("[InputManager] WiFi reconnection failed.");
    }
    // Update UIManager WiFi status regardless of success/failure
    uiManager_.setWifiStatus(wifiStatus);
    uiManager_.forceRedraw(); // Force UI update to show status change
}
*/ 