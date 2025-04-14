#include "communication_manager.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <algorithm>

// Define the global pointer
CommunicationManager* globalCommManagerPtr = nullptr;

#if ARDUINOJSON_VERSION_MAJOR < 6
#error "Requires ArduinoJson 6 or higher"
#endif

CommunicationManager::CommunicationManager(I2SMicManager* micMgr) :
    server(nullptr),
    audioWs(nullptr),
    isRunning(false),
    micManagerPtr(micMgr),
    audioClientsMutex(nullptr) // Initialize mutex handle to null
{
    clients.reserve(MAX_CLIENTS);
    audioWsClients.reserve(MAX_AUDIO_WS_CLIENTS);
    globalCommManagerPtr = this;

    // Create the mutex
    audioClientsMutex = xSemaphoreCreateMutex();
    if (audioClientsMutex == nullptr) {
        Serial.println("ERR: Failed to create audioClientsMutex!");
        // Handle error appropriately, maybe prevent starting?
    }
}

CommunicationManager::~CommunicationManager() {
    stop();
    // Delete the mutex
    if (audioClientsMutex != nullptr) {
        vSemaphoreDelete(audioClientsMutex);
    }
    globalCommManagerPtr = nullptr;
}

bool CommunicationManager::begin() {
    if (isRunning) return true;
    
    server = new WiFiServer(SERVER_PORT);
    if (!server) {
        Serial.println("ERR: Failed to create Command Server");
        return false;
    }
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
    
    JsonDocument doc;
    doc["timestamp"] = data.timestamp;
    doc["decibels"] = data.decibels;
    doc["humidity"] = data.humidity;
    doc["temperature"] = data.temperature;
    doc["lux"] = data.lux;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    client.println(jsonString);
}

void CommunicationManager::processClientCommand(WiFiClient& client, const String& command) {
    Serial.printf("收到客户端命令: %s\n", command.c_str());
    
    if (command == "GET_CURRENT") {
        if (currentData.timestamp != 0) {
            sendJsonData(client, currentData);
        } else {
            Serial.println("警告：没有可用的当前数据");
            client.println("NO_DATA");
        }
    }
    else if (command == "GET_HISTORY") {
        Serial.println("处理GET_HISTORY命令 (未实现)");
        client.println("HISTORY_NOT_IMPLEMENTED");
    }
    else {
        Serial.printf("未知命令: %s\n", command.c_str());
        client.println("UNKNOWN_COMMAND");
    }
}

