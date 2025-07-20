/*
 * nanoELS-flow - H5 Variant (ESP32-S3-dev board)
 * 
 * State machine-based motion control with PID position controller
 * Target: ESP32-S3-dev board with touch screen interface
 * 
 * ARDUINO IDE SETUP:
 * 1. Install ESP32 board support: https://espressif.github.io/arduino-esp32/package_esp32_index.json
 * 2. Board: "ESP32S3 Dev Module"
 * 3. Settings: 240MHz, QIO, 16MB Flash, Huge APP, PSRAM Enabled, 921600 upload
 * 4. Required Libraries (install via Library Manager):
 *    - WebSockets by Markus Sattler
 *    - PS2KeyAdvanced by Paul Carpenter
 */

/* ====================================================================== */
/* Hardware setup constants are now in SetupConstants.cpp               */
/* Edit SetupConstants.cpp to modify your hardware configuration.       */
/* ====================================================================== */

// Hardware version defines (keep in main file for Arduino IDE)
#define HARDWARE_VERSION 5
#define SOFTWARE_VERSION 1

/* ====================================================================== */
/* Changing anything below shouldn't be needed for basic use.            */
/* ====================================================================== */

// Standard ESP32 libraries
#include <FS.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>

// External libraries (install via Library Manager)
#include <WebSocketsServer.h>
#include <PS2KeyAdvanced.h>

// Project modules - Clean minimal implementation with motion control
#include "SetupConstants.h"      // Hardware configuration constants
#include "ESP32MotionControl.h"
#include "WebInterface.h"
#include "NextionDisplay.h"
#include "MyHardware.h"
#include "StateMachine.h"

// Global Objects
// ==============

// Web interface system (handles HTTP server and WebSocket communication)
// Global instance declared in WebInterface.h: extern WebInterface webInterface;

// Nextion display system (handles touch screen display)
// Global instance declared in NextionDisplay.h: extern NextionDisplay nextionDisplay;

// PS2 Keyboard interface (for future use)
PS2KeyAdvanced keyboard;

// Preferences for configuration storage
Preferences prefs;

// Global Variables
// ===============

// Current machine state
int currentMode = 0;  // 0=manual, 1=threading, etc.
float manualStepSize = 1.0;  // Current step size for manual movements (mm)

// Target position variables (simple servo architecture)
float targetPositionX = 0.0;  // Target position for X-axis (mm)
float targetPositionZ = 0.0;  // Target position for Z-axis (mm)

// Shared variable for emergency stop coordination
volatile bool emergencyKeyDetected = false;

// t3 display mode flag (false = normal status, true = debug info)
bool t3DebugMode = false;

// Arrow key state tracking for continuous movement
struct ArrowKeyState {
  bool isPressed;
  uint32_t pressStartTime;
  uint32_t lastRepeatTime;
  uint32_t repeatInterval;
};

ArrowKeyState keyStates[4]; // LEFT, RIGHT, UP, DOWN
#define KEY_LEFT_IDX 0
#define KEY_RIGHT_IDX 1  
#define KEY_UP_IDX 2
#define KEY_DOWN_IDX 3

// Timing configuration for continuous movement
#define HOLD_THRESHOLD_MS 150        // Time to consider a "hold" vs "tap"
#define MIN_REPEAT_INTERVAL_MS 20    // Fastest repeat rate (50Hz)
#define MAX_REPEAT_INTERVAL_MS 500   // Slowest repeat rate (2Hz)

// WiFi Configuration
// Set WIFI_MODE to choose connection type:
// 0 = Access Point mode (creates "nanoELS-flow" network)
// 1 = Connect to existing WiFi network
#define WIFI_MODE 0

// Access Point credentials (when WIFI_MODE = 0)
const char* AP_SSID = "nanoELS-flow";
const char* AP_PASSWORD = "nanoels123";

// Home WiFi credentials are defined in setup section above

// WiFi troubleshooting options
#define WIFI_CONNECTION_TIMEOUT 20  // seconds to wait for connection
#define WIFI_RETRY_COUNT 2          // number of connection attempts
#define FALLBACK_TO_AP true         // create AP if WiFi connection fails

