#include "memory_utils.h"

// 紧急内存指针定义
uint8_t* emergencyMemory = nullptr;

bool isLowMemory(uint32_t threshold) {
    uint32_t freeHeap = ESP.getFreeHeap();
    return freeHeap < threshold;
}

void releaseEmergencyMemory() {
    if (emergencyMemory != nullptr) {
        free(emergencyMemory);
        emergencyMemory = nullptr;
        Serial.println("已释放紧急内存");
    }
}

// 初始化紧急内存（在setup时调用）
bool initEmergencyMemory() {
    if (emergencyMemory == nullptr) {
        emergencyMemory = (uint8_t*)malloc(EMERGENCY_MEMORY_SIZE);
        if (emergencyMemory == nullptr) {
            Serial.println("错误：无法分配紧急内存！");
            return false;
        }
        Serial.printf("已预留 %d 字节紧急内存\n", EMERGENCY_MEMORY_SIZE);
        return true;
    }
    return true;
} 