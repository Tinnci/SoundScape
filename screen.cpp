#include "screen.h"
#include "data_manager.h" // Include the full definition of DataManager

// Constructor Implementation
Screen::Screen(TFT_eSPI& display, DataManager& dataMgr) :
    tft(display),
    dataManager_(dataMgr),
    uiManagerPtr_(nullptr) // Initialize uiManagerPtr_ to nullptr
{}

// getLatestData Implementation
const EnvironmentData& Screen::getLatestData() const {
    // Now we have the full definition of DataManager, so we can call its methods
    return dataManager_.getLatestData();
} 