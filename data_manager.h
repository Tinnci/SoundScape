#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "EnvironmentData.h"
#include "i2s_mic_manager.h"
#include "temp_hum_sensor.h"
#include "light_sensor.h"
#include "FS.h"
#include "SD_MMC.h"
#include "memory_utils.h"
#include "ui_manager.h" // For checking time status

class DataManager {
public:
    // Constructor takes references or pointers to sensors and UI Manager
    DataManager(I2SMicManager& micMgr, TempHumSensor& thSensor, LightSensor& lSensor, UIManager& uiMgr);

    bool begin(); // Initialization logic (e.g., SD card check)
    void update(); // Called periodically in loop

    // Manually trigger data recording (e.g., for button press)
    void recordCurrentData();

    // Manually trigger saving data to SD card
    void saveDataToSd();

    // Provides access to the data buffer (const reference)
    const EnvironmentData* getDataBuffer() const;
    int getCurrentDataIndex() const; // Get current write index
    int getDataBufferSize() const;   // Get the total size of the buffer
    const EnvironmentData& getLatestData() const; // Helper to get the most recent valid entry
    bool isSdCardInitialized() const; // Getter for SD card status

private:
    static const int DATA_BUFFER_MINUTES = 24 * 60; // 24 hours of data
    EnvironmentData envData[DATA_BUFFER_MINUTES];
    int dataIndex;

    I2SMicManager& micManager_;
    TempHumSensor& tempHumSensor_;
    LightSensor& lightSensor_;
    UIManager& uiManager_; // Reference to UIManager to check time status

    bool sdCardOk_;
    unsigned long lastSensorReadTime_;
    unsigned long lastSaveTime_;
    bool isRecording_;

    static const unsigned long SENSOR_READ_INTERVAL = 1000; // ms
    static const unsigned long SAVE_INTERVAL = 60000; // ms

    // Internal helper methods
    bool initSDCardInternal();
    void createHeaderIfNeededInternal();
    void recordEnvironmentDataInternal();
    void saveEnvironmentDataToSDInternal();
};

#endif // DATA_MANAGER_H 