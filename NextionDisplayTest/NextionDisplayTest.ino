#include "MotionControl.h"
MotionControl motionControl;
#include "NextionDisplay.h"
// NextionDisplay nextionDisplay; // Removed: already defined in NextionDisplay.cpp

// Global variables required by NextionDisplay (matching original h5.ino)
int currentMode = 0;     // 0=manual, 1=threading, 2=turning, etc.
int stepSize = 100;      // Current step size for manual movements

// Test variables to simulate real operation
unsigned long lastModeChange = 0;
unsigned long lastStepChange = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting NextionDisplayTest...");
  Serial.println("This test simulates the original h5.ino display behavior");
  
  // Initialize display - this starts the splash screen
  nextionDisplay.initialize();
  
  Serial.println("Display initialized, splash screen should be visible");
  Serial.println("After splash, will show simulated ELS data like original");
}

void loop() {
  // Update motion control (provides test data)
  motionControl.update();
  
  // CRITICAL: Must call update() for splash screen timing and transitions
  nextionDisplay.update();
  
  // Simulate mode changes every 10 seconds (like user pressing F1-F9)
  if (millis() - lastModeChange > 10000) {
    currentMode = (currentMode + 1) % 6;  // Cycle through modes 0-5
    lastModeChange = millis();
    Serial.println("Simulated mode change to: " + String(currentMode));
  }
  
  // Simulate step size changes every 15 seconds
  if (millis() - lastStepChange > 15000) {
    stepSize = (stepSize == 100) ? 10 : ((stepSize == 10) ? 1000 : 100);
    lastStepChange = millis();
    Serial.println("Simulated step size change to: " + String(stepSize));
  }
  
  // Debug: Print current state every 2 seconds
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 2000) {
    Serial.print("Display state: ");
    Serial.print(nextionDisplay.getState());
    Serial.print(" | Mode: ");
    Serial.print(currentMode);
    Serial.print(" | Step: ");
    Serial.print(stepSize);
    Serial.print(" | X pos: ");
    Serial.print(motionControl.getPosition(0));
    Serial.print(" | Z pos: ");
    Serial.print(motionControl.getPosition(1));
    Serial.print(" | RPM: ");
    Serial.print(motionControl.getSpindleRPM());
    Serial.print(" | Moving: ");
    Serial.println(motionControl.isMoving() ? "YES" : "NO");
    lastDebug = millis();
  }
  
  delay(50); // Small delay to prevent overwhelming the display
}