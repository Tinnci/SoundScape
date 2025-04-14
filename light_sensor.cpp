#include "light_sensor.h"
#include <Arduino.h> // For Serial

LightSensor::LightSensor(uint8_t address) :
    sensor_(address), // Initialize BH1750 with the address
    i2c_address_(address),
    initialized_(false)
{}

bool LightSensor::begin() {
    if (initialized_) return true;

    // sensor_.begin() uses Wire internally, ensure Wire.begin() was called before this.
    // It requires a mode, use CONTINUOUS_HIGH_RES_MODE as before.
    initialized_ = sensor_.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
    if (!initialized_) {
        Serial.printf("ERR: BH1750 sensor (0x%02X) failed to initialize.\n", i2c_address_);
    } else {
        Serial.printf("BH1750 sensor (0x%02X) initialized successfully.\n", i2c_address_);
        // Read initial value to confirm communication
        float initialLux = sensor_.readLightLevel();
        if (initialLux >= 0) {
            Serial.printf("BH1750 initial reading: %.1f lx\n", initialLux);
        } else {
             Serial.printf("WARN: BH1750 initial reading failed (Code: %.0f).\n", initialLux);
             initialized_ = false; // Mark as not initialized if initial read fails
        }
    }
    return initialized_;
}

bool LightSensor::readData(float& lux) {
    if (!initialized_) {
        lux = NAN;
        return false;
    }

    float lux_reading = sensor_.readLightLevel();
    bool success = false;

    // Check if reading was successful (BH1750 returns negative on error)
    if (lux_reading >= 0) {
        float validated_lux = DataValidator::validateLux(lux_reading);
        // Check validation result (validateLux returns -1.0f for invalid)
        if (validated_lux != -1.0f) {
            lux = validated_lux;
            success = true;
        } else {
            // Serial.println("DBG: Lux validation failed.");
            lux = NAN; // Mark as invalid
        }
    } else {
        // Serial.printf("DBG: BH1750 read failed with code: %.0f\n", lux_reading);
        lux = NAN; // Mark as invalid
    }
    return success;
}

bool LightSensor::isInitialized() const {
    return initialized_;
} 