#include "data_manager.h"
#include <Arduino.h>
#include <time.h>   // For time() and time formatting
#include <SD_MMC.h> // Ensure SD MMC library is included

// Constructor
DataManager::DataManager(I2SMicManager& micMgr, TempHumSensor& thSensor, LightSensor& lSensor, UIManager& uiMgr) :
    dataIndex(0),
    micManager_(micMgr),
    tempHumSensor_(thSensor),
    lightSensor_(lSensor),
    uiManager_(uiMgr),
    sdCardOk_(false),
    lastSensorReadTime_(0),
    lastSaveTime_(0),
    isRecording_(false)
{
    // Optionally initialize the buffer with default/invalid data
    for (int i = 0; i < DATA_BUFFER_MINUTES; ++i) {
        envData[i] = EnvironmentData(); // Uses default constructor (all NAN/0)
    }
}

// Initialization logic
bool DataManager::begin() {
    sdCardOk_ = initSDCardInternal();
    if (sdCardOk_) {
        createHeaderIfNeededInternal();
        Serial.println("[DataManager] SD Card Initialized OK.");
    } else {
        Serial.println("[DataManager] WARN: SD Card Failed to Initialize.");
    }
    lastSensorReadTime_ = millis(); // Initialize timers
    lastSaveTime_ = millis();
    return true; // DataManager itself always "begins" successfully
}

// Update function called periodically in loop
void DataManager::update() {
    unsigned long currentMillis = millis();

    // --- 1. Sensor Data Recording ---
    if (currentMillis - lastSensorReadTime_ >= SENSOR_READ_INTERVAL) {
        lastSensorReadTime_ = currentMillis;
        recordEnvironmentDataInternal();
        // Notify UI Manager that data might have changed (it will decide if redraw is needed)
        uiManager_.setNeedsDataUpdate(true);
    }

    // --- 2. SD Card Saving Logic ---
    if (sdCardOk_) {
        // Check if buffer is full OR save interval passed (and there's data to save)
        if (dataIndex >= DATA_BUFFER_MINUTES ||
            (dataIndex > 0 && currentMillis - lastSaveTime_ >= SAVE_INTERVAL))
        {
            saveEnvironmentDataToSDInternal(); // This resets dataIndex
            lastSaveTime_ = currentMillis;
        } else if (dataIndex == 0) {
            // If index was reset by saving, update save timer
            lastSaveTime_ = currentMillis;
        }
    } else {
        // No SD Card: Buffer full logic is handled inside recordEnvironmentDataInternal (wraps index)
        // We might still want to reset the save timer conceptually if the buffer fills
         if (dataIndex >= DATA_BUFFER_MINUTES) {
             lastSaveTime_ = currentMillis;
         }
    }
}

// Manually trigger data recording
void DataManager::recordCurrentData() {
    Serial.println("[DataManager] Manual data recording triggered.");
    recordEnvironmentDataInternal();
    uiManager_.setNeedsDataUpdate(true); // Data changed, notify UI
}

// Manually trigger saving data to SD card
void DataManager::saveDataToSd() {
    if (sdCardOk_) {
        Serial.println("[DataManager] Manual SD save triggered.");
        saveEnvironmentDataToSDInternal();
        lastSaveTime_ = millis(); // Reset save timer
    } else {
        Serial.println("[DataManager] Manual SD save failed: SD card not available.");
    }
}


// --- Data Accessors ---

const EnvironmentData* DataManager::getDataBuffer() const {
    return envData;
}

int DataManager::getCurrentDataIndex() const {
    return dataIndex;
}

int DataManager::getDataBufferSize() const {
    return DATA_BUFFER_MINUTES;
}

// Helper to get the most recent valid entry
const EnvironmentData& DataManager::getLatestData() const {
    // Calculate the index of the last written element in the circular buffer
    // If dataIndex is 0, it means the buffer either is empty or just wrapped around.
    // If it wrapped, the last element is at the end. If empty, this still returns envData[size-1] which is default.
    int latestIdx = (dataIndex == 0) ? (DATA_BUFFER_MINUTES - 1) : (dataIndex - 1);

    // Add bounds check just in case (should not happen with correct logic)
    if (latestIdx < 0 || latestIdx >= DATA_BUFFER_MINUTES) {
         Serial.printf("ERR: Invalid latestIdx %d calculated (dataIndex=%d). Returning dummy.\n", latestIdx, dataIndex);
         static EnvironmentData dummyData; // Return a static dummy if index is invalid
         dummyData = EnvironmentData(); // Reset dummy
         return dummyData;
    }

    return envData[latestIdx];
}

