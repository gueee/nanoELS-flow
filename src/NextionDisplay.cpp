#include "NextionDisplay.h"

// Global instance
NextionDisplay nextionDisplay;

NextionDisplay::NextionDisplay() {
  currentState = DISPLAY_STATE_BOOT;
  lastUpdate = 0;
  displayTimeout = 100;  // 100ms update interval
  messageCount = 0;
  
  // Initialize splash screen (from original h5.ino)
  splashScreen = true;
  splashShown = false;
  splashStartTime = 0;
  
  // Initialize hash tracking (from original h5.ino)
  for (int i = 0; i < 4; i++) {
    lastHash[i] = LCD_HASH_INITIAL;
  }
}

void NextionDisplay::initialize() {
  Serial.println("Initializing Nextion display...");
  
  // Initialize Serial1 for Nextion communication (EXACTLY matching original h5.ino)
  // CRITICAL: Must use exact same pins as original - GPIO 44 (RX) and 43 (TX)
  Serial1.begin(115200, SERIAL_8N1, NEXTION_RX, NEXTION_TX);
  
  // CRITICAL: Original waits 1300ms for Nextion to boot
  // From original h5.ino: "Nextion needs time to boot or first display update will be ignored."
  Serial.println("Waiting for Nextion to boot (1300ms)...");
  delay(1300);  // This delay is MANDATORY - do not reduce!
  
  // Send a simple wakeup command to ensure communication
  toScreen("sleep=0");
  
  // Small additional delay after first command
  delay(100);
  
  // Set splash screen flag (like original reset() function) 
  splashScreen = true;
  splashShown = false;
  splashStartTime = millis();  // Set start time immediately
  
  Serial.println("✓ Nextion display initialized with proper 1300ms boot delay");
}

void NextionDisplay::toScreen(const String& command) {
  Serial1.print(command);
  Serial1.write(0xFF);
  Serial1.write(0xFF);
  Serial1.write(0xFF);
  
  // Debug output
  Serial.println("Nextion: " + command);
}

void NextionDisplay::setText(uint8_t id, const String& text) {
  if (hasChanged(id, text)) {
    // Match original h5.ino format exactly
    toScreen("t" + String(id) + ".txt=\"" + text + "\"");
  }
}

void NextionDisplay::screenClear() {
  for (int i = 0; i < 4; i++) {
    String command = "t" + String(i) + ".txt=\"\"";
    toScreen(command);
    lastHash[i] = 0;  // Reset hash tracking
  }
}

long NextionDisplay::calculateHash(const String& text) {
  long hash = 0;
  for (unsigned int i = 0; i < text.length(); i++) {
    hash = hash * 31 + text.charAt(i);
  }
  return hash;
}

bool NextionDisplay::hasChanged(uint8_t id, const String& text) {
  if (id >= 4) return false;
  
  long newHash = calculateHash(text);
  if (newHash != lastHash[id]) {
    lastHash[id] = newHash;
    return true;
  }
  return false;
}

void NextionDisplay::setState(DisplayState state) {
  if (currentState != state) {
    currentState = state;
    Serial.println("Display state changed to: " + String(state));
  }
}

DisplayState NextionDisplay::getState() {
  return currentState;
}

void NextionDisplay::setTopLine(const String& text, DisplayPriority priority) {
  setText(NEXTION_T0, text);
}

void NextionDisplay::setPitchLine(const String& text, DisplayPriority priority) {
  setText(NEXTION_T1, text);
}

void NextionDisplay::setPositionLine(const String& text, DisplayPriority priority) {
  setText(NEXTION_T2, text);
}

void NextionDisplay::setStatusLine(const String& text, DisplayPriority priority) {
  setText(NEXTION_T3, text);
}

void NextionDisplay::showWiFiStatus(const String& status, bool connecting) {
  if (connecting) {
    setTopLine("WiFi: Connecting...");
    setStatusLine(status);
  } else {
    setTopLine("WiFi: " + status);
  }
}

void NextionDisplay::showMotionStatus() {
  // Top line: Mode and status (matching original h5.ino format)
  extern int currentMode;
  extern float manualStepSize;
  String topLine = "";
  
  // Dummy values for clean display - no motion control
  bool emergencyStop = false;
  bool xMoving = false;
  bool zMoving = false;
  
  if (emergencyStop) {
    topLine = "EMERGENCY STOP";
  } else if (xMoving || zMoving) {
    topLine = "MOVING - ";
    switch(currentMode) {
      case 0: topLine += "Manual"; break;
      case 1: topLine += "Threading"; break;
      case 2: topLine += "Turning"; break;
      case 3: topLine += "Facing"; break;
      case 4: topLine += "Cone"; break;
      case 5: topLine += "Cutting"; break;
      default: topLine += "Mode " + String(currentMode); break;
    }
  } else {
    switch(currentMode) {
      case 0: topLine = "Manual Mode"; break;
      case 1: topLine = "Threading"; break;
      case 2: topLine = "Turning"; break;
      case 3: topLine = "Facing"; break;
      case 4: topLine = "Cone"; break;
      case 5: topLine = "Cutting"; break;
      default: topLine = "Mode " + String(currentMode); break;
    }
    topLine += " Step:" + String(manualStepSize) + "mm";
  }
  setTopLine(topLine);
  
  // Pitch line: Thread pitch info (matching original format)
  String pitchLine = "Pitch 1.25mm x1";  // Default values for now
  setPitchLine(pitchLine);
  
  // Position line: Axis positions (matching original format "Z:xxx.xx X:xxx.xx")
  // Get real position values from motion control
  float xPos = esp32Motion.getPosition(0);
  float zPos = esp32Motion.getPosition(1);
  String posLine = "Z:" + String(zPos, 2) + " X:" + String(xPos, 2);
  setPositionLine(posLine);
  
  // Status line: RPM and encoder info (matching original format)
  String statusLine = "";
  
  // Get real values from motion control
  int rpm = 0;
  int spindlePos = esp32Motion.getSpindlePosition();
  int xMPG = esp32Motion.getXMPGCount();
  int zMPG = esp32Motion.getZMPGCount();
  float xError = esp32Motion.getPositionError(0);
  float zError = esp32Motion.getPositionError(1);
  uint32_t xSteps = esp32Motion.getAxisStepCount(0);
  uint32_t zSteps = esp32Motion.getAxisStepCount(1);
  
  if (rpm > 0) {
    statusLine = String(rpm) + "rpm ";
  }
  statusLine += "Enc:" + String(spindlePos);
  statusLine += " X:" + String(xMPG) + "(" + String(xError, 2) + "/" + String(xSteps) + ")";
  statusLine += " Z:" + String(zMPG) + "(" + String(zError, 2) + "/" + String(zSteps) + ")";
  
  // Add test sequence status
  if (esp32Motion.isTestSequenceActive()) {
    statusLine += " TEST-RUNNING";
  } else if (esp32Motion.isTestSequenceCompleted()) {
    statusLine += " TEST-DONE";
  } else if (esp32Motion.getEmergencyStop()) {
    statusLine += " E-STOP";
  } else {
    statusLine += " READY";
  }
  
  setStatusLine(statusLine);
}

