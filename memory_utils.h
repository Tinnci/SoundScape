#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <Arduino.h>

// 内存管理相关函数
bool isLowMemory(uint32_t threshold);
void releaseEmergencyMemory();
bool initEmergencyMemory();  // 新增：初始化紧急内存的函数声明

// 紧急内存大小常量
#define EMERGENCY_MEMORY_SIZE 4096  // 4KB 紧急内存

// 紧急内存指针声明（注意：使用extern关键字）
extern uint8_t* emergencyMemory;

#endif // MEMORY_UTILS_H 