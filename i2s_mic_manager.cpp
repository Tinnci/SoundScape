#include "i2s_mic_manager.h"
#include <math.h>

I2SMicManager::I2SMicManager(uint32_t sample_rate, uint8_t ws_pin, uint8_t sd_pin, uint8_t sck_pin, i2s_port_t port_num) :
    sample_rate_(sample_rate),
    ws_pin_(ws_pin),
    sd_pin_(sd_pin),
    sck_pin_(sck_pin),
    port_num_(port_num),
    initialized_(false),
    lastValue_(30.0f),
    isHighNoise_(false) {
}

bool I2SMicManager::begin() {
    if (initialized_) {
        return true;
    }
    
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(port_num_, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &rx_handle_);
    if (err != ESP_OK) {
        Serial.printf("I2S通道创建失败: %s\n", esp_err_to_name(err));
        return false;
    }
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_24BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = 1,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)sck_pin_,
            .ws = (gpio_num_t)ws_pin_,
            .dout = I2S_GPIO_UNUSED,
            .din = (gpio_num_t)sd_pin_,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    err = i2s_channel_init_std_mode(rx_handle_, &std_cfg);
    if (err != ESP_OK) {
        Serial.printf("I2S标准模式初始化失败: %s\n", esp_err_to_name(err));
        return false;
    }
    
    err = i2s_channel_enable(rx_handle_);
    if (err != ESP_OK) {
        Serial.printf("I2S通道启用失败: %s\n", esp_err_to_name(err));
        return false;
    }
    
    initialized_ = true;
    Serial.println("I2S麦克风初始化成功");
    return true;
}

float I2SMicManager::readNoiseLevel(int timeout_ms) {
    if (!initialized_) {
        if (!begin()) {
            return 0.0f;
        }
    }
    
    size_t bytes_read = 0;
    esp_err_t result = i2s_channel_read(rx_handle_, samples_, sizeof(samples_), &bytes_read, timeout_ms);
    
    if (result != ESP_OK || bytes_read == 0) {
        Serial.printf("I2S读取失败: %s\n", esp_err_to_name(result));
        return lastValue_;  // 读取失败时返回上一次的值
    }
    
    size_t validSamples = bytes_read / sizeof(int32_t);
    validSamples = min(validSamples, BUFFER_SIZE);
    
    // 计算RMS值
    double rms = calculateRMS(samples_, validSamples);
    
    // 计算分贝值
    float db = 0.0f;
    if (rms > 0) {
        // 计算原始分贝值
        db = 20.0f * log10f(rms);
        
        // 应用噪声基准和偏移
        if (db < NOISE_FLOOR) {
            db = NOISE_FLOOR;
        }
        db = (db - NOISE_FLOOR) * CALIBRATION_FACTOR + OFFSET_DB;
    }
    
    // 处理噪声级别并返回结果
    return processNoiseLevel(db);
}

float I2SMicManager::processNoiseLevel(float rawDb) {
    float db = rawDb;
    
    // 检查是否超过高噪声阈值
    if (db >= HIGH_NOISE_THRESHOLD) {
        // 检测到高噪声时立即返回实际值，不进行平滑处理
        isHighNoise_ = true;
        lastValue_ = db;
        Serial.printf("检测到高噪声: %.1f dB\n", db);
        return db;
    }
    
    // 非高噪声情况下进行正常处理
    isHighNoise_ = false;
    
    // 确保结果在有效范围内
    if (isinf(db) || db < 30.0f) {
        db = 30.0f;
    } else if (db > 120.0f) {
        db = 120.0f;
    }
    
    // 应用指数平滑
    db = applyExponentialSmoothing(db);
    
    // 使用DataValidator验证结果
    db = DataValidator::validateDecibels(db);
    
    // 保存当前值作为下一次的lastValue
    lastValue_ = db;
    return db;
}

double I2SMicManager::calculateRMS(const int32_t* samples, size_t count) {
    if (count == 0) return 0.0;
    
    double sum = 0.0;
    int64_t dc_sum = 0;
    
    // 首先计算DC偏移
    for (size_t i = 0; i < count; i++) {
        int32_t sample = samples[i];
        if (sample & 0x800000) {
            sample |= 0xFF000000;
        }
        dc_sum += sample;
    }
    double dc_offset = static_cast<double>(dc_sum) / count;
    
    // 计算去除DC偏移后的RMS
    for (size_t i = 0; i < count; i++) {
        int32_t sample = samples[i];
        if (sample & 0x800000) {
            sample |= 0xFF000000;
        }
        
        // 去除DC偏移并标准化
        double value = (static_cast<double>(sample) - dc_offset) / 8388608.0;
        sum += value * value;
    }
    
    return sqrt(sum / count);
}

float I2SMicManager::applyExponentialSmoothing(float newValue) {
    // 根据数值变化方向选择不同的平滑系数
    float alpha = (newValue > lastValue_) ? ALPHA_RISE : ALPHA_FALL;
    
    // 应用指数平滑
    return alpha * newValue + (1.0f - alpha) * lastValue_;
}

void I2SMicManager::end() {
    if (initialized_) {
        i2s_channel_disable(rx_handle_);
        i2s_del_channel(rx_handle_);
        initialized_ = false;
        Serial.println("I2S麦克风已关闭");
    }
} 