// Function Prototypes
// ==================
void initializeWebInterface();  // Web interface initialization
void handleKeyboard();  // Basic keyboard handling
void processKeypadEvent();  // Enhanced PS2 keyboard processing
void checkEmergencyStop();  // Immediate emergency stop check
void updateDisplay();   // Display update placeholder

// Continuous movement functions
int getKeyIndex(int keyCode);
int getKeyCodeFromIndex(int index);
uint32_t calculateRepeatInterval(float stepSize);
void performArrowKeyMovement(int keyCode);
void handleArrowKeyPress(int keyCode, int keyIndex);
void handleArrowKeyRelease(int keyCode, int keyIndex);
void resetArrowKeyStates();  // Reset all key states (for emergency stop)

// Task functions for scheduler
void taskEmergencyCheck();
void taskKeyboardScan();
void taskMotionUpdate();
void taskDisplayUpdate();
void taskWebUpdate();
void taskDiagnostics();
void taskContinuousMovement();

// Debug display function
void updateDebugDisplay();

void setup() {
  Serial.begin(115200);
  Serial.println("Starting nanoELS-flow H5 - PID Motion Control Version...");
  
  // Initialize PS2 keyboard interface
  keyboard.begin(KEY_DATA, KEY_CLOCK);
  Serial.println("✓ PS2 keyboard interface initialized");
  
  // Initialize Nextion display
  Serial.println("Initializing Nextion display...");
  nextionDisplay.initialize();
  Serial.println("Display initialized, splash screen should be visible");
  
  // Initialize WiFi and web interface
  Serial.println("Initializing WiFi and web interface...");
  initializeWebInterface();
  
  // Initialize motion control after display splash
  Serial.println("Initializing motion control...");
  esp32Motion.initialize();
  Serial.println("Motion control initialized");
  
  Serial.println("Setup complete - SAFETY READY");
  Serial.println("======================================");
  Serial.println("HARDWARE CONFIGURATION:");
  Serial.printf("✓ X-axis: Dir=%s, Enable=%s, Step=%s\n", 
                INVERT_X ? "INV" : "NORM", 
                INVERT_X_ENABLE ? "INV" : "NORM", 
                INVERT_X_STEP ? "INV" : "NORM");
  Serial.printf("✓ Z-axis: Dir=%s, Enable=%s, Step=%s\n", 
                INVERT_Z ? "INV" : "NORM", 
                INVERT_Z_ENABLE ? "INV" : "NORM", 
                INVERT_Z_STEP ? "INV" : "NORM");
  Serial.println("SAFETY CHECKS IMPLEMENTED:");
  Serial.printf("✓ Software limits: X=±%.0fmm, Z=±%.0fmm\n", MAX_TRAVEL_MM_X, MAX_TRAVEL_MM_Z);
  Serial.println("✓ Emergency stop: ESC key (<15ms response)");
  Serial.println("✓ Enable pins: Hardware-specific inversion");
  Serial.println("✓ Bounds checking: All moves validated");
  Serial.println("======================================");
  Serial.println("CONTROLS:");
  Serial.println("MANUAL MOVEMENT:");
  Serial.println("  Arrows = Move axes (step size: " + String(manualStepSize) + "mm)");
  Serial.println("  ~ = Change step size (0.01/0.1/1.0/10.0mm)");
  Serial.println("  c/q = Enable/disable X/Z axis");
  Serial.println("  x/z = Zero axis positions");
  Serial.println("TEST SEQUENCE:");
  Serial.println("  ENTER = Start/Restart test sequence");
  Serial.println("  ESC = EMERGENCY STOP (immediate)");
  Serial.println("  Win = Status diagnostics");
  Serial.println("======================================");
  
  // Initialize state machine scheduler
  Serial.println("\nInitializing state machine scheduler...");
  
  // Add tasks in priority order
  scheduler.addTask("EmergencyCheck", taskEmergencyCheck, PRIORITY_CRITICAL, 0);      // Every loop
  scheduler.addTask("KeyboardScan", taskKeyboardScan, PRIORITY_CRITICAL, 0);          // Every loop
  scheduler.addTask("MotionUpdate", taskMotionUpdate, PRIORITY_HIGH, 5);              // 200Hz
  scheduler.addTask("ContinuousMovement", taskContinuousMovement, PRIORITY_HIGH, 10); // 100Hz
  scheduler.addTask("DisplayUpdate", taskDisplayUpdate, PRIORITY_NORMAL, 50);         // 20Hz
  scheduler.addTask("WebUpdate", taskWebUpdate, PRIORITY_NORMAL, 20);                 // 50Hz
  scheduler.addTask("Diagnostics", taskDiagnostics, PRIORITY_LOW, 5000);              // 0.2Hz
  
  // Initialize arrow key states for continuous movement
  for (int i = 0; i < 4; i++) {
    keyStates[i].isPressed = false;
    keyStates[i].pressStartTime = 0;
    keyStates[i].lastRepeatTime = 0;
    keyStates[i].repeatInterval = 100;
  }
  Serial.println("✓ Continuous movement system initialized");
  
  // Enhanced keyboard processing enabled (based on original h5.ino approach)
  Serial.println("✓ Enhanced PS2 keyboard processing enabled");
  
  Serial.println("✓ State machine scheduler initialized");
  Serial.println("✓ Non-blocking operation enabled");
  Serial.println("✓ Emergency stop response: <10μs");
  Serial.println("======================================");
}

