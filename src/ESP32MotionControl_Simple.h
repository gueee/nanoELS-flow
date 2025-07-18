#ifndef ESP32MOTIONCONTROL_SIMPLE_H
#define ESP32MOTIONCONTROL_SIMPLE_H

#include <Arduino.h>
#include "CircularBuffer.h"
#include "MyHardware.h"

/**
 * ESP32MotionControl - Simple Arduino-Compatible ESP32-S3 Motion Control System
 * 
 * This version uses the simplest possible approach that works with all Arduino ESP32 versions:
 * 1. Task-based stepper control (no hardware timers)
 * 2. Interrupt-based encoder counting (optimized)
 * 3. Real-time motion queue with circular buffer
 * 4. Maximum Arduino compatibility
 */

// Motion command structure
struct MotionCommand {
    enum Type {
        MOVE_RELATIVE,
        MOVE_ABSOLUTE, 
        SET_SPEED,
        SET_ACCELERATION,
        STOP_AXIS,
        ENABLE_AXIS,
        DISABLE_AXIS
    } type;
    
    uint8_t axis;           // 0=X, 1=Z
    int32_t value;          // Steps, speed, or target position
    uint32_t timestamp;     // Execution time (microseconds)
    bool blocking;          // Wait for completion
};

// Axis configuration
struct AxisConfig {
    // GPIO configuration
    uint8_t stepPin;
    uint8_t dirPin;
    uint8_t enablePin;
    
    // Motion parameters
    volatile int32_t position;      // Current position in steps
    int32_t targetPosition;         // Target position
    uint32_t currentSpeed;          // Current speed in Hz
    uint32_t targetSpeed;           // Target speed in Hz
    uint32_t maxSpeed;              // Maximum speed in Hz
    uint32_t acceleration;          // Acceleration in steps/s²
    
    // Motion timing
    uint32_t stepInterval;          // Microseconds between steps
    uint32_t lastStepTime;          // Last step timestamp
    
    // Status flags
    bool enabled;
    volatile bool moving;
    bool inverted;
    
    // Motion state
    enum State {
        IDLE,
        ACCELERATING,
        CONSTANT_SPEED,
        DECELERATING
    } state;
};

// Encoder configuration (optimized interrupt-based)
struct EncoderConfig {
    uint8_t pinA;
    uint8_t pinB;
    volatile int32_t count;
    volatile int32_t lastCount;
    int32_t offset;
    volatile uint32_t errorCount;
    volatile uint8_t lastState;
    const char* name;
};

class ESP32MotionControl {
private:
    // Hardware configuration
    AxisConfig axes[2];         // X and Z axes
    EncoderConfig encoders[3];  // Spindle, Z-MPG, X-MPG
    
    // Motion queue
    CircularBuffer<MotionCommand, 64> motionQueue;
    
    // Static instance for ISR access
    static ESP32MotionControl* instance;
    
    // Encoder interrupt handlers (optimized)
    static void IRAM_ATTR encoderISR_Spindle(void);
    static void IRAM_ATTR encoderISR_XMPG(void);
    static void IRAM_ATTR encoderISR_ZMPG(void);
    
    // Internal methods
    void updateAxisMotion(int axis);
    void generateStepPulse(int axis);
    void calculateAcceleration(int axis);
    uint32_t calculateStepInterval(int axis);
    
    // Hardware initialization
    bool initializeEncoders();
    void initializeGPIO();
    
    // Motion control task
    static void motionControlTask(void* parameter);
    TaskHandle_t motionTaskHandle;
    
    // Safety
    bool emergencyStop;
    
public:
    ESP32MotionControl();
    ~ESP32MotionControl();
    
    // System initialization
    bool initialize();
    void shutdown();
    
    // Motion control interface
    bool moveRelative(int axis, int32_t steps, bool blocking = false);
    bool moveAbsolute(int axis, int32_t position, bool blocking = false);
    bool setSpeed(int axis, uint32_t speed);
    bool setAcceleration(int axis, uint32_t accel);
    bool stopAxis(int axis);
    bool stopAll();
    
    // Position control
    int32_t getPosition(int axis);
    bool setPosition(int axis, int32_t position);
    
    // Axis control
    bool enableAxis(int axis);
    bool disableAxis(int axis);
    bool isAxisEnabled(int axis);
    bool isAxisMoving(int axis);
    
    // Encoder interface
    int32_t getEncoderCount(int encoderIndex);
    void resetEncoderCount(int encoderIndex);
    int32_t getSpindlePosition() { return getEncoderCount(0); }
    int32_t getXMPGCount() { return getEncoderCount(2); }
    int32_t getZMPGCount() { return getEncoderCount(1); }
    
    // Queue management
    bool queueCommand(const MotionCommand& cmd);
    void processMotionQueue();
    void clearMotionQueue();
    size_t getQueueSize() { return motionQueue.size(); }
    
    // Safety
    void setEmergencyStop(bool stop);
    bool getEmergencyStop() { return emergencyStop; }
    
    // Status and diagnostics
    String getStatusReport();
    void printDiagnostics();
    
    // Update method (call from main loop)
    void update();
    
    // MPG interface
    void processMPGInputs();
    
    // Placeholder methods for compatibility
    bool startTurningMode(float feedRatio, int passes);
    bool stopTurningMode();
    bool isTurningModeActive();
};

// Global instance
extern ESP32MotionControl esp32Motion;

#endif // ESP32MOTIONCONTROL_SIMPLE_H