#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <Arduino.h>

/**
 * StateMachine - Non-blocking state machine for real-time safety-critical systems
 * 
 * Features:
 * - Zero blocking delays
 * - Sub-millisecond emergency stop response
 * - Configurable update rates for each subsystem
 * - Priority-based task scheduling
 * - Performance monitoring
 */

// Task priorities (higher = more critical)
enum TaskPriority {
    PRIORITY_CRITICAL = 0,  // Runs every loop (emergency stop, keyboard)
    PRIORITY_HIGH = 1,      // Runs frequently (motion updates)
    PRIORITY_NORMAL = 2,    // Runs at moderate rate (display)
    PRIORITY_LOW = 3        // Runs occasionally (diagnostics)
};

// System states for main state machine
enum SystemState {
    STATE_EMERGENCY_CHECK,
    STATE_KEYBOARD_SCAN,
    STATE_MOTION_UPDATE,
    STATE_DISPLAY_UPDATE,
    STATE_WEB_UPDATE,
    STATE_DIAGNOSTICS,
    SYS_STATE_IDLE  // Renamed to avoid conflict with OperationManager::STATE_IDLE
};

// Task structure for time-sliced scheduler
struct ScheduledTask {
    const char* name;
    void (*function)();
    TaskPriority priority;
    uint32_t interval_ms;
    uint32_t last_run;
    uint32_t execution_count;
    uint32_t max_duration_us;
    bool enabled;
};

class TimeSlicedScheduler {
private:
    static const int MAX_TASKS = 10;
    ScheduledTask tasks[MAX_TASKS];
    int taskCount;
    uint32_t loopCount;
    uint32_t lastDiagnosticTime;
    
    // Performance tracking
    uint32_t maxLoopTime_us;
    uint32_t totalLoopTime_us;
    uint32_t loopStartTime_us;
    
public:
    TimeSlicedScheduler();
    
    // Task management
    bool addTask(const char* name, void (*function)(), TaskPriority priority, uint32_t interval_ms);
    void enableTask(const char* name, bool enable);
    void updateTaskInterval(const char* name, uint32_t interval_ms);
    
    // Main update function - call from loop()
    void update();
    
    // Performance monitoring
    void printDiagnostics();
    uint32_t getLoopFrequency();
    uint32_t getMaxLoopTime() { return maxLoopTime_us; }
    
    // Emergency override - forces immediate execution of critical tasks
    void executeEmergencyTasks();
};

// Global scheduler instance
extern TimeSlicedScheduler scheduler;

// System state machine class
class SystemStateMachine {
private:
    SystemState currentState;
    uint32_t stateStartTime;
    uint32_t lastStateChange;
    
    // State timing configuration
    struct StateConfig {
        SystemState state;
        uint32_t max_duration_ms;  // Maximum time in state
        bool can_interrupt;        // Can be interrupted by emergency
    };
    
    static const StateConfig stateConfigs[];
    
    // Internal state handlers
    void handleEmergencyCheck();
    void handleKeyboardScan();
    void handleMotionUpdate();
    void handleDisplayUpdate();
    void handleWebUpdate();
    void handleDiagnostics();
    void handleIdle();
    
public:
    SystemStateMachine();
    
    // Main state machine update
    void update();
    
    // State control
    void forceState(SystemState state);
    SystemState getCurrentState() { return currentState; }
    const char* getStateName(SystemState state);
    
    // Emergency handling
    void triggerEmergency();
    
    // Diagnostics
    void printStateDiagnostics();
};

// Global state machine instance
extern SystemStateMachine stateMachine;

// Helper functions for non-blocking delays
class NonBlockingDelay {
private:
    uint32_t startTime;
    uint32_t delayTime;
    bool active;
    
public:
    NonBlockingDelay() : startTime(0), delayTime(0), active(false) {}
    
    void start(uint32_t delay_ms) {
        startTime = millis();
        delayTime = delay_ms;
        active = true;
    }
    
    bool isReady() {
        if (!active) return false;
        if (millis() - startTime >= delayTime) {
            active = false;
            return true;
        }
        return false;
    }
    
    void reset() {
        active = false;
    }
    
    uint32_t elapsed() {
        return active ? (millis() - startTime) : 0;
    }
};

#endif // STATEMACHINE_H