/**
 * ESP32S3 环境监测系统
 * 
 * 功能：
 * 1. 使用INMP441 I2S麦克风监测环境噪声
 * 2. 使用Si7021传感器监测温湿度
 * 3. 使用BH1750传感器监测光照强度
 * 4. 通过WS2812B LED显示环境状态
 * 5. 支持SD卡数据存储
 * 6. WiFi连接和NTP时间同步
 * 7. 多按钮交互界面 (使用UIManager)
 * 
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
#include "driver/i2s_std.h"
#include "driver/i2s_common.h"
#include <WiFi.h>
#include <time.h>
#include "FS.h"
#include "SD_MMC.h"
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_Si7021.h>
#include <BH1750.h>
#include <TFT_eSPI.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#include "ui_manager.h"
#include "main_screen.h"
#include "noise_screen.h"
#include "temp_hum_screen.h"
#include "light_screen.h"
#include "status_screen.h"
#include "ui.h"
#include "ui_constants.h"
#include "data_validator.h"
#include "i2s_mic_manager.h"
#include "memory_utils.h"
#include "communication_manager.h"

// GPIO定义
// I2S麦克风引脚
#define I2S_WS_PIN 16     // Word Select (WS)
#define I2S_SD_PIN 17     // Serial Data (SD)
#define I2S_SCK_PIN 15    // Serial Clock (SCK)
#define I2S_PORT_NUM I2S_NUM_0 // Use I2S port 0

// SD卡引脚
#define SD_CMD_PIN  45 // SD_CMD
#define SD_CLK_PIN  47 // SD_CLK
#define SD_DATA_PIN 21 // SD_DATA
#define SD_CD_PIN   48 // SD_CD (Card Detect)
#define SD_WP_PIN   14 // SD_WP (Write Protect)

// 按钮引脚
#define BTN1_PIN 2  // Up
#define BTN2_PIN 1  // Right
#define BTN3_PIN 41 // Down
#define BTN4_PIN 40 // Left
#define BTN5_PIN 42 // Center

// WS2812B LED引脚
#define LED_PIN 18
#define NUM_LEDS 4

// Buzzer Pin
#define BUZZER_PIN 4

// I2C引脚
#define I2C_SDA 38
#define I2C_SCL 39

// 数据记录配置
#define RECORD_TIME_MS 1000  // 每次记录的时长（毫秒）
#define SAMPLE_RATE 16000    // 采样率
#define SAMPLE_BITS 32       // 采样位深
#define SENSOR_READ_INTERVAL 1000 // 每 1000ms (1秒) 读取一次传感器
#define BROADCAST_INTERVAL 5000   // 每 5000ms 广播一次数据
#define HOURS_TO_KEEP 24     // 保存24小时数据

// 噪声阈值设定
#define NOISE_THRESHOLD_LOW 50     // dB
#define NOISE_THRESHOLD_MEDIUM 70  // dB
#define NOISE_THRESHOLD_HIGH 85    // dB

// LED状态定义在 ui.h 中

// 变量声明
EnvironmentData envData[24 * 60]; // 存储24小时中每分钟的数据
int dataIndex = 0;
unsigned long lastSensorReadTime = 0; // 用于传感器读取的计时器
unsigned long lastBroadcastTime = 0;  // 用于数据广播的计时器
unsigned long lastSaveTime = 0;       // 用于SD卡保存的计时器
unsigned long startTime = 0;
volatile bool isRecording = false;  // 添加记录状态标志
// Shared states (wifi, sd, time, led mode) are now managed by uiManager

// 按键长按检测 (Keep for now, might integrate later)
const unsigned long LONG_PRESS_MS = 1000; // 1 second for long press
unsigned long btn4PressTime = 0;
unsigned long btn5PressTime = 0;
bool btn4LongPressTriggered = false;
bool btn5LongPressTriggered = false;

// 创建LED对象
Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// 按钮状态变量 (用于边沿检测)
bool lastBtn1State = HIGH;
bool lastBtn2State = HIGH;
bool lastBtn3State = HIGH;
bool lastBtn4State = HIGH;
bool lastBtn5State = HIGH;

// 添加传感器对象
Adafruit_Si7021 si7021 = Adafruit_Si7021();
BH1750 lightMeter(0x23); // 默认使用0x23地址

// 显示屏引脚
#define TFT_SCLK 12    // LCD_SCL
#define TFT_MOSI 11    // LCD_SDA
#define TFT_RST  9     // LCD_RST
#define TFT_DC   46    // LCD_DC
#define TFT_CS   3     // LCD_CS
#define TFT_BL   8     // LCD_BL

// 创建TFT对象
TFT_eSPI tft = TFT_eSPI();

// --- Instantiate UI Manager and Screens ---
UIManager uiManager;
MainScreen mainScreen(tft, envData, dataIndex);
NoiseScreen noiseScreen(tft, envData, dataIndex);
TempHumScreen tempHumScreen(tft, envData, dataIndex);
LightScreen lightScreen(tft, envData, dataIndex);
StatusScreen statusScreen(tft, envData, dataIndex);
// --- End Instantiation ---

// I2S配置变量
i2s_chan_handle_t rx_handle;

// 内存监控相关变量
unsigned long lastMemoryLog = 0;
const unsigned long MEMORY_LOG_INTERVAL = 600000; // 每10分钟记录一次内存状态
unsigned long lastFragCheck = 0;
const unsigned long FRAG_CHECK_INTERVAL = 1800000; // 每30分钟检查一次内存分片

// 看门狗定时器
hw_timer_t* watchdog = NULL;

// 创建I2S麦克风管理器实例
I2SMicManager micManager(SAMPLE_RATE, I2S_WS_PIN, I2S_SD_PIN, I2S_SCK_PIN, I2S_PORT_NUM);

// 常量定义
#define EMERGENCY_MEMORY_SIZE 4096  // 4KB 紧急内存

// WiFi配置
const char* ssid = "501_2.4G";
const char* password = "12340000";

// NTP配置
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800;
const int daylightOffset_sec = 0;

// 添加通信管理器实例
CommunicationManager commManager(&micManager); // Pass micManager instance

// --- Instantiate Web Server --- (需要确保 commManager 已经实例化)
AsyncWebServer httpServer(80); // 在端口 80 创建 HTTP 服务器

/**
 * 连接WiFi网络
 * 尝试连接配置的WiFi网络，超时时间为10秒
 * 如果连接失败，系统将继续运行但使用相对时间
 */