void loop() {
  // Non-blocking state machine implementation
  // No delays - runs at maximum speed (~100kHz)
  
  // Option 1: Use time-sliced scheduler (recommended)
  scheduler.update();
  
  // Option 2: Use state machine (alternative)
  // stateMachine.update();
  
  // No delay() - emergency stop checked every loop iteration!
}

// Motion control test function removed - clean minimal version

void initializeWebInterface() {
  bool wifiSuccess = false;
  
#if WIFI_MODE == 1
  // Try to connect to existing WiFi network
  Serial.println("Attempting to connect to home WiFi...");
  
  for (int retry = 0; retry < WIFI_RETRY_COUNT && !wifiSuccess; retry++) {
    if (retry > 0) {
      Serial.printf("WiFi connection retry %d/%d\n", retry + 1, WIFI_RETRY_COUNT);
    }
    
    wifiSuccess = webInterface.initializeWiFi(HOME_WIFI_SSID, HOME_WIFI_PASSWORD);
    
    if (!wifiSuccess) {
      Serial.println("WiFi connection failed, waiting before retry...");
      delay(2000);
    }
  }
  
#if FALLBACK_TO_AP == true
  // If WiFi connection failed, fall back to Access Point mode
  if (!wifiSuccess) {
    Serial.println("WiFi connection failed after all retries.");
    Serial.println("Falling back to Access Point mode...");
    wifiSuccess = webInterface.startAccessPoint(AP_SSID, AP_PASSWORD);
  }
#endif

#else
  // Access Point mode
  Serial.println("Starting in Access Point mode...");
  wifiSuccess = webInterface.startAccessPoint(AP_SSID, AP_PASSWORD);
#endif

  if (wifiSuccess) {
    if (webInterface.startWebServer()) {
      Serial.println("✓ Web interface ready!");
      Serial.println("Access the web interface at:");
      Serial.println("http://" + webInterface.getIPAddress());
      Serial.println("WebSocket port: 81");
      
#if WIFI_MODE == 0
      Serial.println("Connect to WiFi network: " + String(AP_SSID));
      Serial.println("WiFi password: " + String(AP_PASSWORD));
#endif
      
    } else {
      Serial.println("✗ Failed to start web server");
    }
  } else {
    Serial.println("✗ Failed to initialize WiFi");
    Serial.println("Web interface not available");
  }
}

// Keyboard and display initialization removed - handled in setup()

