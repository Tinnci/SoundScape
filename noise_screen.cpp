#include "noise_screen.h"

// Define constants from the old ui.cpp (consider moving to a shared config header later)
const int V_PADDING = 5;
const int TITLE_Y = V_PADDING + 5;

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

    // Check if decibels value is valid (using the dummy data check from base class)
    if (latestData.temperature > -900) { // Check against dummy temp value as indicator
        tft.drawFloat(latestData.decibels, 1, tft.width() / 2, valueY);
    } else {
        tft.drawString("N/A", tft.width() / 2, valueY);
    }

    // Unit - Below value, Size 3
    tft.setTextSize(3);
    tft.drawString("dB", tft.width() / 2, valueY + 50); // Apply offset (relative to valueY)

    tft.setTextDatum(TL_DATUM); // Reset datum
}
