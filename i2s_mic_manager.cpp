#include "i2s_mic_manager.h"
#include <math.h>
#include <cmath> // Include for isnan

I2SMicManager::I2SMicManager(uint32_t sample_rate, uint8_t ws_pin, uint8_t sd_pin, uint8_t sck_pin, i2s_port_t port_num) :
    sample_rate_(sample_rate),
    ws_pin_(ws_pin),
    sd_pin_(sd_pin),
    sck_pin_(sck_pin),
    port_num_(port_num),
    initialized_(false),
    lastValue_(30.0f), // Initial last value, will be updated by valid readings
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
        i2s_del_channel(rx_handle_); // Clean up channel if init fails
        return false;
    }
    
    err = i2s_channel_enable(rx_handle_);
    if (err != ESP_OK) {
        Serial.printf("I2S通道启用失败: %s\n", esp_err_to_name(err));
        i2s_channel_disable(rx_handle_); // Disable before deleting
        i2s_del_channel(rx_handle_); // Clean up channel
        return false;
    }
    
    initialized_ = true;
    Serial.println("I2S麦克风初始化成功");
    return true;
}

float I2SMicManager::readNoiseLevel(int timeout_ms) {
    if (!initialized_) {
        if (!begin()) {
             Serial.println("ERR: Attempt to read failed, I2S not initialized and begin() failed.");
             return NAN;
        }
    }

    size_t bytes_read = 0;
    esp_err_t result = i2s_channel_read(rx_handle_, samples_, sizeof(samples_), &bytes_read, timeout_ms);

    // 增加对具体错误的日志记录
    if (result != ESP_OK) {
        if (result == ESP_ERR_TIMEOUT) {
             // Serial.println("DBG: I2S read timeout."); // 超时可能比较常见，不一定需要记录
        } else {
            // 记录其他更严重的错误
            Serial.printf("ERR: I2S read failed with error: %s (%d)\\n", esp_err_to_name(result), result);
        }
        return NAN; // 返回 NaN on failure
    }

    if (bytes_read == 0) {
        // Serial.println("DBG: I2S read 0 bytes."); // 0 字节也算读取失败
        return NAN;
    }

    size_t validSamples = bytes_read / sizeof(int32_t);
    validSamples = min(validSamples, BUFFER_SIZE);

    double rms = calculateRMS(samples_, validSamples);

    if (rms <= 0 || isnan(rms)) { // 也检查rms是否为NaN
        // Serial.println("DBG: Calculated RMS is zero or invalid.");
        return NAN;
    }

    // 计算分贝值 (rms > 0 and not NaN here)
    float db = 20.0f * log10f(rms);

    // 应用噪声基准和偏移
    if (db < NOISE_FLOOR) {
        db = NOISE_FLOOR;
    }
    db = (db - NOISE_FLOOR) * CALIBRATION_FACTOR + OFFSET_DB;

    // processNoiseLevel 现在也能处理 NaN 输入，但这里我们已经保证了 db 不是 NaN
    return processNoiseLevel(db);
}

float I2SMicManager::processNoiseLevel(float rawDb) {
    // 如果输入是NaN，直接返回NaN
    if (isnan(rawDb)) {
        isHighNoise_ = false; // Reset high noise flag if reading is invalid
        // Do not update lastValue_ with NaN
        return NAN;
    }
    
    float db = rawDb; // Work with a copy
    
    // 检查是否超过高噪声阈值
    if (db >= HIGH_NOISE_THRESHOLD) {
        // 检测到高噪声时立即返回实际值，不进行平滑处理
        isHighNoise_ = true;
        // Use DataValidator before assigning to lastValue_ and returning
        db = DataValidator::validateDecibels(db);
        lastValue_ = db; // Update lastValue only with valid, validated data
        Serial.printf("检测到高噪声: %.1f dB\n", db);
        return db;
    }
    
    // --- 移除人为的 30dB 检查 ---
    // if (isinf(db) || db < 30.0f) {
    //     db = 30.0f;
    // }
    
    // --- 移除人为的 120dB 检查, 让 DataValidator 处理 ---
    // else if (db > 120.0f) {
    //    db = 120.0f;
    // }
    
    // 非高噪声情况下进行正常处理
    isHighNoise_ = false;
    
    // 应用指数平滑 (only if db is not NaN, which is already checked)
    // Check if lastValue_ is valid before smoothing
    if (!isnan(lastValue_)) {
        db = applyExponentialSmoothing(db);
    } else {
        // If lastValue is NaN (e.g., first reading after failure), don't smooth yet
        // or just use the current db value directly.
    }
    
    // 使用DataValidator最终验证结果 (handles NaN, min, max)
    db = DataValidator::validateDecibels(db);
    
    // 保存当前有效且验证后的值作为下一次的lastValue_
    lastValue_ = db; // lastValue should only store valid, smoothed, validated results
    return db;
}

double I2SMicManager::calculateRMS(const int32_t* samples, size_t count) {
    if (count == 0 || samples == nullptr) return 0.0; // 增加空指针检查
    
    double sum_sq = 0.0; // Sum of squares
    int64_t dc_sum = 0;
    
    // --- Standardize sample reading and DC offset calculation ---
    // First pass: Calculate DC offset
    for (size_t i = 0; i < count; i++) {
        // Correctly sign-extend 24-bit value from 32-bit buffer slot
        int32_t sample = samples[i] << 8; // Shift left to align MSB
        sample >>= 8;                     // Shift right to sign-extend
        dc_sum += sample;
    }
    double dc_offset = static_cast<double>(dc_sum) / count;
    
    // Second pass: Calculate sum of squares of (sample - dc_offset)
    for (size_t i = 0; i < count; i++) {
        int32_t sample = samples[i] << 8;
        sample >>= 8;
        double value = static_cast<double>(sample) - dc_offset;
        sum_sq += value * value;
    }
    
    // Calculate RMS: sqrt(mean of squares)
    // Normalize based on the potential full scale range of a 24-bit signed integer (2^23)
    // Normalization factor: 8388608.0 (which is 2^23)
    double mean_sq = sum_sq / count;
    double normalized_rms = sqrt(mean_sq) / 8388608.0;
    
    return normalized_rms; // Return normalized RMS
}

float I2SMicManager::applyExponentialSmoothing(float newValue) {
    // Ensure lastValue is not NaN before using it in calculation
    if (isnan(lastValue_)) {
        return newValue; // If previous value was invalid, don't smooth, return current
    }
    
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

size_t I2SMicManager::readRawSamples(int32_t* buffer, size_t maxSamples, int timeout_ms) {
    if (!initialized_ || buffer == nullptr || maxSamples == 0) {
        return 0; // Return 0 if not initialized or buffer invalid
    }

    size_t bytes_to_read = maxSamples * sizeof(int32_t);
    size_t bytes_read = 0;
    esp_err_t result = i2s_channel_read(rx_handle_, buffer, bytes_to_read, &bytes_read, timeout_ms);

    if (result != ESP_OK || bytes_read == 0) {
        if (result != ESP_ERR_TIMEOUT) {
            Serial.printf("ERR: I2S raw read failed with error: %s (%d)\n", esp_err_to_name(result), result);
        }
        return 0; // Return 0 on failure or timeout
    }

    // Return number of samples read
    return bytes_read / sizeof(int32_t);
} 