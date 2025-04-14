#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <BH1750.h>
#include <Wire.h>       // BH1750 uses I2C
#include <cmath>        // For NAN
#include "data_validator.h"

class LightSensor {
public:
    LightSensor(uint8_t address = 0x23); // Default BH1750 address

    bool begin();
    bool readData(float& lux);
    bool isInitialized() const;

private:
    BH1750 sensor_;
    uint8_t i2c_address_;
    bool initialized_;
};

#endif // LIGHT_SENSOR_H 