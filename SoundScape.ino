/**
 * ESP32S3 环境监测系统 (Refactored)
 * 
 * 功能：
 * 1. 使用 I2S 麦克风监测噪声 (I2SMicManager)
 * 2. 使用 Si7021 监测温湿度 (TempHumSensor)
 * 3. 使用 BH1750 监测光照 (LightSensor)
 * 4. 通过 WS2812B LED 显示状态 (LedController)
 * 5. SD卡数据存储和管理 (DataManager)
 * 6. WiFi连接和NTP时间同步 (Main Sketch / Future NetworkManager)
 * 7. 网络通信服务 (CommunicationManager)
 * 8. 多按钮交互界面 (UIManager, InputManager)
 * 9. 内存监控 (memory_utils, Main Sketch)
 * 硬件连接：
 * - INMP441: SCK->GPIO15, WS->GPIO16, SD->GPIO17
 * - Si7021和BH1750 (I2C): 
 *   - SDA->GPIO10
 *   - SCL->GPIO11
 * - SD卡: MISO->GPIO37, MOSI->GPIO35, SCK->GPIO36, CS->GPIO5
 * - 按钮: BTN1->GPIO2, BTN2->GPIO1, BTN3->GPIO41, BTN4->GPIO40, BTN5->GPIO42
 * - WS2812B LED: DATA->GPIO18
 * 
 * 传感器说明：
 * - Si7021: 工业级温湿度传感器
 *   - 温度范围: -40°C至125°C，精度±0.4°C
 *   - 湿度范围: 0-100%RH，精度±3%RH
 *   - I2C地址: 0x40
 * 
 * - BH1750: 光照强度传感器
 *   - 测量范围: 1-65535 lx
 *   - 分辨率: 1 lx
 *   - I2C地址: 0x23/0x5C
 */

#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "driver/i2s_std.h" // Needed for I2S_NUM_0 ? Maybe remove if not directly used.
#include "driver/i2s_common.h"

// --- Include New Manager Headers ---
#include "ui_manager.h"
#include "data_manager.h"
#include "input_manager.h"
#include "led_controller.h"
#include "communication_manager.h"
#include "i2s_mic_manager.h"
#include "temp_hum_sensor.h"
#include "light_sensor.h"
#include "BleManager.h"

// --- Include Screen Headers ---
#include "main_screen.h"
#include "noise_screen.h"
#include "temp_hum_screen.h"
#include "light_screen.h"
#include "status_screen.h"

// --- Include Utility Headers ---
#include "ui.h" // For startup animation, constants
#include "memory_utils.h"
// ui_constants.h is included by other headers
// data_validator.h is included by other headers
// EnvironmentData.h is included by other headers

// --- GPIO Definitions (保持不变) ---
// I2S麦克风引脚
#define I2S_WS_PIN 16
#define I2S_SD_PIN 17
#define I2S_SCK_PIN 15
#define I2S_PORT_NUM I2S_NUM_0 // Still needed for I2SMicManager constructor

// SD卡引脚 (SDMMC)
#define SD_CMD_PIN  45
#define SD_CLK_PIN  47
#define SD_DATA_PIN 21
#define SD_CD_PIN   48
#define SD_WP_PIN   14

// WS2812B LED引脚 (Moved to LedController)
// #define LED_PIN 18
// #define NUM_LEDS 4

// Buzzer Pin
#define BUZZER_PIN 4

// I2C引脚
#define I2C_SDA 38
#define I2C_SCL 39

// TFT 显示屏引脚 (TFT_eSPI uses User_Setup.h or these defines if not in User_Setup.h)
// #define TFT_SCLK 12
// #define TFT_MOSI 11
// #define TFT_RST  9
// #define TFT_DC   46
// #define TFT_CS   3
// #define TFT_BL   8

// --- Configuration ---
#define SAMPLE_RATE 16000    // 采样率 (Used by I2SMicManager)

// WiFi & NTP Configuration (Now passed to CommunicationManager)
const char* WIFI_SSID = "501_2.4G";
const char* WIFI_PASSWORD = "12340000";
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 28800;
const int DAYLIGHT_OFFSET_SEC = 0;

// --- Global Objects / Instances ---
TFT_eSPI tft = TFT_eSPI(); // TFT instance

// Instantiate Managers (Order matters for dependencies)
UIManager uiManager; // Needs to be accessible by screens and other managers

// Sensors
I2SMicManager micManager(SAMPLE_RATE, I2S_WS_PIN, I2S_SD_PIN, I2S_SCK_PIN, I2S_PORT_NUM);
TempHumSensor tempHumSensor;
LightSensor lightSensor; // Uses default address 0x23

// Data Manager (depends on sensors and UI Manager)
DataManager dataManager(micManager, tempHumSensor, lightSensor, uiManager);

// Communication Manager (Pass network config and UIManager reference)
CommunicationManager commManager(&micManager, &uiManager, WIFI_SSID, WIFI_PASSWORD, NTP_SERVER, GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC);

