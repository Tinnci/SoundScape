#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <WiFi.h>
#include <vector>
#include <time.h>
#include "EnvironmentData.h"
#include "i2s_mic_manager.h"
#include <ESPAsyncWebServer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

class UIManager;

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
    static const size_t WS_AUDIO_BUFFER_SAMPLES = 512; // Number of samples per WebSocket message (Adjust as needed)
    int16_t wsAudioBuffer[WS_AUDIO_BUFFER_SAMPLES]; // New buffer (512 * 16-bit samples = 1024 bytes)

    bool isRunning;
    EnvironmentData currentData;
    I2SMicManager* micManagerPtr;
    UIManager* uiManagerPtr_;

    // Mutex for protecting audioWsClients vector
    SemaphoreHandle_t audioClientsMutex;

    // Network Configuration (Moved from .ino)
    const char* wifiSsid_;
    const char* wifiPassword_;
    const char* ntpServer_;
    long gmtOffsetSec_;
    int daylightOffsetSec_;

public:
    CommunicationManager(I2SMicManager* micMgr, UIManager* uiMgr,
                         const char* ssid, const char* password,
                         const char* ntpServer = "pool.ntp.org",
                         long gmtOffset = 28800, int daylightOffset = 0);
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

    // --- Network Management Methods ---
    bool connectWiFi();         // Tries to connect to WiFi
    bool syncNTPTime();       // Tries to sync NTP time
    bool reconnectWiFi();     // Disconnects, connects, syncs time
    bool isWiFiConnected() const; // Checks current WiFi status
    String getIPAddress() const;  // Gets local IP address
    // --- End Network Management ---

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