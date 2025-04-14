#include "BleManager.h"
#include <cmath> // For isnan and round
#include <limits> // For INT16_MIN, UINT16_MAX

BleManager::BleManager(DataManager& dataMgr) :
    dataManager_(dataMgr),
    pAdvertising_(nullptr)
{}

void BleManager::begin() {
    Serial.println("Initializing BLE...");
    BLEDevice::init("SoundScapeSensor"); // Set BLE device name
    pAdvertising_ = BLEDevice::getAdvertising();

    // Set advertising parameters directly on the advertising object
    // advertisementData.setAppearance(0); // Appearance is set on BLEAdvertisementData now
    pAdvertising_->addServiceUUID(BLEUUID((uint16_t)0x181A)); // Advertise Environmental Sensing Service UUID - Set here
    // pAdvertising_->setScanResponse(false); // This is usually set on the AdvertisementData object if needed

    bleInitialized_ = true;
    Serial.println("BLE Initialized. Starting Advertising...");
    updateAdvertisingData(); // Set initial advertising data using the new method
    BLEDevice::startAdvertising();
}

void BleManager::updateAdvertisingData() {
    if (!bleInitialized_ || !pAdvertising_) return;

    std::string manufDataStr;
    buildManufacturerData(manufDataStr);

    // Create advertisement data object
    BLEAdvertisementData advertisementData = BLEAdvertisementData();

    // Set Flags (0x06 = LE General Discoverable Mode, BR/EDR Not Supported)
    advertisementData.setFlags(0x06);

    // Set Appearance (0 = Unknown/Generic)
    advertisementData.setAppearance(0);

    // Add Service UUID (Removed from here, set directly on pAdvertising_ in begin())
    // advertisementData.addServiceUUID(BLEUUID((uint16_t)0x181A));

    // Set Manufacturer Data (Convert std::string to Arduino String, handling potential nulls)
    advertisementData.setManufacturerData(String(manufDataStr.c_str(), manufDataStr.length()));

    // Apply the advertisement data
    pAdvertising_->setAdvertisementData(advertisementData);

    // Note: Scan Response data can be set similarly using pAdvertising_->setScanResponseData()
    // if needed, but we are keeping it simple for now.

    // Restarting advertising might still be necessary on some platforms/versions if data doesn't update.
    // If needed, uncomment:
    // BLEDevice::stopAdvertising();
    // BLEDevice::startAdvertising();
}

void BleManager::buildManufacturerData(std::string& data) {
    const EnvironmentData& latest = dataManager_.getLatestData();
    data.clear();
    // Resize to 10 bytes: 2 bytes for Company ID + 8 bytes for sensor data
    data.resize(10);

    // 0. Company ID (uint16, Little Endian)
    data[0] = (uint8_t)(companyId_ & 0xFF);
    data[1] = (uint8_t)((companyId_ >> 8) & 0xFF);

    // 1. Temperature (sint16, 0.01 C, Little Endian) - Now at index 2, 3
    int16_t temp_ble = isnan(latest.temperature) ? std::numeric_limits<int16_t>::min() : (int16_t)round(latest.temperature * 100.0f);
    data[2] = (uint8_t)(temp_ble & 0xFF);
    data[3] = (uint8_t)((temp_ble >> 8) & 0xFF);

    // 2. Humidity (uint16, 0.01 %, Little Endian) - Now at index 4, 5
    uint16_t hum_ble = isnan(latest.humidity) ? std::numeric_limits<uint16_t>::max() : (uint16_t)round(latest.humidity * 100.0f);
    data[4] = (uint8_t)(hum_ble & 0xFF);
    data[5] = (uint8_t)((hum_ble >> 8) & 0xFF);

    // 3. Illuminance (uint24, 0.01 lx, Little Endian) - Now at index 6, 7, 8
    uint32_t lux_ble32 = isnan(latest.lux) ? 0xFFFFFF : (uint32_t)round(latest.lux * 100.0f);
    if (lux_ble32 > 0xFFFFFF) lux_ble32 = 0xFFFFFF; // Clamp to 24 bits max value
    data[6] = (uint8_t)(lux_ble32 & 0xFF);
    data[7] = (uint8_t)((lux_ble32 >> 8) & 0xFF);
    data[8] = (uint8_t)((lux_ble32 >> 16) & 0xFF);

    // 4. Noise (uint8, dB) - Now at index 9
    uint8_t noise_ble = 0; // Default to 0 if invalid
    if (!isnan(latest.decibels) && latest.decibels >= 0.0f) {
        noise_ble = (uint8_t)round(latest.decibels);
        if (latest.decibels > 255.0f) { // Clamp to 8 bits max value
            noise_ble = 255;
        }
    }
    data[9] = noise_ble;
} 