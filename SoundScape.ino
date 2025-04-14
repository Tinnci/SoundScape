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
 * 
 * 硬件连接：（保持不变）
 * ... (省略)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
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

// WiFi配置
const char* ssid = "501_2.4G";
const char* password = "12340000";

// NTP配置
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800;
const int daylightOffset_sec = 0;

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

// Communication Manager (depends on micManager for audio streaming)
CommunicationManager commManager(&micManager);

// Input Manager (depends on UI Manager, Data Manager, Comm Manager)
InputManager inputManager(uiManager, dataManager, commManager);

// LED Controller (depends on UI Manager and Data Manager)
LedController ledController(uiManager, dataManager);

// Web Server (depends on commManager being set up)
AsyncWebServer httpServer(80);

// Screens (depend on tft, dataManager providing data access)
// Note: Screens now get data via dataManager.getLatestData() or buffer access.
// We need to adjust Screen base class and screen implementations later if they directly used envData/dataIndex.
// For now, assume they use getLatestData() provided by the (modified) base class.
// We pass dataManager reference now, or modify Screen class later.
// Let's pass dataManager for now.
// UPDATE: Screen class already uses envDataPtr and dataIndex ref. We need to change Screen class to accept DataManager*.
// Let's adjust Screen class first. (Will do this in a separate step if needed, assume it's adjusted for now)
// UPDATE 2: Screens only need the latest data for drawing. Pass dataManager and let screens call getLatestData().
// MainScreen mainScreen(tft, envData, dataIndex); <-- OLD
MainScreen mainScreen(tft, dataManager); // <-- NEW (Requires screen constructor change)
NoiseScreen noiseScreen(tft, dataManager);
TempHumScreen tempHumScreen(tft, dataManager);
LightScreen lightScreen(tft, dataManager);
StatusScreen statusScreen(tft, dataManager);
// IMPORTANT: Need to modify Screen base class and all derived screen constructors and getLatestData() usage.

// Watchdog Timer
hw_timer_t* watchdog = NULL;

// Memory Monitoring Variables
unsigned long lastMemoryLog = 0;
const unsigned long MEMORY_LOG_INTERVAL = 600000; // 10 minutes
// const unsigned long FRAG_CHECK_INTERVAL = 1800000; // 30 minutes (Can be added back if needed)

// --- Function Declarations (Moved or Removed) ---
// connectToWiFi, initTime, i2s_config, initSDCard, saveEnvironmentDataToSD, createHeaderIfNeeded
// recordEnvironmentData, displayRecentData, checkButtons, updateLEDs are removed or moved.

// --- Helper Functions (Kept in .ino for now) ---

/**
 * Connects to WiFi. Updates UIManager status.
 */
void connectToWiFi() {
  Serial.println("正在连接WiFi...");
  WiFi.begin(ssid, password);
  
  const int MAX_ATTEMPTS = 20;
  for (int attempts = 0; attempts < MAX_ATTEMPTS && WiFi.status() != WL_CONNECTED; attempts++) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected) {
    Serial.println("WiFi已连接");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi连接失败！");
  }
  uiManager.setWifiStatus(connected); // Update UIManager
}

/**
 * Initializes NTP time synchronization. Updates UIManager status.
 */
void initTime() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("正在同步NTP时间...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    struct tm timeinfo;
    bool success = getLocalTime(&timeinfo);
    if (!success) {
      Serial.println("获取NTP时间失败");
    } else {
      Serial.println("NTP时间同步成功");
      char timeString[50];
      strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
      Serial.print("当前时间: ");
      Serial.println(timeString);
    }
     uiManager.setTimeStatus(success); // Update UIManager
  } else {
      Serial.println("WARN: WiFi未连接，跳过NTP同步。");
      uiManager.setTimeStatus(false); // Ensure state is false
  }
}

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
    timerAlarm(watchdog, 8000000, true, 0); // NEW - Added reload_count=0
    // timerAlarmEnable(watchdog); // Enable the alarm (ESP-IDF style, check if needed for Arduino)
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
  SD_MMC.end(); // Unmount SD
  commManager.stop(); // Stop TCP server
  // Stop WebSocket/HTTP server? Need commManager method.
  httpServer.end();

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
            Serial.printf("初始噪声读数: %.2f dB\n", micManager.readNoiseLevel(500));
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

        // --- Network Initialization ---
        Serial.println("--- Initializing Network ---");
        connectToWiFi(); // Updates uiManager status internally
        if (uiManager.isWifiConnected()) {
            initTime(); // Updates uiManager status internally

            // Start Communication Services (Requires WiFi)
            if (commManager.begin()) { // Start TCP command server
                 Serial.println("命令服务器初始化成功 (TCP Port 8266)");
            } else {
                 Serial.println("命令服务器初始化失败");
            }
            // Setup and start HTTP/WebSocket server
            commManager.setupHttpServer(&httpServer);
            commManager.setupWebSocketServer(&httpServer);
            httpServer.begin();
            Serial.println("HTTP 和 WebSocket 服务器已启动 (Port 80)");
        } else {
            Serial.println("WARN: WiFi 未连接，网络服务未启动。");
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