void connectToWiFi() {
  Serial.println("正在连接WiFi...");
  WiFi.begin(ssid, password);
  
  // 尝试连接，最多等待10秒（20次尝试，每次500ms）
  const int MAX_ATTEMPTS = 20;
  for (int attempts = 0; attempts < MAX_ATTEMPTS && WiFi.status() != WL_CONNECTED; attempts++) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi已连接");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi连接失败！将使用相对时间记录。");
  }
}

/**
 * 初始化NTP时间同步
 * 通过NTP服务器同步系统时间
 * 成功后更新UIManager状态
 */
void initTime() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("正在同步NTP时间...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("获取NTP时间失败");
      uiManager.setTimeStatus(false); // Update UIManager state
      return;
    }
    
    Serial.println("NTP时间同步成功");
    char timeString[50];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.print("当前时间: ");
    Serial.println(timeString);

    uiManager.setTimeStatus(true); // Update UIManager state

    // 可以断开WiFi以省电
    // WiFi.disconnect(true);
    // WiFi.mode(WIFI_OFF);
  } else {
      uiManager.setTimeStatus(false); // Ensure state is false if WiFi not connected
  }
}

/**
 * 配置I2S接口
 * 使用I2SMicManager类初始化I2S麦克风
 */
void i2s_config() {
    if (!micManager.begin()) {
        Serial.println("警告：使用原始i2s_config()尝试重新初始化I2S");
        
        // 如果micManager初始化失败，尝试使用原始代码作为备用
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT_NUM, I2S_ROLE_MASTER);
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle)); // Allocate a new RX channel

        // Configuration for the I2S standard mode
        i2s_std_config_t std_cfg = {
            .clk_cfg = {
                .sample_rate_hz = SAMPLE_RATE,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
            .slot_cfg = {
                .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_MONO,
                .slot_mask = I2S_STD_SLOT_LEFT,
                .ws_width = 1,
                .ws_pol = false,
                .bit_shift = false,
                .left_align = false,
                .big_endian = false,
                .bit_order_lsb = false,
            },
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = (gpio_num_t)I2S_SCK_PIN,
                .ws = (gpio_num_t)I2S_WS_PIN,
                .dout = I2S_GPIO_UNUSED,
                .din = (gpio_num_t)I2S_SD_PIN,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false,
                },
            },
        };

        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
        
        Serial.println("I2S备用初始化完成，使用引脚: SCK=" + String(I2S_SCK_PIN) + ", WS=" + String(I2S_WS_PIN) + ", SD=" + String(I2S_SD_PIN));
    }
    
    Serial.println("I2S初始化完成");
}

/**
 * 初始化SD卡
 * 检查SD卡是否存在并可用
 * 返回true表示初始化成功，false表示失败
 */
bool initSDCard() {
  // Initialize SDMMC (using default settings: mount point "/sdcard", 1-bit mode initially, then 4-bit if possible)
  if (!SD_MMC.begin()) { 
    Serial.println("SD_MMC 卡初始化失败!");
    return false;
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("未找到 SD_MMC 卡");
    return false;
  }
  
  Serial.print("SD_MMC 卡类型: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("未知");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC 卡大小: %lluMB\n", cardSize);
  Serial.println("SD_MMC 卡初始化成功");
  return true;
}

/**
 * 保存环境数据到SD卡
 * 将环境数据数组中的数据写入CSV文件
 * 并重置数据索引
 */
