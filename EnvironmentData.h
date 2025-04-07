#ifndef ENVIRONMENT_DATA_H
#define ENVIRONMENT_DATA_H

#include <time.h>

struct EnvironmentData {
    time_t timestamp;    // 时间戳
    float decibels;      // 噪声值 (dB)
    float humidity;      // 湿度 (%)
    float temperature;   // 温度 (°C)
    float lux;          // 光照强度 (lx)
    
    EnvironmentData() : 
        timestamp(0), 
        decibels(0.0f), 
        humidity(0.0f), 
        temperature(0.0f), 
        lux(0.0f) {}
};

#endif // ENVIRONMENT_DATA_H 