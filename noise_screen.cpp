#include "noise_screen.h"
#include "ui_constants.h" // Include constants for DB_MIN
#include <cmath>           // Include for isnan

// Define constants from the old ui.cpp (consider moving to a shared config header later)
// const int V_PADDING = 5; // Defined in ui_constants.h - REMOVE/COMMENT OUT
// const int TITLE_Y = V_PADDING + 5; // Defined in ui_constants.h - REMOVE/COMMENT OUT

// Constructor implementation
NoiseScreen::NoiseScreen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef) :
    Screen(display, data, dataIdxRef) // Call base class constructor
{}

// draw() method implementation (moved from ui.cpp::drawNoiseScreen)
void NoiseScreen::draw(int yOffset /* = 0 */) { // Add yOffset parameter
    // Assume UIManager clears screen during transition
    // tft.fillScreen(TFT_NAVY); // Remove fillScreen for transitions
    tft.setTextColor(TFT_WHITE, TFT_NAVY); // Set color (background might be needed if not clearing)

    // Title - Centered, Size 3
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(3);
    tft.drawString("Noise Level", tft.width() / 2, TITLE_Y + 10 + yOffset); // Apply offset

    // Value - Centered, Large Size 5
    tft.setTextSize(5);
    int valueY = tft.height() / 2 - 20 + yOffset; // Apply offset
    const EnvironmentData& latestData = getLatestData(); // Use helper from base class

    // Check if decibels value is valid (not NaN and potentially above a minimum floor if needed)
    if (!isnan(latestData.decibels) && latestData.decibels >= DB_MIN) { // Check against NaN and DB_MIN
        tft.drawFloat(latestData.decibels, 1, tft.width() / 2, valueY);
    } else {
        // Display "N/A" or similar if the reading is invalid (NaN or below reasonable minimum)
        tft.drawString("---", tft.width() / 2, valueY); // Use "---" for no signal/invalid
    }

    // Unit - Below value, Size 3
    tft.setTextSize(3);
    tft.drawString("dB", tft.width() / 2, valueY + 50); // Apply offset (relative to valueY)

    tft.setTextDatum(TL_DATUM); // Reset datum
}