void CommunicationManager::setupHttpServer(AsyncWebServer* httpServer) {
    if (!httpServer) return;

    // 提供根路径的 HTML 页面
    httpServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        // 发送包含 JavaScript WebSocket 客户端和 Web Audio API 播放器的 HTML
        String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Audio Stream</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
        body { font-family: sans-serif; text-align: center; padding: 20px; }
        button { font-size: 1.2em; padding: 10px 20px; margin: 10px; cursor: pointer; }
        #status { margin-top: 20px; font-weight: bold; }
    </style>
</head>
<body>
    <h1>ESP32 Live Audio Stream</h1>
    <button id='playButton'>Play</button>
    <button id='stopButton' disabled>Stop</button>
    <div id='status'>Status: Disconnected</div>

    <script>
        let ws = null;
        let audioContext = null;
        let audioBufferQueue = [];
        let isPlaying = false;
        let isBuffering = true;
        let nextStartTime = 0;
        const sampleRate = 16000; // 与 ESP32 I2S 采样率匹配
        const bufferSizeSeconds = 0.5; // 客户端缓冲秒数
        const targetBufferSize = sampleRate * bufferSizeSeconds; // 目标缓冲区大小 (samples)

        const statusDiv = document.getElementById('status');
        const playButton = document.getElementById('playButton');
        const stopButton = document.getElementById('stopButton');

        function updateStatus(message) {
            statusDiv.textContent = 'Status: ' + message;
            console.log('Status:', message);
        }

        function connectWebSocket() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                updateStatus('Already connected');
                return;
            }

            const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${wsProtocol}//${window.location.hostname}/audio`; // 使用相对路径 /audio
            updateStatus(`Connecting to ${wsUrl}...`);

            ws = new WebSocket(wsUrl);

            ws.onopen = () => {
                updateStatus('Connected');
                playButton.disabled = false;
                stopButton.disabled = true; // Stop is disabled until play starts
                isBuffering = true; // Start buffering on connect
                audioBufferQueue = []; // Clear queue on new connection
                nextStartTime = 0; // Reset scheduling time on new connection
            };

            ws.onmessage = (event) => {
                if (event.data instanceof Blob) {
                    // Read the blob as an ArrayBuffer
                    event.data.arrayBuffer().then(arrayBuffer => {
                        // Assuming the data is 16-bit PCM samples
                        const pcmData = new Int16Array(arrayBuffer);
                        audioBufferQueue.push(pcmData); // Add Int16Array to queue

                        // Check if we need to start playing after buffering
                        if (isBuffering) {
                            let currentBufferedSamples = audioBufferQueue.reduce((sum, arr) => sum + arr.length, 0);
                            updateStatus(`Buffering... ${Math.round((currentBufferedSamples / targetBufferSize) * 100)}%`);
                            if (currentBufferedSamples >= targetBufferSize) {
                                isBuffering = false;
                                updateStatus('Buffering complete. Ready to play.');
                                if (isPlaying) { // If play was pressed during buffering
                                    startPlayback();
                                }
                            }
                        }
                    });
                } else {
                    console.log('Received non-binary message:', event.data);
                }
            };

            ws.onerror = (error) => {
                updateStatus('WebSocket Error');
                console.error('WebSocket Error:', error);
                cleanupAudio();
            };

            ws.onclose = (event) => {
                updateStatus(`Disconnected (Code: ${event.code}, Reason: ${event.reason || 'N/A'})`);
                console.log('WebSocket closed:', event);
                cleanupAudio();
            };
        }

        function scheduleBuffers() {
            if (!isPlaying || !audioContext) { // Check audioContext as well
                return;
            }

            const now = audioContext.currentTime;
            const lookaheadTime = 0.2; // Try to schedule ~200ms ahead
            let scheduledDuration = 0; // Track duration scheduled in this call

            // Check if we need to reset scheduling due to major lag or initial start
            if (nextStartTime < now) {
                 console.warn(`Scheduling lag detected: nextStartTime (${nextStartTime.toFixed(3)}) < now (${now.toFixed(3)}). Resetting.`);
                 nextStartTime = now + 0.05; // Reset to start slightly ahead of now
                 isBuffering = true; // Trigger buffering state again if we lagged badly
            }

            // --- Refined Buffer Scheduling Logic ---
            while (audioBufferQueue.length > 0 && nextStartTime < now + lookaheadTime + scheduledDuration) { // Schedule slightly ahead
                const pcmData = audioBufferQueue.shift();
                if (!pcmData || pcmData.length === 0) continue; // Skip empty buffers

                const float32Data = new Float32Array(pcmData.length);
                const maxInt16Value = 32767.0; // 2^15 - 1 (for int16)
                for (let i = 0; i < pcmData.length; i++) {
                    float32Data[i] = pcmData[i] / maxInt16Value; // Normalize int16 to float -1.0 to 1.0
                }

                // Prevent creating zero-length buffers
                if (float32Data.length === 0) {
                    console.warn("Skipping zero-length audio buffer.");
                    continue;
                }

                try {
                    const audioBuffer = audioContext.createBuffer(1, float32Data.length, sampleRate);
                    audioBuffer.copyToChannel(float32Data, 0);

                    const sourceNode = audioContext.createBufferSource();
                    sourceNode.buffer = audioBuffer;
                    sourceNode.connect(audioContext.destination);

                    // Log scheduling time only if significantly different
                    // if (Math.abs(nextStartTime - audioContext.currentTime) > 0.1) {
                    //      console.log(`Scheduling buffer of duration ${(audioBuffer.duration).toFixed(3)}s at ${nextStartTime.toFixed(3)} (now: ${now.toFixed(3)})`);
                    // }
                    sourceNode.start(nextStartTime);
                    nextStartTime += audioBuffer.duration; // Schedule next buffer right after this one
                    scheduledDuration += audioBuffer.duration;

                } catch (e) {
                    console.error("Error creating or scheduling audio buffer:", e);
                    // Potentially stop playback or try to recover
                    // For now, just log the error and continue trying
                }
            }
            // --- End Refined Buffer Scheduling ---

            // Check buffer levels after scheduling
            let currentBufferedSamples = audioBufferQueue.reduce((sum, arr) => sum + (arr ? arr.length : 0), 0);
            let currentBufferedSeconds = currentBufferedSamples / sampleRate;

            if (isBuffering && currentBufferedSamples >= targetBufferSize) {
                console.log("Buffering complete during playback scheduling.");
                isBuffering = false;
                updateStatus('Playing...'); // Update status if we just finished buffering
                 // Ensure nextStartTime is reasonable after buffering
                 if (nextStartTime < now) {
                     nextStartTime = now + 0.05;
                 }
            } else if (!isBuffering && currentBufferedSeconds < bufferSizeSeconds * 0.3) { // If buffer runs low (e.g., < 30%)
                 console.warn(`Buffer low (${currentBufferedSeconds.toFixed(2)}s). Attempting to re-buffer.`);
                 isBuffering = true; // Enter buffering state
                 updateStatus(`Re-buffering...`);
                 // Stop requesting frames temporarily? The loop condition already handles empty queue.
                 // We rely on the server sending more data.
            } else if (isBuffering) {
                 // Still buffering, update status
                 updateStatus(`Buffering... ${Math.round((currentBufferedSamples / targetBufferSize) * 100)}%`);
            }


            // Keep scheduling if playing and not critically lagging
            if (isPlaying) {
                // Schedule the next check slightly before the last scheduled buffer ends,
                // but not more frequent than requestAnimationFrame allows.
                // Using requestAnimationFrame is generally fine.
                requestAnimationFrame(scheduleBuffers);
            }
        }


        function startPlayback() {
            // Ensure AudioContext is ready (especially after user interaction)
             if (!audioContext) {
                 try {
                    audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: sampleRate });
                    console.log(`AudioContext created. State: ${audioContext.state}, Sample Rate: ${audioContext.sampleRate}`);
                 } catch (e) {
                     updateStatus("Error creating AudioContext: " + e.message);
                     console.error("AudioContext creation failed:", e);
                     cleanupAudio(); // Clean up if context fails
                     return;
                 }
            }

             // If context is suspended, try to resume it (requires user gesture)
             if (audioContext.state === 'suspended') {
                 audioContext.resume().then(() => {
                     console.log("AudioContext resumed successfully.");
                     // Proceed with playback logic only after successful resume
                     initiatePlaybackSequence();
                 }).catch(e => {
                     updateStatus("AudioContext resume failed. Please click Play again.");
                     console.error("AudioContext resume failed:", e);
                     // Keep UI state indicating stopped/needs interaction
                     isPlaying = false;
                     playButton.disabled = false;
                     stopButton.disabled = true;
                 });
             } else if (audioContext.state === 'running') {
                 initiatePlaybackSequence(); // Context already running
             } else {
                  updateStatus(`AudioContext in unexpected state: ${audioContext.state}`);
                  console.error(`AudioContext state is ${audioContext.state}`);
             }
        }

        function initiatePlaybackSequence() {
            // This function contains the logic previously directly in startPlayback,
            // called after ensuring AudioContext is running.
             if (isBuffering && audioBufferQueue.length < targetBufferSize / sampleRate * 0.5) { // Require some buffer before starting
                 updateStatus("Buffering, please wait...");
                 // isPlaying should be true, scheduleBuffers will check buffering state
                 // Start requesting frames if not already doing so
                 if (isPlaying) requestAnimationFrame(scheduleBuffers);
                 return;
             }
             if (!isBuffering && audioBufferQueue.length === 0) {
                 updateStatus("Buffer empty, waiting for data...");
                 isBuffering = true; // Go back to buffering state
                 if (isPlaying) requestAnimationFrame(scheduleBuffers);
                 return;
             }

             isPlaying = true;
             playButton.disabled = true;
             stopButton.disabled = false;
             updateStatus('Playing...'); // Or Buffering if isBuffering is still true

             // Reset scheduling time relative to current context time ONLY if it's lagging significantly
             // or if starting fresh. The scheduleBuffers function handles minor lag.
             if (nextStartTime < audioContext.currentTime - 0.1) { // If significantly behind
                 nextStartTime = audioContext.currentTime + 0.1; // Add small delay before first buffer
                 console.log("Resetting nextStartTime for playback start.");
             } else if (nextStartTime === 0) { // Initial start
                 nextStartTime = audioContext.currentTime + 0.1;
             }


             requestAnimationFrame(scheduleBuffers); // Start the scheduling loop
        }


        function stopPlayback() {
            isPlaying = false; // Stop the scheduling loop
            if (audioContext && audioContext.state === 'running') {
                audioContext.suspend().then(() => console.log("AudioContext suspended.")); // Suspend context
            }
            // Keep the buffer queue for potential resume
            playButton.disabled = false;
            stopButton.disabled = true;
            updateStatus('Stopped');
            console.log("Playback stopped.");
             // Do not reset nextStartTime here, allow resume from roughly where it left off
        }

         function cleanupAudio() {
            stopPlayback(); // Ensure playback is stopped
            if (audioContext) {
                audioContext.close().then(() => {
                     console.log("AudioContext closed");
                     audioContext = null;
                });
            }
            audioBufferQueue = []; // Clear queue on disconnect/error
            playButton.disabled = true; // Disable play until reconnected
            stopButton.disabled = true;
            if (ws && ws.readyState !== WebSocket.CLOSED) {
                 ws.close();
            }
            ws = null;
        }


        playButton.onclick = () => {
             if (!ws || ws.readyState !== WebSocket.OPEN) {
                connectWebSocket(); // Connect first if not connected
             }
             isPlaying = true; // Set flag immediately, actual playback starts after buffering
             startPlayback();
        };

        stopButton.onclick = stopPlayback;

        // Attempt to connect on page load
        connectWebSocket();

        // Cleanup on page unload
        window.addEventListener('beforeunload', cleanupAudio);

    </script>
