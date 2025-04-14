#include "main_screen.h"
#include "ui_manager.h" // Include UIManager to access state getters
#include "data_manager.h" // Include DataManager for getLatestData
#include <WiFi.h> // Still needed for WiFi.localIP() if used directly
#include <time.h> // Needed for time formatting

// Define constants (consider moving all to ui_constants.h if not already there)
const int H_PADDING_MAIN = 10; // Use unique names to avoid conflicts if ui_constants isn't perfect
const int V_PADDING_MAIN = 5;
const int LINE_HEIGHT_MAIN = 25;
const int TITLE_Y_MAIN = V_PADDING_MAIN + 5;
const int STATUS_Y_MAIN = 280;

// Constructor implementation - Pass DataManager reference to base class
MainScreen::MainScreen(TFT_eSPI& display, DataManager& dataMgr) :
    Screen(display, dataMgr) // Call base class constructor with DataManager
{}

// draw() method implementation
void MainScreen::draw(int yOffset /* = 0 */) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Title
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("Environment Monitor", tft.width() / 2, TITLE_Y_MAIN + yOffset);
    tft.setTextDatum(TL_DATUM); // Reset datum

    // Data Section
    int currentY = TITLE_Y_MAIN + LINE_HEIGHT_MAIN + V_PADDING_MAIN + yOffset;
    int labelX = H_PADDING_MAIN;
    int valueX = H_PADDING_MAIN + 110;

    tft.setTextSize(2);
    // Use getLatestData() helper from base class (which now uses DataManager)
    const EnvironmentData& latestData = getLatestData();

    // Temperature
    tft.drawString("Temp:", labelX, currentY);
    if (!isnan(latestData.temperature)) {
        tft.drawFloat(latestData.temperature, 1, valueX, currentY);
        tft.drawString(" C", valueX + tft.textWidth("00.0") + 5, currentY);
    } else {
        tft.drawString("---", valueX, currentY);
    }
    currentY += LINE_HEIGHT_MAIN - 5;

    // Humidity
    tft.drawString("Humidity:", labelX, currentY);
    if (!isnan(latestData.humidity)) {
        tft.drawFloat(latestData.humidity, 1, valueX, currentY);
        tft.drawString(" %", valueX + tft.textWidth("00.0") + 5, currentY);
    } else {
        tft.drawString("---", valueX, currentY);
    }
    currentY += LINE_HEIGHT_MAIN - 5;

    // Light
    tft.drawString("Light:", labelX, currentY);
    if (!isnan(latestData.lux)) {
        tft.drawFloat(latestData.lux, 0, valueX, currentY);
        tft.drawString(" lx", valueX + tft.textWidth("00000") + 5, currentY);
    } else {
        tft.drawString("---", valueX, currentY);
    }
    currentY += LINE_HEIGHT_MAIN - 5;

    // Noise
    tft.drawString("Noise:", labelX, currentY);
    if (!isnan(latestData.decibels)) {
        tft.drawFloat(latestData.decibels, 1, valueX, currentY);
        tft.drawString(" dB", valueX + tft.textWidth("00.0") + 5, currentY);
    } else {
        tft.drawString("---", valueX, currentY);
    }
    currentY += LINE_HEIGHT_MAIN - 5;

    // Separator Line
    tft.drawFastHLine(H_PADDING_MAIN, currentY, tft.width() - 2 * H_PADDING_MAIN, TFT_DARKGREY);
    currentY += V_PADDING_MAIN;

    // Time (Access timeInitialized via UIManager pointer)
    if (uiManagerPtr_ && uiManagerPtr_->isTimeInitialized()) {
      struct tm timeinfo;
      time_t now;
      time(&now);
      localtime_r(&now, &timeinfo);
      char timeString[20];
      strftime(timeString, sizeof(timeString), "%Y-%m-%d", &timeinfo);
      tft.drawString(timeString, labelX, currentY);
      currentY += LINE_HEIGHT_MAIN - 10;
      strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
      tft.drawString(timeString, labelX, currentY);
    } else {
      tft.drawString("Time not synced", labelX, currentY);
    }

    // Status Icons (Access status flags via UIManager pointer)
    if (uiManagerPtr_) {
        bool sdStatus = uiManagerPtr_->isSdCardInitialized();
        bool wifiStatus = uiManagerPtr_->isWifiConnected();

        tft.setTextSize(1);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int statusX = tft.width() - H_PADDING_MAIN;

        // SD Status
        tft.setTextDatum(TR_DATUM);
        if (sdStatus) {
            tft.drawString("SD", statusX, STATUS_Y_MAIN + yOffset);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("SD!", statusX, STATUS_Y_MAIN + yOffset);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
        }
        statusX -= (tft.textWidth(sdStatus ? "SD" : "SD!") + 10);

        // WiFi Status
        tft.setTextDatum(TR_DATUM);
        if (wifiStatus) {
             tft.drawString("WiFi", statusX, STATUS_Y_MAIN + yOffset);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("WiFi!", statusX, STATUS_Y_MAIN + yOffset);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
        }
    }

    tft.setTextDatum(TL_DATUM); // Reset datum
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Reset color
}