// Input Manager (depends on UI Manager, Data Manager, Comm Manager)
InputManager inputManager(uiManager, dataManager, commManager);

// LED Controller (depends on UI Manager and Data Manager)
LedController ledController(uiManager, dataManager);

// BLE Manager (depends on Data Manager)
BleManager bleManager(dataManager);

// Web Server (passed to commManager for setup)
AsyncWebServer httpServer(80);

// Screens (depend on tft, dataManager providing data access)
MainScreen mainScreen(tft, dataManager);
NoiseScreen noiseScreen(tft, dataManager);
TempHumScreen tempHumScreen(tft, dataManager);
LightScreen lightScreen(tft, dataManager);
StatusScreen statusScreen(tft, dataManager);

// Watchdog Timer
hw_timer_t* watchdog = NULL;

// Memory Monitoring Variables
unsigned long lastMemoryLog = 0;
const unsigned long MEMORY_LOG_INTERVAL = 600000; // 10 minutes

// --- Helper Functions (Moved or Removed) ---
// connectToWiFi and initTime removed

/**
 * Logs memory status to Serial.
 */
void logMemoryStatus() {
  Serial.println("\n==== 内存状态 ====");
  Serial.printf("空闲堆内存: %lu 字节\n", ESP.getFreeHeap());
  Serial.printf("最大空闲块: %lu 字节\n", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.printf("最小空闲堆: %lu 字节\n", ESP.getMinFreeHeap());
  
  float fragmentation = 0;
  if (ESP.getFreeHeap() > 0) {
    fragmentation = 100.0 - (float)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) * 100.0 / ESP.getFreeHeap();
    Serial.printf("内存分片率: %.1f%%\n", fragmentation);
  }
  if (fragmentation > 50) Serial.println("警告: 内存分片严重!");
  
  Serial.printf("程序空间: %lu 字节\n", ESP.getFreeSketchSpace());
  Serial.println("==================");
}

/**
 * Watchdog reset callback.
 */
void IRAM_ATTR resetModule(void) {
  ets_printf("看门狗触发重启!\n");
  esp_restart();
}

/**
 * Initializes the watchdog timer.
 */
void setupWatchdog() {
    watchdog = timerBegin(80000000); // 80 MHz clock
    if (watchdog == nullptr) {
        Serial.println("看门狗定时器初始化失败");
        return;
    }
    timerAttachInterrupt(watchdog, &resetModule);
    // Set alarm for 8 seconds, auto-reload=true, reload_count=0 (ESP-IDF v5.x style)
    timerAlarm(watchdog, 8000000, true, 0);
    // timerAlarmEnable is deprecated/not needed in ESP-IDF v5.x Arduino Core
    // timerAlarmEnable(watchdog);
    timerStart(watchdog); // Start the timer
    Serial.println("看门狗定时器已启用 (8秒超时)");
}

/**
 * Performs cleanup before restarting.
 */
void cleanup() {
  Serial.println("开始清理资源...");
  micManager.end(); // Stop I2S
  if (dataManager.isSdCardInitialized()) { // Use DataManager to check SD status
     dataManager.saveDataToSd(); // Save remaining data
  }
  // SD_MMC.end(); // Consider if DataManager should handle SD unmount? Keep here for now.
  commManager.stop(); // Stop TCP/WebSocket clients/server
  httpServer.end();   // Stop HTTP server

  // Clear display
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("系统安全关闭", tft.width() / 2, tft.height() / 2);
  
  ledController.setBrightness(0); // Turn off LEDs

  Serial.println("资源清理完成");
}

