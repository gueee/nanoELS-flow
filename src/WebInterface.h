#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <FS.h>
#include "indexhtml.h"
#include "ESP32MotionControl.h"
#include "NextionDisplay.h"

class WebInterface {
private:
  WebServer* webServer;
  WebSocketsServer* webSocket;
  bool wifiConnected;
  bool webServerRunning;
  String lastCommand;
  
  // Web server route handlers
  void handleRoot();
  void handleStatus();
  void handleGCodeList();
  void handleGCodeGet();
  void handleGCodeAdd();
  void handleGCodeRemove();
  void handleNotFound();
  
  // WebSocket handlers
  void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
  void processWebSocketCommand(String command);
  
  // GCode file management
  bool saveGCodeFile(const String& name, const String& content);
  String loadGCodeFile(const String& name);
  bool deleteGCodeFile(const String& name);
  String listGCodeFiles();
  
  // Utility functions
  String urlDecode(String str);
  String getStatusInfo();
  
public:
  WebInterface();
  ~WebInterface();
  
  // Initialization and control
  bool initializeWiFi(const char* ssid, const char* password);
  bool startAccessPoint(const char* ssid, const char* password = nullptr);
  bool startWebServer();
  void stopWebServer();
  
  // Main update function
  void update();
  
  // Status and control
  bool isWiFiConnected();
  bool isWebServerRunning();
  String getIPAddress();
  void broadcastMessage(const String& message);
  
  // Command interface
  void processCommand(const String& command);
  void sendMotionStatus();
};

// Global web interface instance
extern WebInterface webInterface;

#endif // WEBINTERFACE_H