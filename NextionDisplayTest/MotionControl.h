#ifndef MOTIONCONTROL_H
#define MOTIONCONTROL_H

#include <Arduino.h>

class MotionControl {
private:
  unsigned long lastUpdate = 0;
  int testCounter = 0;
  
public:
  MotionControl() {}
  
  void update() {
    // Simulate some activity for testing
    if (millis() - lastUpdate > 1000) {
      testCounter++;
      lastUpdate = millis();
    }
  }
  
  int getEmergencyStop() { return 0; }
  int isMoving() { return (testCounter % 10) < 3; } // Moving 30% of the time
  
  // Return simulated axis positions (X=0, Z=1)
  long getPosition(int axis) { 
    if (axis == 0) return testCounter * 100;        // X axis: 0, 100, 200, 300...
    if (axis == 1) return testCounter * -50;        // Z axis: 0, -50, -100, -150...
    return 0; 
  }
  
  int getSpindleRPM() { return 800 + (testCounter % 200); }  // RPM 800-1000
  int getSpindlePosition() { return testCounter * 10; }      // Encoder position
};

#endif // MOTIONCONTROL_H 