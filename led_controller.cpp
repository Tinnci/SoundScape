#include "led_controller.h"
#include "ui_manager.h"    // Need access to getLedMode
#include "data_manager.h"  // Need access to getLatestData
#include <Arduino.h>      // For Serial

// Constructor
LedController::LedController(UIManager& uiMgr, DataManager& dataMgr) :
    pixels_(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800),
    uiManager_(uiMgr),
    dataManager_(dataMgr)
{}

// Initialize NeoPixel library
void LedController::begin() {
    pixels_.begin();
    pixels_.setBrightness(50); // Default brightness
    pixels_.clear();
    pixels_.show();
    Serial.println("[LedController] Initialized.");
}

// Update LEDs based on mode and data
void LedController::update() {
    uint8_t currentMode = uiManager_.getCurrentLedMode();
    const EnvironmentData& latestData = dataManager_.getLatestData();

    // Get the calculated color based on mode and data
    uint32_t color = calculateColor(currentMode, latestData);

    // Apply the color to all LEDs
    // Check if mode is OFF separately, as calculateColor might return black
    if (currentMode == LED_MODE_OFF) {
        if (pixels_.getPixelColor(0) != 0) { // Only clear if not already off
            pixels_.clear();
            pixels_.show();
        }
    } else {
        // Check if the color needs updating to avoid redundant writes
        if (pixels_.getPixelColor(0) != color) {
             for (int i = 0; i < NUM_LEDS; i++) {
                pixels_.setPixelColor(i, color);
            }
            pixels_.show();
        }
    }
}

// Set LED brightness
void LedController::setBrightness(uint8_t brightness) {
    pixels_.setBrightness(brightness);
    // Re-show the pixels with the new brightness if they are not off
    if (uiManager_.getCurrentLedMode() != LED_MODE_OFF) {
        pixels_.show();
    }
}

// Internal helper to calculate color based on mode and data
// Logic moved from SoundScape.ino::updateLEDs
uint32_t LedController::calculateColor(uint8_t mode, const EnvironmentData& data) {
    uint32_t color = 0; // Default Black/Off

    // Handle case where no valid data is available yet (timestamp is 0)
    if (data.timestamp == 0) {
        return pixels_.Color(0, 0, 32); // Dim blue for 'no data'
    }

    switch (mode) {
        case LED_MODE_OFF:
            color = 0; // Black
            break;

        case LED_MODE_NOISE:
            {
                float db = data.decibels;
                // Use constants from ui_constants.h included via led_controller.h
                if (isnan(db) || db < NOISE_THRESHOLD_LOW) {
                    color = pixels_.Color(0, 0, 255);   // Blue - Quiet or Invalid
                } else if (db < NOISE_THRESHOLD_MEDIUM) {
                    color = pixels_.Color(0, 255, 0);   // Green - Low
                } else if (db < NOISE_THRESHOLD_HIGH) {
                    color = pixels_.Color(255, 255, 0); // Yellow - Medium
                } else { // db >= NOISE_THRESHOLD_HIGH
                    color = pixels_.Color(255, 0, 0);  // Red - High
                }
            }
            break;

        case LED_MODE_TEMP:
            {
                float temp = data.temperature;
                // Use constants from ui_constants.h
                 if (isnan(temp) || temp < TEMP_MIN || temp > TEMP_MAX) {
                    color = pixels_.Color(128, 0, 128); // Purple for invalid
                } else if (temp < 16.0f) { // Example thresholds
                    color = pixels_.Color(0, 0, 255);  // Blue - Cold
                } else if (temp > 28.0f) {
                    color = pixels_.Color(255, 0, 0);  // Red - Hot
                } else {
                    color = pixels_.Color(0, 255, 0);  // Green - Comfortable
                }
            }
            break;

        case LED_MODE_HUMIDITY:
            {
                float humidity = data.humidity;
                 // Use constants from ui_constants.h
                 if (isnan(humidity) || humidity < HUM_MIN || humidity > HUM_MAX) {
                    color = pixels_.Color(128, 0, 128); // Purple for invalid
                } else if (humidity < 30.0f) { // Example thresholds
                    color = pixels_.Color(255, 165, 0); // Orange - Dry (changed from red)
                } else if (humidity > 70.0f) {
                    color = pixels_.Color(0, 0, 255);  // Blue - Humid
                } else {
                    color = pixels_.Color(0, 255, 0);  // Green - Comfortable
                }
            }
            break;

        default:
            color = pixels_.Color(32, 32, 32); // Dim white for unknown mode
            break;
    }
    return color;
} 