void saveEnvironmentDataToSD() {
  // Note: Calling SD_MMC.begin() again might not be necessary if it succeeded in setup().
  // Consider just trying to open the file directly or checking if the card is mounted.
  // However, keeping the structure similar to the original SD code for now.
  if (!SD_MMC.begin()) { 
    Serial.println("无法访问 SD_MMC 卡，数据未保存");
    return;
  }
  
  File dataFile = SD_MMC.open("/env_data.csv", FILE_APPEND);
  if (!dataFile) {
    Serial.println("无法打开数据文件进行写入 (SD_MMC)");
    return;
  }
  
  Serial.println("正在保存数据到SD卡...");
  
  for (int i = 0; i < dataIndex; i++) {
    // 获取时间戳的日期和时间格式
    char timeString[30];
    struct tm *timeinfo;
    timeinfo = localtime(&envData[i].timestamp);
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    // 写入数据行：timestamp,datetime,decibels,humidity,temperature,light
    dataFile.print(envData[i].timestamp);
    dataFile.print(",");
    dataFile.print(timeString);
    dataFile.print(",");
    dataFile.print(envData[i].decibels);
    dataFile.print(",");
    dataFile.print(envData[i].humidity);
    dataFile.print(",");
    dataFile.print(envData[i].temperature);
    dataFile.print(",");
    dataFile.println(envData[i].lux);
  }
  
  dataFile.close();
  Serial.print("成功保存 ");
  Serial.print(dataIndex);
  Serial.println(" 条记录到SD卡");
  
  // 重置数据索引
  dataIndex = 0;
}

/**
 * 创建CSV文件头
 * 如果数据文件不存在，创建新文件并写入列标题
 * 文件格式：timestamp,datetime,decibels,humidity,temperature,light
 */
void createHeaderIfNeeded() {
  if (!SD_MMC.exists("/env_data.csv")) {
    File dataFile = SD_MMC.open("/env_data.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("timestamp,datetime,decibels,humidity,temperature,light");
      dataFile.close();
      Serial.println("创建CSV头文件 (SD_MMC)");
    }
  }
}

/**
 * 记录当前环境数据
 * 采集并处理噪声、温度、湿度、光照数据
 * 存储到环境数据数组中，并处理无效数据
 */
void recordEnvironmentData() {
    // 保留 isRecording 标志作为基本的重入保护
    if (isRecording) {
        return;
    }
    isRecording = true;

    // 检查内存状态
    if (isLowMemory(15000)) { // 阈值可以调整
        Serial.println("WARN: Low memory detected before recording, releasing emergency memory.");
        releaseEmergencyMemory();
        // 可以在释放后再检查一次，如果仍然很低，可能需要更激进的操作或日志
        if (isLowMemory(10000)) {
             Serial.println("ERR: Memory critically low even after releasing emergency pool!");
             // 可以考虑跳过本次记录或重启
             // isRecording = false; // 确保退出前重置标志
             // return;
        }
    }

    // 为本次记录准备数据结构
    EnvironmentData newData;
    time_t now = uiManager.isTimeInitialized() ? time(NULL) : millis() / 1000;
    newData.timestamp = now;
    newData.decibels = NAN; // 初始化为无效
    newData.humidity = NAN;
    newData.temperature = NAN;
    newData.lux = NAN;

    bool micReadSuccess = false;
    bool tempHumReadSuccess = false;
    bool lightReadSuccess = false;

    // --- 1. 读取声音数据 ---
    try {
        float db_reading = micManager.readNoiseLevel(500); // 500ms timeout
        if (!isnan(db_reading)) {
            newData.decibels = db_reading; // 存储经过处理和验证的值
            micReadSuccess = true;
            // 高噪声检测和处理 (如果需要立即响应，可以放在这里)
            if (micManager.isHighNoise()) {
                Serial.printf("HIGH NOISE detected: %.1f dB\n", newData.decibels);
                // Maybe trigger immediate LED update or other actions
                // updateLEDs(); // Consider if calling this here is too frequent
                // uiManager.forceRedraw();
            }
        } else {
            // Serial.println("DBG: Microphone read returned NaN.");
            // newData.decibels 保持为 NAN
        }
    } catch (const std::exception& e) {
        Serial.printf("ERR: Exception during microphone reading: %s\n", e.what());
        newData.decibels = NAN; // 确保异常时也为无效
    } catch (...) {
        Serial.println("ERR: Unknown exception during microphone reading.");
        newData.decibels = NAN;
    }

    // --- 2. 读取温湿度数据 ---
    static bool si7021_initialized = false; // 保持静态以避免每次检查
    if (!si7021_initialized) {
        si7021_initialized = si7021.begin();
        if (!si7021_initialized) {
            Serial.println("WARN: Si7021 sensor failed to initialize (or is not connected). Skipping Temp/Hum.");
        } else {
            Serial.println("Si7021 sensor initialized successfully.");
        }
    }

    if (si7021_initialized) {
        try {
            float h = si7021.readHumidity();
            float t = si7021.readTemperature();

            // 检查 Si7021 读取是否成功 (返回 NaN on failure)
            if (!isnan(h) && !isnan(t)) {
                // 使用 DataValidator 验证
                newData.temperature = DataValidator::validateTemperature(t);
                newData.humidity = DataValidator::validateHumidity(h);
                // 再次检查验证后的值是否有效 (DataValidator 可能返回特定无效值如 -1)
                if (newData.temperature != -1.0f && newData.humidity != -1.0f) {
                     tempHumReadSuccess = true;
                } else {
                     // Serial.println("DBG: Temp/Hum validation failed.");
                     newData.temperature = NAN; // 标记为无效
                     newData.humidity = NAN;
                }
            } else {
                // Serial.println("DBG: Si7021 read returned NaN.");
                newData.temperature = NAN;
                newData.humidity = NAN;
            }
        } catch (const std::exception& e) {
            Serial.printf("ERR: Exception during Si7021 reading: %s\n", e.what());
            newData.temperature = NAN;
            newData.humidity = NAN;
        } catch (...) {
            Serial.println("ERR: Unknown exception during Si7021 reading.");
            newData.temperature = NAN;
            newData.humidity = NAN;
        }
    } // end if si7021_initialized

    // --- 3. 读取光照强度 ---
    // BH1750 的 begin 应该在 setup 中完成，这里假设它已初始化
    // lightMeter.begin() 返回 bool，readLightLevel 返回 float (-1 or -2 on error)
    try {
        float lux_reading = lightMeter.readLightLevel();
        // 检查是否读取成功 (BH1750 返回负值表示错误)
        if (lux_reading >= 0) {
            newData.lux = DataValidator::validateLux(lux_reading);
            // 检查验证结果
            if (newData.lux != -1.0f) { // validateLux 返回 -1 表示无效
                lightReadSuccess = true;
            } else {
                 // Serial.println("DBG: Lux validation failed.");
                 newData.lux = NAN; // 标记为无效
            }
        } else {
            // Serial.printf("DBG: BH1750 read failed with code: %.0f\n", lux_reading);
            newData.lux = NAN; // 标记为无效
        }
    } catch (const std::exception& e) {
        Serial.printf("ERR: Exception during BH1750 reading: %s\n", e.what());
        newData.lux = NAN;
    } catch (...) {
        Serial.println("ERR: Unknown exception during BH1750 reading.");
        newData.lux = NAN;
    }


    // --- 4. 存储数据 ---
    // 决定是否存储这次的数据。可以根据需求调整逻辑，
    // 例如，只要麦克风成功就存储，或者所有数据都成功才存储。
    // 当前逻辑：只要时间戳有效就存储，无效传感器读数用 NAN 表示。

    // 安全检查 - 防止dataIndex越界 (如果达到末尾，则循环覆盖)
    if (dataIndex >= (sizeof(envData) / sizeof(envData[0]))) {
        // Serial.println("DBG: Data buffer full, wrapping around.");
        dataIndex = 0; // 回到开头实现循环缓冲区
    }

    // 存储准备好的 newData 结构
    envData[dataIndex] = newData;

    // --- 5. 打印调试信息 (只打印本次读取的数据) ---
    Serial.printf("\n==== Record [%d] @ %lld ====\n", dataIndex, (long long)now);
    if (!isnan(newData.decibels)) Serial.printf("Noise: %.1f dB %s\n", newData.decibels, micReadSuccess ? "" : "(ERR)"); else Serial.println("Noise: ---");
    if (!isnan(newData.humidity)) Serial.printf("Humid: %.1f %% %s\n", newData.humidity, tempHumReadSuccess ? "" : "(ERR)"); else Serial.println("Humid: ---");
    if (!isnan(newData.temperature)) Serial.printf("Temp:  %.1f C %s\n", newData.temperature, tempHumReadSuccess ? "" : "(ERR)"); else Serial.println("Temp:  ---");
    if (!isnan(newData.lux)) Serial.printf("Light: %.1f lx %s\n", newData.lux, lightReadSuccess ? "" : "(ERR)"); else Serial.println("Light: ---");
    Serial.println("==========================");


    // --- 6. 增加数据索引 ---
    dataIndex++;


    // --- 7. 重置标志 ---
    isRecording = false;
}