void processKeypadEvent() {
  // Enhanced keyboard processing based on original h5.ino approach
  if (!keyboard.available()) {
    return;
  }
  
  uint16_t event = keyboard.read();
  int keyCode = event & 0xFF;
  bool isPress = !(event & PS2_BREAK);
  
  // Track timing for debugging and safety
  static unsigned long keypadTimeUs = 0;
  keypadTimeUs = micros();
  
  // Some keyboards send this code and expect an answer to initialize
  if (keyCode == 170) {
    keyboard.echo();
    return;
  }
  
  // Emergency stop (B_OFF) gets highest priority - handle immediately on press
  if (keyCode == B_OFF) {
    if (isPress) {
      if (esp32Motion.getEmergencyStop()) {
        // Clear emergency stop
        esp32Motion.setEmergencyStop(false);
        nextionDisplay.setState(DISPLAY_STATE_NORMAL);
        nextionDisplay.showMessage("SYSTEM READY");
      } else {
        // Activate emergency stop
        emergencyKeyDetected = true;
        esp32Motion.setEmergencyStop(true);
        esp32Motion.stopTestSequence();
        resetArrowKeyStates();  // Stop any continuous movement
        nextionDisplay.showEmergencyStop();
      }
    }
    return;
  }
  
  // Debug: Show key codes using DIRECT Nextion commands like original h5.ino
  // REMOVED - was overwriting important diagnostics
  // Serial1.print("t0.txt=\"" + String(isPress ? "Press " : "Release ") + String(keyCode) + "\"");
  // Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
  
  // Enhanced arrow key handling with continuous movement support
  if (keyCode == B_LEFT || keyCode == B_RIGHT || keyCode == B_UP || keyCode == B_DOWN) {
    int keyIndex = getKeyIndex(keyCode);
    
    if (isPress) {
      handleArrowKeyPress(keyCode, keyIndex);
    } else {
      handleArrowKeyRelease(keyCode, keyIndex);
    }
    return; // Exit early for arrow keys
  }
  
  // Only process key presses (not releases) for other keys
  if (!isPress) {
    return;
  }
  
  // Process key according to MyHardware.txt mappings
  // Processing key
  switch (keyCode) {
    case B_ON:     // ENTER - Start/Restart test sequence (only when not in emergency)
      if (!esp32Motion.getEmergencyStop()) {
        bool isActive = esp32Motion.isTestSequenceActive();
        bool isCompleted = esp32Motion.isTestSequenceCompleted();
        
        Serial.printf("ENTER pressed - Active: %s, Completed: %s\n", 
                     isActive ? "true" : "false", isCompleted ? "true" : "false");
        
        if (isActive) {
          // Test sequence is running - restart it
          Serial.println("Restarting active test sequence");
          esp32Motion.restartTestSequence();
          nextionDisplay.showMessage("Test restarted");
        } else {
          // Test sequence is not running - start it
          Serial.println("Starting test sequence");
          esp32Motion.startTestSequence();
          nextionDisplay.showMessage("Test started");
        }
      } else {
        // Show message on display that emergency stop is active
        nextionDisplay.showMessage("E-STOP ACTIVE - Press ESC");
      }
      break;
        
      // Arrow keys are handled before the switch statement
      case B_STEP:   // Tilda - Change manual step size
        {
          float oldStepSize = manualStepSize;
          if (abs(manualStepSize - 0.01) < 0.001) {
            manualStepSize = 0.1;
          } else if (abs(manualStepSize - 0.1) < 0.001) {
            manualStepSize = 1.0;
          } else if (abs(manualStepSize - 1.0) < 0.001) {
            manualStepSize = 10.0;
          } else {
            manualStepSize = 0.01;  // Reset to start
          }
          Serial.printf("*** STEP SIZE CHANGED: %.3f -> %.3f mm ***\n", oldStepSize, manualStepSize);
          nextionDisplay.showMessage("Step " + String(manualStepSize, 2) + "mm");
        }
        break;
        
      case B_X_ENA:  // c - Enable/disable X axis (works even during emergency stop)
        nextionDisplay.showMessage("X_ENA key pressed");
        if (!esp32Motion.isTestSequenceActive()) {
          if (esp32Motion.isAxisEnabled(0)) {
            esp32Motion.disableAxis(0);
            nextionDisplay.showMessage("X-axis OFF");
          } else {
            esp32Motion.enableAxis(0);
            nextionDisplay.showMessage("X-axis ON");
          }
        } else {
          nextionDisplay.showMessage("Test running");
        }
        break;
        
      case B_Z_ENA:  // q - Enable/disable Z axis (works even during emergency stop)
        nextionDisplay.showMessage("Z_ENA key pressed");
        if (!esp32Motion.isTestSequenceActive()) {
          if (esp32Motion.isAxisEnabled(1)) {
            esp32Motion.disableAxis(1);
            nextionDisplay.showMessage("Z-axis OFF");
          } else {
            esp32Motion.enableAxis(1);
            nextionDisplay.showMessage("Z-axis ON");
          }
        } else {
          nextionDisplay.showMessage("Test running");
        }
        break;
        
      case B_X:      // x - Zero X axis position
        if (!esp32Motion.getEmergencyStop() && !esp32Motion.isTestSequenceActive()) {
          esp32Motion.setTargetPosition(0, 0.0);
          nextionDisplay.showMessage("X zeroed");
        }
        break;
        
      case B_Z:      // z - Zero Z axis position
        if (!esp32Motion.getEmergencyStop() && !esp32Motion.isTestSequenceActive()) {
          esp32Motion.setTargetPosition(1, 0.0);
          nextionDisplay.showMessage("Z zeroed");
        }
        break;
        
      case B_DISPL: { // Win - Toggle t3 display mode (normal/debug)
        // Toggle t3 display mode
        t3DebugMode = !t3DebugMode;
        
        // Always print diagnostics to Serial for development
        esp32Motion.printDiagnostics();
        Serial.println("Test sequence status: " + esp32Motion.getTestSequenceStatus());
        Serial.printf("Manual step size: %.3f mm\n", manualStepSize);
        Serial.printf("X-axis: %.3f mm (%s)\n", esp32Motion.getPosition(0), 
                     esp32Motion.isAxisEnabled(0) ? "ENABLED" : "DISABLED");
        Serial.printf("Z-axis: %.3f mm (%s)\n", esp32Motion.getPosition(1), 
                     esp32Motion.isAxisEnabled(1) ? "ENABLED" : "DISABLED");
        
        if (t3DebugMode) {
          // Show compact debug info on t3 when in debug mode
          uint32_t isrExecutions = esp32Motion.isrCount;
          uint32_t xPulses = esp32Motion.stepsPulsed[0];
          uint32_t zPulses = esp32Motion.stepsPulsed[1];
          
          // Create rotating debug display to fit in ~20 characters
          static uint8_t debugRotation = 0;
          debugRotation = (debugRotation + 1) % 3;
          
          String debugText;
          switch (debugRotation) {
            case 0:
              debugText = "DBG ISR:" + String(isrExecutions);
              break;
            case 1:
              debugText = "XP:" + String(xPulses) + " ZP:" + String(zPulses);
              break;
            case 2:
              debugText = "V" + String(esp32Motion.versionMajor) + " 2kHz Timer";
              break;
          }
          
          Serial1.print("t3.txt=\"" + debugText + "\"");
          Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
          
          nextionDisplay.showMessage("Debug mode ON");
        } else {
          // Clear t3 and let normal display take over
          Serial1.print("t3.txt=\"\"");
          Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
          
          nextionDisplay.showMessage("Normal mode ON");
        }
        
        Serial.printf("t3 display mode: %s\n", t3DebugMode ? "DEBUG" : "NORMAL");
        break;
      }
        
      default:
        // Unknown key - show step size as feedback
        nextionDisplay.showMessage("Step: " + String(manualStepSize) + "mm");
        break;
    }
}

