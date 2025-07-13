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
  splashStartTime = 0;
  splashShown = false;  // Initialize splash display tracking
  
  // Initialize hash tracking (from original h5.ino)
  for (int i = 0; i < 4; i++) {
    lastHash[i] = LCD_HASH_INITIAL;
  }
}

void NextionDisplay::initialize() {
  Serial.println("Initializing Nextion display...");
  
  // Initialize Serial1 for Nextion communication (EXACTLY matching original h5.ino)
  // Original uses pins 44 (RX) and 43 (TX) but let's try default pins first
  Serial1.begin(115200, SERIAL_8N1);
  
  // CRITICAL: Original waits 1300ms for Nextion to boot
  Serial.println("Waiting for Nextion to boot (1300ms)...");
  delay(1300);  // Nextion needs time to boot or first display update will be ignored.
  
  // Set splash screen flag (like original reset() function)
  splashScreen = true;
  splashShown = false;
  splashStartTime = 0;  // Will be set on first update() call
  
  Serial.println("✓ Nextion display initialized - ready for commands");
}

void NextionDisplay::toScreen(const String& command) {
  // EXACT format from original h5.ino
  Serial1.print(command);
  Serial1.write(0xFF);
  Serial1.write(0xFF);
  Serial1.write(0xFF);
  
  // Debug output (but no delays like original)
  Serial.println("Nextion: " + command);
}

void NextionDisplay::setText(uint8_t id, const String& text) {
  if (hasChanged(id, text)) {
    // EXACT format from original h5.ino: setText("t0", text) becomes t0.txt="text"
    toScreen("t" + String(id) + ".txt=\"" + text + "\"");
  }
}

void NextionDisplay::screenClear() {
  // EXACT implementation from original h5.ino - just clear text fields
  setText(0, "");
  setText(1, "");
  setText(2, "");
  setText(3, "");
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
  // Top line: System status
  String topLine = "nanoELS-H5 Ready";
  if (motionControl.getEmergencyStop()) {
    topLine = "EMERGENCY STOP";
  } else if (motionControl.isMoving()) {
    topLine = "MOVING";
  }
  setTopLine(topLine);
  
  // Pitch line: Current mode and step size
  extern int currentMode;
  extern int stepSize;
  String pitchLine = "Mode:" + String(currentMode) + " Step:" + String(stepSize);
  setPitchLine(pitchLine);
  
  // Position line: Axis positions
  String posLine = "X:" + String(motionControl.getPosition(0)) + 
                   " Z:" + String(motionControl.getPosition(1));
  setPositionLine(posLine);
  
  // Status line: Spindle info
  String statusLine = "RPM:" + String(motionControl.getSpindleRPM()) + 
                     " Enc:" + String(motionControl.getSpindlePosition());
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
  // EXACT splash screen logic from original h5.ino
  if (splashScreen) {
    splashScreen = false;  // Clear flag immediately (like original)
    screenClear();         // Clear all text fields
    setText(0, "NanoEls H5 V9");  // Match original format exactly
    
    // Reset all hash values (from original h5.ino)
    for (int i = 0; i < 4; i++) {
      lastHash[i] = LCD_HASH_INITIAL;
    }
    
    Serial.println("Showing splash screen for 2 seconds");
    delay(2000);  // Show splash for 2 seconds (like original)
    
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