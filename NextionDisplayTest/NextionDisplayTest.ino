#include "MotionControl.h"
MotionControl motionControl;
#include "NextionDisplay.h"
// NextionDisplay nextionDisplay; // Removed: already defined in NextionDisplay.cpp

// Global variables required by NextionDisplay
int currentMode = 0;  // 0=manual, 1=threading, etc.
int stepSize = 100;   // Current step size for manual movements

void setup() {
  Serial.begin(115200);
  Serial.println("Starting NextionDisplayTest...");
  
  // Initialize display - this starts the splash screen
  nextionDisplay.initialize();
  
  Serial.println("Display initialized, splash screen should be visible");
}

void loop() {
  // CRITICAL: Must call update() for splash screen timing and transitions
  nextionDisplay.update();
  
  // Debug: Print current state every 500ms
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 500) {
    Serial.print("Display state: ");
    Serial.print(nextionDisplay.getState());
    Serial.print(" (");
    switch(nextionDisplay.getState()) {
      case 0: Serial.print("BOOT"); break;
      case 1: Serial.print("WIFI_CONNECTING"); break;
      case 2: Serial.print("NORMAL"); break;
      case 3: Serial.print("EMERGENCY_STOP"); break;
      case 4: Serial.print("ERROR"); break;
      default: Serial.print("UNKNOWN"); break;
    }
    Serial.println(")");
    lastDebug = millis();
  }
  
  // Check if splash screen is done
  static bool testMessageShown = false;
  if (!testMessageShown && nextionDisplay.getState() == DISPLAY_STATE_NORMAL) {
    Serial.println("Splash screen complete, showing test message");
    nextionDisplay.setTopLine("HELLO TEST");
    testMessageShown = true;
  }
  
  delay(50); // Small delay to prevent overwhelming the display
}