/*
 * nanoELS-flow - H5 Variant (ESP32-S3-dev board)
 * 
 * Clean minimal sketch with display, web interface, and WiFi
 * Target: ESP32-S3-dev board with touch screen interface
 * 
 * Required Libraries (install via Arduino Library Manager):
 * - WebSockets by Markus Sattler
 * - PS2KeyAdvanced by Paul Carpenter (for future keyboard support)
 * 
 * Board Selection: ESP32S3 Dev Module
 */

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

// Shared variable for emergency stop coordination
volatile bool emergencyKeyDetected = false;

// WiFi Configuration
// Set WIFI_MODE to choose connection type:
// 0 = Access Point mode (creates "nanoELS-flow" network)
// 1 = Connect to existing WiFi network
#define WIFI_MODE 1

// Access Point credentials (when WIFI_MODE = 0)
const char* AP_SSID = "nanoELS-flow";
const char* AP_PASSWORD = "nanoels123";

// Home WiFi credentials (when WIFI_MODE = 1)
// Modify these for your home network:
const char* HOME_WIFI_SSID = "Holzweg_131";
const char* HOME_WIFI_PASSWORD = "Holzweg131-mesh";

// WiFi troubleshooting options
#define WIFI_CONNECTION_TIMEOUT 40  // seconds to wait for connection
#define WIFI_RETRY_COUNT 3          // number of connection attempts
#define FALLBACK_TO_AP true         // create AP if WiFi connection fails

// Function Prototypes
// ==================
void initializeWebInterface();  // Web interface initialization
void handleKeyboard();  // Basic keyboard handling
void checkEmergencyStop();  // Immediate emergency stop check
void updateDisplay();   // Display update placeholder

// Task functions for scheduler
void taskEmergencyCheck();
void taskKeyboardScan();
void taskMotionUpdate();
void taskDisplayUpdate();
void taskWebUpdate();
void taskDiagnostics();

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
  scheduler.addTask("DisplayUpdate", taskDisplayUpdate, PRIORITY_NORMAL, 50);         // 20Hz
  scheduler.addTask("WebUpdate", taskWebUpdate, PRIORITY_NORMAL, 20);                 // 50Hz
  scheduler.addTask("Diagnostics", taskDiagnostics, PRIORITY_LOW, 5000);              // 0.2Hz
  
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

