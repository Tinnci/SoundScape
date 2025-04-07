#ifndef DATA_VALIDATOR_H
#define DATA_VALIDATOR_H

#include <Arduino.h>
#include "ui_constants.h"

/**
 * 数据验证工具类
 * 集中处理传感器数据的验证和范围限制
 */
class DataValidator {
public:
    /**
     * 验证温度数据
     * @param value 原始温度值
     * @return 验证后的温度值，无效数据返回-1
     */
    static float validateTemperature(float value) {
        if (isnan(value) || value < TEMP_MIN || value > TEMP_MAX) {
            return -1.0f; // 无效温度返回-1
        }
        return value;
    }

    /**
     * 验证湿度数据
     * @param value 原始湿度值
     * @return 验证后的湿度值，无效数据返回-1
     */
    static float validateHumidity(float value) {
        if (isnan(value) || value < HUM_MIN || value > HUM_MAX) {
            return -1.0f; // 无效湿度返回-1
        }
        return value;
    }

    /**
     * 验证光照强度数据
     * @param value 原始光照强度值
     * @return 验证后的光照强度值，无效数据返回-1
     */
    static float validateLux(float value) {
        if (isnan(value) || value < LUX_MIN || value > LUX_MAX) {
            return -1.0f; // 无效光照返回-1
        }
        return value;
    }

    /**
     * 验证分贝值
     * @param value 原始分贝值
     * @return 验证后的分贝值
     */
    static float validateDecibels(float value) {
        // 只有当值为NaN或严重异常的负值时才设为0
        if (isnan(value) || value < -50.0f) {
            Serial.println("警告: 检测到异常分贝值，设置为0");
            return 0.0f;
        }
        // 小于最小可能值但大于严重异常值时，设置为最小有效值
        if (value < DB_MIN) {
            return DB_MIN;
        }
        if (value > DB_MAX) {
            return DB_MAX; // 超过最大值，返回最大值
        }
        return value;
    }

    /**
     * 检查传感器数据是否有效
     * @param value 传感器数据
     * @return 数据是否有效
     */
    static bool isValid(float value) {
        return !isnan(value) && value >= 0;
    }
};

#endif // DATA_VALIDATOR_H 