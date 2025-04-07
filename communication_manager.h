#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <WiFi.h>
#include <vector>
#include "EnvironmentData.h"

class CommunicationManager {
private:
    WiFiServer* server;
    std::vector<WiFiClient> clients;
    static const uint16_t SERVER_PORT = 8266;  // 默认端口号
    static const size_t MAX_CLIENTS = 5;       // 最大客户端连接数
    bool isRunning;
    EnvironmentData currentData;  // 添加当前数据成员变量

public:
    CommunicationManager();
    ~CommunicationManager();

    bool begin();
    void stop();
    void update();
    bool isServerRunning() const { return isRunning; }
    
    // 数据发送方法
    void broadcastEnvironmentData(const EnvironmentData& data);
    void sendHistoricalData(WiFiClient& client, const std::vector<EnvironmentData>& data);

private:
    void handleNewConnections();
    void handleClientMessages();
    void removeDisconnectedClients();
    
    // 协议相关方法
    void sendJsonData(WiFiClient& client, const EnvironmentData& data);
    void processClientCommand(WiFiClient& client, const String& command);
};

#endif // COMMUNICATION_MANAGER_H 