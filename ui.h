#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h>     // Required for time_t in EnvironmentData

// LED显示模式
#define LED_MODE_OFF      0
#define LED_MODE_NOISE    1
#define LED_MODE_TEMP     2
#define LED_MODE_HUMIDITY 3

// ScreenState enum is no longer used globally
// EnvironmentData struct definition moved to screen.h
// Global variables are now managed by UIManager or passed via constructor

// 函数声明
void runStartupAnimation(TFT_eSPI& tft, uint32_t durationMs);
void displayTemporaryMessage(TFT_eSPI& tft, const char* message, uint32_t durationMs);

#endif // UI_H
