#include "WebInterface.h"

// Global instance
WebInterface webInterface;

WebInterface::WebInterface() {
  webServer = nullptr;
  webSocket = nullptr;
  wifiConnected = false;
  webServerRunning = false;
}

WebInterface::~WebInterface() {
  stopWebServer();
}

bool WebInterface::initializeWiFi(const char* ssid, const char* password) {
  Serial.println("Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  
  // Update Nextion display
  nextionDisplay.setState(DISPLAY_STATE_WIFI_CONNECTING);
  nextionDisplay.showWiFiStatus("Connecting to " + String(ssid), true);
  
  // Common fixes for home WiFi connection issues:
  
  // 1. Disconnect any previous connections
  WiFi.disconnect(true);
  delay(1000);
  
  // 2. Set WiFi mode explicitly to station mode
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // 3. Disable power saving mode (can cause connection issues)
  WiFi.setSleep(false);
  
  // 4. Set hostname for network identification
  WiFi.setHostname("nanoELS-H5");
  
  // 5. Enable auto-reconnect
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  
  // 6. Optional: Set static DNS servers (helps with some routers)
  // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8,8,8,8), IPAddress(8,8,4,4));
  
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Start connection
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) { // Increased timeout
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Print detailed status every 10 attempts
    if (attempts % 10 == 0) {
      Serial.println();
      Serial.print("WiFi Status: ");
      String statusText = "";
      
      switch (WiFi.status()) {
        case WL_IDLE_STATUS:     
          Serial.print("IDLE"); 
          statusText = "Initializing...";
          break;
        case WL_NO_SSID_AVAIL:   
          Serial.print("NO_SSID_AVAIL"); 
          statusText = "Network not found";
          break;
        case WL_SCAN_COMPLETED:  
          Serial.print("SCAN_COMPLETED"); 
          statusText = "Scanning...";
          break;
        case WL_CONNECTED:       
          Serial.print("CONNECTED"); 
          statusText = "Connected!";
          break;
        case WL_CONNECT_FAILED:  
          Serial.print("CONNECT_FAILED"); 
          statusText = "Auth failed";
          break;
        case WL_CONNECTION_LOST: 
          Serial.print("CONNECTION_LOST"); 
          statusText = "Connection lost";
          break;
        case WL_DISCONNECTED:    
          Serial.print("DISCONNECTED"); 
          statusText = "Disconnected";
          break;
        default:                 
          Serial.print("UNKNOWN"); 
          statusText = "Status unknown";
          break;
      }
      Serial.print(" (");
      Serial.print(WiFi.status());
      Serial.println(")");
      
      // Update display with current status
      nextionDisplay.showWiFiStatus(statusText, true);
      
      // If connection failed, try again
      if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_NO_SSID_AVAIL) {
        Serial.println("Retrying connection...");
        nextionDisplay.showWiFiStatus("Retrying...", true);
        WiFi.disconnect();
        delay(1000);
        WiFi.begin(ssid, password);
      }
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println();
    Serial.println("✓ WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    // Update display with success
    nextionDisplay.showWiFiStatus("Connected!", false);
    nextionDisplay.showMessage("IP: " + WiFi.localIP().toString(), NEXTION_T3, 5000);
    nextionDisplay.setState(DISPLAY_STATE_NORMAL);
    
    return true;
  } else {
    Serial.println();
    Serial.println("✗ Failed to connect to WiFi");
    Serial.print("Final status: ");
    Serial.println(WiFi.status());
    
    // Update display with failure
    nextionDisplay.showWiFiStatus("Failed", false);
    nextionDisplay.showMessage("Check credentials", NEXTION_T3, 5000);
    
    // Print available networks for debugging
    Serial.println("Available networks:");
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      Serial.printf("%d: %s (%d dBm) %s\n", 
                    i + 1, 
                    WiFi.SSID(i).c_str(), 
                    WiFi.RSSI(i),
                    (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured");
    }
    
    return false;
  }
}

bool WebInterface::startAccessPoint(const char* ssid, const char* password) {
  Serial.println("Starting WiFi Access Point...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  
  // Update display
  nextionDisplay.setState(DISPLAY_STATE_WIFI_CONNECTING);
  nextionDisplay.showWiFiStatus("Starting AP...", true);
  
  bool success = WiFi.softAP(ssid, password);
  
  if (success) {
    wifiConnected = true;
    Serial.println("✓ Access Point started!");
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
    
    // Update display with success
    nextionDisplay.showWiFiStatus("AP Ready", false);
    nextionDisplay.showMessage("Connect to: " + String(ssid), NEXTION_T2, 5000);
    nextionDisplay.showMessage("IP: " + WiFi.softAPIP().toString(), NEXTION_T3, 5000);
    nextionDisplay.setState(DISPLAY_STATE_NORMAL);
    
    return true;
  } else {
    Serial.println("✗ Failed to start Access Point");
    
    // Update display with failure
    nextionDisplay.showWiFiStatus("AP Failed", false);
    nextionDisplay.showMessage("Check hardware", NEXTION_T3, 5000);
    
    return false;
  }
}

bool WebInterface::startWebServer() {
  if (!wifiConnected) {
    Serial.println("ERROR: WiFi not connected, cannot start web server");
    return false;
  }
  
  // Initialize LittleFS for GCode storage
  if (!LittleFS.begin()) {
    Serial.println("LittleFS initialization failed");
    return false;
  }
  
  // Create web server and WebSocket server
  webServer = new WebServer(80);
  webSocket = new WebSocketsServer(81);
  
  // Set up web server routes
  webServer->on("/", [this]() { handleRoot(); });
  webServer->on("/status", [this]() { handleStatus(); });
  webServer->on("/gcode/list", [this]() { handleGCodeList(); });
  webServer->on("/gcode/get", [this]() { handleGCodeGet(); });
  webServer->on("/gcode/add", HTTP_POST, [this]() { handleGCodeAdd(); });
  webServer->on("/gcode/remove", HTTP_POST, [this]() { handleGCodeRemove(); });
  webServer->onNotFound([this]() { handleNotFound(); });
  
  // Set up WebSocket event handler
  webSocket->onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    webSocketEvent(num, type, payload, length);
  });
  
  // Start servers
  webServer->begin();
  webSocket->begin();
  
  webServerRunning = true;
  Serial.println("✓ Web server started on port 80");
  Serial.println("✓ WebSocket server started on port 81");
  
  return true;
}

void WebInterface::stopWebServer() {
  if (webServer) {
    webServer->stop();
    delete webServer;
    webServer = nullptr;
  }
  
  if (webSocket) {
    webSocket->close();
    delete webSocket;
    webSocket = nullptr;
  }
  
  webServerRunning = false;
  Serial.println("Web server stopped");
}

void WebInterface::update() {
  if (webServerRunning) {
    webServer->handleClient();
    webSocket->loop();
  }
}

// Web server route handlers
void WebInterface::handleRoot() {
  webServer->send_P(200, "text/html", indexhtml);
}

void WebInterface::handleStatus() {
  String status = getStatusInfo();
  webServer->send(200, "text/plain", status);
}

void WebInterface::handleGCodeList() {
  String gcodeList = listGCodeFiles();
  webServer->send(200, "text/plain", gcodeList);
}

void WebInterface::handleGCodeGet() {
  if (webServer->hasArg("name")) {
    String name = urlDecode(webServer->arg("name"));
    String content = loadGCodeFile(name);
    if (content.length() > 0) {
      webServer->send(200, "text/plain", content);
    } else {
      webServer->send(404, "text/plain", "GCode file not found");
    }
  } else {
    webServer->send(400, "text/plain", "Missing name parameter");
  }
}

void WebInterface::handleGCodeAdd() {
  if (webServer->hasArg("name") && webServer->hasArg("gcode")) {
    String name = urlDecode(webServer->arg("name"));
    String content = urlDecode(webServer->arg("gcode"));
    
    if (saveGCodeFile(name, content)) {
      webServer->send(200, "text/plain", "GCode saved successfully: " + name);
    } else {
      webServer->send(500, "text/plain", "Failed to save GCode");
    }
  } else {
    webServer->send(400, "text/plain", "Missing name or gcode parameter");
  }
}

void WebInterface::handleGCodeRemove() {
  if (webServer->hasArg("name")) {
    String name = urlDecode(webServer->arg("name"));
    
    if (deleteGCodeFile(name)) {
      webServer->send(200, "text/plain", "GCode removed successfully: " + name);
    } else {
      webServer->send(500, "text/plain", "Failed to remove GCode");
    }
  } else {
    webServer->send(400, "text/plain", "Missing name parameter");
  }
}

void WebInterface::handleNotFound() {
  webServer->send(404, "text/plain", "File not found");
}

// WebSocket event handler
void WebInterface::webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("WebSocket[%u] Disconnected\n", num);
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket->remoteIP(num);
        Serial.printf("WebSocket[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        
        // Send welcome message
        String welcome = "Connected to nanoELS-flow H5";
        webSocket->sendTXT(num, welcome);
        
        // Send current status
        String status = motionControl.getStatus();
        webSocket->sendTXT(num, status);
      }
      break;
      
    case WStype_TEXT:
      {
        String command = String((char*)payload);
        command.trim();
        Serial.printf("WebSocket[%u] received: %s\n", num, command.c_str());
        
        processWebSocketCommand(command);
        
        // Echo command back to all clients
        String response = "Processed: " + command;
        String responseCopy = response;
        webSocket->broadcastTXT(responseCopy);
      }
      break;
      
    case WStype_BIN:
      Serial.printf("WebSocket[%u] received binary data\n", num);
      break;
      
    default:
      break;
  }
}

