#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <WiFi.h>
#include <vector>
#include "EnvironmentData.h"
#include "i2s_mic_manager.h"
#include <ESPAsyncWebServer.h>

class CommunicationManager {
private:
    // Command Server
    WiFiServer* server;
    std::vector<WiFiClient> clients;
    static const uint16_t SERVER_PORT = 8266;
    static const size_t MAX_CLIENTS = 5;

    // WebSocket Audio Server
    AsyncWebSocket* audioWs;
    std::vector<AsyncWebSocketClient*> audioWsClients;
    static const size_t MAX_AUDIO_WS_CLIENTS = 2;
    uint8_t wsAudioBuffer[1024];

    bool isRunning;
    EnvironmentData currentData;
    I2SMicManager* micManagerPtr;

public:
    CommunicationManager(I2SMicManager* micMgr);
    ~CommunicationManager();

    // Command server methods
    bool begin();
    void stop();
    void update(); // Handles command clients
    bool isServerRunning() const { return isRunning; }
    void broadcastEnvironmentData(const EnvironmentData& data); // Still just updates internal data
    void sendHistoricalData(WiFiClient& client, const std::vector<EnvironmentData>& data);

    // HTTP and WebSocket setup methods
    void setupHttpServer(AsyncWebServer* httpServer);
    void setupWebSocketServer(AsyncWebServer* httpServer);

    // WebSocket Audio Streaming Method
    void streamAudioViaWebSocket();

private:
    // Command handling helpers
    void handleNewConnections();
    void handleClientMessages();
    void removeDisconnectedClients();
    void sendJsonData(WiFiClient& client, const EnvironmentData& data);
    void processClientCommand(WiFiClient& client, const String& command);

    // WebSocket Event Handler
    void onAudioWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    static void staticOnAudioWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

};

// Declare a global pointer to the CommunicationManager instance for the static wrapper
extern CommunicationManager* globalCommManagerPtr;

#endif // COMMUNICATION_MANAGER_H 