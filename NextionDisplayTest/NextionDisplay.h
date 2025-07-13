#ifndef NEXTIONDISPLAY_H
#define NEXTIONDISPLAY_H

#include <Arduino.h>
#include "MyHardware.h"
#include "MotionControl.h"

// Nextion display object IDs (from original h5.ino)
#define NEXTION_T0  0  // Top line status display
#define NEXTION_T1  1  // Pitch and starts information
#define NEXTION_T2  2  // Axis position details  
#define NEXTION_T3  3  // Bottom line context/status information

// Hash initialization value from original h5.ino
#define LCD_HASH_INITIAL -3845709

// Display update priorities
enum DisplayPriority {
  DISPLAY_PRIORITY_LOW = 0,
  DISPLAY_PRIORITY_NORMAL = 1,
  DISPLAY_PRIORITY_HIGH = 2,
  DISPLAY_PRIORITY_CRITICAL = 3
};

// Display states
enum DisplayState {
  DISPLAY_STATE_BOOT,
  DISPLAY_STATE_WIFI_CONNECTING,
  DISPLAY_STATE_NORMAL,
  DISPLAY_STATE_EMERGENCY_STOP,
  DISPLAY_STATE_ERROR
};

class NextionDisplay {
private:
  // Display state management
  DisplayState currentState;
  unsigned long lastUpdate;
  unsigned long displayTimeout;
  
  // Splash screen management (from original h5.ino)
  bool splashScreen;
  unsigned long splashStartTime;
  bool splashShown;  // Track if splash message has been displayed
  
  // Hash tracking for change detection (from original code)
  long lastHash[4];  // Hash for t0, t1, t2, t3 (changed to long to match original)
  
  // Message queue for priority display
  struct DisplayMessage {
    String text;
    uint8_t objectId;
    DisplayPriority priority;
    unsigned long timestamp;
    unsigned long duration;  // 0 = permanent
  };
  
  DisplayMessage messageQueue[8];
  int messageCount;
  
  // Internal methods
  void toScreen(const String& command);
  void setText(uint8_t id, const String& text);
  void screenClear();
  long calculateHash(const String& text);
  bool hasChanged(uint8_t id, const String& text);
  void processMessageQueue();
  
public:
  NextionDisplay();
  
  // Initialization
  void initialize();
  
  // Display state management
  void setState(DisplayState state);
  DisplayState getState();
  
  // Text display methods
  void setTopLine(const String& text, DisplayPriority priority = DISPLAY_PRIORITY_NORMAL);
  void setPitchLine(const String& text, DisplayPriority priority = DISPLAY_PRIORITY_NORMAL);
  void setPositionLine(const String& text, DisplayPriority priority = DISPLAY_PRIORITY_NORMAL);
  void setStatusLine(const String& text, DisplayPriority priority = DISPLAY_PRIORITY_NORMAL);
  
  // Specific status methods
  void showWiFiStatus(const String& status, bool connecting = false);
  void showMotionStatus();
  void showSystemStatus();
  void showError(const String& error);
  void showEmergencyStop();
  
  // Temporary message display
  void showMessage(const String& message, uint8_t objectId = NEXTION_T3, 
                   unsigned long duration = 3000, DisplayPriority priority = DISPLAY_PRIORITY_HIGH);
  
  // Boot/startup display
  void showBootScreen();
  void showInitProgress(const String& step);
  
  // Main update function
  void update();
  
  // Utility functions
  void clearAll();
  void setBrightness(uint8_t brightness);  // 0-100
  void wakeUp();
  void sleep();
};

// Global display instance
extern NextionDisplay nextionDisplay;
// Make the stub global visible to all translation units
extern MotionControl motionControl;

#endif // NEXTIONDISPLAY_H