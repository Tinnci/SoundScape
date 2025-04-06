#include "temp_hum_screen.h"

// Define constants from the old ui.cpp (consider moving to a shared config header later)
const int H_PADDING = 10;
const int V_PADDING = 5;
const int TITLE_Y = V_PADDING + 5;

// Constructor implementation
TempHumScreen::TempHumScreen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef) :
    Screen(display, data, dataIdxRef) // Call base class constructor
{}

// draw() method implementation (moved from ui.cpp::drawTempHumScreen)
void TempHumScreen::draw(int yOffset /* = 0 */) { // Add yOffset parameter
    // Assume UIManager clears screen during transition
    // tft.fillScreen(TFT_DARKGREEN); // Remove fillScreen for transitions
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN); // Set color

    // Title - Centered, Size 2 (改小标题字体)
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("Temp & Humidity", tft.width() / 2, TITLE_Y + 10 + yOffset);
    tft.setTextDatum(TL_DATUM);

    // Layout values stacked for clarity
    int valueY = TITLE_Y + 40 + yOffset; // 由于字体变小，可以减少顶部间距
    int labelX = H_PADDING + 10;  // 减小左边距
    int valueX = tft.width() - H_PADDING - 50; // 调整值的位置，距离右边缘留出更多空间

    const EnvironmentData& latestData = getLatestData();

    // Temperature - Size 3 (改小数值字体)
    tft.setTextSize(3);
    tft.drawString("Temp:", labelX, valueY + 5);
    tft.setTextDatum(TR_DATUM);
    if (latestData.temperature > -100) {
        tft.drawFloat(latestData.temperature, 1, valueX, valueY);
        tft.drawString("C", valueX + 25, valueY + 5);
    } else {
        tft.drawString("N/A", valueX, valueY);
    }
    tft.setTextDatum(TL_DATUM);
    valueY += 70; // 适当调整垂直间距

    // Humidity - Size 3 (改小数值字体)
    tft.drawString("Humidity:", labelX, valueY + 5);
    tft.setTextDatum(TR_DATUM);
    if (latestData.humidity >= 0) {
        tft.drawFloat(latestData.humidity, 1, valueX, valueY);
        tft.drawString("%", valueX + 25, valueY + 5);
    } else {
        tft.drawString("N/A", valueX, valueY);
    }
    tft.setTextDatum(TL_DATUM);
}
