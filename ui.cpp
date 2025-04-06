#include "ui.h"
#include <Arduino.h>
#include <cmath> // <-- 确保包含 cmath 以使用 std::lerp

// 注意：旧的 lerp 函数定义已被完全删除

// Helper function to display temporary messages
// Modified to accept tft object and remove screen state manipulation
void displayTemporaryMessage(TFT_eSPI& tft, const char* msg, uint32_t durationMs) {
    // ScreenState screenToRedraw = currentScreen; // Removed

    tft.fillScreen(TFT_BLACK); // Clear screen
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2); // Use a standard size for messages
    tft.setTextDatum(MC_DATUM); // Middle Center datum
    tft.drawString(msg, tft.width() / 2, tft.height() / 2);
    tft.setTextDatum(TL_DATUM); // Reset datum

    if (durationMs > 0) {
        delay(durationMs);
        // currentScreen = screenToRedraw; // Removed
        // redrawScreen = true; // Removed - UIManager should handle redraw after message if needed
        // Consider forcing redraw via UIManager if message duration > 0?
        // extern UIManager uiManager; // Need access to uiManager? Better to handle outside.
        // uiManager.forceRedraw();
    }
    // If durationMs is 0, the message stays until the next screen redraw triggered elsewhere
}


// Define constants needed by utility functions
// (Consider moving to a shared config header later)
const int V_PADDING_ANIM = 5; // Use different name to avoid redefinition if included elsewhere
const int TITLE_Y_ANIM = V_PADDING_ANIM + 5; // Note: This constant seems unused in the new animation

// Startup Animation Implementation (Using Sprite for smoother animation)
void runStartupAnimation(TFT_eSPI& tft, uint32_t durationMs) {
    // 设置动画参数
    const int titleStartY = tft.height() + 30;  // 开始位置（屏幕下方）
    const int titleEndY = tft.height() / 2;     // 结束位置（屏幕中间）

    unsigned long startTime = millis();
    float progress = 0.0f;
    int titleCurrentY = titleStartY;

    while (progress < 1.0f) {
        progress = (float)(millis() - startTime) / durationMs;
        progress = min(1.0f, progress);  // 确保不超过1.0

        // 使用 std::lerp 计算当前Y位置
        titleCurrentY = (int)std::lerp((float)titleStartY, (float)titleEndY, progress);

        // 清屏并绘制标题
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE);
        tft.drawString("ESP32-S3", tft.width() / 2, titleCurrentY);
        tft.drawString("环境监测器", tft.width() / 2, titleCurrentY + 30);

        delay(10);  // 短暂延迟以控制动画速度
    }
    // 动画结束后，确保最终状态绘制正确并停留片刻
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("ESP32-S3", tft.width() / 2, titleEndY);
    tft.drawString("环境监测器", tft.width() / 2, titleEndY + 30);
    tft.setTextDatum(TL_DATUM); // Reset datum
    delay(500); // 停留 500ms
}
