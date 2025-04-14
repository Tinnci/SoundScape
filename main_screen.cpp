#include "main_screen.h"
#include "ui_manager.h" // Include UIManager to access state getters
#include <WiFi.h> // Still needed for WiFi.localIP() if used directly
#include <time.h> // Needed for time formatting

// Define constants from the old ui.cpp (consider moving to a shared config header later)
const int H_PADDING = 10;
const int V_PADDING = 5;
const int LINE_HEIGHT = 25;
const int TITLE_Y = V_PADDING + 5;
const int STATUS_Y = 280; // Y position for status line (Adjust if screen height differs)

// Constructor implementation
MainScreen::MainScreen(TFT_eSPI& display, EnvironmentData* data, int& dataIdxRef) :
    Screen(display, data, dataIdxRef) // Call base class constructor
{}

// draw() method implementation (moved from ui.cpp::drawMainScreen)
void MainScreen::draw(int yOffset /* = 0 */) { // Add yOffset parameter
    // Note: We don't fillScreen here during transitions, UIManager handles clearing.
    // However, for a full redraw (not transition), we might still want to clear.
    // Let's assume UIManager clears before calling draw during transitions.
    // If it's a forced redraw (redrawNeeded=true, isTransitioning=false), we should clear.
    // This logic might need refinement in UIManager::update()
    // For now, let's keep fillScreen but it might cause flicker during transition.
    // A better approach uses sprites or double buffering.
    // tft.fillScreen(TFT_BLACK); // Remove this line

    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Title - Centered, Size 2
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("Environment Monitor", tft.width() / 2, TITLE_Y + yOffset); // Apply offset
    tft.setTextDatum(TL_DATUM); // Reset datum

    // Data Section - Size 2, aligned
    int currentY = TITLE_Y + LINE_HEIGHT + V_PADDING + yOffset; // Apply offset
    int labelX = H_PADDING;
    int valueX = H_PADDING + 110; // X position for values (adjust as needed)

    tft.setTextSize(2);
    const EnvironmentData& latestData = getLatestData(); // Use helper from base class

    // Temperature
    tft.drawString("Temp:", labelX, currentY);
    if (!isnan(latestData.temperature)) {
        tft.drawFloat(latestData.temperature, 1, valueX, currentY);
        tft.drawString(" C", valueX + tft.textWidth("00.0") + 5, currentY);
    } else {
        tft.drawString("---", valueX, currentY);
    }
    currentY += LINE_HEIGHT - 5; // 减小行间距

    // Humidity
    tft.drawString("Humidity:", labelX, currentY);
    if (!isnan(latestData.humidity)) {
        tft.drawFloat(latestData.humidity, 1, valueX, currentY);
        tft.drawString(" %", valueX + tft.textWidth("00.0") + 5, currentY);
    } else {
        tft.drawString("---", valueX, currentY);
    }
    currentY += LINE_HEIGHT - 5; // 减小行间距

    // Light
    tft.drawString("Light:", labelX, currentY);
    if (!isnan(latestData.lux)) {
        tft.drawFloat(latestData.lux, 0, valueX, currentY);
        tft.drawString(" lx", valueX + tft.textWidth("00000") + 5, currentY);
    } else {
        tft.drawString("---", valueX, currentY);
    }
    currentY += LINE_HEIGHT - 5; // 减小行间距

    // Noise
    tft.drawString("Noise:", labelX, currentY);
    if (!isnan(latestData.decibels)) {
        tft.drawFloat(latestData.decibels, 1, valueX, currentY);
        tft.drawString(" dB", valueX + tft.textWidth("00.0") + 5, currentY);
    } else {
        tft.drawString("---", valueX, currentY);
    }
    currentY += LINE_HEIGHT - 5; // 减小行间距

    // Optional Separator Line (Apply offset to Y)
    tft.drawFastHLine(H_PADDING, currentY, tft.width() - 2 * H_PADDING, TFT_DARKGREY);
    currentY += V_PADDING; // 减小分隔线后的间距

    // Time (if available) - Size 2 (Apply offset to Y)
    // Access timeInitialized via UIManager pointer
    if (uiManagerPtr_ && uiManagerPtr_->isTimeInitialized()) {
      struct tm timeinfo;
      time_t now;
      time(&now);
      localtime_r(&now, &timeinfo);
      char timeString[20];
      strftime(timeString, sizeof(timeString), "%Y-%m-%d", &timeinfo);
      tft.drawString(timeString, labelX, currentY);
      currentY += LINE_HEIGHT - 10; // 减小日期和时间之间的间距
      strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
      tft.drawString(timeString, labelX, currentY);
    } else {
      tft.drawString("Time not synced", labelX, currentY);
    }

    // Status Icons - Bottom Right, Size 1 (Apply offset to Y)
    // Access status flags via UIManager pointer
    if (uiManagerPtr_) { // Check if uiManagerPtr_ is valid
        bool sdStatus = uiManagerPtr_->isSdCardInitialized();
        bool wifiStatus = uiManagerPtr_->isWifiConnected();

        tft.setTextSize(1);
        tft.setTextColor(TFT_GREEN, TFT_BLACK); // Default to green
        int statusX = tft.width() - H_PADDING;

        // SD Status
        tft.setTextDatum(TR_DATUM); // Top Right
        if (sdStatus) {
            tft.drawString("SD", statusX, STATUS_Y + yOffset); // Apply offset
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("SD!", statusX, STATUS_Y + yOffset); // Apply offset
            tft.setTextColor(TFT_GREEN, TFT_BLACK); // Reset color
        }
        statusX -= (tft.textWidth(sdStatus ? "SD" : "SD!") + 10); // Adjust spacing

        // WiFi Status
        tft.setTextDatum(TR_DATUM);
        if (wifiStatus) {
             tft.drawString("WiFi", statusX, STATUS_Y + yOffset); // Apply offset
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("WiFi!", statusX, STATUS_Y + yOffset); // Apply offset
            tft.setTextColor(TFT_GREEN, TFT_BLACK); // Reset color
        }
    } // end if (uiManagerPtr_)

    tft.setTextDatum(TL_DATUM); // Reset datum
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Reset color
}