//=============================================================================
// SETUP
//=============================================================================
void setup() {
    try {
        Serial.begin(115200);
        while (!Serial) { yield(); }
        Serial.println("\n======================================");
        Serial.println(" ESP32S3 环境监测系统启动中 (Refactored)");
        Serial.println("======================================");

        // --- Initializations ---
        initEmergencyMemory(); // Initialize emergency memory pool first
        logMemoryStatus();     // Log initial memory status
        setupWatchdog();       // Start watchdog early

        Wire.begin(I2C_SDA, I2C_SCL); // Initialize I2C
        Serial.printf("I2C已初始化: SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);
        // I2C Scan (Optional, can be removed if devices are known)
        // ... (I2C scan code removed for brevity, was present in original)

        tft.init();            // Initialize TFT display
        tft.setRotation(3);
        tft.fillScreen(TFT_BLACK);
        pinMode(TFT_BL, OUTPUT); // Setup backlight pin
        digitalWrite(TFT_BL, HIGH); // Turn on backlight

        // --- Manager Initializations ---
        Serial.println("--- Initializing Managers ---");
        if (micManager.begin()) {
            Serial.printf("初始噪声读数: %.2f dB\n", micManager.readNoiseLevel(50)); // Use updated timeout
        } else {
            Serial.println("ERR: I2S Mic Manager 初始化失败!");
        }
        tempHumSensor.begin(); // Logs success/failure internally
        lightSensor.begin();   // Logs success/failure internally

        // DataManager needs sensors initialized, begin checks/inits SD card
        dataManager.begin();
        // Update UI with SD status AFTER DataManager has checked it in its begin() method
        uiManager.setSdCardStatus(dataManager.isSdCardInitialized());

        inputManager.begin();    // Initializes button pins
        ledController.begin();   // Initializes NeoPixels

        // --- BLE Initialization ---
        bleManager.begin();            // Call BleManager's begin

        // --- Network Initialization (using CommunicationManager) ---
        Serial.println("--- Initializing Network ---");
        if (commManager.connectWiFi()) { // Try connecting (updates UI status internally)
             commManager.syncNTPTime(); // Try syncing time (updates UI status internally)

             // Start Communication Servers (Requires WiFi)
             if (commManager.begin()) { // Start TCP command server
                  Serial.println("Communication servers (TCP/WebSocket) potentially started.");
                  // Setup HTTP/WebSocket server handlers via commManager
                  commManager.setupHttpServer(&httpServer);
                  commManager.setupWebSocketServer(&httpServer);
                  httpServer.begin(); // Start the actual AsyncWebServer
                  Serial.println("HTTP 和 WebSocket 服务器已启动 (Port 80)");
             } else {
                  Serial.println("Communication servers failed to start (likely WiFi issue during check).");
             }
        } else {
             Serial.println("WARN: WiFi 未连接，网络服务未启动。");
             // Ensure time status is false if WiFi failed initially
             uiManager.setTimeStatus(false);
             uiManager.forceRedraw();
        }


        // --- UI Initialization ---
        Serial.println("--- Initializing UI ---");
        runStartupAnimation(tft, 1500); // Run startup animation

        // Add screens to UI Manager
        uiManager.addScreen(&mainScreen);
        uiManager.addScreen(&noiseScreen);
        uiManager.addScreen(&tempHumScreen);
        uiManager.addScreen(&lightScreen);
        uiManager.addScreen(&statusScreen);
        uiManager.setInitialScreen(); // Set the first screen active

        // --- Final Steps ---
        lastMemoryLog = millis(); // Initialize memory log timer
        Serial.println("======================================");
        Serial.println("       系统初始化完成!");
        Serial.println("======================================");
        logMemoryStatus(); // Log memory status after all initializations

    } catch (const std::exception& e) {
        Serial.printf("设置过程中发生异常: %s\n", e.what());
        delay(1000); ESP.restart();
    } catch (...) {
        Serial.println("设置过程中发生未知异常");
        delay(1000); ESP.restart();
    }
}

//=============================================================================
// LOOP
//=============================================================================
void loop() {
    try {
        // --- Feed Watchdog ---
        timerWrite(watchdog, 0); // Reset watchdog counter

        // --- Core Updates ---
        commManager.update();      // Handle TCP commands and WebSocket cleanup
        commManager.streamAudioViaWebSocket(); // Send audio stream if clients connected
        inputManager.update();     // Check buttons (handles short/long presses)
        dataManager.update();      // Read sensors periodically, handle SD saving
        ledController.update();    // Update LED strip based on mode and data
        uiManager.update();        // Update active screen, handle transitions

        // --- Update BLE Advertising Data ---
        static unsigned long lastBleUpdate = 0;
        const unsigned long BLE_UPDATE_INTERVAL = 2000; // Update advertising data every 2 seconds
        if (millis() - lastBleUpdate >= BLE_UPDATE_INTERVAL) {
            lastBleUpdate = millis();
            bleManager.updateAdvertisingData();
        }

        // --- Memory Monitoring ---
        unsigned long currentMillis = millis();
        if (currentMillis - lastMemoryLog >= MEMORY_LOG_INTERVAL) {
            lastMemoryLog = currentMillis;
            logMemoryStatus();
            // Optional: Add fragmentation check here too
        }

        // Check for critical low memory
        if (ESP.getFreeHeap() < 5000) { // Threshold slightly higher than emergency release
             Serial.println("警告：内存严重不足，尝试释放紧急内存...");
             releaseEmergencyMemory();
             if (ESP.getFreeHeap() < 4096) { // Check again after release
                  Serial.println("错误：内存仍然不足，准备重启");
                  cleanup();
                  ESP.restart();
                  return; // Should not be reached
             }
        }

        // --- Yield/Delay ---
        yield(); // Allow background tasks (like WiFi, AsyncTCP) to run
        // delay(1); // Small delay can sometimes help stability but yield is preferred

    } catch (const std::exception& e) {
        Serial.printf("主循环中发生异常: %s\n", e.what());
        delay(1000); ESP.restart();
    } catch (...) {
        Serial.println("主循环中发生未知异常");
        delay(1000); ESP.restart();
    }
}
