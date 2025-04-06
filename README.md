# 声景坊 (SoundScape) - ESP32-S3 环境监测设备

> **注意**: 这是一个正在开发中的课程项目，功能和稳定性尚未完全测试，可能存在不可靠的情况。仅用于教学和学习目的。

## 项目概述

本项目旨在使用ESP32-S3开发一个多功能环境监测设备，主要功能包括：

- 环境噪声监测 (使用INMP441麦克风)
- 温湿度监测 (Si7021传感器)
- 光照强度监测 (BH1750传感器)
- 数据可视化显示 (ST7789V屏幕)
- 无线数据传输 (WiFi和蓝牙)

## 硬件组成

### 核心开发板
- ESP32-S3 WROOM模块
- GPIO引脚扩展
- USB Type-C接口
  - 直接USB输出
  - CH340转UART串口

### 扩展板
- INMP441麦克风 (噪声检测)
- ST7789V显示屏
- 4组I2C接口
- WS2812B LED灯 (4个级联)
- 五向按键 (UI控制)
- 蜂鸣器
- ADC接口
- SD卡读卡器

## 引脚分配

### 显示屏 (ST7789V)
- LCD_SCL: IO12
- LCD_SDA: IO11
- LCD_RST: IO9
- LCD_DC: IO46
- LCD_CS: IO3
- LCD_BL: IO8

### I2C接口
- I2C SDA: IO38
- I2C SCL: IO39
- 连接传感器:
  - Si7021 (温湿度传感器，地址0x40)
  - BH1750 (光照传感器，地址0x23)

### 按键
- BTN1: IO2
- BTN2: IO1
- BTN3: IO41
- BTN4: IO40
- BTN5: IO42

### SD卡读卡器
- SD_CMD: IO45
- SD_CLK: IO47
- SD_DATA: IO21
- SD_CD: IO48
- SD_WP: IO14

### WS2812B LED
- LED_DATA: IO18

### INMP441麦克风
- SD: IO15
- WS: IO16
- SCK: IO17
- L/R: 接地

### 蜂鸣器
- 引脚: IO4

## 软件开发

### 开发环境选择
- Arduino IDE

### 固件功能
- 传感器数据采集与处理
- 屏幕显示驱动
- 无线通信 (WiFi和蓝牙)
- SD卡数据存储
- 用户交互界面

### PC端应用程序
- 有线/无线设备连接
- 历史数据存储
- 数据可视化界面
  - 噪声曲线图表
  - 环境指标实时显示
  - 历史数据查询与分析

## 开发计划

1. 硬件连接与测试
2. 传感器驱动开发
3. 显示屏UI设计
4. 无线通信实现
5. SD卡数据存储
6. PC端应用开发
7. 系统集成与测试

## 依赖库

- TFT_eSPI (显示屏)
- Adafruit_NeoPixel (WS2812B)
- Arduino I2C
- ESP32 BLE
- WiFi库
- SD库
- 传感器相关库

## 贡献者

[待添加]

## 许可证

[MIT] 