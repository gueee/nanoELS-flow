#include "StateMachine.h"
#include "ESP32MotionControl.h"
#include "NextionDisplay.h"
#include "WebInterface.h"
#include "SetupConstants.h"
#include <PS2KeyAdvanced.h>

// Global instances
TimeSlicedScheduler scheduler;
SystemStateMachine stateMachine;

// External references
extern PS2KeyAdvanced keyboard;
extern int currentMode;
extern float manualStepSize;

// TimeSlicedScheduler Implementation
TimeSlicedScheduler::TimeSlicedScheduler() {
    taskCount = 0;
    loopCount = 0;
    lastDiagnosticTime = 0;
    maxLoopTime_us = 0;
    totalLoopTime_us = 0;
    loopStartTime_us = 0;
}

bool TimeSlicedScheduler::addTask(const char* name, void (*function)(), TaskPriority priority, uint32_t interval_ms) {
    if (taskCount >= MAX_TASKS) {
        Serial.println("ERROR: Maximum tasks reached");
        return false;
    }
    
    tasks[taskCount] = {
        name,
        function,
        priority,
        interval_ms,
        0,  // last_run
        0,  // execution_count
        0,  // max_duration_us
        true  // enabled
    };
    
    taskCount++;
    Serial.printf("✓ Task added: %s (priority=%d, interval=%dms)\n", name, priority, interval_ms);
    return true;
}

void TimeSlicedScheduler::enableTask(const char* name, bool enable) {
    for (int i = 0; i < taskCount; i++) {
        if (strcmp(tasks[i].name, name) == 0) {
            tasks[i].enabled = enable;
            Serial.printf("Task %s %s\n", name, enable ? "enabled" : "disabled");
            return;
        }
    }
}

void TimeSlicedScheduler::updateTaskInterval(const char* name, uint32_t interval_ms) {
    for (int i = 0; i < taskCount; i++) {
        if (strcmp(tasks[i].name, name) == 0) {
            tasks[i].interval_ms = interval_ms;
            return;
        }
    }
}

void TimeSlicedScheduler::update() {
    // Performance tracking
    loopStartTime_us = micros();
    loopCount++;
    
    uint32_t currentTime = millis();
    
    // Execute tasks based on priority and timing
    for (int i = 0; i < taskCount; i++) {
        ScheduledTask& task = tasks[i];
        
        if (!task.enabled) continue;
        
        // Critical tasks run every loop
        bool shouldRun = (task.priority == PRIORITY_CRITICAL) ||
                        (currentTime - task.last_run >= task.interval_ms);
        
        if (shouldRun) {
            uint32_t taskStart = micros();
            
            // Execute task
            task.function();
            
            // Track performance
            uint32_t duration = micros() - taskStart;
            if (duration > task.max_duration_us) {
                task.max_duration_us = duration;
            }
            
            task.execution_count++;
            task.last_run = currentTime;
        }
    }
    
    // Update loop performance metrics
    uint32_t loopDuration = micros() - loopStartTime_us;
    totalLoopTime_us += loopDuration;
    if (loopDuration > maxLoopTime_us) {
        maxLoopTime_us = loopDuration;
    }
    
    // Print diagnostics every 5 seconds
    if (currentTime - lastDiagnosticTime >= 5000) {
        printDiagnostics();
        lastDiagnosticTime = currentTime;
    }
}

void TimeSlicedScheduler::executeEmergencyTasks() {
    // Force execution of all critical tasks immediately
    for (int i = 0; i < taskCount; i++) {
        if (tasks[i].priority == PRIORITY_CRITICAL && tasks[i].enabled) {
            tasks[i].function();
        }
    }
}

void TimeSlicedScheduler::printDiagnostics() {
    Serial.println("\n======= SCHEDULER DIAGNOSTICS =======");
    Serial.printf("Loop frequency: %d Hz\n", getLoopFrequency());
    Serial.printf("Max loop time: %d μs\n", maxLoopTime_us);
    Serial.printf("Avg loop time: %d μs\n", loopCount > 0 ? totalLoopTime_us / loopCount : 0);
    
    Serial.println("\nTask Performance:");
    for (int i = 0; i < taskCount; i++) {
        ScheduledTask& task = tasks[i];
        Serial.printf("  %s: %d runs, max %d μs%s\n", 
                     task.name, 
                     task.execution_count,
                     task.max_duration_us,
                     task.enabled ? "" : " [DISABLED]");
    }
    Serial.println("====================================\n");
    
    // Reset counters
    loopCount = 0;
    totalLoopTime_us = 0;
    maxLoopTime_us = 0;
    for (int i = 0; i < taskCount; i++) {
        tasks[i].execution_count = 0;
        tasks[i].max_duration_us = 0;
    }
}

uint32_t TimeSlicedScheduler::getLoopFrequency() {
    if (totalLoopTime_us == 0) return 0;
    return (loopCount * 1000000UL) / totalLoopTime_us;
}

// SystemStateMachine Implementation
const SystemStateMachine::StateConfig SystemStateMachine::stateConfigs[] = {
    {STATE_EMERGENCY_CHECK, 1, true},     // 1ms max, interruptible
    {STATE_KEYBOARD_SCAN, 2, true},       // 2ms max, interruptible
    {STATE_MOTION_UPDATE, 5, true},       // 5ms max, interruptible
    {STATE_DISPLAY_UPDATE, 10, false},    // 10ms max, not interruptible
    {STATE_WEB_UPDATE, 20, false},        // 20ms max, not interruptible
    {STATE_DIAGNOSTICS, 50, false},       // 50ms max, not interruptible
    {STATE_IDLE, 100, true}               // 100ms max, interruptible
};

