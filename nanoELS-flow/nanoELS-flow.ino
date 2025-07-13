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
#include <FastAccelStepper.h>

// Project modules
#include "MotionControl.h"
#include "WebInterface.h"
#include "NextionDisplay.h"

// Global Objects
// ==============

// Motion control system (handles all stepper control via FastAccelStepper)
// Global instance declared in MotionControl.h: extern MotionControl motionControl;

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
void testRoutine(); // New test routine function

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=================================");
  Serial.println("nanoELS-flow H5 (ESP32-S3-dev)");
  Serial.println("Electronic Lead Screw Controller");
  Serial.println("=================================");
  
  // Initialize hardware subsystems
  Serial.println("Initializing Motion Control System...");
  if (!motionControl.initialize()) {
    Serial.println("CRITICAL ERROR: Motion control initialization failed!");
    while(1) delay(1000); // Halt on critical error
  }
  
  Serial.println("Initializing PS2 keyboard interface...");
  initializeKeyboard();
  
  Serial.println("Initializing Nextion display...");
  nextionDisplay.initialize();
  nextionDisplay.showInitProgress("Motion control...");
  delay(1000); // Give the display time to finish booting
  
  // Skip motion control tests during startup to avoid hang
  // Uncomment this line after verifying basic connectivity:
  // testMotionControl();
  Serial.println("Skipping motion tests during startup...");
  
  Serial.println("Initializing web interface...");
  nextionDisplay.showInitProgress("Starting WiFi...");
  initializeWebInterface();
  
  // Initialize preferences
  prefs.begin("nanoels", false);
  
  Serial.println("nanoELS-flow H5 initialization complete");
  Serial.println("Ready for operation...");

  // --- BEGIN TEST ROUTINE ---
  testRoutine();
  // --- END TEST ROUTINE ---
}

void loop() {
  // Main control loop
  motionControl.update();    // Process motion commands and update spindle data
  webInterface.update();     // Process web server and WebSocket events
  nextionDisplay.update();   // Update Nextion display
  handleKeyboard();          // Process keyboard input
  processMovement();         // Handle movement commands
  
  // No delay needed - motion control handles timing internally
}

void testMotionControl() {
  Serial.println("=== Motion Control Test ===");
  
  // Print initial status
  motionControl.printDiagnostics();
  
  // Test basic movements
  Serial.println("Testing basic movements...");
  
  // Move X-axis 1000 steps forward
  Serial.println("Moving X-axis 1000 steps forward...");
  motionControl.moveRelative(0, 1000, true);  // X-axis, 1000 steps, blocking
  
  // Move Z-axis 500 steps forward
  Serial.println("Moving Z-axis 500 steps forward...");
  motionControl.moveRelative(1, 500, true);   // Z-axis, 500 steps, blocking
  
  // Move both axes back to zero
  Serial.println("Moving both axes to zero...");
  motionControl.moveAbsolute(0, 0, false);    // X-axis to zero, non-blocking
  motionControl.moveAbsolute(1, 0, false);    // Z-axis to zero, non-blocking
  
  // Wait for completion
  while (motionControl.isMoving()) {
    motionControl.update();
    delay(10);
  }
  
  // Test spindle encoder reading
  Serial.println("Spindle encoder position: " + String(motionControl.getSpindlePosition()));
  
  // Print final status
  motionControl.printDiagnostics();
  
  Serial.println("=== Motion Control Test Complete ===");
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
        motionControl.moveRelative(1, -stepSize);
        Serial.println("Z-axis moving left");
        break;
        
      case B_RIGHT:  // Z-axis right  
        motionControl.moveRelative(1, stepSize);
        Serial.println("Z-axis moving right");
        break;
        
      case B_UP:     // X-axis forward
        motionControl.moveRelative(0, stepSize);
        Serial.println("X-axis moving forward");
        break;
        
      case B_DOWN:   // X-axis backward
        motionControl.moveRelative(0, -stepSize);
        Serial.println("X-axis moving backward");
        break;
        
      case B_OFF:    // Emergency stop
        motionControl.setEmergencyStop(true);
        nextionDisplay.showEmergencyStop();
        Serial.println("EMERGENCY STOP ACTIVATED");
        break;
        
      case B_ON:     // Release emergency stop
        motionControl.setEmergencyStop(false);
        nextionDisplay.setState(DISPLAY_STATE_NORMAL);
        nextionDisplay.showMessage("E-Stop released", NEXTION_T3, 2000);
        Serial.println("Emergency stop released");
        break;
        
      case B_X:      // Zero X-axis
        motionControl.setPosition(0, 0);
        nextionDisplay.showMessage("X-axis zeroed", NEXTION_T3, 2000);
        Serial.println("X-axis zeroed");
        break;
        
      case B_Z:      // Zero Z-axis
        motionControl.setPosition(1, 0);
        nextionDisplay.showMessage("Z-axis zeroed", NEXTION_T3, 2000);
        Serial.println("Z-axis zeroed");
        break;
        
      case B_X_ENA:  // Toggle X-axis enable
        if (motionControl.isAxisEnabled(0)) {
          motionControl.disableAxis(0);
        } else {
          motionControl.enableAxis(0);
        }
        break;
        
      case B_Z_ENA:  // Toggle Z-axis enable
        if (motionControl.isAxisEnabled(1)) {
          motionControl.disableAxis(1);
        } else {
          motionControl.enableAxis(1);
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
        motionControl.printDiagnostics();
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
  // Motion commands are processed automatically by motionControl.update()
  
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
    if (motionControl.isMoving()) {
      Serial.println("Motion Status: X=" + String(motionControl.getPosition(0)) + 
                     " Z=" + String(motionControl.getPosition(1)) + 
                     " Spindle=" + String(motionControl.getSpindlePosition()));
    }
    lastStatusPrint = millis();
  }
}

void testRoutine() {
  // Display initial MPG and spindle values
  nextionDisplay.setTopLine("X MPG: " + String(motionControl.getXMPGPulseCount()));
  nextionDisplay.setPitchLine("Z MPG: " + String(motionControl.getZMPGPulseCount()));
  nextionDisplay.setPositionLine("Spindle: " + String(motionControl.getSpindlePosition()));
  nextionDisplay.setStatusLine("Test: Start");
  delay(2000);

  // Move X axis +4mm, then -4mm
  nextionDisplay.setStatusLine("X +4mm");
  motionControl.moveRelative(0, 4000, true);
  delay(500);
  nextionDisplay.setStatusLine("X -4mm");
  motionControl.moveRelative(0, -4000, true);
  delay(500);

  // Move Z axis +5mm, then -5mm
  nextionDisplay.setStatusLine("Z +5mm");
  motionControl.moveRelative(1, 5000, true);
  delay(500);
  nextionDisplay.setStatusLine("Z -5mm");
  motionControl.moveRelative(1, -5000, true);
  delay(500);

  // Show final MPG and spindle values
  nextionDisplay.setTopLine("X MPG: " + String(motionControl.getXMPGPulseCount()));
  nextionDisplay.setPitchLine("Z MPG: " + String(motionControl.getZMPGPulseCount()));
  nextionDisplay.setPositionLine("Spindle: " + String(motionControl.getSpindlePosition()));
  nextionDisplay.setStatusLine("Test: Done");
  delay(2000);
}

// Old updateDisplay function removed - now handled by NextionDisplay.update() in main loop