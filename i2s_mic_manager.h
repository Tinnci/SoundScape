#ifndef I2S_MIC_MANAGER_H
#define I2S_MIC_MANAGER_H

#include <Arduino.h>
#include "driver/i2s_std.h"
#include "driver/i2s_common.h"
#include "data_validator.h"

class I2SMicManager {
private:
    // Match initialization order in constructor
    uint32_t sample_rate_;
    uint8_t ws_pin_;
    uint8_t sd_pin_;
    uint8_t sck_pin_;
    i2s_port_t port_num_;
    i2s_chan_handle_t rx_handle_;
    bool initialized_;
    
    // 采样缓冲区（静态分配以减少内存碎片）
    static constexpr size_t BUFFER_SIZE = 256;  // 保持较小的缓冲区以维持快速响应
    int32_t samples_[BUFFER_SIZE];
    
    // 噪声测量校准参数
    static constexpr double REF_LEVEL = 1.0;  // 参考电平设为1.0以简化计算
    static constexpr double CALIBRATION_FACTOR = 0.70;  // 继续降低增益
    static constexpr double OFFSET_DB = 31.0;  // 继续提高偏移值以降低整体读数
    static constexpr double NOISE_FLOOR = -75.0;  // 保持噪声基准不变
    
public:
    I2SMicManager(uint32_t sample_rate = 16000, 
                uint8_t ws_pin = 16, 
                uint8_t sd_pin = 17, 
                uint8_t sck_pin = 15,
                i2s_port_t port_num = I2S_NUM_0);
    
    bool begin();
    float readNoiseLevel(int timeout_ms = 50);
    void end();
    bool isInitialized() const { return initialized_; }

    // Reads raw I2S samples into the provided buffer.
    // Returns the number of samples read (not bytes).
    // Returns 0 on failure or timeout.
    size_t readRawSamples(int32_t* buffer, size_t maxSamples, int timeout_ms);
    
private:
    // 私有辅助函数
    double calculateRMS(const int32_t* samples, size_t count);
};

#endif // I2S_MIC_MANAGER_H 