</body>
</html>
        )";
        request->send(200, "text/html", html);
    });

    // 可以添加其他 HTTP 路由，例如用于发送控制命令或获取状态
    httpServer->on("/status", HTTP_GET, [this](AsyncWebServerRequest *request){
        // Example: Send status as JSON
        JsonDocument doc;
        doc["commandServer"] = isRunning;
        doc["audioClients"] = audioWsClients.size();
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

     httpServer->onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });

    Serial.println("HTTP server routes configured.");
}

void CommunicationManager::setupWebSocketServer(AsyncWebServer* httpServer) {
    if (!httpServer) return;

    audioWs = new AsyncWebSocket("/audio"); // 在 /audio 路径上创建 WebSocket
    if (!audioWs) {
        Serial.println("ERR: Failed to create WebSocket");
        return;
    }

    // 注册事件处理函数 (使用静态包装器)
    audioWs->onEvent(staticOnAudioWsEvent);

    // 将 WebSocket 服务器附加到 HTTP 服务器
    httpServer->addHandler(audioWs);

    Serial.println("WebSocket server configured on /audio");
}

void CommunicationManager::streamAudioViaWebSocket() {
    // Check if mutex exists before trying to use it
    if (!micManagerPtr || !audioWs || audioClientsMutex == nullptr) {
        return; 
    }

    // Take the mutex to safely check and access audioWsClients
    if (xSemaphoreTake(audioClientsMutex, (TickType_t)10) != pdTRUE) { // Use a small timeout
        Serial.println("WARN: Could not obtain audioClientsMutex in streamAudio");
        return; // Could not get mutex, skip this cycle
    }

    if (audioWsClients.empty()) {
        xSemaphoreGive(audioClientsMutex); // Release mutex before returning
        return; // No clients, release mutex and return
    }

    // Temporary buffer to read 32-bit samples from I2S
    int32_t tempReadBuffer[WS_AUDIO_BUFFER_SAMPLES];

    // 从 I2S 读取原始样本 (read into temporary 32-bit buffer)
    size_t samplesRead = micManagerPtr->readRawSamples(tempReadBuffer, WS_AUDIO_BUFFER_SAMPLES, 15); // 短超时

    if (samplesRead > 0) {
        // Convert 32-bit samples to 16-bit and store in wsAudioBuffer
        for (size_t i = 0; i < samplesRead; ++i) {
            // Correctly sign-extend 24-bit data from 32-bit slot, then take upper 16 bits
            int32_t sample32 = tempReadBuffer[i] << 8; // Align MSB if data is in lower bits (check I2S config)
            // Right shift to get upper 16 bits (sign bit preserved)
            wsAudioBuffer[i] = (int16_t)(sample32 >> 16); 
        }

        size_t bytesToSend = samplesRead * sizeof(int16_t); // Calculate bytes for 16-bit data

        // 将原始字节作为二进制消息发送给所有连接的 WebSocket 客户端
        // Manual iteration remains safer within the mutex lock
        for (AsyncWebSocketClient* client : audioWsClients) {
             if (client && client->status() == WS_CONNECTED) {
                 if (client->canSend()) { // 检查客户端是否可以接收数据
                    client->binary((const uint8_t*)wsAudioBuffer, bytesToSend); // Send the 16-bit data buffer
                 } else {
                     // Serial.printf("WARN: WebSocket client %u cannot send data now.\\n\", client->id());
                     // 客户端可能忙碌或缓冲区已满
                 }
             }
        }

        audioWs->cleanupClients(); // It's generally safe to call cleanupClients outside the loop, 
                                   // but the internal implementation should be thread-safe if possible.
                                   // Keep it here for now, assuming library handles internal locking or queueing.
                                   // Alternatively, iterate and mark clients for cleanup, then cleanup after releasing mutex.
        yield(); // 发送后让出时间
    }

    // Release the mutex
    xSemaphoreGive(audioClientsMutex);
}

