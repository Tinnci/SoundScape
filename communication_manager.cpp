#include "communication_manager.h"
#include <ArduinoJson.h>

#if ARDUINOJSON_VERSION_MAJOR < 6
#error "Requires ArduinoJson 6 or higher"
#endif

CommunicationManager::CommunicationManager() : server(nullptr), isRunning(false) {
    clients.reserve(MAX_CLIENTS);
}

CommunicationManager::~CommunicationManager() {
    stop();
}

bool CommunicationManager::begin() {
    if (isRunning) return true;
    
    server = new WiFiServer(SERVER_PORT);
    server->begin();
    isRunning = true;
    Serial.printf("通信服务器已启动在端口 %d\n", SERVER_PORT);
    return true;
}

void CommunicationManager::stop() {
    if (!isRunning) return;
    
    // 关闭所有客户端连接
    for (auto& client : clients) {
        if (client.connected()) {
            client.stop();
        }
    }
    clients.clear();
    
    // 关闭服务器
    if (server) {
        server->end();
        delete server;
        server = nullptr;
    }
    
    isRunning = false;
    Serial.println("通信服务器已停止");
}

void CommunicationManager::update() {
    if (!isRunning || !server) return;
    
    static unsigned long lastUpdateTime = 0;
    unsigned long currentTime = millis();
    
    // 限制更新频率，避免过于频繁的客户端检查
    if (currentTime - lastUpdateTime < 50) { // 50ms 间隔
        return;
    }
    lastUpdateTime = currentTime;
    
    // 处理新连接
    handleNewConnections();
    
    // 处理现有客户端的消息
    handleClientMessages();
    
    // 移除断开的客户端
    removeDisconnectedClients();
}

void CommunicationManager::handleNewConnections() {
    if (!server->hasClient()) return;
    
    WiFiClient newClient = server->accept();
    if (!newClient) return;
    
    if (clients.size() < MAX_CLIENTS) {
        clients.push_back(newClient);
        Serial.printf("新客户端连接: %s\n", newClient.remoteIP().toString().c_str());
        newClient.println("CONNECTED");
    } else {
        Serial.println("达到最大客户端数量限制，拒绝新连接");
        newClient.println("SERVER_FULL");
        newClient.stop();
    }
}

void CommunicationManager::handleClientMessages() {
    for (auto& client : clients) {
        if (!client.connected() || !client.available()) continue;
        
        String command = client.readStringUntil('\n');
        if (command.length() == 0) continue;
        
        command.trim();
        processClientCommand(client, command);
        yield(); // 让出CPU时间给其他任务
    }
}

void CommunicationManager::removeDisconnectedClients() {
    auto it = clients.begin();
    while (it != clients.end()) {
        if (!it->connected()) {
            Serial.println("移除断开的客户端");
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

void CommunicationManager::broadcastEnvironmentData(const EnvironmentData& data) {
    if (!isRunning) return;
    
    // 只更新当前数据，不主动发送
    currentData = data;
    Serial.println("已更新当前环境数据");
}

void CommunicationManager::sendHistoricalData(WiFiClient& client, const std::vector<EnvironmentData>& data) {
    JsonDocument doc;
    JsonArray array = doc["data"].to<JsonArray>();
    
    for (const auto& record : data) {
        JsonObject obj = array.add<JsonObject>();
        obj["timestamp"] = record.timestamp;
        obj["decibels"] = record.decibels;
        obj["temperature"] = record.temperature;
        obj["humidity"] = record.humidity;
        obj["lux"] = record.lux;
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    client.println(jsonString);
}

void CommunicationManager::sendJsonData(WiFiClient& client, const EnvironmentData& data) {
    if (!client.connected()) return;
    
    StaticJsonDocument<200> doc;
    doc["timestamp"] = data.timestamp;
    doc["decibels"] = data.decibels;
    doc["humidity"] = data.humidity;
    doc["temperature"] = data.temperature;
    doc["lux"] = data.lux;
    
    String jsonString;
    serializeJson(doc, jsonString);
    Serial.printf("发送的JSON数据: %s\n", jsonString.c_str());
    
    client.println(jsonString);
    // 等待数据发送完成
    client.flush();
}

void CommunicationManager::processClientCommand(WiFiClient& client, const String& command) {
    Serial.printf("收到客户端命令: %s\n", command.c_str());
    
    if (command == "GET_CURRENT") {
        Serial.println("处理GET_CURRENT命令");
        if (currentData.timestamp != 0) {
            Serial.printf("发送当前数据: 噪声=%.1f dB, 温度=%.1f °C, 湿度=%.1f %%, 光照=%.1f lx\n",
                currentData.decibels, currentData.temperature, currentData.humidity, currentData.lux);
            sendJsonData(client, currentData);
            Serial.println("已发送当前数据");
        } else {
            Serial.println("警告：没有可用的当前数据");
            client.println("NO_DATA");
        }
    }
    else if (command == "GET_HISTORY") {
        Serial.println("处理GET_HISTORY命令");
        client.println("OK");
    }
    else {
        Serial.printf("未知命令: %s\n", command.c_str());
        client.println("UNKNOWN_COMMAND");
    }
} 