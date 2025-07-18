/*
 * nanoELS-flow - H5 Variant (ESP32-S3-dev board)
 * 
 * Complete rewrite of nanoELS Electronic Lead Screw controller
 * Target: ESP32-S3-dev board with touch screen interface
 * 
 * Hardware Configuration: See MyHardware.txt for pin assignments
 * 
 * Required Libraries (install via Arduino Library Manager):
 * - FastAccelStepper by gin66
 * - WebSockets by Markus Sattler
 * - PS2KeyAdvanced by Paul Carpenter
 * 
 * Board Selection: ESP32S3 Dev Module
 * 
 * CRITICAL: Pin assignments are PERMANENT - see MyHardware.txt
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

// Project modules - Native ESP32-S3 implementation (Arduino Compatible)
#include "ESP32MotionControl.h"
#include "WebInterface.h"
#include "NextionDisplay.h"

// Global Objects
// ==============

// Motion control system (native ESP32-S3 implementation with hardware PCNT)
// Global instance declared in ESP32MotionControl.h: extern ESP32MotionControl esp32Motion;

// Web interface system (handles HTTP server and WebSocket communication)
// Global instance declared in WebInterface.h: extern WebInterface webInterface;

// Nextion display system (handles touch screen display)
// Global instance declared in NextionDisplay.h: extern NextionDisplay nextionDisplay;

// PS2 Keyboard interface
PS2KeyAdvanced keyboard;

// Preferences for configuration storage
Preferences prefs;

// Global Variables
// ===============

// Current machine state
int currentMode = 0;  // 0=manual, 1=threading, etc.
int stepSize = 100;   // Current step size for manual movements

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
void initializeKeyboard();
void initializeDisplay();
void handleKeyboard();
void updateDisplay();
void processMovement();
void testMotionControl();  // Testing function for motion control
void initializeWebInterface();  // Web interface initialization

void setup() {
  Serial.begin(115200);
  Serial.println("Starting nanoELS-flow H5...");
  
  // Initialize PS2 keyboard interface
  keyboard.begin(KEY_DATA, KEY_CLOCK);
  Serial.println("✓ PS2 keyboard interface initialized");
  Serial.printf("Debug: B_ON=%d, B_OFF=%d\n", B_ON, B_OFF);
  
  // MINIMAL setup - exactly like NextionDisplayTest
  Serial.println("Initializing Nextion display...");
  nextionDisplay.initialize();
  Serial.println("Display initialized, splash screen should be visible");
  Serial.println("After splash, will show ELS data");
}

void loop() {
  // EXACTLY like NextionDisplayTest that works - minimal calls only
  
  // Update motion control (but don't initialize until after splash)
  static bool motionInitialized = false;
  if (motionInitialized) {
    esp32Motion.update();
  }
  
  // CRITICAL: Must call update() for splash screen timing and display updates
  nextionDisplay.update();
  
  // Initialize motion control ONLY after splash screen works
  if (!motionInitialized && nextionDisplay.getState() == DISPLAY_STATE_NORMAL) {
    Serial.println("Splash screen complete, initializing ESP32-S3 motion control...");
    esp32Motion.initialize();
    motionInitialized = true;
    Serial.println("ESP32-S3 motion control initialized");
  }
  
  // Process motion control updates (includes MPG inputs)
  if (motionInitialized) {
    esp32Motion.update();
  }
  
  // Handle keyboard input
  if (motionInitialized) {
    handleKeyboard();
  }
  
  delay(50); // Small delay like NextionDisplayTest
}

void testMotionControl() {
  Serial.println("=== ESP32-S3 Motion Control Test ===");
  
  // Print initial status
  esp32Motion.printDiagnostics();
  
  // Test basic movements
  Serial.println("Testing basic movements...");
  
  // Move X-axis 1000 steps forward
  Serial.println("Moving X-axis 1000 steps forward...");
  esp32Motion.moveRelative(0, 1000, true);  // X-axis, 1000 steps, blocking
  
  // Move Z-axis 500 steps forward
  Serial.println("Moving Z-axis 500 steps forward...");
  esp32Motion.moveRelative(1, 500, true);   // Z-axis, 500 steps, blocking
  
  // Move both axes back to zero
  Serial.println("Moving both axes to zero...");
  esp32Motion.moveAbsolute(0, 0, false);    // X-axis to zero, non-blocking
  esp32Motion.moveAbsolute(1, 0, false);    // Z-axis to zero, non-blocking
  
  // Wait for completion
  while (esp32Motion.isAxisMoving(0) || esp32Motion.isAxisMoving(1)) {
    esp32Motion.update();
    delay(10);
  }
  
  // Test spindle encoder reading
  Serial.println("Spindle encoder position: " + String(esp32Motion.getSpindlePosition()));
  
  // Print final status
  esp32Motion.printDiagnostics();
  
  Serial.println("=== ESP32-S3 Motion Control Test Complete ===");
}

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

void initializeKeyboard() {
  // Initialize PS2 keyboard interface
  keyboard.begin(KEY_DATA, KEY_CLOCK);
  Serial.println("✓ PS2 keyboard interface initialized");
}

// Old display function removed - now handled by NextionDisplay class

void handleKeyboard() {
  if (keyboard.available()) {
    uint16_t key = keyboard.read();
    
    // Process key according to MyHardware.txt mappings
    switch (key) {
      case B_LEFT:   // Z-axis left
        esp32Motion.moveRelative(1, -stepSize);
        Serial.println("Z-axis moving left");
        break;
        
      case B_RIGHT:  // Z-axis right  
        esp32Motion.moveRelative(1, stepSize);
        Serial.println("Z-axis moving right");
        break;
        
      case B_UP:     // X-axis forward
        esp32Motion.moveRelative(0, stepSize);
        Serial.println("X-axis moving forward");
        break;
        
      case B_DOWN:   // X-axis backward
        esp32Motion.moveRelative(0, -stepSize);
        Serial.println("X-axis moving backward");
        break;
        
      case B_OFF:    // IMMEDIATE EMERGENCY STOP - CRITICAL SAFETY
        // Stop ALL motion immediately - no conditions
        esp32Motion.setEmergencyStop(true);
        esp32Motion.stopAll();
        nextionDisplay.showEmergencyStop();
        Serial.println("*** EMERGENCY STOP ACTIVATED ***");
        break;
        
      case B_ON:     // Start turning mode / Release emergency stop
        if (esp32Motion.getEmergencyStop()) {
          esp32Motion.setEmergencyStop(false);
          nextionDisplay.setState(DISPLAY_STATE_NORMAL);
          nextionDisplay.showMessage("E-Stop released", NEXTION_T3, 2000);
        } else {
          esp32Motion.startTurningMode(0.5f, 3); // 0.5mm feed ratio, 3 passes
          nextionDisplay.showMessage("TURNING MODE", NEXTION_T3, 2000);
        }
        break;
        
      case B_X:      // Zero X-axis
        esp32Motion.setPosition(0, 0);
        nextionDisplay.showMessage("X-axis zeroed", NEXTION_T3, 2000);
        Serial.println("X-axis zeroed");
        break;
        
      case B_Z:      // Zero Z-axis
        esp32Motion.setPosition(1, 0);
        nextionDisplay.showMessage("Z-axis zeroed", NEXTION_T3, 2000);
        Serial.println("Z-axis zeroed");
        break;
        
      case B_X_ENA:  // Toggle X-axis enable
        if (esp32Motion.isAxisEnabled(0)) {
          esp32Motion.disableAxis(0);
        } else {
          esp32Motion.enableAxis(0);
        }
        break;
        
      case B_Z_ENA:  // Toggle Z-axis enable
        if (esp32Motion.isAxisEnabled(1)) {
          esp32Motion.disableAxis(1);
        } else {
          esp32Motion.enableAxis(1);
        }
        break;
        
      case B_STEP:   // Change step size
        if (stepSize == 1) stepSize = 10;
        else if (stepSize == 10) stepSize = 100;
        else if (stepSize == 100) stepSize = 1000;
        else stepSize = 1;
        nextionDisplay.showMessage("Step: " + String(stepSize), NEXTION_T3, 2000);
        Serial.println("Step size: " + String(stepSize));
        break;
        
      case B_DISPL:  // Display status
        esp32Motion.printDiagnostics();
        nextionDisplay.showMessage("Status in Serial", NEXTION_T3, 2000);
        break;
        
      default:
        Serial.print("Key pressed: ");
        Serial.println(key);
        break;
    }
  }
}

void processMovement() {
  // Handle any specialized movement operations here
  // Motion commands are processed automatically by esp32Motion.update()
  
  // Broadcast motion status to web interface
  static unsigned long lastWebUpdate = 0;
  if (millis() - lastWebUpdate > 1000) { // Update web interface every second
    if (webInterface.isWebServerRunning()) {
      webInterface.sendMotionStatus();
    }
    lastWebUpdate = millis();
  }
  
  // Example: Future threading operations, synchronized movements, etc.
  static unsigned long lastStatusPrint = 0;
  if (millis() - lastStatusPrint > 5000) { // Print status every 5 seconds
    if (esp32Motion.isAxisMoving(0) || esp32Motion.isAxisMoving(1)) {
      Serial.println("Motion Status: X=" + String(esp32Motion.getPosition(0)) + 
                     " Z=" + String(esp32Motion.getPosition(1)) + 
                     " Spindle=" + String(esp32Motion.getSpindlePosition()));
    }
    lastStatusPrint = millis();
  }
}


// Old updateDisplay function removed - now handled by NextionDisplay.update() in main loop