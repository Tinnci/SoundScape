#include "light_screen.h"

// Define constants from the old ui.cpp (consider moving to a shared config header later)
const int V_PADDING = 5;
const int TITLE_Y = V_PADDING + 5;

// Constructor implementation
LightScreen::LightScreen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef) :
    Screen(display, data, dataIdxRef) // Call base class constructor
{}

// draw() method implementation (moved from ui.cpp::drawLightScreen)
void LightScreen::draw(int yOffset /* = 0 */) { // Add yOffset parameter
    // Assume UIManager clears screen during transition
    // tft.fillScreen(TFT_ORANGE); // Remove fillScreen for transitions
    tft.setTextColor(TFT_BLACK, TFT_ORANGE); // Black text on orange (Set color)

    // Title - Centered, Size 3
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(3);
    tft.drawString("Light Intensity", tft.width() / 2, TITLE_Y + 10 + yOffset); // Apply offset

    // Value - Centered, Large Size 5
    tft.setTextSize(5);
    int valueY = tft.height() / 2 - 20 + yOffset; // Apply offset
    const EnvironmentData& latestData = getLatestData(); // Use helper from base class

    if (latestData.lux >= 0) { // Check for valid reading
       tft.drawFloat(latestData.lux, 0, tft.width() / 2, valueY);
    } else {
       tft.drawString("N/A", tft.width() / 2, valueY);
    }

    // Unit - Below value, Size 3
    tft.setTextSize(3);
    tft.drawString("lx", tft.width() / 2, valueY + 50); // Apply offset (relative to valueY)

    tft.setTextDatum(TL_DATUM); // Reset datum
}
