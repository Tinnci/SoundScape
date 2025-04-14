#ifndef TEMP_HUM_SENSOR_H
#define TEMP_HUM_SENSOR_H

#include <Adafruit_Si7021.h>
#include <Wire.h>        // Si7021 uses I2C
#include <cmath>         // For NAN
#include "data_validator.h"

class TempHumSensor {
public:
    TempHumSensor();

    bool begin();
    // Reads both temperature and humidity, returns true if both are valid
    bool readData(float& temperature, float& humidity);
    bool isInitialized() const;

private:
    Adafruit_Si7021 sensor_;
    bool initialized_;
};

#endif // TEMP_HUM_SENSOR_H 