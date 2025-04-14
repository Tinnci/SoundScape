#include "light_screen.h"
#include "data_manager.h" // Include DataManager for getLatestData
#include "ui_constants.h" // Include constants (e.g., TITLE_Y)

// Constructor implementation - Pass DataManager reference to base class
LightScreen::LightScreen(TFT_eSPI& display, DataManager& dataMgr) :
    Screen(display, dataMgr) // Call base class constructor with DataManager
{}

// draw() method implementation
void LightScreen::draw(int yOffset /* = 0 */) {
    tft.setTextColor(TFT_BLACK, TFT_ORANGE); // Set color

    // Title - Centered, Size 3
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(3);
    // Use TITLE_Y from ui_constants.h
    tft.drawString("Light Intensity", tft.width() / 2, TITLE_Y + 10 + yOffset);

    // Value - Centered, Large Size 5
    tft.setTextSize(5);
    int valueY = tft.height() / 2 - 20 + yOffset;
    // Use getLatestData() helper from base class (which now uses DataManager)
    const EnvironmentData& latestData = getLatestData();

    // Check lux value
    if (!isnan(latestData.lux)) {
       tft.drawFloat(latestData.lux, 0, tft.width() / 2, valueY);
    } else {
       tft.drawString("---", tft.width() / 2, valueY);
    }

    // Unit - Below value, Size 3
    tft.setTextSize(3);
    tft.drawString("lx", tft.width() / 2, valueY + 50);

    tft.setTextDatum(TL_DATUM); // Reset datum
}
