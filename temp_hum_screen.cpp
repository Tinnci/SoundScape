#include "temp_hum_screen.h"
#include "data_manager.h" // Include DataManager for getLatestData
#include "ui_constants.h" // Include constants if needed (e.g., TITLE_Y)

// Constructor implementation - Pass DataManager reference to base class
TempHumScreen::TempHumScreen(TFT_eSPI& display, DataManager& dataMgr) :
    Screen(display, dataMgr) // Call base class constructor with DataManager
{}

// draw() method implementation
void TempHumScreen::draw(int yOffset /* = 0 */) {
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN); // Set color

    // Title - Centered, Size 2
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    // Use TITLE_Y from ui_constants.h
    tft.drawString("Temp & Humidity", tft.width() / 2, TITLE_Y + 10 + yOffset);
    tft.setTextDatum(TL_DATUM);

    // Layout values
    // Use TITLE_Y, H_PADDING from ui_constants.h
    int valueY = TITLE_Y + 40 + yOffset;
    int labelX = H_PADDING + 10;
    int valueX = tft.width() - H_PADDING - 50;

    // Use getLatestData() helper from base class (which now uses DataManager)
    const EnvironmentData& latestData = getLatestData();

    // Temperature - Size 3
    tft.setTextSize(3);
    tft.drawString("Temp:", labelX, valueY + 5);
    tft.setTextDatum(TR_DATUM);
    if (!isnan(latestData.temperature)) {
        tft.drawFloat(latestData.temperature, 1, valueX, valueY);
        tft.drawString("C", valueX + 25, valueY + 5);
    } else {
        tft.drawString("---", valueX, valueY);
    }
    tft.setTextDatum(TL_DATUM);
    valueY += 70;

    // Humidity - Size 3
    tft.drawString("Humidity:", labelX, valueY + 5);
    tft.setTextDatum(TR_DATUM);
    if (!isnan(latestData.humidity)) {
        tft.drawFloat(latestData.humidity, 1, valueX, valueY);
        tft.drawString("%", valueX + 25, valueY + 5);
    } else {
        tft.drawString("---", valueX, valueY);
    }
    tft.setTextDatum(TL_DATUM);
}
