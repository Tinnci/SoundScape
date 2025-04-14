#include "noise_screen.h"
#include "data_manager.h" // Include DataManager for getLatestData
#include "ui_constants.h" // Include constants for DB_MIN, TITLE_Y etc.
#include <cmath>           // Include for isnan

// Constructor implementation - Pass DataManager reference to base class
NoiseScreen::NoiseScreen(TFT_eSPI& display, DataManager& dataMgr) :
    Screen(display, dataMgr) // Call base class constructor with DataManager
{}

// draw() method implementation
void NoiseScreen::draw(int yOffset /* = 0 */) {
    tft.setTextColor(TFT_WHITE, TFT_NAVY); // Set color

    // Title - Centered, Size 3
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(3);
    // Use TITLE_Y from ui_constants.h
    tft.drawString("Noise Level", tft.width() / 2, TITLE_Y + 10 + yOffset);

    // Value - Centered, Large Size 5
    tft.setTextSize(5);
    int valueY = tft.height() / 2 - 20 + yOffset;
    // Use getLatestData() helper from base class (which now uses DataManager)
    const EnvironmentData& latestData = getLatestData();

    // Check decibels value
    // Use DB_MIN from ui_constants.h
    if (!isnan(latestData.decibels) && latestData.decibels >= DB_MIN) {
        tft.drawFloat(latestData.decibels, 1, tft.width() / 2, valueY);
    } else {
        tft.drawString("---", tft.width() / 2, valueY);
    }

    // Unit - Below value, Size 3
    tft.setTextSize(3);
    tft.drawString("dB", tft.width() / 2, valueY + 50); // Apply offset (relative to valueY)

    tft.setTextDatum(TL_DATUM); // Reset datum
}
