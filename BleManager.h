#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include "data_manager.h"

class BleManager {
public:
    BleManager(DataManager& dataMgr);
    void begin();
    void updateAdvertisingData();

private:
    DataManager& dataManager_;
    BLEAdvertising* pAdvertising_;
    uint16_t companyId_ = 0xFFFF; // Default Company ID for Manufacturer Data
    bool bleInitialized_ = false;

    void buildManufacturerData(std::string& data);
};

#endif // BLE_MANAGER_H 