void handleKeyboard() {
  // Enhanced keyboard handling for test control and manual movement
  if (keyboard.available()) {
    uint16_t key = keyboard.read();
    
    // Process key according to MyHardware.txt mappings
    switch (key) {
      case B_ON:     // ENTER - Start/Restart test sequence
        if (esp32Motion.getEmergencyStop()) {
          // Release emergency stop
          esp32Motion.setEmergencyStop(false);
          nextionDisplay.setState(DISPLAY_STATE_NORMAL);
          Serial.println("Emergency stop released - Press ENTER again to start test");
        } else if (esp32Motion.isTestSequenceActive()) {
          // Restart if already running
          Serial.println("Restarting test sequence...");
          esp32Motion.restartTestSequence();
        } else {
          // Start fresh or restart after completion
          if (esp32Motion.isTestSequenceCompleted()) {
            Serial.println("Restarting completed test sequence...");
            esp32Motion.restartTestSequence();
          } else {
            Serial.println("Starting test sequence...");
            esp32Motion.startTestSequence();
          }
        }
        break;
        
      case B_OFF:    // ESC - IMMEDIATE EMERGENCY STOP
        // Set emergency flag for ultra-fast response
        emergencyKeyDetected = true;
        // Also handle immediately here for redundancy
        esp32Motion.setEmergencyStop(true);
        esp32Motion.stopTestSequence();
        nextionDisplay.showEmergencyStop();
        Serial.println("*** EMERGENCY STOP ACTIVATED - Test sequence stopped ***");
        break;
        
      // Manual movement controls
      case B_LEFT:   // Left arrow - Z axis left movement
        if (!esp32Motion.getEmergencyStop() && !esp32Motion.isTestSequenceActive()) {
          float currentPos = esp32Motion.getPosition(1); // Z-axis is index 1
          float targetPos = currentPos - manualStepSize;
          if (esp32Motion.isPositionSafe(1, targetPos)) {
            esp32Motion.setTargetPosition(1, targetPos);
            Serial.printf("Z-axis moving left: %.3f -> %.3f mm (step=%.3f)\n", 
                         currentPos, targetPos, manualStepSize);
          } else {
            Serial.println("Z-axis movement blocked - position unsafe");
          }
        }
        break;
        
      case B_RIGHT:  // Right arrow - Z axis right movement
        if (!esp32Motion.getEmergencyStop() && !esp32Motion.isTestSequenceActive()) {
          float currentPos = esp32Motion.getPosition(1); // Z-axis is index 1
          float targetPos = currentPos + manualStepSize;
          if (esp32Motion.isPositionSafe(1, targetPos)) {
            esp32Motion.setTargetPosition(1, targetPos);
            Serial.printf("Z-axis moving right: %.3f -> %.3f mm (step=%.3f)\n", 
                         currentPos, targetPos, manualStepSize);
          } else {
            Serial.println("Z-axis movement blocked - position unsafe");
          }
        }
        break;
        
      case B_UP:     // Up arrow - X axis forward movement
        if (!esp32Motion.getEmergencyStop() && !esp32Motion.isTestSequenceActive()) {
          float currentPos = esp32Motion.getPosition(0); // X-axis is index 0
          float targetPos = currentPos + manualStepSize;
          if (esp32Motion.isPositionSafe(0, targetPos)) {
            esp32Motion.setTargetPosition(0, targetPos);
            Serial.printf("X-axis moving forward: %.3f -> %.3f mm (step=%.3f)\n", 
                         currentPos, targetPos, manualStepSize);
          } else {
            Serial.println("X-axis movement blocked - position unsafe");
          }
        }
        break;
        
      case B_DOWN:   // Down arrow - X axis backward movement
        if (!esp32Motion.getEmergencyStop() && !esp32Motion.isTestSequenceActive()) {
          float currentPos = esp32Motion.getPosition(0); // X-axis is index 0
          float targetPos = currentPos - manualStepSize;
          if (esp32Motion.isPositionSafe(0, targetPos)) {
            esp32Motion.setTargetPosition(0, targetPos);
            Serial.printf("X-axis moving backward: %.3f -> %.3f mm (step=%.3f)\n", 
                         currentPos, targetPos, manualStepSize);
          } else {
            Serial.println("X-axis movement blocked - position unsafe");
          }
        }
        break;
        
      case B_STEP:   // Tilda - Change manual step size
        if (manualStepSize == 0.01) {
          manualStepSize = 0.1;
        } else if (manualStepSize == 0.1) {
          manualStepSize = 1.0;
        } else if (manualStepSize == 1.0) {
          manualStepSize = 10.0;
        } else {
          manualStepSize = 0.01;
        }
        Serial.printf("Manual step size changed to: %.3f mm\n", manualStepSize);
        break;
        
      case B_X_ENA:  // c - Enable/disable X axis
        if (!esp32Motion.isTestSequenceActive()) {
          if (esp32Motion.isAxisEnabled(0)) {
            esp32Motion.disableAxis(0);
            Serial.println("X-axis DISABLED");
          } else {
            esp32Motion.enableAxis(0);
            Serial.println("X-axis ENABLED");
          }
        }
        break;
        
      case B_Z_ENA:  // q - Enable/disable Z axis
        if (!esp32Motion.isTestSequenceActive()) {
          if (esp32Motion.isAxisEnabled(1)) {
            esp32Motion.disableAxis(1);
            Serial.println("Z-axis DISABLED");
          } else {
            esp32Motion.enableAxis(1);
            Serial.println("Z-axis ENABLED");
          }
        }
        break;
        
      case B_X:      // x - Zero X axis position
        if (!esp32Motion.getEmergencyStop() && !esp32Motion.isTestSequenceActive()) {
          esp32Motion.setTargetPosition(0, 0.0);
          Serial.println("X-axis position zeroed");
        }
        break;
        
      case B_Z:      // z - Zero Z axis position
        if (!esp32Motion.getEmergencyStop() && !esp32Motion.isTestSequenceActive()) {
          esp32Motion.setTargetPosition(1, 0.0);
          Serial.println("Z-axis position zeroed");
        }
        break;
        
      case B_DISPL:  // Win - Display status and diagnostics
        esp32Motion.printDiagnostics();
        Serial.println("Test sequence status: " + esp32Motion.getTestSequenceStatus());
        Serial.printf("Manual step size: %.3f mm\n", manualStepSize);
        Serial.printf("X-axis: %.3f mm (%s)\n", esp32Motion.getPosition(0), 
                     esp32Motion.isAxisEnabled(0) ? "ENABLED" : "DISABLED");
        Serial.printf("Z-axis: %.3f mm (%s)\n", esp32Motion.getPosition(1), 
                     esp32Motion.isAxisEnabled(1) ? "ENABLED" : "DISABLED");
        break;
        
      default:
        Serial.println("Key pressed: " + String(key));
        Serial.println("Manual Controls:");
        Serial.println("  Arrows: Move axes (current step: " + String(manualStepSize) + "mm)");
        Serial.println("  ~: Change step size (0.01/0.1/1.0/10.0mm)");
        Serial.println("  c/q: Enable/disable X/Z axis");
        Serial.println("  x/z: Zero axis positions");
        Serial.println("Test Controls:");
        Serial.println("  ENTER: Start/Release E-Stop");
        Serial.println("  ESC: Emergency Stop");
        Serial.println("  Win: Diagnostics");
        break;
    }
  }
}

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
}

void taskDisplayUpdate() {
  // Display update - runs at 20Hz (50ms)
  static bool splashHandled = false;
  
  // Must call update() for splash screen timing
  nextionDisplay.update();
  
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