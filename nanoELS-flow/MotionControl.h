#ifndef MOTIONCONTROL_H
#define MOTIONCONTROL_H

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <queue>
#include "MyHardware.h"

// Simple interrupt-based encoder support (avoid PCNT conflicts with FastAccelStepper)

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
  CMD_SYNC_SPEED,         // Sync with spindle speed (future)
  CMD_MPG_MOVE,           // MPG-controlled movement
  CMD_MPG_SYNC,           // MPG synchronized movement
  CMD_MPG_SETUP           // MPG setup mode
};

// Motion command structure for queue execution
struct MotionCommand {
  MotionCommandType type;
  uint8_t axis;           // 0=X, 1=Z
  int32_t value;          // Steps, speed, or position
  uint32_t timestamp;     // Execution timestamp (micros)
  bool blocking;          // Wait for completion
  float mpgRatio;         // MPG ratio for synchronized moves
};

// MPG configuration structure
struct MPGConfig {
  uint8_t pulsePinA;        // MPG quadrature encoder pin A
  uint8_t pulsePinB;        // MPG quadrature encoder pin B  
  uint8_t axis;             // Which axis this MPG controls (0=X, 1=Z)
  volatile int32_t pulseCount;      // Current MPG pulse count
  volatile int32_t lastPulseCount;  // Previous count for delta calculation
  uint32_t lastPulseTime;   // Timestamp for debouncing
  float stepRatio;          // Steps per MPG pulse (for feel adjustment)
  bool enabled;             // Whether MPG is active for manual control
  bool operationActive;     // Whether automatic operation is running (disables MPG)
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
  MPGConfig* mpg;         // Associated MPG configuration
};

// Spindle encoder data structure
struct SpindleData {
  volatile int32_t position;     // Current encoder position
  volatile int32_t rpm;          // Current RPM
  volatile uint32_t lastUpdate;  // Last update timestamp
  bool synchronized;             // Sync status with motion
};

// Operation setup structure
struct OperationSetup {
  float threadPitch;      // Thread pitch in mm
  int threadStarts;       // Number of thread starts
  bool threadLeftHand;    // Left-hand thread
  float taperAngle;       // Taper angle in degrees
  int operationPasses;    // Number of operation passes
  float feedRate;         // Feed rate for turning/facing
  bool operationActive;   // Operation in progress
};

// Turning mode state
enum TurningState {
  TURNING_IDLE,
  TURNING_FEEDING,     // Z-axis following spindle
  TURNING_RETRACTING,  // X-axis retracting
  TURNING_RETURNING,   // Z-axis returning to start
  TURNING_ADVANCING    // X-axis advancing
};

struct TurningMode {
  bool active;           // Turning mode active
  TurningState state;    // Current turning state
  int currentPass;       // Current pass number (1-3)
  int32_t startZPos;     // Starting Z position
  int32_t startXPos;     // Starting X position  
  int32_t targetZPos;    // Target Z position (40mm)
  int32_t spindleStartPos; // Spindle position at start of pass
  int32_t spindleSyncPos; // Target spindle position for synchronization
  float zFeedRatio;      // Z movement ratio to spindle
  bool waitingForSync;   // Waiting for spindle to reach sync position
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
  
  // MPG configurations
  MPGConfig mpgX;
  MPGConfig mpgZ;
  
  // Command queue for real-time execution
  std::queue<MotionCommand> commandQueue;
  
  // Spindle encoder data
  SpindleData spindle;
  
  // Operation setup
  OperationSetup operation;
  
  // Turning mode
  TurningMode turning;
  
  // Safety and limits
  bool emergencyStop;
  bool limitsEnabled;
  
  // MPG interrupt handling
  static void IRAM_ATTR mpgXISR();
  static void IRAM_ATTR mpgZISR();
  
  // Internal methods
  void initializeAxis(uint8_t axis);
  void initializeMPG(uint8_t axis);
  void executeCommand(const MotionCommand& cmd);
  bool checkLimits(uint8_t axis, int32_t targetPosition);
  void updateSpindleData();
  void processMPGInput(uint8_t axis);
  
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
  
  // MPG control methods
  bool enableMPG(uint8_t axis);
  bool disableMPG(uint8_t axis);
  bool isMPGEnabled(uint8_t axis);
  void setMPGRatio(uint8_t axis, float ratio);
  float getMPGRatio(uint8_t axis);
  int32_t getMPGPulseCount(uint8_t axis);
  
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
  
  // Operation setup methods
  void setThreadPitch(float pitch);
  float getThreadPitch();
  void setThreadStarts(int starts);
  int getThreadStarts();
  void setThreadLeftHand(bool leftHand);
  bool getThreadLeftHand();
  void setTaperAngle(float angle);
  float getTaperAngle();
  void setOperationPasses(int passes);
  int getOperationPasses();
  void setFeedRate(float feedRate);
  float getFeedRate();
  void startOperation();
  void stopOperation();
  bool isOperationActive();
  
  // Turning mode methods
  void startTurningMode();
  void stopTurningMode();
  bool isTurningModeActive();
  void updateTurningMode();
  
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
  int32_t getXMPGPulseCount() const;
  int32_t getZMPGPulseCount() const;
  
  // MPG real-time control
  void processMPGInputs();
};

// Global motion control instance
extern MotionControl motionControl;

// Encoder interrupt handlers (ISR functions)
void IRAM_ATTR spindleEncoderISR();
void IRAM_ATTR xMPGEncoderISR();
void IRAM_ATTR zMPGEncoderISR();

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
MotionCommand createMPGCommand(uint8_t axis, int32_t pulses, float ratio = 1.0);

#endif // MOTIONCONTROL_H