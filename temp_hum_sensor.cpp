#include "temp_hum_sensor.h"
#include <Arduino.h> // For Serial

TempHumSensor::TempHumSensor() :
    sensor_(),      // Initialize Adafruit_Si7021 object
    initialized_(false)
{}

bool TempHumSensor::begin() {
    if (initialized_) return true;

    // sensor_.begin() uses Wire internally, ensure Wire.begin() was called.
    initialized_ = sensor_.begin();
    if (!initialized_) {
        Serial.println("ERR: Si7021 sensor failed to initialize (or not connected).");
    } else {
        Serial.println("Si7021 sensor initialized successfully.");
        // Perform initial read to check communication
        float temp, hum;
        if (!readData(temp, hum)) { // Use readData to check
            Serial.println("WARN: Si7021 initial reading failed.");
            // Keep initialized_ true for now, readData handles NaN
        } else {
             Serial.printf("Si7021 initial reading: Temp=%.1f C, Hum=%.1f %%\n", temp, hum);
        }
    }
    return initialized_;
}

bool TempHumSensor::readData(float& temperature, float& humidity) {
    // Always initialize output parameters to NAN
    temperature = NAN;
    humidity = NAN;

    if (!initialized_) {
        return false;
    }

    float t = sensor_.readTemperature();
    float h = sensor_.readHumidity();
    bool temp_valid = false;
    bool hum_valid = false;

    // Check if readings are not NaN (sensor library returns NaN on failure)
    if (!isnan(t)) {
        float validated_temp = DataValidator::validateTemperature(t);
        if (validated_temp != -1.0f) { // validateTemperature returns -1.0f for invalid
            temperature = validated_temp;
            temp_valid = true;
        }
    }

    if (!isnan(h)) {
        float validated_hum = DataValidator::validateHumidity(h);
        if (validated_hum != -1.0f) { // validateHumidity returns -1.0f for invalid
            humidity = validated_hum;
            hum_valid = true;
        }
    }

    // Optionally log if only one reading failed
    // if (temp_valid != hum_valid) {
    //     Serial.printf("WARN: Partial Si7021 read: Temp %s, Hum %s\n",
    //                   temp_valid ? "OK" : "FAIL", hum_valid ? "OK" : "FAIL");
    // }

    // Return true only if both temperature and humidity were read and validated successfully
    return temp_valid && hum_valid;
}

bool TempHumSensor::isInitialized() const {
    return initialized_;
} 