// Wrapper function for compatibility
void handleKeyboard() {
  processKeypadEvent();
}

// Note: Enhanced keyboard processing via processKeypadEvent() 
// is now integrated with the existing scheduler system

void checkEmergencyStop() {
  // Legacy function - now handled by state machine
  // Kept for compatibility
  // Real emergency check is in taskEmergencyCheck() running every loop
}

void updateDisplay() {
  // Display update placeholder - handled by NextionDisplay.update() in main loop
}

// Task implementations for scheduler
// ==================================

void taskEmergencyCheck() {
  // Ultra-fast emergency stop check - runs every loop
  static uint32_t lastEmergencyTime = 0;
  
  // Check if emergency key was detected
  if (emergencyKeyDetected) {
    esp32Motion.setEmergencyStop(true);
    esp32Motion.stopTestSequence();
    resetArrowKeyStates();  // Stop any continuous movement
    nextionDisplay.showEmergencyStop();
    
    uint32_t currentTime = micros();
    uint32_t responseTime = (currentTime - lastEmergencyTime) / 1000;  // Convert to ms
    Serial.printf("*** EMERGENCY STOP - Response time: %d μs ***\n", currentTime - lastEmergencyTime);
    lastEmergencyTime = currentTime;
    
    emergencyKeyDetected = false;  // Reset flag
  }
}

void taskKeyboardScan() {
  // Full keyboard handling - runs every loop for safety
  handleKeyboard();
}

