#ifndef MOTIONCONTROL_H
#define MOTIONCONTROL_H

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <queue>
#include "MyHardware.h"

// ESP32 IDF v5.x compatible includes - avoid PCNT conflicts with FastAccelStepper
// We'll implement a simplified encoder interface without direct PCNT dependency

// Motion command types for real-time execution
enum MotionCommandType {
  CMD_MOVE_RELATIVE,      // Relative movement in steps
  CMD_MOVE_ABSOLUTE,      // Absolute position movement
  CMD_SET_SPEED,          // Change speed (Hz)
  CMD_SET_ACCELERATION,   // Change acceleration
  CMD_STOP,               // Immediate stop
  CMD_ENABLE_AXIS,        // Enable axis
  CMD_DISABLE_AXIS,       // Disable axis
  CMD_SYNC_POSITION,      // Sync with spindle position (future)
  CMD_SYNC_SPEED          // Sync with spindle speed (future)
};

// Motion command structure for queue execution
struct MotionCommand {
  MotionCommandType type;
  uint8_t axis;           // 0=X, 1=Z
  int32_t value;          // Steps, speed, or position
  uint32_t timestamp;     // Execution timestamp (micros)
  bool blocking;          // Wait for completion
};

// Axis configuration structure
struct AxisConfig {
  uint8_t stepPin;
  uint8_t dirPin;
  uint8_t enablePin;
  int32_t maxSpeed;       // Maximum speed in Hz
  int32_t maxAccel;       // Maximum acceleration
  int32_t position;       // Current position in steps
  int32_t minLimit;       // Software limit (negative)
  int32_t maxLimit;       // Software limit (positive)
  bool enabled;           // Axis enable status
  bool inverted;          // Direction inversion
};

// Spindle encoder data structure
struct SpindleData {
  volatile int32_t position;     // Current encoder position
  volatile int32_t rpm;          // Current RPM
  volatile uint32_t lastUpdate;  // Last update timestamp
  bool synchronized;             // Sync status with motion
};

class MotionControl {
private:
  // FastAccelStepper objects
  FastAccelStepperEngine* engine;
  FastAccelStepper* stepperX;
  FastAccelStepper* stepperZ;
  
  // Axis configurations
  AxisConfig axisX;
  AxisConfig axisZ;
  
  // Command queue for real-time execution
  std::queue<MotionCommand> commandQueue;
  
  // Spindle encoder data
  SpindleData spindle;
  
  // Safety and limits
  bool emergencyStop;
  bool limitsEnabled;
  
  // Internal methods
  void initializeAxis(uint8_t axis);
  void executeCommand(const MotionCommand& cmd);
  bool checkLimits(uint8_t axis, int32_t targetPosition);
  void updateSpindleData();
  
public:
  MotionControl();
  ~MotionControl();
  
  // Initialization and configuration
  bool initialize();
  void shutdown();
  
  // Axis control methods
  bool enableAxis(uint8_t axis);
  bool disableAxis(uint8_t axis);
  bool isAxisEnabled(uint8_t axis);
  
  // Motion command methods
  bool queueCommand(const MotionCommand& cmd);
  bool executeImmediate(const MotionCommand& cmd);
  void processCommandQueue();
  void clearCommandQueue();
  
  // Position and status methods
  int32_t getPosition(uint8_t axis);
  bool setPosition(uint8_t axis, int32_t position);
  bool isMoving(uint8_t axis);
  bool isMoving();  // Any axis moving
  
  // Speed and acceleration control
  bool setSpeed(uint8_t axis, uint32_t speed);
  bool setAcceleration(uint8_t axis, uint32_t accel);
  uint32_t getSpeed(uint8_t axis);
  uint32_t getAcceleration(uint8_t axis);
  
  // Movement methods
  bool moveRelative(uint8_t axis, int32_t steps, bool blocking = false);
  bool moveAbsolute(uint8_t axis, int32_t position, bool blocking = false);
  bool stopAxis(uint8_t axis);
  bool stopAll();
  
  // Limit and safety methods
  bool setLimits(uint8_t axis, int32_t minLimit, int32_t maxLimit);
  void enableLimits(bool enable);
  void setEmergencyStop(bool stop);
  bool getEmergencyStop();
  
  // Spindle synchronization methods (for future implementation)
  void initializeSpindleEncoder();
  int32_t getSpindlePosition();
  int32_t getSpindleRPM();
  bool syncWithSpindle(uint8_t axis, float ratio);
  void stopSync(uint8_t axis);
  
  // Utility methods
  void update();  // Call frequently in main loop
  String getStatus();
  void printDiagnostics();
};

// Global motion control instance
extern MotionControl motionControl;

// Spindle encoder interrupt handlers (PCNT_UNIT_0)
void IRAM_ATTR spindleEncoderISR();

// Motion control helper functions
inline uint8_t getAxisFromChar(char axis) {
  return (axis == 'X' || axis == 'x') ? 0 : 1;
}

inline char getCharFromAxis(uint8_t axis) {
  return (axis == 0) ? 'X' : 'Z';
}

// Motion command creation helpers
MotionCommand createMoveCommand(uint8_t axis, int32_t steps, bool relative = true);
MotionCommand createSpeedCommand(uint8_t axis, uint32_t speed);
MotionCommand createStopCommand(uint8_t axis);

#endif // MOTIONCONTROL_H