#include "status_screen.h"
#include "ui_manager.h" // Include UIManager to access state getters
#include "data_manager.h" // Include DataManager (though not directly used for data here)
#include "ui.h" // Include for LED_MODE definitions
#include "ui_constants.h" // Include constants
#include <WiFi.h> // Still needed for WiFi.localIP()

// Constructor implementation - Pass DataManager reference to base class
StatusScreen::StatusScreen(TFT_eSPI& display, DataManager& dataMgr) :
    Screen(display, dataMgr) // Call base class constructor with DataManager
{}

// draw() method implementation
void StatusScreen::draw(int yOffset /* = 0 */) {
    tft.setTextColor(TFT_WHITE, TFT_DARKCYAN); // Set color

    // Title - Centered, Size 2
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    // Use TITLE_Y from ui_constants.h
    tft.drawString("System Status", tft.width() / 2, TITLE_Y + 5 + yOffset);
    tft.setTextDatum(TL_DATUM);

    // Status Info - Size 2, Aligned
    tft.setTextSize(2);
    // Use TITLE_Y, LINE_HEIGHT, H_PADDING from ui_constants.h
    int currentY = TITLE_Y + LINE_HEIGHT + yOffset;
    int labelX = H_PADDING;
    int statusX = H_PADDING + 120;

    // Access status flags via UIManager pointer (available via base Screen class)
    if (!uiManagerPtr_) {
        tft.drawString("Error: UIManager N/A", labelX, currentY);
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
        currentY += LINE_HEIGHT - 5;
        tft.drawString("IP:", labelX + 10, currentY);
        tft.drawString(WiFi.localIP().toString(), statusX, currentY);
    } else {
        tft.setTextColor(TFT_RED, TFT_DARKCYAN);
        tft.drawString("Not Connected", statusX, currentY);
        tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    }
    currentY += LINE_HEIGHT;

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
    currentY += LINE_HEIGHT;

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
    currentY += LINE_HEIGHT;

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