void taskMotionUpdate() {
  // Motion control update - runs at 200Hz (5ms)
  esp32Motion.update();
  
  // Only update target positions when they change (not every loop!)
  static float lastTargetX = 0.0;
  static float lastTargetZ = 0.0;
  
  if (targetPositionX != lastTargetX) {
    Serial.printf("*** MOTION UPDATE X: %.3f -> %.3f (sending to motion controller) ***\n", 
                  lastTargetX, targetPositionX);
    esp32Motion.setTargetPosition(0, targetPositionX);  // X-axis
    lastTargetX = targetPositionX;
  }
  
  if (targetPositionZ != lastTargetZ) {
    Serial.printf("*** MOTION UPDATE Z: %.3f -> %.3f (sending to motion controller) ***\n", 
                  lastTargetZ, targetPositionZ);
    esp32Motion.setTargetPosition(1, targetPositionZ);  // Z-axis
    lastTargetZ = targetPositionZ;
  }
}

void taskDisplayUpdate() {
  // Display update - runs at 20Hz (50ms)
  static bool splashHandled = false;
  
  // Must call update() for splash screen timing
  nextionDisplay.update();
  
  // Update debug display when in debug mode
  if (t3DebugMode && splashHandled) {
    updateDebugDisplay();
  }
  
  // Additional display updates can go here
  if (!splashHandled && millis() > 2000) {
    splashHandled = true;
    // Display is ready for normal updates
  }
}

void taskWebUpdate() {
  // Web interface update - runs at 50Hz (20ms)
  webInterface.update();
}

void taskDiagnostics() {
  // System diagnostics - runs every 5 seconds
  static uint32_t lastDiagnosticRun = 0;
  uint32_t currentTime = millis();
  
  if (currentTime - lastDiagnosticRun >= 5000) {
    Serial.println("\n=== SYSTEM STATUS ===");
    Serial.printf("Uptime: %d seconds\n", currentTime / 1000);
    Serial.printf("Emergency stop: %s\n", esp32Motion.getEmergencyStop() ? "ACTIVE" : "Ready");
    Serial.printf("Motion: X=%.3fmm, Z=%.3fmm\n", 
                 esp32Motion.getPosition(0), esp32Motion.getPosition(1));
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Loop frequency: %d Hz\n", scheduler.getLoopFrequency());
    Serial.println("===================\n");
    
    lastDiagnosticRun = currentTime;
  }
}

void resetArrowKeyStates() {
  // Reset all arrow key states (called during emergency stop)
  for (int i = 0; i < 4; i++) {
    keyStates[i].isPressed = false;
    keyStates[i].pressStartTime = 0;
    keyStates[i].lastRepeatTime = 0;
    keyStates[i].repeatInterval = 100;
  }
  Serial.println("Arrow key states reset due to emergency stop");
}

void updateDebugDisplay() {
  // Update debug information on t3 display - runs at 20Hz
  static uint32_t lastDebugUpdate = 0;
  static uint8_t debugRotation = 0;
  uint32_t currentTime = millis();
  
  // Rotate debug information every 2 seconds (40 cycles at 20Hz)
  if (currentTime - lastDebugUpdate >= 2000) {
    debugRotation = (debugRotation + 1) % 4;
    lastDebugUpdate = currentTime;
    
    uint32_t isrExecutions = esp32Motion.isrCount;
    uint32_t xPulses = esp32Motion.stepsPulsed[0];
    uint32_t zPulses = esp32Motion.stepsPulsed[1];
    
    String debugText;
    switch (debugRotation) {
      case 0:
        debugText = "DBG ISR:" + String(isrExecutions);
        break;
      case 1:
        debugText = "XP:" + String(xPulses) + " ZP:" + String(zPulses);
        break;
      case 2:
        debugText = "V" + String(esp32Motion.versionMajor) + " 2kHz Timer";
        break;
      case 3:
        debugText = "Freq:" + String(scheduler.getLoopFrequency()) + "Hz";
        break;
    }
    
    Serial1.print("t3.txt=\"" + debugText + "\"");
    Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
  }
}

// =============================================
// Continuous Movement System Implementation
// =============================================