/**
 * 显示最近记录的环境数据
 * 在串口监视器中显示最近60个数据点
 * 包括时间和对应的环境参数
 */
void displayRecentData() {
  Serial.println("最近噪声数据:");
  int count = min(60, dataIndex); // 最多显示60个数据点
  int startIdx = (dataIndex > count) ? (dataIndex - count) : 0;
  
  for (int i = startIdx; i < dataIndex; i++) {
    struct tm *timeinfo;
    timeinfo = localtime(&envData[i].timestamp);
    
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeinfo);
    
    Serial.print(timeStr);
    Serial.print(" - ");
    Serial.print(envData[i].decibels);
    Serial.println(" dB");
  }
}

/**
 * 检查按钮状态并处理按钮事件
 * 包括模式切换、数据保存、显示数据等功能
 * 使用上升沿触发以避免重复触发, 并加入长按检测
 */
void checkButtons() {
  unsigned long currentTime = millis();

  // 读取所有按钮状态 - 使用位运算进行状态比较
  int btnState1 = digitalRead(BTN1_PIN);
  int btnState2 = digitalRead(BTN2_PIN);
  int btnState3 = digitalRead(BTN3_PIN);
  int btnState4 = digitalRead(BTN4_PIN);
  int btnState5 = digitalRead(BTN5_PIN);
  
  // 检测上升沿和下降沿 (引脚为LOW时按钮被按下，为HIGH时释放)
  
  // 按钮1 (上): 向上导航
  if (btnState1 == LOW && lastBtn1State == HIGH) {
    uiManager.handleInput(BTN1_PIN);
  }
  
  // 按钮2 (右): 切换LED模式
  if (btnState2 == LOW && lastBtn2State == HIGH) {
    uint8_t nextMode = (uiManager.getCurrentLedMode() + 1) % 4;
    uiManager.setLedMode(nextMode);
    updateLEDs();
    Serial.printf("Button 2: LED Mode (%d)\n", nextMode);
  }
  
  // 按钮3 (下): 向下导航
  if (btnState3 == LOW && lastBtn3State == HIGH) {
    uiManager.handleInput(BTN3_PIN);
  }
  
  // BTN4 (左): 短按保存数据, 长按重连 WiFi
  if (btnState4 == LOW && lastBtn4State == HIGH) {
    // 按钮按下
    btn4PressTime = currentTime;
    btn4LongPressTriggered = false;
    Serial.println("Button 4 Down");
  } else if (btnState4 == HIGH && lastBtn4State == LOW) {
    // 按钮释放
    if (!btn4LongPressTriggered) {
      if (uiManager.isSdCardInitialized()) {
        saveEnvironmentDataToSD();
        Serial.println("Button 4 Short Press: Saving data to SD");
        uiManager.forceRedraw();
      } else {
        Serial.println("Button 4 Short Press: SD Card not ready, cannot save.");
        uiManager.forceRedraw();
      }
    }
    btn4PressTime = 0;
    Serial.println("Button 4 Up");
  } else if (btnState4 == LOW && lastBtn4State == LOW) {
    // 按钮保持按下
    if (!btn4LongPressTriggered && btn4PressTime != 0 && (currentTime - btn4PressTime >= LONG_PRESS_MS)) {
      Serial.println("Button 4 Long Press: Reconnecting WiFi...");
      connectToWiFi();
      bool wifiStatus = (WiFi.status() == WL_CONNECTED);
      uiManager.setWifiStatus(wifiStatus);
      if (wifiStatus) {
        initTime();
      }
      btn4LongPressTriggered = true;
    }
  }

  // BTN5 (中): 短按手动刷新数据, 长按重启
  if (btnState5 == LOW && lastBtn5State == HIGH) {
    // 按钮按下
    btn5PressTime = currentTime;
    btn5LongPressTriggered = false;
    Serial.println("Button 5 Down");
  } else if (btnState5 == HIGH && lastBtn5State == LOW) {
    // 按钮释放
    if (!btn5LongPressTriggered) {
      Serial.println("Button 5 Short Press: 手动刷新数据");
      
      // 数据刷新操作
      recordEnvironmentData();
      
      // 给系统一些时间稳定
      delay(50);
      
      // 更新LED显示
      updateLEDs();
      
      // 通知UI更新 - 仅强制重绘，不调用handleInput避免屏幕切换
      uiManager.forceRedraw();
    }
    btn5PressTime = 0;
    Serial.println("Button 5 Up");
  } else if (btnState5 == LOW && lastBtn5State == LOW) {
    // 按钮保持按下
    if (!btn5LongPressTriggered && btn5PressTime != 0 && (currentTime - btn5PressTime >= LONG_PRESS_MS)) {
      Serial.println("Button 5 Long Press: Restarting...");
      Serial.flush(); // 确保所有串口数据都被发送出去
      delay(100);     // 短暂延迟，让消息发送完成
      ESP.restart();
    }
  }

  // 更新按钮状态
  lastBtn1State = btnState1;
  lastBtn2State = btnState2;
  lastBtn3State = btnState3;
  lastBtn4State = btnState4;
  lastBtn5State = btnState5;
}