void WebInterface::processWebSocketCommand(String command) {
  lastCommand = command;
  
  if (command == "?") {
    // Status request
    String status = motionControl.getStatus();
    String statusMsg = "Status: " + status;
    webSocket->broadcastTXT(statusMsg);
    
  } else if (command.startsWith("=")) {
    // Key code simulation
    int keyCode = command.substring(1).toInt();
    Serial.printf("Simulating key press: %d\n", keyCode);
    // TODO: Implement key simulation
    String keyMsg = "Key simulated: " + String(keyCode);
    webSocket->broadcastTXT(keyMsg);
    
  } else if (command == "!") {
    // Emergency stop
    motionControl.setEmergencyStop(true);
    String stopMsg = "Emergency stop activated";
    webSocket->broadcastTXT(stopMsg);
    
  } else if (command == "~") {
    // Release emergency stop
    motionControl.setEmergencyStop(false);
    String releaseMsg = "Emergency stop released";
    webSocket->broadcastTXT(releaseMsg);
    
  } else if (command == "\"\"") {
    // Remove all GCode files
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    int count = 0;
    while (file) {
      if (String(file.name()).endsWith(".gcode")) {
        LittleFS.remove(file.path());
        count++;
      }
      file = root.openNextFile();
    }
    String removeMsg = "Removed " + String(count) + " GCode files";
    webSocket->broadcastTXT(removeMsg);
    
  } else if (command.startsWith("X") || command.startsWith("Z")) {
    // Motion commands
    char axis = command.charAt(0);
    int steps = command.substring(1).toInt();
    uint8_t axisNum = (axis == 'X') ? 0 : 1;
    
    motionControl.moveRelative(axisNum, steps);
    String moveMsg = "Moving " + String(axis) + " axis " + String(steps) + " steps";
    webSocket->broadcastTXT(moveMsg);
    
  } else {
    // Unknown command
    String unknownMsg = "Unknown command: " + command;
    webSocket->broadcastTXT(unknownMsg);
  }
}