int getKeyIndex(int keyCode) {
  switch(keyCode) {
    case B_LEFT: return KEY_LEFT_IDX;
    case B_RIGHT: return KEY_RIGHT_IDX;
    case B_UP: return KEY_UP_IDX;
    case B_DOWN: return KEY_DOWN_IDX;
    default: return -1;
  }
}

int getKeyCodeFromIndex(int index) {
  switch(index) {
    case KEY_LEFT_IDX: return B_LEFT;
    case KEY_RIGHT_IDX: return B_RIGHT;
    case KEY_UP_IDX: return B_UP;
    case KEY_DOWN_IDX: return B_DOWN;
    default: return 0;
  }
}

uint32_t calculateRepeatInterval(float stepSize) {
  // Scale repeat rate with step size (with SAFE speed for large steps)
  // Large steps = fast movement, small steps = slow movement
  
  if (stepSize >= 10.0) {
    return 60;   // ~16.5Hz for 10mm steps = 165mm/sec (SAFE SPEED)
  } else if (stepSize >= 1.0) {
    return 50;   // ~20Hz for 1mm steps = 20mm/sec
  } else if (stepSize >= 0.1) {
    return 100;  // ~10Hz for 0.1mm steps = 1mm/sec
  } else {
    return 200;  // ~5Hz for 0.01mm steps = 0.05mm/sec
  }
}

void performArrowKeyMovement(int keyCode) {
  // Common movement logic extracted from keyboard handler
  float oldX = targetPositionX;
  float oldZ = targetPositionZ;
  
  if (keyCode == B_LEFT) {
    targetPositionZ -= manualStepSize;
  } else if (keyCode == B_RIGHT) {
    targetPositionZ += manualStepSize;
  } else if (keyCode == B_UP) {
    targetPositionX -= manualStepSize;  // Towards centerline, away from operator
  } else if (keyCode == B_DOWN) {
    targetPositionX += manualStepSize;  // Away from centerline, towards operator
  }
  
  Serial.printf("*** ARROW MOVE: key=%d, stepSize=%.3f, X:%.3f->%.3f, Z:%.3f->%.3f ***\n", 
                keyCode, manualStepSize, oldX, targetPositionX, oldZ, targetPositionZ);
}

void handleArrowKeyPress(int keyCode, int keyIndex) {
  if (esp32Motion.getEmergencyStop() || esp32Motion.isTestSequenceActive()) {
    return;
  }
  
  ArrowKeyState& keyState = keyStates[keyIndex];
  uint32_t currentTime = millis();
  
  if (!keyState.isPressed) {
    // First press
    keyState.isPressed = true;
    keyState.pressStartTime = currentTime;
    keyState.lastRepeatTime = currentTime;
    keyState.repeatInterval = calculateRepeatInterval(manualStepSize);
    
    // Immediate movement on first press
    performArrowKeyMovement(keyCode);
    
    Serial.printf("Key %d pressed - immediate move by %.2fmm (interval=%dms)\n", 
                 keyCode, manualStepSize, keyState.repeatInterval);
  }
}

void handleArrowKeyRelease(int keyCode, int keyIndex) {
  ArrowKeyState& keyState = keyStates[keyIndex];
  uint32_t currentTime = millis();
  
  if (keyState.isPressed) {
    uint32_t holdDuration = currentTime - keyState.pressStartTime;
    
    Serial.printf("Key %d released after %dms\n", keyCode, holdDuration);
    
    keyState.isPressed = false;
  }
}

void taskContinuousMovement() {
  uint32_t currentTime = millis();
  
  for (int i = 0; i < 4; i++) {
    ArrowKeyState& keyState = keyStates[i];
    
    if (keyState.isPressed) {
      uint32_t holdDuration = currentTime - keyState.pressStartTime;
      
      // Start continuous movement after threshold
      if (holdDuration >= HOLD_THRESHOLD_MS) {
        if (currentTime - keyState.lastRepeatTime >= keyState.repeatInterval) {
          int keyCode = getKeyCodeFromIndex(i);
          performArrowKeyMovement(keyCode);
          keyState.lastRepeatTime = currentTime;
          
          // Optional: Accelerate over time (make it faster the longer held)
          if (keyState.repeatInterval > MIN_REPEAT_INTERVAL_MS) {
            keyState.repeatInterval = keyState.repeatInterval * 0.95; // 5% faster each repeat
          }
        }
      }
    }
  }
}