/**
 * 更新LED显示
 * 根据当前显示模式更新LED状态 (Reads mode from UIManager)
 * 支持关闭、噪声、温度、湿度四种模式
 */
void updateLEDs() {
  // 首先检查Neopixel库是否已正确初始化
  try {
    uint32_t color = 0; // 默认颜色（黑色/关闭）
    
    // 如果尚未采集数据，直接返回
    if (dataIndex <= 0) {
      // 在没有数据时设置一个默认颜色（暗蓝色）
      for (int i = 0; i < NUM_LEDS; i++) {
        pixels.setPixelColor(i, pixels.Color(0, 0, 32)); // 暗蓝色
      }
      pixels.show();
      return;
    }
    
    // 索引越界保护
    int latestIdx = (dataIndex > 0) ? (dataIndex - 1) : 0;
    if (latestIdx >= (sizeof(envData) / sizeof(envData[0]))) {
      latestIdx = 0; // 重置为首个索引
    }
    
    // 记录当前模式，防止中途变化
    uint8_t currentMode = uiManager.getCurrentLedMode();
    
    // 根据模式设置颜色
    switch (currentMode) {
      case LED_MODE_OFF:
        pixels.clear();
        pixels.show();
        return; // 直接返回，不需要设置颜色
      
      case LED_MODE_NOISE:
        {
          // 读取本地变量，防止多次访问数组
          float db = envData[latestIdx].decibels;
          
          // 范围检查
          if (isnan(db) || db < 0) {
            db = 0;
          }
          
          if (db > NOISE_THRESHOLD_HIGH) {
            color = pixels.Color(255, 0, 0);  // 红色 - 高噪声
          } else if (db > NOISE_THRESHOLD_MEDIUM) {
            color = pixels.Color(255, 255, 0); // 黄色 - 中等噪声
          } else if (db > NOISE_THRESHOLD_LOW) {
            color = pixels.Color(0, 255, 0);   // 绿色 - 低噪声
          } else {
            color = pixels.Color(0, 0, 255);   // 蓝色 - 安静
          }
        }
        break;
        
      case LED_MODE_TEMP:
        {
          // 读取本地变量，防止多次访问数组
          float temp = envData[latestIdx].temperature;
          
          // 范围检查
          if (isnan(temp) || temp < -40 || temp > 125) {
            color = pixels.Color(128, 0, 128); // 紫色表示无效温度
            break;
          }
          
          if (temp < 16) {
            color = pixels.Color(0, 0, 255);  // 蓝色 - 低温
          } else if (temp > 28) {
            color = pixels.Color(255, 0, 0);  // 红色 - 高温
          } else {
            color = pixels.Color(0, 255, 0);  // 绿色 - 适宜温度
          }
        }
        break;
        
      case LED_MODE_HUMIDITY:
        {
          // 读取本地变量，防止多次访问数组
          float humidity = envData[latestIdx].humidity;
          
          // 范围检查
          if (isnan(humidity) || humidity < 0 || humidity > 100) {
            color = pixels.Color(128, 0, 128); // 紫色表示无效湿度
            break;
          }
          
          if (humidity < 30) {
            color = pixels.Color(255, 0, 0);  // 红色 - 干燥
          } else if (humidity > 70) {
            color = pixels.Color(0, 0, 255);  // 蓝色 - 潮湿
          } else {
            color = pixels.Color(0, 255, 0);  // 绿色 - 适中
          }
        }
        break;
        
      default:
        // 未知模式，使用白色
        color = pixels.Color(32, 32, 32); // 暗白色
    }
    
    // 仅当颜色已设置时，更新所有LED
    if (currentMode != LED_MODE_OFF) {
      for (int i = 0; i < NUM_LEDS; i++) {
        pixels.setPixelColor(i, color);
      }
      pixels.show();
    }
  } catch (...) {
    Serial.println("更新LED显示时发生错误");
  }
}

