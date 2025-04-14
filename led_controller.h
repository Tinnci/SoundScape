#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Adafruit_NeoPixel.h>
#include "EnvironmentData.h" // Need data structure
#include "ui_constants.h"    // Need LED modes and noise thresholds
#include <cmath>              // For isnan

// Forward declarations
class UIManager;
class DataManager;

class LedController {
public:
    // Constructor takes references to managers for state/data
    LedController(UIManager& uiMgr, DataManager& dataMgr);

    void begin(); // Initialize NeoPixel library
    void update(); // Update LEDs based on mode and data
    void setBrightness(uint8_t brightness); // Control brightness

private:
    // LED configuration
    static const uint8_t LED_PIN = 18;
    static const uint8_t NUM_LEDS = 4;

    Adafruit_NeoPixel pixels_;
    UIManager& uiManager_;
    DataManager& dataManager_;

    uint32_t calculateColor(uint8_t mode, const EnvironmentData& data);
};

#endif // LED_CONTROLLER_H 