void CommunicationManager::staticOnAudioWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (globalCommManagerPtr) {
        globalCommManagerPtr->onAudioWsEvent(server, client, type, arg, data, len);
    }
}

void CommunicationManager::onAudioWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    // Check if mutex exists before trying to use it
    if (audioClientsMutex == nullptr) {
        Serial.println("ERR: audioClientsMutex is null in onAudioWsEvent!");
        return;
    }

    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%lu connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            // --- Lock Mutex ---
            if (xSemaphoreTake(audioClientsMutex, portMAX_DELAY) == pdTRUE) {
                if (audioWsClients.size() < MAX_AUDIO_WS_CLIENTS) {
                    audioWsClients.push_back(client);
                    Serial.printf("Client #%lu added. Total audio clients: %d\n", client->id(), audioWsClients.size());
                } else {
                    Serial.printf("Max WebSocket audio clients (%d) reached. Rejecting client #%lu.\n", MAX_AUDIO_WS_CLIENTS, client->id());
                    // Close client outside the lock if possible, or be quick
                    client->close(); 
                }
                 // --- Unlock Mutex ---
                 xSemaphoreGive(audioClientsMutex);
            } else {
                Serial.println("ERR: Could not obtain audioClientsMutex for WS_EVT_CONNECT");
                 // Maybe close the client if we can't add it? Handle error.
                 client->close();
            }
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%lu disconnected\n", client->id());
            // --- Lock Mutex ---
            if (xSemaphoreTake(audioClientsMutex, portMAX_DELAY) == pdTRUE) {
                size_t oldSize = audioWsClients.size();
                audioWsClients.erase(
                    std::remove_if(audioWsClients.begin(), audioWsClients.end(),
                                [client](AsyncWebSocketClient* c) { return c->id() == client->id(); }),
                    audioWsClients.end());
                size_t newSize = audioWsClients.size();
                 // --- Unlock Mutex ---
                 xSemaphoreGive(audioClientsMutex);
                 if (newSize < oldSize) {
                    Serial.printf("Client #%lu removed. Total audio clients: %d\n", client->id(), newSize);
                 } else {
                    Serial.printf("Client #%lu not found for removal? Total audio clients: %d\n", client->id(), newSize);
                 }
            } else {
                 Serial.println("ERR: Could not obtain audioClientsMutex for WS_EVT_DISCONNECT");
            }
            break;
        case WS_EVT_DATA:
            Serial.printf("WebSocket client #%lu sent data: %s\n", client->id(), (char*)data);
            break;
        case WS_EVT_PONG:
            // Serial.printf("WebSocket client #%lu pong\n", client->id());
            break;
        case WS_EVT_ERROR:
             Serial.printf("WebSocket client #%lu error(%u): %s\n", client->id(), *((uint16_t*)arg), (char*)data);
             // --- Lock Mutex ---
             if (xSemaphoreTake(audioClientsMutex, portMAX_DELAY) == pdTRUE) {
                 size_t oldSize = audioWsClients.size();
                 audioWsClients.erase(
                    std::remove_if(audioWsClients.begin(), audioWsClients.end(),
                                [client](AsyncWebSocketClient* c) { return c->id() == client->id(); }),
                    audioWsClients.end());
                 size_t newSize = audioWsClients.size();
                 // --- Unlock Mutex ---
                 xSemaphoreGive(audioClientsMutex);
                 if (newSize < oldSize) {
                    Serial.printf("Client #%lu removed due to error. Total audio clients: %d\n", client->id(), newSize);
                 } else {
                    Serial.printf("Client #%lu not found for removal on error? Total audio clients: %d\n", client->id(), newSize);
                 }
             } else {
                 Serial.println("ERR: Could not obtain audioClientsMutex for WS_EVT_ERROR");
             }
            break;
        case WS_EVT_PING:
            // Client sent a ping, library handles pong automatically
            // Serial.printf("WebSocket client #%lu ping\n", client->id());
            break;
        default:
            // Serial.printf("Unhandled WebSocket event type: %d for client #%lu\n", type, client->id());
            break; 
    }
} 