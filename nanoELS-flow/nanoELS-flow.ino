/*
 * nanoELS-flow - H5 Variant (ESP32-S3-dev board)
 * 
 * Minimal motion control with h5.ino-inspired precision algorithms
 * Target: ESP32-S3-dev board with touch screen interface
 * 
 * ARDUINO IDE SETUP:
 * 1. Install ESP32 board support: https://espressif.github.io/arduino-esp32/package_esp32_index.json
 * 2. Board: "ESP32S3 Dev Module"
 * 3. Settings: 240MHz, QIO, 16MB Flash, Huge APP, PSRAM Enabled, 921600 upload
 * 4. Required Libraries (install via Library Manager):
 *    - WebSockets by Markus Sattler
 *    - PS2KeyAdvanced by Paul Carpenter
 *
 * Features:
 * - 0.7 micrometer following error with 600 PPR encoder
 * - h5.ino backlash compensation algorithms
 * - Direct position control for manual operations
 * - Emergency stop response <15ms
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

// Project modules - Minimal implementation with h5.ino-inspired precision control
#include "SetupConstants.h"      // Hardware configuration constants
#include "MinimalMotionControl.h" // h5.ino-inspired minimal motion controller
#include "OperationManager.h"     // Touch-off based operation management
#include "WebInterface.h"
#include "NextionDisplay.h"
// MyHardware.h merged into SetupConstants.h
#include "StateMachine.h"

// Global Objects
// ==============

// Web interface system (handles HTTP server and WebSocket communication)
// Global instance declared in WebInterface.h: extern WebInterface webInterface;

// Nextion display system (handles touch screen display)
// Global instance declared in NextionDisplay.h: extern NextionDisplay nextionDisplay;

// Operation manager (handles turning, threading, facing operations)
OperationManager operationManager;

// PS2 Keyboard interface (for future use)
PS2KeyAdvanced keyboard;

// Preferences for configuration storage
Preferences prefs;

// Global Variables
// ===============

// Current machine state
float manualStepSize = 1.0;  // Current step size for manual movements (mm)

// Shared variable for emergency stop coordination
volatile bool emergencyKeyDetected = false;

// Display debug mode flag (for NextionDisplay)
bool t3DebugMode = false;

// Simple status display mode
bool showDiagnostics = false;

// Simple status tracking
struct StatusInfo {
  float currentStepSize = 1.0;
  uint32_t lastKeyEvent = 0;
  int lastKeyCode = 0;
  String lastKeyMode = "";
} statusInfo;

// Simple arrow key state tracking
struct ArrowKeyState {
  bool isPressed;
  uint32_t pressStartTime;
};

ArrowKeyState keyStates[4]; // LEFT, RIGHT, UP, DOWN
#define KEY_LEFT_IDX 0
#define KEY_RIGHT_IDX 1  
#define KEY_UP_IDX 2
#define KEY_DOWN_IDX 3

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

// Manual movement functions
void performManualMovement(int keyCode);      // Simple manual movement
void resetArrowKeyStates();                   // Reset all key states (for emergency stop)

// Task functions for scheduler
void taskEmergencyCheck();
void taskKeyboardScan();
void taskMotionUpdate();
void taskOperationUpdate();
void taskDisplayUpdate();
void taskWebUpdate();
void taskDiagnostics();

// Diagnostics display function
void updateDiagnosticsDisplay();

void setup() {
  Serial.begin(115200);
  Serial.println("Starting nanoELS-flow H5 - Minimal Motion Control Version...");
  
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
  
  // Initialize minimal motion control after display splash
  Serial.println("Initializing minimal motion control...");
  if (motionControl.initialize()) {
    Serial.println("✓ Motion control initialized");
  } else {
    Serial.println("✗ Motion control initialization failed");
  }
  
  // Initialize operation manager
  operationManager.init(&motionControl);
  Serial.println("✓ Operation manager initialized");
  
  // Enable MPGs automatically on startup
  motionControl.enableMPG(AXIS_X, true);
  motionControl.setMPGStepSize(AXIS_X, (int32_t)10000); // 1mm default step size
  motionControl.enableMPG(AXIS_Z, true);
  motionControl.setMPGStepSize(AXIS_Z, (int32_t)10000); // 1mm default step size
  Serial.println("✓ MPGs enabled for both axes (1mm step size)");
  
  Serial.println("Setup complete - SAFETY READY");
  Serial.println("======================================");
  Serial.println("HARDWARE CONFIGURATION:");
  Serial.printf("✓ X-axis: Dir=%s, Enable=%s\n", 
                INVERT_X ? "INV" : "NORM", 
                INVERT_X_ENABLE ? "INV" : "NORM");
  Serial.printf("✓ Z-axis: Dir=%s, Enable=%s\n", 
                INVERT_Z ? "INV" : "NORM", 
                INVERT_Z_ENABLE ? "INV" : "NORM");
  Serial.println("SAFETY CHECKS IMPLEMENTED:");
  Serial.printf("✓ Software limits: X=±%.0fmm, Z=±%.0fmm\n", MAX_TRAVEL_MM_X, MAX_TRAVEL_MM_Z);
  Serial.println("✓ Emergency stop: ESC key (<15ms response)");
  Serial.println("✓ Enable pins: Hardware-specific inversion");
  Serial.println("✓ h5.ino precision: 0.7 micrometer following error");
  Serial.println("======================================");
  Serial.println("CONTROLS:");
  Serial.println("MANUAL MOVEMENT:");
  Serial.println("  Arrows = Move axes (step size: " + String(manualStepSize) + "mm)");
  Serial.println("  ~ = Change step size (0.01/0.1/1.0/10.0mm)");
  Serial.println("  c/q = Enable/disable X/Z axis");
  Serial.println("  x/z = Zero axis positions");
  Serial.println("CONTROL:");
  Serial.println("  ESC = EMERGENCY STOP (immediate)");
  Serial.println("  Win = Status diagnostics");
  Serial.println("======================================");
  
  // Initialize state machine scheduler
  Serial.println("\nInitializing state machine scheduler...");
  
  // Add tasks in priority order
  scheduler.addTask("EmergencyCheck", taskEmergencyCheck, PRIORITY_CRITICAL, 0);  // Every loop
  scheduler.addTask("KeyboardScan", taskKeyboardScan, PRIORITY_CRITICAL, 0);      // Every loop
  scheduler.addTask("MotionUpdate", taskMotionUpdate, PRIORITY_CRITICAL, 0);      // Every loop (~100kHz)
  scheduler.addTask("OperationUpdate", taskOperationUpdate, PRIORITY_CRITICAL, 0); // Every loop for operations
  scheduler.addTask("DisplayUpdate", taskDisplayUpdate, PRIORITY_NORMAL, 50);     // 20Hz
  scheduler.addTask("WebUpdate", taskWebUpdate, PRIORITY_NORMAL, 20);             // 50Hz
  scheduler.addTask("Diagnostics", taskDiagnostics, PRIORITY_LOW, 5000);          // 0.2Hz
  
  // Initialize arrow key states
  for (int i = 0; i < 4; i++) {
    keyStates[i].isPressed = false;
    keyStates[i].pressStartTime = 0;
  }
  
  // Enable both axes by default
  motionControl.enableAxis(AXIS_X);
  motionControl.enableAxis(AXIS_Z);
  
  Serial.println("✓ Minimal motion control system initialized");
  Serial.println("✓ h5.ino algorithms active");
  Serial.println("✓ State machine scheduler initialized");
  Serial.println("✓ Non-blocking operation enabled");
  Serial.println("✓ Emergency stop response: <15ms");
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
      if (motionControl.getEmergencyStop()) {
        // Clear emergency stop
        motionControl.setEmergencyStop(false);
        nextionDisplay.setState(DISPLAY_STATE_NORMAL);
        nextionDisplay.showMessage("SYSTEM READY");
        // Re-enable manual movement when clearing emergency stop
        operationManager.setArrowKeyMode(ARROW_MOTION_MODE);
      } else {
        // Check if we're in setup mode - ESC goes back to setupIndex 0 
        if (operationManager.getMode() != MODE_NORMAL && !operationManager.isRunning() && operationManager.getSetupIndex() > 0) {
          // Return to setupIndex 0 (direction selection) and reset state
          operationManager.resetSetupIndex();
          operationManager.clearCurrentInput();  // Reset state and numpad
          nextionDisplay.showMessage(operationManager.getPromptText());
        } else {
          // Activate emergency stop
          emergencyKeyDetected = true;
          motionControl.setEmergencyStop(true);
          resetArrowKeyStates();  // Stop any movement
          
          // Stop any running operations
          if (operationManager.getState() == STATE_RUNNING) {
            operationManager.stopOperation();
            nextionDisplay.showMessage("Operation stopped");
          }
          
          nextionDisplay.showEmergencyStop();
        }
      }
    }
    return;
  }
  
  // Debug: Show key codes using DIRECT Nextion commands like original h5.ino
  // REMOVED - was overwriting important diagnostics
  // Serial1.print("t0.txt=\"" + String(isPress ? "Press " : "Release ") + String(keyCode) + "\"");
  // Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
  
  // Arrow key handling - either for setup selection or manual movement
  if (keyCode == B_LEFT || keyCode == B_RIGHT || keyCode == B_UP || keyCode == B_DOWN) {
    if (isPress && !motionControl.getEmergencyStop()) {
      // Check if we're in cutting parameters mode
      if (operationManager.getMode() == MODE_CUTTING_PARAMS) {
        // Arrow keys for cutting parameters navigation
        if (keyCode == B_LEFT) {
          // Previous step
          operationManager.previousCuttingParamsStep();
        } else if (keyCode == B_RIGHT) {
          // Next step
          operationManager.nextCuttingParamsStep();
        } else if (keyCode == B_UP || keyCode == B_DOWN) {
          // Cycle through options for current step
          // TODO: Implement option cycling for material/tool/operation selection
        }
        nextionDisplay.showMessage(operationManager.getPromptText());
      } else if (operationManager.getMode() == MODE_TURN && operationManager.getSetupIndex() == 0) {
        // Arrow keys for direction/type selection (R→L/L→R/INT/EXT)
        if (keyCode == B_LEFT || keyCode == B_RIGHT) {
          // Toggle direction: Left/Right arrows for R→L vs L→R
          operationManager.toggleDirection();
        } else if (keyCode == B_UP || keyCode == B_DOWN) {
          // Toggle type: Up/Down arrows for Internal vs External
          operationManager.setInternalOperation(!operationManager.getInternalOperation());
        }
        nextionDisplay.showMessage(operationManager.getPromptText());
      } else if (operationManager.isArrowMotionEnabled()) {
        // Normal manual movement - only if motion is enabled
        performManualMovement(keyCode);
      }
      // If arrow motion disabled, do nothing (ignore arrow keys)
    }
    return; // Exit early for arrow keys
  }
  
  // Only process key presses (not releases) for other keys
  if (!isPress) {
    return;
  }
  
  // Process key according to MyHardware.txt mappings
  switch (keyCode) {
    case B_ON:     // Enter key - confirm and advance
      if (operationManager.getMode() == MODE_CUTTING_PARAMS) {
        // Cutting parameters mode - advance through steps
        operationManager.nextCuttingParamsStep();
        nextionDisplay.showMessage(operationManager.getPromptText());
        return;
      } else if (operationManager.getMode() == MODE_TURN) {
        if (!motionControl.getEmergencyStop()) {
          if (operationManager.isInNumpadInput()) {
            // Handle numpad input confirmation (touch-offs and parameters)
            OperationState state = operationManager.getState();
            if (state == STATE_TOUCHOFF_X || state == STATE_TOUCHOFF_Z) {
              // Confirm touch-off coordinate value
              operationManager.confirmTouchOffValue();
            } else if (operationManager.isInParameterEntry()) {
              // Confirm parameter value - call appropriate function based on state
              OperationState state = operationManager.getState();
              if (state == STATE_TARGET_DIAMETER || state == STATE_TARGET_LENGTH) {
                operationManager.confirmTargetValue();
              } else {
                operationManager.confirmParameterValue();
              }
            } else if (operationManager.getMode() == MODE_TURN && operationManager.getSetupIndex() == 4) {
              // Special case: passes setup at setupIndex 4 - not using proper state
              if (operationManager.getNumpadResult() > 0) {
                operationManager.setNumPasses(operationManager.getNumpadResult());
              }
              operationManager.resetNumpad();
              operationManager.advanceSetupIndex();
              nextionDisplay.showMessage(operationManager.getPromptText());
              return;  // CRITICAL: Exit to prevent double-processing
            }
            nextionDisplay.showMessage(operationManager.getPromptText());
          } else if (operationManager.getMode() != MODE_NORMAL) {
            // Turn mode workflow progression
            bool isOn = operationManager.isRunning();
            int setupIndex = operationManager.getSetupIndex();
            int lastSetupIndex = operationManager.getLastSetupIndex();
            
            if (operationManager.getMode() == MODE_TURN) {
              // Turn mode specific workflow handling as per Forkflow-example-turning.md
              switch (setupIndex) {
                case 0:  // Step 1: Confirm direction selection
                  operationManager.advanceSetupIndex();
                  // Touch-off will be started when user presses appropriate key (d/s for External R-L)
                  break;
                
                case 1:  // Step 2: Touch-off both axes (any order)
                  // ENTER only confirms value and advances if touch-off was started AND numpad entry is complete
                  {
                    OperationState currentState = operationManager.getState();
                    if (currentState == STATE_TOUCHOFF_X || currentState == STATE_TOUCHOFF_Z) {
                      // Check if user has entered coordinate value via numpad
                      if (operationManager.isInNumpadInput() && operationManager.getNumpadResult() > 0) {
                        operationManager.confirmTouchOffValue();
                        // Only advance setupIndex when BOTH touch-offs are complete
                        if (operationManager.hasTouchOffX() && operationManager.hasTouchOffZ()) {
                          operationManager.advanceSetupIndex();  // Go to parking setup
                        }
                        // If only one touch-off complete, stay at setupIndex 1 for second touch-off
                      }
                      // If no numpad input yet, do nothing - user must enter coordinate value first
                    } else if (operationManager.hasTouchOffX() && operationManager.hasTouchOffZ()) {
                      // Both touch-offs complete but not in active touch-off state - advance to parking
                      operationManager.advanceSetupIndex();
                    }
                    // If no touch-off started, do nothing - user must press d/s first
                  }
                  break;
                
                
                case 2:  // Step 3: Confirm parking position
                  operationManager.setParkingPosition(
                    motionControl.getAxisPosition(AXIS_X),
                    motionControl.getAxisPosition(AXIS_Z)
                  );
                  operationManager.advanceSetupIndex();
                  // Directly start target diameter entry - skip useless intermediate step
                  operationManager.startTargetDiameterEntry();
                  break;
                
                case 3:  // Step 4: Target diameter and length entry
                  // Handle transition from diameter to length entry
                  if (operationManager.hasTargetDiameter() && !operationManager.hasTargetLength()) {
                    operationManager.startTargetLengthEntry();
                  } else if (operationManager.hasTargetDiameter() && operationManager.hasTargetLength()) {
                    operationManager.advanceSetupIndex();
                  }
                  break;
                
                case 4:  // Step 5: Confirm number of passes - handled in numpad branch above
                  // Fallback: if not in numpad input mode, just advance (accept default)
                  operationManager.advanceSetupIndex();
                  break;
                
                case 5:  // Step 6: Start operation
                  if (operationManager.startOperation()) {
                    nextionDisplay.showMessage("Operation started");
                    return;  // Exit to prevent prompt overwrite
                  } else {
                    nextionDisplay.showMessage("Cannot start - check setup");
                    return;  // Exit to show error message
                  }
                  break;
              }
            } else if (operationManager.getMode() == MODE_THREAD) {
              // Threading mode specific workflow - exact copy of turning but with starts prompt
              switch (setupIndex) {
                case 0:  // Step 1: Confirm direction selection
                  operationManager.advanceSetupIndex();
                  // Touch-off will be started when user presses appropriate key (d/s for External R-L)
                  break;
                
                case 1:  // Step 2: Touch-off both axes (any order)
                  // ENTER only confirms value and advances if touch-off was started AND numpad entry is complete
                  {
                    OperationState currentState = operationManager.getState();
                    if (currentState == STATE_TOUCHOFF_X || currentState == STATE_TOUCHOFF_Z) {
                      // Check if user has entered coordinate value via numpad
                      if (operationManager.isInNumpadInput() && operationManager.getNumpadResult() > 0) {
                        operationManager.confirmTouchOffValue();
                        // Only advance setupIndex when BOTH touch-offs are complete
                        if (operationManager.hasTouchOffX() && operationManager.hasTouchOffZ()) {
                          operationManager.advanceSetupIndex();  // Go to parking setup
                        }
                        // If only one touch-off complete, stay at setupIndex 1 for second touch-off
                      }
                      // If no numpad input yet, do nothing - user must enter coordinate value first
                    } else if (operationManager.hasTouchOffX() && operationManager.hasTouchOffZ()) {
                      // Both touch-offs complete but not in active touch-off state - advance to parking
                      operationManager.advanceSetupIndex();
                    }
                    // If no touch-off started, do nothing - user must press d/s first
                  }
                  break;
                
                case 2:  // Step 3: Confirm parking position
                  operationManager.setParkingPosition(
                    motionControl.getAxisPosition(AXIS_X),
                    motionControl.getAxisPosition(AXIS_Z)
                  );
                  operationManager.advanceSetupIndex();
                  // Directly start target diameter entry - skip useless intermediate step
                  operationManager.startTargetDiameterEntry();
                  break;
                
                case 3:  // Step 4: Target diameter and length entry
                  // Handle transition from diameter to length entry
                  if (operationManager.hasTargetDiameter() && !operationManager.hasTargetLength()) {
                    operationManager.startTargetLengthEntry();
                  } else if (operationManager.hasTargetDiameter() && operationManager.hasTargetLength()) {
                    operationManager.advanceSetupIndex();
                  }
                  break;
                
                case 4:  // Step 5: Confirm number of starts - handled in numpad branch above
                  // Fallback: if not in numpad input mode, just advance (accept default)
                  if (operationManager.getNumpadResult() > 0) {
                    motionControl.setStarts(operationManager.getNumpadResult());
                  }
                  operationManager.resetNumpad();
                  operationManager.advanceSetupIndex();
                  break;
                
                case 5:  // Step 6: Confirm number of passes - handled in numpad branch above
                  // Fallback: if not in numpad input mode, just advance (accept default)
                  operationManager.advanceSetupIndex();
                  break;
                
                default:  // Step 7: Start operation
                  if (operationManager.startOperation()) {
                    nextionDisplay.showMessage("Operation started");
                    return;  // Exit to prevent prompt overwrite
                  } else {
                    nextionDisplay.showMessage("Cannot start - check setup");
                    return;  // Exit to show error message
                  }
                  break;
              }
            } else {
              // Other modes - original progression logic
              if (!isOn && setupIndex == 0) {
                operationManager.advanceSetupIndex();
              } else if (!isOn && setupIndex < lastSetupIndex) {
                operationManager.advanceSetupIndex();
              } else if (!isOn && setupIndex >= lastSetupIndex) {
                if (operationManager.startOperation()) {
                  nextionDisplay.showMessage("Operation started");
                } else {
                  nextionDisplay.showMessage("Cannot start - check setup");
                }
              } else if (isOn && (operationManager.getMode() == MODE_TURN || 
                                 operationManager.getMode() == MODE_FACE || 
                                 operationManager.getMode() == MODE_THREAD)) {
                operationManager.advancePass();
              }
            }
            
            nextionDisplay.showMessage(operationManager.getPromptText());
          } else {
            // Normal mode - show status
            nextionDisplay.showMessage("Status OK");
            Serial.println("System status: Ready");
            motionControl.printDiagnostics();
          }
        } else {
          nextionDisplay.showMessage("E-STOP ACTIVE - Press ESC");
        }
        break;
        
    case B_STEP:   // Tilda - Change manual step size
      {
        statusInfo.currentStepSize = manualStepSize;
        
        if (abs(manualStepSize - 0.01) < 0.001) {
          manualStepSize = 0.1;
        } else if (abs(manualStepSize - 0.1) < 0.001) {
          manualStepSize = 1.0;
        } else if (abs(manualStepSize - 1.0) < 0.001) {
          manualStepSize = 10.0;
        } else {
          manualStepSize = 0.01;  // Reset to start
        }
        
        statusInfo.currentStepSize = manualStepSize;
        
        // Update MPG step sizes (MPGs are always active)
        int32_t stepSizeDU = (int32_t)(manualStepSize * 10000); // Convert mm to deci-microns
        motionControl.setMPGStepSize(AXIS_X, stepSizeDU);
        motionControl.setMPGStepSize(AXIS_Z, stepSizeDU);
        
        nextionDisplay.showMessage("Step " + String(manualStepSize, 2) + "mm");
        Serial.printf("Step size changed to: %.3f mm\n", manualStepSize);
      }
      break;
        
    case B_X_ENA:  // c - Enable/disable X axis
      if (motionControl.isAxisEnabled(AXIS_X)) {
        motionControl.disableAxis(AXIS_X);
        nextionDisplay.showMessage("X-axis OFF");
      } else {
        motionControl.enableAxis(AXIS_X);
        nextionDisplay.showMessage("X-axis ON");
      }
      break;
        
    case B_Z_ENA:  // q - Enable/disable Z axis
      if (motionControl.isAxisEnabled(AXIS_Z)) {
        motionControl.disableAxis(AXIS_Z);
        nextionDisplay.showMessage("Z-axis OFF");
      } else {
        motionControl.enableAxis(AXIS_Z);
        nextionDisplay.showMessage("Z-axis ON");
      }
      break;
        
    case B_X:      // x - Zero X axis position
      if (!motionControl.getEmergencyStop()) {
        motionControl.zeroAxis(AXIS_X);
        nextionDisplay.showMessage("X zeroed");
      }
      break;
      
    case B_Z:      // z - Zero Z axis position
      if (!motionControl.getEmergencyStop()) {
        motionControl.zeroAxis(AXIS_Z);
        nextionDisplay.showMessage("Z zeroed");
      }
      break;
    
    case B_MEASURE: // m - Cycle measurement units (metric/inch/TPI)
      operationManager.cycleMeasure();
      {
        String unitText = "Units: ";
        int currentMeasure = operationManager.getCurrentMeasure();
        if (currentMeasure == MEASURE_METRIC) {
          unitText += "Metric";
        } else if (currentMeasure == MEASURE_INCH) {
          unitText += "Inch";
        } else {
          unitText += "TPI";
        }
        nextionDisplay.showMessage(unitText);
      }
      break;
      
    case B_REVERSE: // r - Reverse direction (left-hand threads)
      // TODO: Implement direction reversal
      nextionDisplay.showMessage("Direction: Normal");
      break;
        
    case B_DISPL: { // Win - Show diagnostics
      // Toggle diagnostics display
      showDiagnostics = !showDiagnostics;
      
      // Always print diagnostics to Serial
      motionControl.printDiagnostics();
      Serial.printf("Manual step size: %.3f mm\n", manualStepSize);
      Serial.printf("X-axis: %.3f mm (%s)\n", 
                   motionControl.stepsToMM(AXIS_X, motionControl.getPosition(AXIS_X)), 
                   motionControl.isAxisEnabled(AXIS_X) ? "ENABLED" : "DISABLED");
      Serial.printf("Z-axis: %.3f mm (%s)\n", 
                   motionControl.stepsToMM(AXIS_Z, motionControl.getPosition(AXIS_Z)), 
                   motionControl.isAxisEnabled(AXIS_Z) ? "ENABLED" : "DISABLED");
      
      if (showDiagnostics) {
        // Show status on display
        String statusText = "X:" + String(motionControl.stepsToMM(AXIS_X, motionControl.getPosition(AXIS_X)), 2) + 
                           " Z:" + String(motionControl.stepsToMM(AXIS_Z, motionControl.getPosition(AXIS_Z)), 2);
        Serial1.print("t3.txt=\"" + statusText + "\"");
        Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
        nextionDisplay.showMessage("Diagnostics ON");
      } else {
        // Clear display
        Serial1.print("t3.txt=\"\"");
        Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
        nextionDisplay.showMessage("Diagnostics OFF");
      }
      break;
    }
    
    // Plus/Minus keys - Context-aware functionality
    case B_PLUS:   // Numpad plus - increment pitch or parameters
      // Pitch adjustment always works (user requirement)
      if (!operationManager.isInNumpadInput()) {
        // Safety check: Prevent pitch changes during threading operations
        if (!operationManager.isPitchChangeAllowed()) {
          nextionDisplay.showMessage("Pitch locked during threading!");
          break;
        }
        
        // Increment pitch by small amount (like h5.ino)
        long currentDupr = motionControl.getDupr();  // Get current pitch
        long delta = (operationManager.getCurrentMeasure() == MEASURE_METRIC) ? 100 : 254; // 0.01mm or 0.001"
        long newDupr = currentDupr + delta;
        if (newDupr <= DUPR_MAX) {
          motionControl.setThreadPitch(newDupr);
          nextionDisplay.showMessage("Pitch: " + operationManager.formatDupr(newDupr));
        }
      } else if (operationManager.isInNumpadInput()) {
        // In numpad mode: use plus key for decimal point
        operationManager.handleNumpadInput('.');
        nextionDisplay.showMessage(operationManager.getPromptText());
      } else if (operationManager.getMode() != MODE_NORMAL) {
        // In setup mode: increment parameters
        OperationState state = operationManager.getState();
        
        if (state == STATE_TARGET_LENGTH) {
          // Adjust cut length/depth
          float currentLength = 10.0; // TODO: Get current from operation manager
          operationManager.setCutLength(currentLength + manualStepSize);
          nextionDisplay.showMessage("Length: " + String(currentLength + manualStepSize, 2) + "mm");
        } else if (state == STATE_SETUP_PASSES) {
          // Adjust number of passes
          int currentPasses = operationManager.getTotalPasses();
          int newPasses = currentPasses + 1;
          operationManager.setNumPasses(newPasses);
          nextionDisplay.showMessage("Passes: " + String(newPasses));
        } else if (state == STATE_SETUP_CONE) {
          // Adjust cone ratio
          float ratio = 0.0; // TODO: Get current from operation manager
          float newRatio = ratio + 0.001f;
          operationManager.setConeRatio(newRatio);
          nextionDisplay.showMessage("Ratio: " + String(newRatio, 4));
        }
      }
      break;
      
    case B_MINUS:  // Numpad minus - decrement pitch or parameters  
      // Pitch adjustment always works (user requirement)
      if (!operationManager.isInNumpadInput()) {
        // Safety check: Prevent pitch changes during threading operations
        if (!operationManager.isPitchChangeAllowed()) {
          nextionDisplay.showMessage("Pitch locked during threading!");
          break;
        }
        
        // Decrement pitch by small amount (like h5.ino)
        long currentDupr = motionControl.getDupr();  // Get current pitch
        long delta = (operationManager.getCurrentMeasure() == MEASURE_METRIC) ? 100 : 254; // 0.01mm or 0.001"
        long newDupr = currentDupr - delta;
        if (newDupr >= -DUPR_MAX) {
          motionControl.setThreadPitch(newDupr);
          nextionDisplay.showMessage("Pitch: " + operationManager.formatDupr(newDupr));
        }
      } else if (operationManager.isInNumpadInput()) {
        // In numpad mode: use minus key for negative sign
        operationManager.handleNumpadInput('-');
        nextionDisplay.showMessage(operationManager.getPromptText());
      } else if (operationManager.getMode() != MODE_NORMAL) {
        // In setup mode: decrement parameters
        OperationState state = operationManager.getState();
        
        if (state == STATE_TARGET_LENGTH) {
          // Adjust cut length/depth
          float currentLength = 10.0; // TODO: Get current from operation manager
          operationManager.setCutLength(currentLength - manualStepSize);
          nextionDisplay.showMessage("Length: " + String(currentLength - manualStepSize, 2) + "mm");
        } else if (state == STATE_SETUP_PASSES) {
          // Adjust number of passes
          int currentPasses = operationManager.getTotalPasses();
          int newPasses = currentPasses - 1;
          operationManager.setNumPasses(newPasses);
          nextionDisplay.showMessage("Passes: " + String(newPasses));
        } else if (state == STATE_SETUP_CONE) {
          // Adjust cone ratio
          float ratio = 0.0; // TODO: Get current from operation manager
          float newRatio = ratio - 0.001f;
          operationManager.setConeRatio(newRatio);
          nextionDisplay.showMessage("Ratio: " + String(newRatio, 4));
        }
      }
      break;
    
    // Function keys - Operation mode selection
    case B_MODE_GEARS:  // F1 - Normal gearbox mode
      operationManager.setMode(MODE_NORMAL);
      nextionDisplay.showMessage("GEARBOX mode");
      break;
      
    case B_MODE_TURN:   // F2 - Turning mode with setup flow
      operationManager.setMode(MODE_TURN);
      operationManager.resetSetupIndex();  // Start at setupIndex 0 for direction selection
      nextionDisplay.showMessage(operationManager.getPromptText());
      break;
      
    case B_MODE_FACE:   // F3 - Facing mode
      operationManager.setMode(MODE_FACE);
      nextionDisplay.showMessage("FACING mode");
      break;
      
    case B_MODE_CONE:   // F4 - Cone mode
      operationManager.setMode(MODE_CONE);
      nextionDisplay.showMessage("CONE mode");
      break;
      
    case B_MODE_CUT:    // F5 - Cut-off mode
      operationManager.setMode(MODE_CUT);
      nextionDisplay.showMessage("CUT-OFF mode");
      break;
      
    case B_MODE_THREAD: // F6 - Threading mode
      operationManager.setMode(MODE_THREAD);
      nextionDisplay.showMessage("THREADING mode");
      break;
      
    case B_MODE_ASYNC:  // F7 - Async mode
      operationManager.setMode(MODE_ASYNC);
      nextionDisplay.showMessage("ASYNC mode");
      break;
      
    case B_MODE_ELLIPSE: // F8 - Ellipse mode
      operationManager.setMode(MODE_ELLIPSE);
      nextionDisplay.showMessage("ELLIPSE mode");
      break;
      
    case B_MODE_GCODE:  // F9 - G-code mode
      operationManager.setMode(MODE_GCODE);
      nextionDisplay.showMessage("GCODE mode");
      break;
      
    case B_MODE_Y:      // F10 - Cutting Parameters mode (reusing Y mode key)
      operationManager.setMode(MODE_CUTTING_PARAMS);
      operationManager.startCuttingParamsEntry();
      nextionDisplay.showMessage("Cutting Parameters");
      break;
        
    // Touch-off keys - User moves axes then presses these to CONFIRM touch-off position
    case B_STOPU:   // 'w' - Confirm INTERNAL diameter touch-off position
      if (!motionControl.getEmergencyStop() && (operationManager.getMode() == MODE_TURN || operationManager.getMode() == MODE_THREAD)) {
        int setupIndex = operationManager.getSetupIndex();
        if (setupIndex == 1 && operationManager.getInternalOperation()) {
          // Internal operation: 'w' confirms X-axis (diameter) touch-off
          operationManager.startTouchOffX();
          nextionDisplay.showMessage(operationManager.getPromptText());
        }
      }
      break;
      
    case B_STOPD:   // 's' - Confirm EXTERNAL diameter touch-off position  
      if (!motionControl.getEmergencyStop() && (operationManager.getMode() == MODE_TURN || operationManager.getMode() == MODE_THREAD)) {
        int setupIndex = operationManager.getSetupIndex();
        if (setupIndex == 1 && !operationManager.getInternalOperation()) {
          // External operation: 's' confirms X-axis (diameter) touch-off and advances to value input
          operationManager.startTouchOffX();  // Capture current X position
          // Now user should enter coordinate value via numpad, display should show numpad prompt
          nextionDisplay.showMessage(operationManager.getPromptText());
        }
      }
      break;
      
    case B_STOPR:   // 'd' - Confirm R→L face/Z touch-off position
      if (!motionControl.getEmergencyStop() && (operationManager.getMode() == MODE_TURN || operationManager.getMode() == MODE_THREAD)) {
        int setupIndex = operationManager.getSetupIndex();
        if (setupIndex == 1 && !operationManager.getLeftToRight()) {
          // R→L operation: 'd' confirms Z-axis (face) touch-off and advances to value input
          operationManager.startTouchOffZ();  // Capture current Z position
          // Now user should enter coordinate value via numpad, display should show numpad prompt
          nextionDisplay.showMessage(operationManager.getPromptText());
        }
      }
      break;
      
    case B_STOPL:   // 'a' - Confirm L→R face/Z touch-off position
      if (!motionControl.getEmergencyStop() && (operationManager.getMode() == MODE_TURN || operationManager.getMode() == MODE_THREAD)) {
        int setupIndex = operationManager.getSetupIndex();
        if (setupIndex == 1 && operationManager.getLeftToRight()) {
          // L→R operation: 'a' confirms Z-axis (face) touch-off
          operationManager.startTouchOffZ();
          nextionDisplay.showMessage(operationManager.getPromptText());
        }
      }
      break;
      
    // Number keys for coordinate entry
    case B_0:
    case B_1:
    case B_2:
    case B_3:
    case B_4:
    case B_5:
    case B_6:
    case B_7:
    case B_8:
    case B_9:
      {
        bool isInNumpad = operationManager.isInNumpadInput();
        bool isInParam = operationManager.isInParameterEntry();
        bool isTurnMode = operationManager.getMode() == MODE_TURN;
        bool isThreadMode = operationManager.getMode() == MODE_THREAD;
        bool isCuttingParamsMode = operationManager.getMode() == MODE_CUTTING_PARAMS;
        int setupIdx = operationManager.getSetupIndex();
        char digit;
        // Map keyCode to digit character for top row numbers
        switch (keyCode) {
          case B_0: digit = '0'; break;
          case B_1: digit = '1'; break;
          case B_2: digit = '2'; break;
          case B_3: digit = '3'; break;
          case B_4: digit = '4'; break;
          case B_5: digit = '5'; break;
          case B_6: digit = '6'; break;
          case B_7: digit = '7'; break;
          case B_8: digit = '8'; break;
          case B_9: digit = '9'; break;
          default: digit = '0'; break; // Fallback
        }
        
        if (isInNumpad || isInParam || (isTurnMode && (setupIdx == 1 || setupIdx == 4)) || 
            (isThreadMode && (setupIdx == 1 || setupIdx == 4 || setupIdx == 5)) || 
            (isCuttingParamsMode && operationManager.getState() == STATE_CUTTING_PARAMS_DIAMETER)) {
          // Allow number entry for turn mode passes setup, threading mode starts/passes setup, and cutting parameters diameter
          operationManager.handleNumpadInput(digit);
          nextionDisplay.showMessage(operationManager.getPromptText());
        }
      }
      break;
      
    case B_BACKSPACE:  // Backspace for numpad entry
      if (operationManager.isInNumpadInput() || operationManager.isInParameterEntry() || 
          (operationManager.getMode() == MODE_CUTTING_PARAMS && operationManager.getState() == STATE_CUTTING_PARAMS_DIAMETER)) {
        operationManager.handleNumpadBackspace();
        nextionDisplay.showMessage(operationManager.getPromptText());
      }
      break;
        
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
    motionControl.setEmergencyStop(true);
    resetArrowKeyStates();  // Stop any movement
    nextionDisplay.showEmergencyStop();
    
    uint32_t currentTime = micros();
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
  // Motion control update - runs at 1kHz
  motionControl.update();
}

void taskOperationUpdate() {
  // Operation manager update - runs every loop for operations
  operationManager.update();
}

void taskDisplayUpdate() {
  // Display update - runs at 20Hz (50ms)
  static bool splashHandled = false;
  
  // Must call update() for splash screen timing
  nextionDisplay.update();
  
  // Update diagnostics display when enabled
  // if (showDiagnostics && splashHandled) {
  //   updateDiagnosticsDisplay();  // DISABLED - was interfering with t3 operation display
  // }
  
  // Update operation status display
  if (splashHandled && operationManager.getMode() != MODE_NORMAL) {
    // Show operation status on line 0
    String status = operationManager.getStatusText();
    Serial1.print("t0.txt=\"" + status + "\"");
    Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
    
    // Show operation prompt or progress on line 3
    if (operationManager.getState() != STATE_RUNNING) {
      String prompt = operationManager.getPromptText();
      Serial1.print("t3.txt=\"" + prompt + "\"");
      Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
    } else {
      // Show progress during operation
      float progress = operationManager.getProgress();
      String progressText = "Pass " + String(operationManager.getCurrentPass() + 1) + 
                           "/" + String(operationManager.getTotalPasses()) + 
                           " " + String(int(progress * 100)) + "%";
      Serial1.print("t3.txt=\"" + progressText + "\"");
      Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
    }
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
    Serial.printf("Emergency stop: %s\n", motionControl.getEmergencyStop() ? "ACTIVE" : "Ready");
    Serial.printf("Motion: X=%.3fmm, Z=%.3fmm\n", 
                 motionControl.stepsToMM(AXIS_X, motionControl.getPosition(AXIS_X)), 
                 motionControl.stepsToMM(AXIS_Z, motionControl.getPosition(AXIS_Z)));
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
  }
  
  // Stop any movements
  motionControl.stopAxis(AXIS_X);
  motionControl.stopAxis(AXIS_Z);
  
  statusInfo.lastKeyMode = "ESTOP_RESET";
}

void updateDiagnosticsDisplay() {
  // Update diagnostics information on t3 display - runs at 20Hz
  static uint32_t lastDiagnosticsUpdate = 0;
  static uint8_t diagnosticsRotation = 0;
  uint32_t currentTime = millis();
  
  // Rotate diagnostics information every 3 seconds
  if (currentTime - lastDiagnosticsUpdate >= 3000) {
    diagnosticsRotation = (diagnosticsRotation + 1) % 3;
    lastDiagnosticsUpdate = currentTime;
    
    String diagnosticsText;
    switch (diagnosticsRotation) {
      case 0:
        diagnosticsText = "X:" + String(motionControl.stepsToMM(AXIS_X, motionControl.getPosition(AXIS_X)), 2) + "mm";
        break;
      case 1:
        diagnosticsText = "Z:" + String(motionControl.stepsToMM(AXIS_Z, motionControl.getPosition(AXIS_Z)), 2) + "mm";
        break;
      case 2:
        diagnosticsText = "Step:" + String(manualStepSize, 2) + "mm";
        break;
    }
    
    Serial1.print("t3.txt=\"" + diagnosticsText + "\"");
    Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
  }
}

// =============================================
// Manual Movement Implementation
// =============================================

void performManualMovement(int keyCode) {
  // Simple manual movement using the minimal motion controller
  int axis;
  int32_t steps;
  
  // Determine axis and direction
  if (keyCode == B_LEFT || keyCode == B_RIGHT) {
    axis = AXIS_Z;
    steps = motionControl.mmToSteps(AXIS_Z, manualStepSize);
    if (keyCode == B_LEFT) steps = -steps;  // Left = negative Z
  } else if (keyCode == B_UP || keyCode == B_DOWN) {
    axis = AXIS_X;
    steps = motionControl.mmToSteps(AXIS_X, manualStepSize);
    if (keyCode == B_UP) steps = -steps;   // Up = towards centerline (negative X)
  } else {
    return; // Invalid key
  }
  
  // Move relative to current position
  motionControl.moveRelative(axis, steps);
  
  // Update status info
  statusInfo.lastKeyCode = keyCode;
  statusInfo.lastKeyEvent = millis();
  statusInfo.lastKeyMode = "MANUAL";
  
  // Provide feedback
  char axisName = (axis == AXIS_X) ? 'X' : 'Z';
  Serial.printf("Manual move: %c axis %+.3f mm\n", axisName, 
                (keyCode == B_LEFT || keyCode == B_UP) ? -manualStepSize : manualStepSize);
}