void NextionDisplay::showSystemStatus() {
  switch (currentState) {
    case DISPLAY_STATE_BOOT:
      setTopLine("nanoELS-flow H5");
      setStatusLine("Booting...");
      break;
      
    case DISPLAY_STATE_WIFI_CONNECTING:
      setTopLine("WiFi Setup");
      setStatusLine("Connecting...");
      break;
      
    case DISPLAY_STATE_NORMAL:
      showMotionStatus();
      break;
      
    case DISPLAY_STATE_EMERGENCY_STOP:
      showEmergencyStop();
      break;
      
    case DISPLAY_STATE_ERROR:
      setTopLine("SYSTEM ERROR");
      setStatusLine("Check Serial");
      break;
  }
}

void NextionDisplay::showError(const String& error) {
  setState(DISPLAY_STATE_ERROR);
  setTopLine("ERROR");
  setStatusLine(error);
}

void NextionDisplay::showEmergencyStop() {
  setState(DISPLAY_STATE_EMERGENCY_STOP);
  setTopLine("EMERGENCY STOP");
  setPitchLine("ACTIVE");
  setPositionLine("Press ENTER");
  setStatusLine("to release");
}

void NextionDisplay::showMessage(const String& message, uint8_t objectId, 
                                unsigned long duration, DisplayPriority priority) {
  if (messageCount < 8) {
    messageQueue[messageCount] = {message, objectId, priority, millis(), duration};
    messageCount++;
  }
}

void NextionDisplay::showBootScreen() {
  setState(DISPLAY_STATE_BOOT);
  setTopLine("nanoELS-flow H5");
  setPitchLine("ESP32-S3 Controller");
  setPositionLine("Initializing...");
  setStatusLine("Please wait");
}

void NextionDisplay::showInitProgress(const String& step) {
  setStatusLine(step);
}

void NextionDisplay::processMessageQueue() {
  unsigned long currentTime = millis();
  
  // Process temporary messages
  for (int i = 0; i < messageCount; i++) {
    DisplayMessage& msg = messageQueue[i];
    
    if (msg.duration > 0 && (currentTime - msg.timestamp) > msg.duration) {
      // Message expired, remove it
      for (int j = i; j < messageCount - 1; j++) {
        messageQueue[j] = messageQueue[j + 1];
      }
      messageCount--;
      i--; // Adjust index after removal
    }
  }
}

void NextionDisplay::update() {
  // Handle splash screen timing (from original h5.ino)
  if (splashScreen) {
    if (splashStartTime == 0) {
      splashStartTime = millis();
    }
    
    if (millis() - splashStartTime < 100) {
      return; // Wait a bit before showing splash
    }
    
    // Show splash screen once
    if (millis() - splashStartTime < 2000) {
      static bool splashShown = false;
      if (!splashShown) {
        screenClear();
        setText(0, "NanoEls H5 TFT20250104");  // Match original splash message
        splashShown = true;
      }
      return; // Stay in splash for 2 seconds
    }
    
    // End splash screen
    splashScreen = false;
    screenClear();
    
    // Reset all hash values (from original h5.ino)
    for (int i = 0; i < 4; i++) {
      lastHash[i] = LCD_HASH_INITIAL;
    }
    
    // Transition to normal state
    currentState = DISPLAY_STATE_NORMAL;
    Serial.println("Splash screen complete, transitioning to normal display");
  }
  
  unsigned long currentTime = millis();
  
  if (currentTime - lastUpdate >= displayTimeout) {
    processMessageQueue();
    
    // Update display based on current state
    if (currentState == DISPLAY_STATE_NORMAL) {
      showSystemStatus();
    }
    
    lastUpdate = currentTime;
  }
}

void NextionDisplay::clearAll() {
  screenClear();
}

void NextionDisplay::setBrightness(uint8_t brightness) {
  if (brightness > 100) brightness = 100;
  String command = "dim=" + String(brightness);
  toScreen(command);
}

void NextionDisplay::wakeUp() {
  toScreen("sleep=0");
}

void NextionDisplay::sleep() {
  toScreen("sleep=1");
}