// <<<<< ADDED Implementation for SD card status getter
bool DataManager::isSdCardInitialized() const {
    return sdCardOk_;
}

// --- Internal Helper Methods ---

bool DataManager::initSDCardInternal() {
  // Logic moved from SoundScape.ino::initSDCard
  if (!SD_MMC.begin()) {
    Serial.println("[DataManager] SD_MMC Card Mount Failed!");
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[DataManager] No SD_MMC card attached");
    return false;
  }

  Serial.print("[DataManager] SD_MMC Card Type: ");
  if (cardType == CARD_MMC) Serial.println("MMC");
  else if (cardType == CARD_SD) Serial.println("SDSC");
  else if (cardType == CARD_SDHC) Serial.println("SDHC");
  else Serial.println("UNKNOWN");

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("[DataManager] SD_MMC Card Size: %lluMB\n", cardSize);
  return true;
}

void DataManager::createHeaderIfNeededInternal() {
  // Logic moved from SoundScape.ino::createHeaderIfNeeded
  if (sdCardOk_ && !SD_MMC.exists("/env_data.csv")) {
    File dataFile = SD_MMC.open("/env_data.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("timestamp,datetime,decibels,humidity,temperature,lux");
      dataFile.close();
      Serial.println("[DataManager] Created CSV header file (/env_data.csv)");
    } else {
         Serial.println("[DataManager] ERR: Failed to open env_data.csv for writing header.");
         sdCardOk_ = false; // Mark SD as not OK if we can't write header
    }
  }
}

void DataManager::recordEnvironmentDataInternal() {
    // Logic moved and adapted from SoundScape.ino::recordEnvironmentData
    if (isRecording_) {
        return; // Prevent re-entry
    }
    isRecording_ = true;

    // Check memory (optional, but good practice)
    if (isLowMemory(15000)) {
        Serial.println("[DataManager] WARN: Low memory detected before recording, releasing emergency memory.");
        releaseEmergencyMemory();
        if (isLowMemory(10000)) {
             Serial.println("[DataManager] ERR: Memory critically low even after releasing emergency pool! Skipping record.");
             isRecording_ = false;
             return;
        }
    }

    // --- 1. Prepare Data Structure ---
    EnvironmentData newData;
    // Use UIManager to check if NTP time is synced
    time_t now = uiManager_.isTimeInitialized() ? time(NULL) : millis() / 1000;
    newData.timestamp = now;
    // Initialize sensor readings to NAN
    newData.decibels = NAN;
    newData.humidity = NAN;
    newData.temperature = NAN;
    newData.lux = NAN;

    // --- 2. Read Sensors ---
    bool micReadSuccess = false;
    bool tempHumReadSuccess = false;
    bool lightReadSuccess = false;

    // Noise Level
    float db_reading = micManager_.readNoiseLevel(500); // Use the I2S Manager instance
    if (!isnan(db_reading)) {
        newData.decibels = db_reading; // Already validated by micManager
        micReadSuccess = true;
        // Check for high noise AFTER reading
        if (micManager_.isHighNoise()) {
             // Serial.printf("[DataManager] High noise detected: %.1f dB\n", newData.decibels);
             // Consider signaling UIManager or another component if immediate action needed
        }
    }

    // Temperature & Humidity
    float temp_reading, hum_reading;
    if (tempHumSensor_.readData(temp_reading, hum_reading)) { // Use TempHumSensor instance
        newData.temperature = temp_reading;
        newData.humidity = hum_reading;
        tempHumReadSuccess = true;
    }

    // Light Level
    float lux_reading;
    if (lightSensor_.readData(lux_reading)) { // Use LightSensor instance
        newData.lux = lux_reading;
        lightReadSuccess = true;
    }

    // --- 3. Store Data ---
    // Ensure index wraps around if it reaches the end of the buffer
    if (dataIndex >= DATA_BUFFER_MINUTES) {
        // Serial.println("[DataManager] DBG: Data buffer full, wrapping around.");
        dataIndex = 0;
    }

    // Store the new data (valid or NAN) into the buffer
    envData[dataIndex] = newData;

    // --- 4. Log Data (Optional Debugging) ---
    // Serial.printf("\n==== DM Record [%d] @ %lld ====\n", dataIndex, (long long)now);
    // if (!isnan(newData.decibels)) Serial.printf("Noise: %.1f dB %s\n", newData.decibels, micReadSuccess ? "" : "(ERR)"); else Serial.println("Noise: ---");
    // if (!isnan(newData.humidity)) Serial.printf("Humid: %.1f %% %s\n", newData.humidity, tempHumReadSuccess ? "" : "(ERR)"); else Serial.println("Humid: ---");
    // if (!isnan(newData.temperature)) Serial.printf("Temp:  %.1f C %s\n", newData.temperature, tempHumReadSuccess ? "" : "(ERR)"); else Serial.println("Temp:  ---");
    // if (!isnan(newData.lux)) Serial.printf("Light: %.1f lx %s\n", newData.lux, lightReadSuccess ? "" : "(ERR)"); else Serial.println("Light: ---");
    // Serial.println("==========================");


    // --- 5. Increment Index ---
    dataIndex++;

    // --- 6. Reset Recording Flag ---
    isRecording_ = false;
}