/**
 * 记录内存使用情况
 * 定期输出内存使用数据到串口，帮助监控系统内存状态
 */
void logMemoryStatus() {
  Serial.println("\n==== 内存状态 ====");
  Serial.printf("空闲堆内存: %lu 字节\n", ESP.getFreeHeap());
  Serial.printf("最大空闲块: %lu 字节\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.printf("最小空闲堆: %lu 字节\n", ESP.getMinFreeHeap());
  
  // 计算内存分片率
  float fragmentation = 0;
  if (ESP.getFreeHeap() > 0) {
    fragmentation = 100.0 - (float)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) * 100.0 / ESP.getFreeHeap();
    Serial.printf("内存分片率: %.1f%%\n", fragmentation);
  }
  
  if (fragmentation > 50) {
    Serial.println("警告: 内存分片严重!");
  }
  
  Serial.printf("程序空间: %lu 字节\n", ESP.getFreeSketchSpace());
  Serial.println("==================");
}

/**
 * 看门狗定时器重置回调函数
 * 当看门狗超时时触发系统重启
 */
void IRAM_ATTR resetModule(void) {
  // 使用IRAM_ATTR确保这个函数在中断时可以正常执行
  ets_printf("看门狗触发重启!\n");
  esp_restart();
}

/**
 * 初始化看门狗定时器
 * 设置看门狗超时时间和回调函数
 */
void setupWatchdog() {
    // 使用ESP32 Arduino库3.2.0的API初始化定时器
    // 设置80MHz时钟频率
    watchdog = timerBegin(80000000);
    if (watchdog == nullptr) {
        Serial.println("看门狗定时器初始化失败");
        return;
    }
    
    // 附加中断处理函数
    timerAttachInterrupt(watchdog, &resetModule);
    
    // 设置报警值和启动定时器
    // 8秒超时 = 8,000,000 微秒
    timerAlarm(watchdog, 8000000, false, 0);
    
    // 启动定时器
    timerStart(watchdog);
    
    Serial.println("看门狗定时器已启用 (8秒超时)");
}

/**
 * 检查内存分片情况
 * 当分片严重时可考虑自动重启系统
 */
void checkMemoryFragmentation() {
  size_t free_heap = ESP.getFreeHeap();
  size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  
  // 计算分片率
  float fragmentation = 100.0 - (float)largest_block * 100.0 / free_heap;
  
  Serial.printf("内存分片检查: 总空闲=%d, 最大块=%d, 分片率=%.1f%%\n", 
                free_heap, largest_block, fragmentation);
  
  // 如果最大块小于总空闲内存的50%，说明存在分片
  if (fragmentation > 50) {
    Serial.println("警告: 检测到内存分片");
    
    // 在分片严重时自动重启
    if (fragmentation > 70) {
      Serial.println("严重内存分片，准备重启系统...");
      delay(1000); // 等待消息发送完成
      ESP.restart();
    }
  }
}

// UI functions are now implemented in ui.cpp and declared in ui.h