// GCode file management
bool WebInterface::saveGCodeFile(const String& name, const String& content) {
  String filename = "/" + name + ".gcode";
  File file = LittleFS.open(filename, "w");
  
  if (!file) {
    Serial.println("Failed to open file for writing: " + filename);
    return false;
  }
  
  size_t bytesWritten = file.print(content);
  file.close();
  
  Serial.printf("Saved GCode file: %s (%d bytes)\n", filename.c_str(), bytesWritten);
  return bytesWritten > 0;
}

String WebInterface::loadGCodeFile(const String& name) {
  String filename = "/" + name + ".gcode";
  File file = LittleFS.open(filename, "r");
  
  if (!file) {
    Serial.println("Failed to open file for reading: " + filename);
    return "";
  }
  
  String content = file.readString();
  file.close();
  
  return content;
}

bool WebInterface::deleteGCodeFile(const String& name) {
  String filename = "/" + name + ".gcode";
  bool success = LittleFS.remove(filename);
  
  if (success) {
    Serial.println("Deleted GCode file: " + filename);
  } else {
    Serial.println("Failed to delete file: " + filename);
  }
  
  return success;
}

String WebInterface::listGCodeFiles() {
  String fileList = "";
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  while (file) {
    String fileName = file.name();
    if (fileName.endsWith(".gcode")) {
      // Remove .gcode extension and leading slash
      fileName = fileName.substring(1, fileName.length() - 6);
      if (fileList.length() > 0) {
        fileList += "\n";
      }
      fileList += fileName;
    }
    file = root.openNextFile();
  }
  
  return fileList;
}