void DataManager::saveEnvironmentDataToSDInternal() {
    // Logic moved from SoundScape.ino::saveEnvironmentDataToSD
    if (!sdCardOk_) {
        Serial.println("[DataManager] ERR: Cannot save to SD, card not OK.");
        // If SD becomes unavailable, reset dataIndex to prevent buffer overflow if not handled elsewhere
        // dataIndex = 0; // Or just let it wrap around as per record logic
        return;
    }
    if (dataIndex == 0) {
         // Serial.println("[DataManager] DBG: No new data to save to SD.");
         return; // Nothing to save
    }

    // Attempt to open the file in append mode
    // Use a local File object, don't rely on a global one
    File dataFile = SD_MMC.open("/env_data.csv", FILE_APPEND);
    if (!dataFile) {
        Serial.println("[DataManager] ERR: Failed to open /env_data.csv for appending.");
        sdCardOk_ = false; // Assume SD card issue if file cannot be opened
        // Consider resetting dataIndex or implementing retry logic
        return;
    }

    Serial.printf("[DataManager] Saving %d records to SD card...\n", dataIndex);
    int recordsSaved = 0;

    // Iterate through the data buffer up to the current dataIndex
    for (int i = 0; i < dataIndex; i++) {
        // Format timestamp
        char timeString[25]; // Increased buffer size slightly
        struct tm timeinfo;
        localtime_r(&envData[i].timestamp, &timeinfo); // Use reentrant version
        strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);

        // Prepare data line string
        String dataLine = String(envData[i].timestamp) + "," +
                          String(timeString) + "," +
                          String(envData[i].decibels, 1) + "," + // Format float precision
                          String(envData[i].humidity, 1) + "," +
                          String(envData[i].temperature, 1) + "," +
                          String(envData[i].lux, 0); // Lux usually whole number

        // Write line to file
        if (dataFile.println(dataLine)) {
            recordsSaved++;
        } else {
            Serial.println("[DataManager] ERR: Error writing data line to SD card!");
            // Optionally break or try to continue
            break; // Stop saving on error
        }
    }

    dataFile.close(); // Close the file

    Serial.printf("[DataManager] Successfully saved %d records.\n", recordsSaved);

    // Reset data index ONLY if saving was successful (or partially successful maybe?)
    // If recordsSaved < dataIndex, some data might be lost if we reset.
    // Safest: Reset index regardless to prevent re-saving old data on next attempt.
    dataIndex = 0;

    // Re-initialize buffer with default values after saving (optional)
    // for (int i = 0; i < DATA_BUFFER_MINUTES; ++i) {
    //     envData[i] = EnvironmentData();
    // }
}
