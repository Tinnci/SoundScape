#ifndef UI_CONSTANTS_H
#define UI_CONSTANTS_H

// 共享UI常量
// 这些常量原本在多个屏幕文件中重复定义

// 布局常量
const int H_PADDING = 10;       // 水平边距
const int V_PADDING = 5;        // 垂直边距
const int TITLE_Y = V_PADDING + 5;  // 标题Y坐标
const int LINE_HEIGHT = 25;     // 行高
const int STATUS_Y = 280;       // 状态行Y坐标

// 屏幕颜色
// (使用TFT_eSPI提供的颜色定义)

// LED显示模式
#define LED_MODE_OFF      0
#define LED_MODE_NOISE    1
#define LED_MODE_TEMP     2
#define LED_MODE_HUMIDITY 3

// 传感器阈值
#define NOISE_THRESHOLD_LOW 50     // dB
#define NOISE_THRESHOLD_MEDIUM 70  // dB
#define NOISE_THRESHOLD_HIGH 85    // dB

// 传感器范围限制
const float TEMP_MIN = -40.0f;  // 温度最小值 (°C)
const float TEMP_MAX = 125.0f;  // 温度最大值 (°C)
const float HUM_MIN = 0.0f;     // 湿度最小值 (%)
const float HUM_MAX = 100.0f;   // 湿度最大值 (%)
const float LUX_MIN = 0.0f;     // 光照最小值 (lx)
const float LUX_MAX = 65535.0f; // 光照最大值 (lx)
const float DB_MIN = 0.0f;      // 分贝最小值 (dB)
const float DB_MAX = 120.0f;    // 分贝最大值 (dB)

#endif // UI_CONSTANTS_H 