String WebInterface::getStatusInfo() {
  String info = "";
  info += "WiFi.status=" + String(WiFi.status()) + "\n";
  info += "WiFi.localIP=" + WiFi.localIP().toString() + "\n";
  info += "LittleFS.totalBytes=" + String(LittleFS.totalBytes()) + "\n";
  info += "LittleFS.usedBytes=" + String(LittleFS.usedBytes()) + "\n";
  info += "LittleFS.freeSpace=" + String(LittleFS.totalBytes() - LittleFS.usedBytes()) + "\n";
  info += "MotionControl.status=" + motionControl.getStatus() + "\n";
  info += "LastCommand=" + lastCommand + "\n";
  
  return info;
}

String WebInterface::urlDecode(String str) {
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = str.length();
  unsigned int i = 0;
  
  while (i < len) {
    char decodedChar;
    char encodedChar = str.charAt(i++);
    
    if ((encodedChar == '%') && (i + 1 < len)) {
      temp[2] = str.charAt(i++);
      temp[3] = str.charAt(i++);
      decodedChar = strtol(temp, NULL, 16);
    } else if (encodedChar == '+') {
      decodedChar = ' ';
    } else {
      decodedChar = encodedChar;
    }
    
    decoded += decodedChar;
  }
  
  return decoded;
}

bool WebInterface::isWiFiConnected() {
  return wifiConnected && (WiFi.status() == WL_CONNECTED || WiFi.softAPgetStationNum() > 0);
}

bool WebInterface::isWebServerRunning() {
  return webServerRunning;
}

String WebInterface::getIPAddress() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  } else {
    return WiFi.softAPIP().toString();
  }
}

void WebInterface::broadcastMessage(const String& message) {
  if (webSocket && webServerRunning) {
    // Create a non-const copy for WebSocket library compatibility
    String messageCopy = message;
    webSocket->broadcastTXT(messageCopy);
  }
}

void WebInterface::sendMotionStatus() {
  if (webSocket && webServerRunning) {
    String status = "Motion: X=" + String(motionControl.getPosition(0)) + 
                   " Z=" + String(motionControl.getPosition(1)) + 
                   " RPM=" + String(motionControl.getSpindleRPM());
    String statusCopy = status;
    webSocket->broadcastTXT(statusCopy);
  }
}