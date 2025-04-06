#include "status_screen.h"
#include "ui_manager.h" // Include UIManager to access state getters
#include "ui.h" // Include for LED_MODE definitions
#include <WiFi.h> // Still needed for WiFi.localIP() if used directly

// Define constants from the old ui.cpp (consider moving to a shared config header later)
const int H_PADDING = 10;
const int V_PADDING = 5;
const int LINE_HEIGHT = 25;
const int TITLE_Y = V_PADDING + 5;

// Constructor implementation
StatusScreen::StatusScreen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef) :
    Screen(display, data, dataIdxRef) // Call base class constructor
{}

// draw() method implementation (moved from ui.cpp::drawStatusScreen)
void StatusScreen::draw(int yOffset /* = 0 */) { // Add yOffset parameter
    // Assume UIManager clears screen during transition
    // tft.fillScreen(TFT_DARKCYAN); // Remove fillScreen for transitions
    tft.setTextColor(TFT_WHITE, TFT_DARKCYAN); // Set color

    // Title - Centered, Size 2 (减小标题大小)
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("System Status", tft.width() / 2, TITLE_Y + 5 + yOffset); // 减小顶部间距
    tft.setTextDatum(TL_DATUM);

    // Status Info - Size 2, Aligned
    tft.setTextSize(2);
    int currentY = TITLE_Y + LINE_HEIGHT + yOffset; // 减小标题后的间距
    int labelX = H_PADDING;
    int statusX = H_PADDING + 120;

    // Access status flags via UIManager pointer
    if (!uiManagerPtr_) {
        tft.drawString("Error: UIManager not available", labelX, currentY);
        return;
    }

    bool wifiStatus = uiManagerPtr_->isWifiConnected();
    bool sdStatus = uiManagerPtr_->isSdCardInitialized();
    bool timeStatus = uiManagerPtr_->isTimeInitialized();
    uint8_t ledMode = uiManagerPtr_->getCurrentLedMode();

    // WiFi Status
    tft.drawString("WiFi:", labelX, currentY);
    if (wifiStatus) {
        tft.setTextColor(TFT_GREEN, TFT_DARKCYAN);
        tft.drawString("Connected", statusX, currentY);
        tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
        currentY += LINE_HEIGHT - 5; // 减小行间距
        tft.drawString("IP:", labelX + 10, currentY);
        tft.drawString(WiFi.localIP().toString(), statusX, currentY);
    } else {
        tft.setTextColor(TFT_RED, TFT_DARKCYAN);
        tft.drawString("Not Connected", statusX, currentY);
        tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    }
    currentY += LINE_HEIGHT; // 减小到下一个状态的间距

    // SD Card Status
    tft.drawString("SD Card:", labelX, currentY);
    if (sdStatus) {
        tft.setTextColor(TFT_GREEN, TFT_DARKCYAN);
        tft.drawString("Mounted", statusX, currentY);
        tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    } else {
        tft.setTextColor(TFT_RED, TFT_DARKCYAN);
        tft.drawString("Failed/Missing", statusX, currentY);
        tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    }
    currentY += LINE_HEIGHT; // 减小到下一个状态的间距

    // NTP Time Status
    tft.drawString("NTP Time:", labelX, currentY);
    if (timeStatus) {
        tft.setTextColor(TFT_GREEN, TFT_DARKCYAN);
        tft.drawString("Synced", statusX, currentY);
        tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    } else {
        tft.setTextColor(TFT_YELLOW, TFT_DARKCYAN);
        tft.drawString("Not Synced", statusX, currentY);
        tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    }
    currentY += LINE_HEIGHT; // 减小到下一个状态的间距

    // LED Mode Status
    tft.drawString("LED Mode:", labelX, currentY);
    switch(ledMode) {
        case LED_MODE_OFF: tft.drawString("Off", statusX, currentY); break;
        case LED_MODE_NOISE: tft.drawString("Noise", statusX, currentY); break;
        case LED_MODE_TEMP: tft.drawString("Temp", statusX, currentY); break;
        case LED_MODE_HUMIDITY: tft.drawString("Humidity", statusX, currentY); break;
        default: tft.drawString("Unknown", statusX, currentY); break;
    }
}