SystemStateMachine::SystemStateMachine() {
    currentState = STATE_EMERGENCY_CHECK;
    stateStartTime = 0;
    lastStateChange = 0;
}

void SystemStateMachine::update() {
    uint32_t currentTime = millis();
    
    // Check for state timeout
    uint32_t timeInState = currentTime - stateStartTime;
    const StateConfig* config = nullptr;
    
    for (const auto& cfg : stateConfigs) {
        if (cfg.state == currentState) {
            config = &cfg;
            break;
        }
    }
    
    if (config && timeInState > config->max_duration_ms) {
        Serial.printf("WARNING: State %s exceeded max duration (%dms)\n", 
                     getStateName(currentState), config->max_duration_ms);
    }
    
    // State machine logic
    SystemState previousState = currentState;
    
    switch (currentState) {
        case STATE_EMERGENCY_CHECK:
            handleEmergencyCheck();
            currentState = STATE_KEYBOARD_SCAN;
            break;
            
        case STATE_KEYBOARD_SCAN:
            handleKeyboardScan();
            currentState = STATE_MOTION_UPDATE;
            break;
            
        case STATE_MOTION_UPDATE:
            handleMotionUpdate();
            currentState = STATE_DISPLAY_UPDATE;
            break;
            
        case STATE_DISPLAY_UPDATE:
            handleDisplayUpdate();
            currentState = STATE_WEB_UPDATE;
            break;
            
        case STATE_WEB_UPDATE:
            handleWebUpdate();
            currentState = STATE_DIAGNOSTICS;
            break;
            
        case STATE_DIAGNOSTICS:
            handleDiagnostics();
            currentState = STATE_IDLE;
            break;
            
        case STATE_IDLE:
            handleIdle();
            currentState = STATE_EMERGENCY_CHECK;
            break;
    }
    
    // Track state changes
    if (currentState != previousState) {
        lastStateChange = currentTime;
        stateStartTime = currentTime;
    }
}

void SystemStateMachine::handleEmergencyCheck() {
    // Ultra-fast emergency stop check
    // Check shared flag instead of reading keyboard directly
    extern volatile bool emergencyKeyDetected;
    if (emergencyKeyDetected) {
        triggerEmergency();
        emergencyKeyDetected = false;
    }
}

void SystemStateMachine::handleKeyboardScan() {
    // Full keyboard handling
    extern void handleKeyboard();
    handleKeyboard();
}

void SystemStateMachine::handleMotionUpdate() {
    esp32Motion.update();
}

void SystemStateMachine::handleDisplayUpdate() {
    static uint32_t lastDisplayUpdate = 0;
    uint32_t currentTime = millis();
    
    // Limit display updates to 20Hz
    if (currentTime - lastDisplayUpdate >= 50) {
        nextionDisplay.update();
        lastDisplayUpdate = currentTime;
    }
}

void SystemStateMachine::handleWebUpdate() {
    static uint32_t lastWebUpdate = 0;
    uint32_t currentTime = millis();
    
    // Limit web updates to 50Hz
    if (currentTime - lastWebUpdate >= 20) {
        webInterface.update();
        lastWebUpdate = currentTime;
    }
}

void SystemStateMachine::handleDiagnostics() {
    static uint32_t lastDiagnostics = 0;
    uint32_t currentTime = millis();
    
    // Run diagnostics every second
    if (currentTime - lastDiagnostics >= 1000) {
        // Optional: Add diagnostic code here
        lastDiagnostics = currentTime;
    }
}

void SystemStateMachine::handleIdle() {
    // Idle state - can yield to system
    // This is where we would have had delay(10) before
    // Now we just continue immediately, no blocking!
}

void SystemStateMachine::triggerEmergency() {
    Serial.println("*** EMERGENCY STOP TRIGGERED ***");
    esp32Motion.setEmergencyStop(true);
    esp32Motion.stopTestSequence();
    nextionDisplay.showEmergencyStop();
    
    // Force immediate transition to emergency check
    currentState = STATE_EMERGENCY_CHECK;
}

void SystemStateMachine::forceState(SystemState state) {
    currentState = state;
    stateStartTime = millis();
}

const char* SystemStateMachine::getStateName(SystemState state) {
    switch (state) {
        case STATE_EMERGENCY_CHECK: return "EMERGENCY_CHECK";
        case STATE_KEYBOARD_SCAN: return "KEYBOARD_SCAN";
        case STATE_MOTION_UPDATE: return "MOTION_UPDATE";
        case STATE_DISPLAY_UPDATE: return "DISPLAY_UPDATE";
        case STATE_WEB_UPDATE: return "WEB_UPDATE";
        case STATE_DIAGNOSTICS: return "DIAGNOSTICS";
        case STATE_IDLE: return "IDLE";
        default: return "UNKNOWN";
    }
}

void SystemStateMachine::printStateDiagnostics() {
    Serial.printf("Current state: %s (duration: %dms)\n", 
                 getStateName(currentState), 
                 millis() - stateStartTime);
}