void setup() {
    // 添加异常处理
    try {
        Serial.begin(115200);
        while (!Serial) { yield(); } // 等待串口连接

        Serial.println("ESP32S3 环境监测系统启动中...");

        // 启动内存监控
        initEmergencyMemory();

        // 初始化LED
        pixels.begin(); // 初始化NeoPixel库
        pixels.setBrightness(50); // 设置全局亮度 (0-255), 50 是一个比较暗的值
        pixels.clear(); // 关闭所有LED
        pixels.show();  // 显示更改
        
        // 检查启动时的内存状态
        Serial.println("启动时内存状态检查:");
        logMemoryStatus();
        
        // 初始化紧急内存
        if (!initEmergencyMemory()) {
            ESP.restart();  // 如果无法分配内存，直接重启
            return;
        }
        
        // 初始化看门狗定时器
        setupWatchdog();
        
        // 初始化按钮引脚
        pinMode(BTN1_PIN, INPUT_PULLUP);
        pinMode(BTN2_PIN, INPUT_PULLUP);
        pinMode(BTN3_PIN, INPUT_PULLUP);
        pinMode(BTN4_PIN, INPUT_PULLUP);
        pinMode(BTN5_PIN, INPUT_PULLUP);
        
        // 初始化I2C (Using User Provided Pins)
        Wire.begin(I2C_SDA, I2C_SCL);
        Serial.printf("I2C已初始化: SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);
        
        // 扫描I2C总线，检查可用设备
        Serial.println("扫描I2C总线...");
        byte error, address;
        int deviceCount = 0;
        
        for (address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            error = Wire.endTransmission();
            
            if (error == 0) {
                Serial.printf("发现I2C设备，地址: 0x%02X", address);
                
                // 识别常见设备
                if (address == 0x23) {
                    Serial.print(" (BH1750)");
                } else if (address == 0x40) {
                    Serial.print(" (Si7021)");
                }
                
                Serial.println();
                deviceCount++;
            }
        }
        
        if (deviceCount == 0) {
            Serial.println("警告: 未发现任何I2C设备，请检查连接");
        } else {
            Serial.printf("发现 %d 个I2C设备\n", deviceCount);
        }

        // 初始化无源蜂鸣器引脚
        pinMode(BUZZER_PIN, OUTPUT);
        digitalWrite(BUZZER_PIN, LOW); // 确保蜂鸣器初始状态为关闭
        
        // 初始化Si7021（假设地址为0x40）
        Serial.println("尝试初始化Si7021传感器...");
        if (!si7021.begin()) {
            Serial.println("Si7021传感器初始化失败");
        } else {
            Serial.println("Si7021传感器初始化成功");
        }
        
        // 初始化BH1750 - 使用指定地址0x23
        Serial.println("尝试初始化BH1750传感器...");
        if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
            Serial.println("BH1750传感器初始化成功!");
            float lux = lightMeter.readLightLevel();
            Serial.printf("BH1750初始读数: %.2f lx\n", lux);
        } else {
            Serial.println("BH1750传感器初始化失败，请检查硬件连接");
        }

        // SD Card initialization will now use SDMMC mode based on provided pins.
        // SPI configuration for SD card is removed.

        // 连接WiFi和同步时间
        connectToWiFi();
        bool wifiStatus = (WiFi.status() == WL_CONNECTED);
        uiManager.setWifiStatus(wifiStatus);
        if (wifiStatus) {
            initTime();
        }

        // 初始化I2S (micManager is already instantiated)
        Serial.println("尝试初始化I2S麦克风...");
        if (micManager.begin()) {
            Serial.println("I2S麦克风初始化成功 (使用I2SMicManager类)");
            
            // 进行测试读取尝试
            float initialDb = micManager.readNoiseLevel(1000);
            Serial.printf("初始噪声读数: %.2f dB\n", initialDb);
            
        } else {
            Serial.println("I2SMicManager初始化失败！");
            // Maybe handle this more gracefully than just proceeding
        }

        // 初始化SD卡
        bool sdStatus = initSDCard();
        uiManager.setSdCardStatus(sdStatus); // Update UIManager state
        if (sdStatus) {
            createHeaderIfNeeded();
        } else {
            Serial.println("警告：无法初始化SD卡，数据将不会被永久保存");
        }

        // 获取启动时间
        startTime = millis();
        lastSensorReadTime = startTime; // Initialize data recording timer
        lastMemoryLog = startTime; // 初始化内存记录时间
        // lastDisplayUpdateTime is managed by UIManager

        // 初始化显示屏
        tft.init();
        tft.setRotation(3); // 根据需要调整屏幕方向（0-3）
        tft.fillScreen(TFT_BLACK);
        
        // 设置背光
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);

        // Run startup animation (Re-added)
        runStartupAnimation(tft, 1500); // Pass tft object, run for 1.5 seconds

        // --- Setup UI Manager ---
        uiManager.addScreen(&mainScreen);
        uiManager.addScreen(&noiseScreen);
        uiManager.addScreen(&tempHumScreen);
        uiManager.addScreen(&lightScreen);
        uiManager.addScreen(&statusScreen);
        uiManager.setInitialScreen(); // Sets the first screen and triggers initial draw via update()
        // --- End Setup UI Manager ---

        // 在WiFi连接后初始化通信管理器
        if (WiFi.status() == WL_CONNECTED) {
            // 启动 TCP 命令服务器 (如果仍然需要)
            if (commManager.begin()) {
                Serial.println("命令服务器初始化成功 (TCP Port 8266)");
            } else {
                Serial.println("命令服务器初始化失败");
            }

            // --- 设置 HTTP 和 WebSocket 服务器 ---
            commManager.setupHttpServer(&httpServer);
            commManager.setupWebSocketServer(&httpServer); // WebSocket 附加到 HTTP 服务器

            // --- 启动 HTTP 服务器 --- (
            httpServer.begin();
            Serial.println("HTTP 和 WebSocket 服务器已启动 (Port 80)");

        } else {
             Serial.println("WARN: WiFi 未连接，Web 服务器未启动。");
        }

        Serial.println("系统初始化完成!");
        // 再次检查初始化后内存状态
        logMemoryStatus();
    } catch (const std::exception& e) {
        Serial.printf("设置过程中发生异常: %s\n", e.what());
        ESP.restart();
    } catch (...) {
        Serial.println("设置过程中发生未知异常");
        ESP.restart();
    }
}

