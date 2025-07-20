#ifndef ESP32MOTIONCONTROL_H
#define ESP32MOTIONCONTROL_H

#include <Arduino.h>
#include "MyHardware.h"
#include "SetupConstants.h"

/**
 * ESP32MotionControl - PID Position Controller for ClearPath Servos
 * 
 * Simple servo-like position controller:
 * 1. Target position input
 * 2. PID control loop
 * 3. Step/Direction output for ClearPath servos
 * 4. Position feedback simulation
 * 5. Test motion sequences
 */

// Hardware configuration constants (based on h5.ino pattern)
// These should match your hardware setup

// Motion limits are now defined in SetupConstants.h as user-editable configuration

// Fixed-point math definitions (Q24.8 format like ClearPath)
#define FIXED_POINT_SHIFT 8
#define FIXED_POINT_SCALE (1 << FIXED_POINT_SHIFT)  // 256
#define FLOAT_TO_FIXED(x) ((int32_t)((x) * FIXED_POINT_SCALE))
#define FIXED_TO_FLOAT(x) ((float)(x) / FIXED_POINT_SCALE)

// Motion profile structure (inspired by ClearPath)
struct MotionProfile {
    enum Phase {
        IDLE,
        ACCELERATION,
        CONSTANT_VELOCITY,
        DECELERATION,
        COMPLETED
    } currentPhase;
    
    // Fixed-point position values (Q24.8 format)
    int32_t targetPosition;     // Target position in fixed-point
    int32_t currentPosition;    // Current position in fixed-point
    int32_t startPosition;      // Move start position
    
    // Velocity profile parameters (fixed-point)
    int32_t maxVelocity;        // Maximum velocity (steps/sec * 256)
    int32_t maxAcceleration;    // Maximum acceleration (steps/sec² * 256)
    int32_t currentVelocity;    // Current velocity (steps/sec * 256)
    
    // Pre-computed motion parameters
    int32_t accelDistance;      // Distance to accelerate
    int32_t decelDistance;      // Distance to decelerate
    int32_t totalDistance;      // Total move distance
    int32_t accelTime;          // Time to accelerate (ms * 256)
    int32_t constantTime;       // Time at constant velocity
    int32_t decelTime;          // Time to decelerate
    
    // Motion state
    uint32_t phaseStartTime;    // Phase start time (ms)
    uint32_t moveStartTime;     // Move start time (ms)
    bool moveActive;            // Move in progress
    bool moveCompleted;         // Move completed flag
};

// PID Controller structure (enhanced with fixed-point)
struct PIDController {
    int32_t kP;              // Proportional gain (fixed-point)
    int32_t kI;              // Integral gain (fixed-point)
    int32_t kD;              // Derivative gain (fixed-point)
    
    int32_t lastError;       // Previous error for derivative (fixed-point)
    int32_t integral;        // Accumulated error for integral (fixed-point)
    int32_t maxOutput;       // Maximum output limit (fixed-point)
    int32_t minOutput;       // Minimum output limit (fixed-point)
    
    uint32_t lastUpdateTime; // For derivative calculation
};

// Enhanced servo axis configuration (ClearPath inspired)
struct AxisConfig {
    // GPIO configuration
    uint8_t stepPin;
    uint8_t dirPin;
    uint8_t enablePin;
    
    // Fixed-point position control (Q24.8 format)
    volatile int32_t currentPosition;    // Current position in steps (fixed-point)
    volatile int32_t commandedPosition;  // Commanded position from moves
    volatile int32_t positionError;      // Position error (fixed-point)
    
    // Servo specifications (ClearPath CPM-SDSK-2321S)
    int32_t pulsesPerMM;                // Pulses per mm (fixed-point)
    int32_t maxVelocity;                // Maximum velocity (steps/sec, fixed-point)
    int32_t maxAcceleration;            // Maximum acceleration (steps/sec², fixed-point)
    
    // Safety limits (in steps, fixed-point)
    int32_t minPosition;                // Minimum allowed position
    int32_t maxPosition;                // Maximum allowed position
    bool limitsEnabled;                 // Software limits enabled
    
    // Step generation (deterministic timing)
    volatile int32_t stepCount;         // Total steps generated
    volatile int32_t stepsToGo;         // Steps remaining in current move
    uint32_t stepInterval;              // Current step interval (microseconds)
    volatile bool stepState;            // Current step pin state
    volatile bool stepPending;          // Step pulse pending
    
    // Motion profile
    MotionProfile profile;
    
    // PID controller (fallback for fine positioning)
    PIDController pid;
    
    // Status flags
    bool enabled;
    volatile bool moving;
    bool invertEnable;        // Enable pin inversion
    bool invertStep;          // Step pin inversion
    
    // Motion state
    enum State {
        IDLE,
        PROFILE_MOVE,      // Using motion profile
        PID_FOLLOWING,     // Using PID control
        HOLDING
    } state;
    