// 优雅退出清理函数
void cleanup() {
  Serial.println("开始清理资源...");
  
  // 关闭I2S麦克风
  micManager.end();
  
  // 保存剩余数据到SD卡
  if (dataIndex > 0 && uiManager.isSdCardInitialized()) {
    saveEnvironmentDataToSD();
  }
  
  // 关闭SD卡
  SD_MMC.end();
  
  // 清除显示
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("系统安全关闭", tft.width() / 2, tft.height() / 2);
  
  // 关闭LED
  pixels.clear();
  pixels.show();

  Serial.println("资源清理完成");
}

// 添加一个退出处理函数
void handleSystemExit() {
  cleanup();
  delay(1000);
  ESP.restart();
}

void loop() {
    try {
        // 重置看门狗定时器
        timerWrite(watchdog, 0);
        
        unsigned long currentMillis = millis();
        
        // 检查堆内存是否严重不足
        if (ESP.getFreeHeap() < 4096) { // 如果可用内存小于4KB
            Serial.println("警告：内存严重不足，尝试释放紧急内存");
            releaseEmergencyMemory();
            if (ESP.getFreeHeap() < 4096) {
                Serial.println("错误：内存仍然不足，准备重启");
                cleanup();
                ESP.restart();
                return;
            }
        }
        
        // --- 1. 处理 TCP 命令通信 (如果保留) ---
        commManager.update(); // Handles commands

        // --- 1.5 通过 WebSocket 发送音频流 ---
        commManager.streamAudioViaWebSocket(); // Handles audio streaming via WS

        // --- 2. 检查按钮 ---
        checkButtons();

        // --- 3. 更新UI ---
        uiManager.update();

        // --- 4. 传感器数据采集 & 更新显示 ---
        if (currentMillis - lastSensorReadTime >= SENSOR_READ_INTERVAL) {
            lastSensorReadTime = currentMillis;
            recordEnvironmentData(); // Reads sensors (including mic for dB)
            updateLEDs();
            // Update internal data cache for command server GET_CURRENT
            if (dataIndex > 0) {
                 EnvironmentData latest = envData[(dataIndex - 1 + (sizeof(envData) / sizeof(envData[0]))) % (sizeof(envData) / sizeof(envData[0]))];
                 commManager.broadcastEnvironmentData(latest); // Updates internal state for TCP commands
            }
             uiManager.setNeedsDataUpdate(true);
        }

        // --- 5. SD卡保存逻辑 ---
        const unsigned long SAVE_INTERVAL = 60000; // 每分钟保存一次
        // 检查是否需要保存数据到SD卡 (例如缓冲区满或达到保存间隔)
        if (uiManager.isSdCardInitialized()) {
            // 使用 lastSaveTime 而不是 lastDataRecordTime
            if (dataIndex >= (sizeof(envData) / sizeof(envData[0])) || 
                (dataIndex > 0 && currentMillis - lastSaveTime >= SAVE_INTERVAL)) {
                saveEnvironmentDataToSD(); // saveEnvironmentDataToSD 会重置 dataIndex
                lastSaveTime = currentMillis;
            } else if (dataIndex == 0) {
                // 如果 dataIndex 被 saveEnvironmentDataToSD 重置为0，也重置保存计时器
                 lastSaveTime = currentMillis;
            }
        } else {
            // 如果没有SD卡，当缓冲区满时循环覆盖 (dataIndex 在 recordEnvironmentData 中处理)
            if (dataIndex >= (sizeof(envData) / sizeof(envData[0]))) {
                 // 缓冲区已满的处理逻辑在 recordEnvironmentData 中
                 // 这里不需要额外操作，但要确保 recordEnvironmentData 能正确处理循环覆盖
                 // 可能需要重置 save 定时器避免不必要的检查
                 lastSaveTime = currentMillis; 
            }
        }

        // --- 6. 定期记录内存使用情况 ---
        if (currentMillis - lastMemoryLog >= MEMORY_LOG_INTERVAL) {
            lastMemoryLog = currentMillis;
            logMemoryStatus();
        }
        
        // --- 7. 清理 WebSocket 客户端 (库会自动处理部分，但显式调用有时有帮助) ---
         // audioWs->cleanupClients(); // commManager.streamAudioViaWebSocket 内部已调用

        // --- 8. 短暂延迟或 yield ---
         yield(); // 使用 yield() 替代 delay()，允许异步任务运行
        // delay(1); // 或者使用非常小的延迟

    } catch (const std::exception& e) {
        Serial.printf("主循环中发生异常: %s\n", e.what());
        delay(1000);
        ESP.restart();
    } catch (...) {
        Serial.println("主循环中发生未知异常");
        delay(1000);
        ESP.restart();
    }
}