    // Performance tracking
    uint32_t lastProfileUpdate;    // Last profile update time
    uint32_t profileUpdateInterval; // Profile update interval (microseconds)
};

// Test sequence configuration
struct TestSequence {
    struct Move {
        float xTarget;    // X target position in mm
        float zTarget;    // Z target position in mm
        uint32_t holdTime; // Time to hold position in ms
    };
    
    Move moves[4];        // Array of test moves
    uint8_t currentMove;  // Current move index
    uint8_t cycleCount;   // Number of cycles completed
    uint8_t maxCycles;    // Maximum cycles to run
    uint32_t moveStartTime; // Time when current move started
    bool active;          // Test sequence active
    bool completed;       // Test sequence completed
};

class ESP32MotionControl {
private:
    // Hardware configuration
    AxisConfig axes[2];         // X and Z axes (0=X, 1=Z)
    
    // Test sequence
    TestSequence testSequence;
    
    // Static instance for ISR access
    static ESP32MotionControl* instance;
    
    // Motion control task (deterministic timing)
    static void motionControlTask(void* parameter);
    TaskHandle_t motionTaskHandle;
    
    // Step pulse generation (fixed 2kHz like ClearPath)
    static void IRAM_ATTR stepGeneratorISR();
    hw_timer_t* stepGeneratorTimer;
    
    // Internal methods - Motion Profile Based
    void updateAxisProfile(int axis);
    void calculateMotionProfile(int axis, int32_t targetPos);
    void updateProfilePhase(int axis);
    int32_t calculateProfileVelocity(int axis);
    
    // Internal methods - Step Generation
    void generateStepPulse(int axis);
    void updateStepTiming(int axis);
    void updateStepGeneration(int axis);
    
    // Internal methods - PID Fallback
    void updateAxisPID(int axis);
    int32_t calculatePIDOutput(int axis);
    
    // Internal methods - Fixed-Point Math
    int32_t mmToSteps(int axis, float mm);
    float stepsToMM(int axis, int32_t steps);
    int32_t calculateAccelDistance(int32_t velocity, int32_t acceleration);
    int32_t calculateAccelTime(int32_t velocity, int32_t acceleration);
    
    // Position control
    void setAxisTarget(int axis, float targetMM);
    float getAxisPosition(int axis);
    
    // Test sequence methods
    void initializeTestSequence();
    void updateTestSequence();
    
    // Hardware initialization
    void initializeGPIO();
    void initializePID();
    void initializeStepTimers();
    
    // Safety
    bool emergencyStop;
    
public:
    ESP32MotionControl();
    ~ESP32MotionControl();
    
    // System initialization
    bool initialize();
    void shutdown();
    
    // Position control interface (enhanced)
    bool setTargetPosition(int axis, float positionMM);
    bool moveToPosition(int axis, float positionMM);  // Profile-based move
    bool moveRelative(int axis, int steps);  // Move relative by steps
    float getPosition(int axis);
    float getTargetPosition(int axis);
    float getPositionError(int axis);
    bool isMoving(int axis);
    bool moveCompleted(int axis);
    
    // Motion profile interface
    bool setMotionLimits(int axis, float maxVel, float maxAccel);
    void getMotionLimits(int axis, float& maxVel, float& maxAccel);
    MotionProfile::Phase getMotionPhase(int axis);
    float getProfileVelocity(int axis);
    
    // Axis control
    bool enableAxis(int axis);
    bool disableAxis(int axis);
    bool isAxisEnabled(int axis);
    bool isAxisMoving(int axis);
    
    // Test sequence control
    void startTestSequence();
    void stopTestSequence();
    void restartTestSequence();
    bool isTestSequenceActive();
    bool isTestSequenceCompleted();
    String getTestSequenceStatus();
    
    // PID tuning
    void setPIDGains(int axis, float kP, float kI, float kD);
    void getPIDGains(int axis, float& kP, float& kI, float& kD);
    
    // Safety
    void setEmergencyStop(bool stop);
    bool getEmergencyStop() { return emergencyStop; }
    
    // Safety limits
    bool setSoftwareLimits(int axis, float minMM, float maxMM);
    void getSoftwareLimits(int axis, float& minMM, float& maxMM);
    bool isPositionSafe(int axis, float positionMM);
    
    // Status and diagnostics
    String getStatusReport();
    void printDiagnostics();
    
    // Update method (call from main loop)
    void update();
    
    // Compatibility methods for display
    int32_t getSpindlePosition() { return 0; }
    int32_t getXMPGCount() { return 0; }
    int32_t getZMPGCount() { return 0; }
    int32_t getAxisMPGTargetPosition(int axis) { return (int32_t)(getTargetPosition(axis) * 1000); }
    uint32_t getAxisStepCount(int axis) { return (axis >= 0 && axis < 2) ? axes[axis].stepCount : 0; }
};

// Global instance
extern ESP32MotionControl esp32Motion;

#endif // ESP32MOTIONCONTROL_H