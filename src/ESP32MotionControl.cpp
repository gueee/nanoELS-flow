#include "ESP32MotionControl.h"

// Global instance
ESP32MotionControl esp32Motion;

// Static instance pointer for ISR access
ESP32MotionControl* ESP32MotionControl::instance = nullptr;

ESP32MotionControl::ESP32MotionControl() {
    instance = this;
    motionTaskHandle = nullptr;
    stepGeneratorTimer = nullptr;
    emergencyStop = false;
    
    // Initialize test sequence
    testSequence.active = false;
    testSequence.completed = false;
    testSequence.currentMove = 0;
    testSequence.cycleCount = 0;
    testSequence.maxCycles = 3;
    testSequence.moveStartTime = 0;
}

ESP32MotionControl::~ESP32MotionControl() {
    shutdown();
}

bool ESP32MotionControl::initialize() {
    Serial.println("Initializing ESP32 Motion Control - PID Position Controller");
    
    // Initialize hardware
    initializeGPIO();
    initializePID();
    initializeStepTimers();
    initializeTestSequence();
    
    // Create motion control task
    BaseType_t result = xTaskCreatePinnedToCore(
        motionControlTask,
        "MotionControl",
        4096,
        this,
        2,  // High priority
        &motionTaskHandle,
        1   // Run on Core 1
    );
    
    if (result != pdPASS) {
        Serial.println("Failed to create motion control task");
        return false;
    }
    
    Serial.println("✓ ESP32 Motion Control initialized successfully");
    return true;
}

void ESP32MotionControl::shutdown() {
    // Stop test sequence
    stopTestSequence();
    
    // Disable all axes
    disableAxis(0);
    disableAxis(1);
    
    // Stop step generator timer
    if (stepGeneratorTimer) {
        timerEnd(stepGeneratorTimer);
        stepGeneratorTimer = nullptr;
    }
    
    // Delete motion control task
    if (motionTaskHandle) {
        vTaskDelete(motionTaskHandle);
        motionTaskHandle = nullptr;
    }
    
    Serial.println("ESP32 Motion Control shutdown complete");
}

void ESP32MotionControl::initializeGPIO() {
    // X-axis (axis 0) - Fixed-point configuration with hardware constants
    axes[0].stepPin = X_STEP;
    axes[0].dirPin = X_DIR;
    axes[0].enablePin = X_ENA;
    axes[0].pulsesPerMM = FLOAT_TO_FIXED(4000.0 / 4.0);  // 1000 pulses/mm (fixed-point)
    axes[0].maxVelocity = FLOAT_TO_FIXED(MAX_VELOCITY_X * 1000.0);  // Convert to steps/s
    axes[0].maxAcceleration = FLOAT_TO_FIXED(MAX_ACCELERATION_X * 1000.0);  // Convert to steps/s²
    axes[0].invertEnable = INVERT_X_ENABLE;
    axes[0].invertStep = INVERT_X_STEP;
    
    // Z-axis (axis 1) - Fixed-point configuration with hardware constants
    axes[1].stepPin = Z_STEP;
    axes[1].dirPin = Z_DIR;
    axes[1].enablePin = Z_ENA;
    axes[1].pulsesPerMM = FLOAT_TO_FIXED(4000.0 / 5.0);  // 800 pulses/mm (fixed-point)
    axes[1].maxVelocity = FLOAT_TO_FIXED(MAX_VELOCITY_Z * 800.0);  // Convert to steps/s
    axes[1].maxAcceleration = FLOAT_TO_FIXED(MAX_ACCELERATION_Z * 800.0);  // Convert to steps/s²
    axes[1].invertEnable = INVERT_Z_ENABLE;
    axes[1].invertStep = INVERT_Z_STEP;
    
    // Initialize GPIO pins and state
    for (int i = 0; i < 2; i++) {
        pinMode(axes[i].stepPin, OUTPUT);
        pinMode(axes[i].dirPin, OUTPUT);
        pinMode(axes[i].enablePin, OUTPUT);
        
        // Initialize step and direction pins considering inversion
        bool stepLow = axes[i].invertStep ? HIGH : LOW;
        digitalWrite(axes[i].stepPin, stepLow);
        digitalWrite(axes[i].dirPin, LOW);  // Direction will be set during moves
        // CRITICAL: Hardware-specific enable pin polarity
        // Using hardware constants for proper inversion
        bool disableState = axes[i].invertEnable ? HIGH : LOW;
        digitalWrite(axes[i].enablePin, disableState);  // Disable initially
        
        // Initialize fixed-point position and state
        axes[i].currentPosition = 0;
        axes[i].commandedPosition = 0;
        axes[i].positionError = 0;
        axes[i].stepCount = 0;
        axes[i].stepsToGo = 0;
        axes[i].stepInterval = 1000000;  // 1 Hz default
        axes[i].stepState = false;
        axes[i].stepPending = false;
        axes[i].enabled = false;
        axes[i].moving = false;
        axes[i].state = AxisConfig::IDLE;
        
        // Initialize safety limits (using hardware constants)
        float maxTravel = (i == 0) ? MAX_TRAVEL_MM_X : MAX_TRAVEL_MM_Z;
        axes[i].minPosition = FLOAT_TO_FIXED(-maxTravel * FIXED_TO_FLOAT(axes[i].pulsesPerMM));
        axes[i].maxPosition = FLOAT_TO_FIXED(maxTravel * FIXED_TO_FLOAT(axes[i].pulsesPerMM));
        axes[i].limitsEnabled = true;  // Enable software limits by default
        
        // Initialize motion profile
        axes[i].profile.currentPhase = MotionProfile::IDLE;
        axes[i].profile.targetPosition = 0;
        axes[i].profile.currentPosition = 0;
        axes[i].profile.currentVelocity = 0;
        axes[i].profile.moveActive = false;
        axes[i].profile.moveCompleted = false;
        
        // Initialize profile update timing
        axes[i].lastProfileUpdate = 0;
        axes[i].profileUpdateInterval = 500;  // 500μs = 2kHz like ClearPath
    }
    
    Serial.println("✓ GPIO initialized with hardware-specific inversion (fixed-point)");
    Serial.printf("  X-axis: Dir=%s, Enable=%s, Step=%s\n", 
                 INVERT_X ? "INV" : "NORM",
                 axes[0].invertEnable ? "INV" : "NORM",
                 axes[0].invertStep ? "INV" : "NORM");
    Serial.printf("  Z-axis: Dir=%s, Enable=%s, Step=%s\n", 
                 INVERT_Z ? "INV" : "NORM",
                 axes[1].invertEnable ? "INV" : "NORM",
                 axes[1].invertStep ? "INV" : "NORM");
    
    // Enable axes by default for motion capability
    Serial.println("Enabling axes by default...");
    enableAxis(0);  // Enable X-axis
    enableAxis(1);  // Enable Z-axis
}

void ESP32MotionControl::initializePID() {
    // Initialize PID controllers for both axes (fixed-point)
    for (int i = 0; i < 2; i++) {
        axes[i].pid.kP = FLOAT_TO_FIXED(10.0);        // Proportional gain
        axes[i].pid.kI = FLOAT_TO_FIXED(0.1);         // Integral gain
        axes[i].pid.kD = FLOAT_TO_FIXED(0.05);        // Derivative gain
        axes[i].pid.lastError = 0;
        axes[i].pid.integral = 0;
        axes[i].pid.maxOutput = FLOAT_TO_FIXED(100.0);  // Maximum velocity command
        axes[i].pid.minOutput = FLOAT_TO_FIXED(-100.0); // Minimum velocity command
        axes[i].pid.lastUpdateTime = 0;
    }
    
    Serial.println("✓ PID controllers initialized (fixed-point)");
}

void ESP32MotionControl::initializeStepTimers() {
    // Initialize single step generator timer (2kHz like ClearPath)
    // Dual compatibility for Arduino IDE 3.x and PlatformIO 2.x
    
    #ifdef ESP_ARDUINO_VERSION_MAJOR
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            // Arduino IDE ESP32 3.x API
            stepGeneratorTimer = timerBegin(2000);  // 2kHz frequency
            timerAttachInterrupt(stepGeneratorTimer, &stepGeneratorISR);
            Serial.println("✓ Step generator timer initialized (2kHz) - Arduino 3.x API");
        #else
            // Arduino IDE ESP32 2.x API  
            stepGeneratorTimer = timerBegin(0, 80, true);  // Timer 0, prescaler 80, count up
            timerAttachInterrupt(stepGeneratorTimer, &stepGeneratorISR, true);
            timerAlarmWrite(stepGeneratorTimer, 500, true);  // 500μs
            timerAlarmEnable(stepGeneratorTimer);
            Serial.println("✓ Step generator timer initialized (2kHz) - Arduino 2.x API");
        #endif
    #else
        // PlatformIO/older versions - assume 2.x API
        stepGeneratorTimer = timerBegin(0, 80, true);  // Timer 0, prescaler 80, count up
        timerAttachInterrupt(stepGeneratorTimer, &stepGeneratorISR, true);
        timerAlarmWrite(stepGeneratorTimer, 500, true);  // 500μs
        timerAlarmEnable(stepGeneratorTimer);
        Serial.println("✓ Step generator timer initialized (2kHz) - Legacy API");
    #endif
}

void ESP32MotionControl::initializeTestSequence() {
    // Test sequence: Move Z left 20mm, X in 8mm, Z right 20mm, X out 8mm
    testSequence.moves[0] = {0.0, -20.0, 2000};  // Z left 20mm, hold 2s
    testSequence.moves[1] = {8.0, -20.0, 2000};  // X in 8mm, hold 2s
    testSequence.moves[2] = {8.0, 0.0, 2000};    // Z right 20mm, hold 2s
    testSequence.moves[3] = {0.0, 0.0, 2000};    // X out 8mm, hold 2s
    
    testSequence.currentMove = 0;
    testSequence.cycleCount = 0;
    testSequence.maxCycles = 3;
    testSequence.moveStartTime = 0;
    testSequence.active = false;
    testSequence.completed = false;
    
    Serial.println("✓ Test sequence initialized (3 cycles, 20mm Z, 8mm X)");
}

void IRAM_ATTR ESP32MotionControl::stepGeneratorISR() {
    if (instance) {
        // 2kHz step generator - handles both axes
        instance->generateStepPulse(0);  // X-axis
        instance->generateStepPulse(1);  // Z-axis
    }
}

void ESP32MotionControl::generateStepPulse(int axis) {
    if (!axes[axis].enabled || emergencyStop) return;
    
    // Check if step is needed (based on profile or steps remaining)
    if (axes[axis].stepsToGo <= 0 && !axes[axis].stepPending) return;
    
    // Generate step pulse with hardware-specific inversion
    if (axes[axis].stepState) {
        // Step pulse LOW phase (considering inversion)
        bool lowState = axes[axis].invertStep ? HIGH : LOW;
        digitalWrite(axes[axis].stepPin, lowState);
        axes[axis].stepState = false;
        axes[axis].stepPending = false;
    } else if (axes[axis].stepPending || axes[axis].stepsToGo > 0) {
        // Step pulse HIGH phase (considering inversion)
        bool highState = axes[axis].invertStep ? LOW : HIGH;
        digitalWrite(axes[axis].stepPin, highState);
        axes[axis].stepState = true;
        axes[axis].stepPending = true;
        
        // Update position and counters
        axes[axis].stepCount++;
        if (axes[axis].stepsToGo > 0) {
            axes[axis].stepsToGo--;
        }
        
        // Update position based on step direction (fixed-point)
        bool direction = digitalRead(axes[axis].dirPin);
        if ((axis == 0 && INVERT_X) || (axis == 1 && INVERT_Z)) direction = !direction;
        
        if (direction) {
            axes[axis].currentPosition += FIXED_POINT_SCALE;  // +1 step
        } else {
            axes[axis].currentPosition -= FIXED_POINT_SCALE;  // -1 step
        }
    }
}

void ESP32MotionControl::motionControlTask(void* parameter) {
    ESP32MotionControl* controller = static_cast<ESP32MotionControl*>(parameter);
    
    const TickType_t xFrequency = pdMS_TO_TICKS(1);  // 1kHz update rate (faster than ClearPath)
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (true) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        if (controller->emergencyStop) {
            continue;
        }
        
        // Update motion profiles for both axes
        controller->updateAxisProfile(0);  // X-axis
        controller->updateAxisProfile(1);  // Z-axis
        
        // Update test sequence
        if (controller->testSequence.active) {
            controller->updateTestSequence();
        }
    }
}

// Motion profile update (ClearPath-inspired)
void ESP32MotionControl::updateAxisProfile(int axis) {
    if (!axes[axis].enabled || emergencyStop) return;
    
    MotionProfile& profile = axes[axis].profile;
    
    // Skip if no active move
    if (!profile.moveActive) {
        axes[axis].moving = false;
        axes[axis].state = AxisConfig::IDLE;
        return;
    }
    
    uint32_t currentTime = millis();
    uint32_t elapsedTime = currentTime - profile.moveStartTime;
    
    // Update profile phase and calculate current velocity
    updateProfilePhase(axis);
    int32_t targetVelocity = calculateProfileVelocity(axis);
    
    // Update step generation based on profile
    updateStepTiming(axis);
    
    // Update motion state
    if (profile.currentPhase == MotionProfile::COMPLETED) {
        axes[axis].moving = false;
        axes[axis].state = AxisConfig::IDLE;
        profile.moveActive = false;
        profile.moveCompleted = true;
        axes[axis].stepsToGo = 0;
    } else {
        axes[axis].moving = true;
        axes[axis].state = AxisConfig::PROFILE_MOVE;
    }
}

int32_t ESP32MotionControl::calculatePIDOutput(int axis) {
    PIDController& pid = axes[axis].pid;
    uint32_t currentTime = millis();
    
    if (pid.lastUpdateTime == 0) {
        pid.lastUpdateTime = currentTime;
        return 0.0;
    }
    
    float deltaTime = (currentTime - pid.lastUpdateTime) / 1000.0;  // Convert to seconds
    if (deltaTime <= 0) return 0.0;
    
    float error = axes[axis].positionError;
    
    // Proportional term
    float pTerm = pid.kP * error;
    
    // Integral term
    pid.integral += error * deltaTime;
    float iTerm = pid.kI * pid.integral;
    
    // Derivative term
    float dError = (error - pid.lastError) / deltaTime;
    float dTerm = pid.kD * dError;
    
    // Calculate total output
    float output = pTerm + iTerm + dTerm;
    
    // Limit output
    if (output > pid.maxOutput) output = pid.maxOutput;
    if (output < pid.minOutput) output = pid.minOutput;
    
    // Store values for next iteration
    pid.lastError = error;
    pid.lastUpdateTime = currentTime;
    
    return output;
}

// Calculate motion profile for triangular velocity (ClearPath-inspired)
void ESP32MotionControl::calculateMotionProfile(int axis, int32_t targetPos) {
    MotionProfile& profile = axes[axis].profile;
    
    // Calculate move parameters
    profile.startPosition = axes[axis].currentPosition;
    profile.targetPosition = targetPos;
    profile.totalDistance = abs(targetPos - axes[axis].currentPosition);
    
    if (profile.totalDistance == 0) {
        profile.moveActive = false;
        return;
    }
    
    // Calculate acceleration distance (distance to reach max velocity)
    int32_t maxVel = axes[axis].maxVelocity;
    int32_t maxAccel = axes[axis].maxAcceleration;
    
    profile.accelDistance = calculateAccelDistance(maxVel, maxAccel);
    profile.decelDistance = profile.accelDistance;  // Symmetric profile
    
    // Check if we can reach max velocity
    if (profile.accelDistance + profile.decelDistance > profile.totalDistance) {
        // Triangular profile - can't reach max velocity
        profile.accelDistance = profile.totalDistance / 2;
        profile.decelDistance = profile.totalDistance - profile.accelDistance;
        maxVel = calculateProfileVelocity(axis);  // Recalculate peak velocity
    }
    
    // Calculate timing
    profile.accelTime = calculateAccelTime(maxVel, maxAccel);
    profile.decelTime = profile.accelTime;
    
    int32_t constantDistance = profile.totalDistance - profile.accelDistance - profile.decelDistance;
    if (constantDistance > 0 && maxVel > 0) {
        profile.constantTime = (constantDistance * 1000) / FIXED_TO_FLOAT(maxVel);  // ms
    } else {
        profile.constantTime = 0;
    }
    
    // Initialize move state
    profile.currentPhase = MotionProfile::ACCELERATION;
    profile.currentVelocity = 0;
    profile.moveStartTime = millis();
    profile.phaseStartTime = profile.moveStartTime;
    profile.moveActive = true;
    profile.moveCompleted = false;
    
    // Set direction with hardware-specific inversion
    bool direction = (targetPos > axes[axis].currentPosition);
    if ((axis == 0 && INVERT_X) || (axis == 1 && INVERT_Z)) direction = !direction;
    digitalWrite(axes[axis].dirPin, direction ? HIGH : LOW);
    
    Serial.printf("Profile calculated: Axis %d, Distance=%d, AccelDist=%d, MaxVel=%d\n", 
                 axis, profile.totalDistance, profile.accelDistance, maxVel);
}

// Update motion profile phase
void ESP32MotionControl::updateProfilePhase(int axis) {
    MotionProfile& profile = axes[axis].profile;
    uint32_t currentTime = millis();
    uint32_t elapsedTime = currentTime - profile.phaseStartTime;
    
    switch (profile.currentPhase) {
        case MotionProfile::ACCELERATION:
            if (elapsedTime >= FIXED_TO_FLOAT(profile.accelTime)) {
                profile.currentPhase = MotionProfile::CONSTANT_VELOCITY;
                profile.phaseStartTime = currentTime;
            }
            break;
            
        case MotionProfile::CONSTANT_VELOCITY:
            if (elapsedTime >= profile.constantTime) {
                profile.currentPhase = MotionProfile::DECELERATION;
                profile.phaseStartTime = currentTime;
            }
            break;
            
        case MotionProfile::DECELERATION:
            if (elapsedTime >= FIXED_TO_FLOAT(profile.decelTime)) {
                profile.currentPhase = MotionProfile::COMPLETED;
                profile.currentVelocity = 0;
            }
            break;
            
        default:
            break;
    }
}

// Calculate current velocity based on profile phase
int32_t ESP32MotionControl::calculateProfileVelocity(int axis) {
    MotionProfile& profile = axes[axis].profile;
    uint32_t currentTime = millis();
    uint32_t elapsedTime = currentTime - profile.phaseStartTime;
    
    int32_t maxVel = axes[axis].maxVelocity;
    int32_t maxAccel = axes[axis].maxAcceleration;
    
    switch (profile.currentPhase) {
        case MotionProfile::ACCELERATION: {
            // Linear acceleration
            int32_t velocity = (maxAccel * elapsedTime) / 1000;  // Convert ms to seconds
            profile.currentVelocity = min(velocity, maxVel);
            break;
        }
        
        case MotionProfile::CONSTANT_VELOCITY:
            profile.currentVelocity = maxVel;
            break;
            
        case MotionProfile::DECELERATION: {
            // Linear deceleration
            int32_t decelElapsed = elapsedTime;
            int32_t velocity = maxVel - (maxAccel * decelElapsed) / 1000;
            profile.currentVelocity = max(velocity, (int32_t)0);
            break;
        }
        
        default:
            profile.currentVelocity = 0;
            break;
    }
    
    return profile.currentVelocity;
}

// Update step timing based on current velocity
void ESP32MotionControl::updateStepTiming(int axis) {
    int32_t velocity = axes[axis].profile.currentVelocity;
    
    if (velocity <= 0) {
        axes[axis].stepsToGo = 0;
        return;
    }
    
    // Calculate steps needed in next 500μs (2kHz rate)
    int32_t velocityHz = FIXED_TO_FLOAT(velocity);
    int32_t stepsNeeded = (velocityHz * 500) / 1000000;  // steps in 500μs
    
    if (stepsNeeded < 1) stepsNeeded = 1;  // Minimum 1 step
    
    axes[axis].stepsToGo = stepsNeeded;
}

void ESP32MotionControl::updateStepGeneration(int axis) {
    if (!axes[axis].enabled || emergencyStop) {
        axes[axis].stepsToGo = 0;
        axes[axis].moving = false;
        return;
    }
    
    // Get position error in fixed-point format
    int32_t error = axes[axis].positionError;
    int32_t absError = abs(error);
    
    // If within tolerance, stop movement
    if (absError < FLOAT_TO_FIXED(0.01)) {  // Within 0.01mm tolerance
        axes[axis].stepsToGo = 0;
        axes[axis].moving = false;
        return;
    }
    
    // Set direction based on error direction
    bool direction = error > 0;
    
    // Apply hardware-specific direction inversion
    bool dirState = direction;
    if ((axis == 0 && INVERT_X) || (axis == 1 && INVERT_Z)) {
        dirState = !dirState;
    }
    digitalWrite(axes[axis].dirPin, dirState ? HIGH : LOW);
    
    // Calculate step rate based on position error (simple proportional control)
    float errorMM = FIXED_TO_FLOAT(absError);
    float maxFreqHz = FIXED_TO_FLOAT(axes[axis].maxVelocity) * 1000.0;  // Convert to Hz
    float minFreqHz = 100.0;  // Minimum step rate
    
    // Proportional control: larger error = higher frequency
    float freq = errorMM * 2000.0;  // Scale factor for responsiveness
    if (freq > maxFreqHz) freq = maxFreqHz;
    if (freq < minFreqHz) freq = minFreqHz;
    
    // Convert to step interval for ISR (the ISR runs at 2kHz, so calculate steps per ISR cycle)
    float stepsPerSecond = freq;
    float stepsPerISRCycle = stepsPerSecond / 2000.0;  // 2kHz ISR rate
    
    // Set minimum of 1 step if movement is needed
    if (stepsPerISRCycle < 1.0 && absError > 0) stepsPerISRCycle = 1.0;
    
    axes[axis].stepsToGo = (int32_t)stepsPerISRCycle;
    axes[axis].moving = (axes[axis].stepsToGo > 0);
}

void ESP32MotionControl::updateTestSequence() {
    if (!testSequence.active || testSequence.completed) return;
    
    uint32_t currentTime = millis();
    
    // Check if we need to start a new move
    if (testSequence.moveStartTime == 0) {
        testSequence.moveStartTime = currentTime;
        
        // Set target positions for current move
        TestSequence::Move& move = testSequence.moves[testSequence.currentMove];
        setTargetPosition(0, move.xTarget);  // X-axis
        setTargetPosition(1, move.zTarget);  // Z-axis
        
        Serial.printf("Test Move %d: X=%.1fmm, Z=%.1fmm\n", 
                     testSequence.currentMove, move.xTarget, move.zTarget);
    }
    
    // Check if hold time has elapsed
    TestSequence::Move& currentMove = testSequence.moves[testSequence.currentMove];
    if (currentTime - testSequence.moveStartTime >= currentMove.holdTime) {
        // Move to next move
        testSequence.currentMove++;
        testSequence.moveStartTime = 0;
        
        // Check if we completed all moves in a cycle
        if (testSequence.currentMove >= 4) {
            testSequence.currentMove = 0;
            testSequence.cycleCount++;
            
            Serial.printf("Test Cycle %d completed\n", testSequence.cycleCount);
            
            // Check if we completed all cycles
            if (testSequence.cycleCount >= testSequence.maxCycles) {
                testSequence.active = false;
                testSequence.completed = true;
                Serial.println("==== TEST SEQUENCE COMPLETED ====");
                Serial.printf("Completed %d cycles successfully\n", testSequence.maxCycles);
                Serial.println("Press ENTER to restart test sequence");
                Serial.println("====================================");
            }
        }
    }
}

// Fixed-point math helper methods
int32_t ESP32MotionControl::mmToSteps(int axis, float mm) {
    if (axis < 0 || axis >= 2) return 0;
    return (int32_t)(mm * FIXED_TO_FLOAT(axes[axis].pulsesPerMM));
}

float ESP32MotionControl::stepsToMM(int axis, int32_t steps) {
    if (axis < 0 || axis >= 2) return 0.0;
    return (float)steps / FIXED_TO_FLOAT(axes[axis].pulsesPerMM);
}

int32_t ESP32MotionControl::calculateAccelDistance(int32_t velocity, int32_t acceleration) {
    if (acceleration <= 0) return 0;
    // d = v² / (2*a)
    int64_t velSquared = (int64_t)velocity * velocity;
    return (int32_t)(velSquared / (2 * acceleration));
}

int32_t ESP32MotionControl::calculateAccelTime(int32_t velocity, int32_t acceleration) {
    if (acceleration <= 0) return 0;
    // t = v / a (in milliseconds)
    return FLOAT_TO_FIXED((FIXED_TO_FLOAT(velocity) * 1000.0) / FIXED_TO_FLOAT(acceleration));
}

// Enhanced public interface methods with safety bounds
bool ESP32MotionControl::setTargetPosition(int axis, float positionMM) {
    if (axis < 0 || axis >= 2) return false;
    
    // CRITICAL: Check software limits before moving
    if (!isPositionSafe(axis, positionMM)) {
        Serial.printf("ERROR: Target position %.2fmm is outside safe limits for axis %d\n", positionMM, axis);
        return false;
    }
    
    // Convert to fixed-point and start profile move
    int32_t targetSteps = mmToSteps(axis, positionMM);
    calculateMotionProfile(axis, targetSteps);
    return true;
}

bool ESP32MotionControl::moveToPosition(int axis, float positionMM) {
    return setTargetPosition(axis, positionMM);
}

bool ESP32MotionControl::moveRelative(int axis, int steps) {
    if (axis < 0 || axis >= 2) return false;
    if (emergencyStop) return false;
    
    // Convert current position to steps and add relative movement
    float currentPosMM = getPosition(axis);
    float stepSizeMM = stepsToMM(axis, 1);  // Size of one step in mm
    float relativeMM = steps * stepSizeMM;
    float targetPosMM = currentPosMM + relativeMM;
    
    // Check if target position is safe
    if (!isPositionSafe(axis, targetPosMM)) {
        Serial.printf("Relative move blocked - target position %.3f mm unsafe for axis %d\n", 
                     targetPosMM, axis);
        return false;
    }
    
    return setTargetPosition(axis, targetPosMM);
}

float ESP32MotionControl::getPosition(int axis) {
    if (axis < 0 || axis >= 2) return 0.0;
    return stepsToMM(axis, axes[axis].currentPosition);
}

float ESP32MotionControl::getTargetPosition(int axis) {
    if (axis < 0 || axis >= 2) return 0.0;
    return stepsToMM(axis, axes[axis].profile.targetPosition);
}

float ESP32MotionControl::getPositionError(int axis) {
    if (axis < 0 || axis >= 2) return 0.0;
    int32_t error = axes[axis].profile.targetPosition - axes[axis].currentPosition;
    return stepsToMM(axis, error);
}

// Safety limit methods
bool ESP32MotionControl::setSoftwareLimits(int axis, float minMM, float maxMM) {
    if (axis < 0 || axis >= 2) return false;
    
    axes[axis].minPosition = mmToSteps(axis, minMM);
    axes[axis].maxPosition = mmToSteps(axis, maxMM);
    axes[axis].limitsEnabled = true;
    
    Serial.printf("Axis %d limits set: %.2fmm to %.2fmm\n", axis, minMM, maxMM);
    return true;
}

void ESP32MotionControl::getSoftwareLimits(int axis, float& minMM, float& maxMM) {
    if (axis < 0 || axis >= 2) {
        minMM = maxMM = 0.0;
        return;
    }
    
    minMM = stepsToMM(axis, axes[axis].minPosition);
    maxMM = stepsToMM(axis, axes[axis].maxPosition);
}

bool ESP32MotionControl::isPositionSafe(int axis, float positionMM) {
    if (axis < 0 || axis >= 2) return false;
    
    if (!axes[axis].limitsEnabled) return true;  // No limits enabled
    
    int32_t positionSteps = mmToSteps(axis, positionMM);
    
    if (positionSteps < axes[axis].minPosition) {
        Serial.printf("Position %.2fmm below minimum %.2fmm\n", 
                     positionMM, stepsToMM(axis, axes[axis].minPosition));
        return false;
    }
    
    if (positionSteps > axes[axis].maxPosition) {
        Serial.printf("Position %.2fmm above maximum %.2fmm\n", 
                     positionMM, stepsToMM(axis, axes[axis].maxPosition));
        return false;
    }
    
    return true;
}

bool ESP32MotionControl::isMoving(int axis) {
    if (axis < 0 || axis >= 2) return false;
    return axes[axis].moving;
}

bool ESP32MotionControl::moveCompleted(int axis) {
    if (axis < 0 || axis >= 2) return true;
    return axes[axis].profile.moveCompleted;
}

bool ESP32MotionControl::setMotionLimits(int axis, float maxVel, float maxAccel) {
    if (axis < 0 || axis >= 2) return false;
    
    // Convert to fixed-point steps per second
    float pulsesPerMM = FIXED_TO_FLOAT(axes[axis].pulsesPerMM);
    axes[axis].maxVelocity = FLOAT_TO_FIXED(maxVel * pulsesPerMM);
    axes[axis].maxAcceleration = FLOAT_TO_FIXED(maxAccel * pulsesPerMM);
    
    return true;
}

void ESP32MotionControl::getMotionLimits(int axis, float& maxVel, float& maxAccel) {
    if (axis < 0 || axis >= 2) return;
    
    float pulsesPerMM = FIXED_TO_FLOAT(axes[axis].pulsesPerMM);
    maxVel = FIXED_TO_FLOAT(axes[axis].maxVelocity) / pulsesPerMM;
    maxAccel = FIXED_TO_FLOAT(axes[axis].maxAcceleration) / pulsesPerMM;
}

MotionProfile::Phase ESP32MotionControl::getMotionPhase(int axis) {
    if (axis < 0 || axis >= 2) return MotionProfile::IDLE;
    return axes[axis].profile.currentPhase;
}

float ESP32MotionControl::getProfileVelocity(int axis) {
    if (axis < 0 || axis >= 2) return 0.0;
    float pulsesPerMM = FIXED_TO_FLOAT(axes[axis].pulsesPerMM);
    return FIXED_TO_FLOAT(axes[axis].profile.currentVelocity) / pulsesPerMM;
}

bool ESP32MotionControl::enableAxis(int axis) {
    if (axis < 0 || axis >= 2) return false;
    
    axes[axis].enabled = true;
    // CRITICAL: Use hardware-specific enable pin polarity
    bool enableState = axes[axis].invertEnable ? LOW : HIGH;
    digitalWrite(axes[axis].enablePin, enableState);
    Serial.printf("Axis %d enabled (invert=%s)\n", axis, axes[axis].invertEnable ? "true" : "false");
    return true;
}

bool ESP32MotionControl::disableAxis(int axis) {
    if (axis < 0 || axis >= 2) return false;
    
    axes[axis].enabled = false;
    // CRITICAL: Use hardware-specific enable pin polarity
    bool disableState = axes[axis].invertEnable ? HIGH : LOW;
    digitalWrite(axes[axis].enablePin, disableState);
    
    // Stop motion profile
    axes[axis].profile.moveActive = false;
    axes[axis].profile.currentPhase = MotionProfile::IDLE;
    axes[axis].stepsToGo = 0;
    axes[axis].moving = false;
    axes[axis].state = AxisConfig::IDLE;
    
    Serial.printf("Axis %d disabled (invert=%s)\n", axis, axes[axis].invertEnable ? "true" : "false");
    return true;
}

bool ESP32MotionControl::isAxisEnabled(int axis) {
    if (axis < 0 || axis >= 2) return false;
    return axes[axis].enabled;
}

bool ESP32MotionControl::isAxisMoving(int axis) {
    if (axis < 0 || axis >= 2) return false;
    return axes[axis].moving;
}

void ESP32MotionControl::startTestSequence() {
    if (testSequence.active) {
        Serial.println("Test sequence already running!");
        return;
    }
    
    if (emergencyStop) {
        Serial.println("Cannot start test sequence - Emergency stop active!");
        return;
    }
    
    // Enable both axes
    enableAxis(0);
    enableAxis(1);
    
    // Reset test sequence
    testSequence.currentMove = 0;
    testSequence.cycleCount = 0;
    testSequence.moveStartTime = 0;
    testSequence.active = true;
    testSequence.completed = false;
    
    Serial.println("=== TEST SEQUENCE STARTED ===");
    Serial.println("3 cycles of: Z left 20mm → X in 8mm → Z right 20mm → X out 8mm");
    Serial.printf("SAFETY: All moves within limits (X=±%.0fmm, Z=±%.0fmm)\n", MAX_TRAVEL_MM_X, MAX_TRAVEL_MM_Z);
    Serial.println("SAFETY: Press ESC for immediate emergency stop");
    Serial.println("SAFETY: Hardware-specific pin inversion active");
    Serial.println("==============================");
}

void ESP32MotionControl::stopTestSequence() {
    bool wasActive = testSequence.active;
    
    testSequence.active = false;
    testSequence.completed = false;
    testSequence.moveStartTime = 0;
    
    // Stop all motion profiles immediately
    for (int i = 0; i < 2; i++) {
        axes[i].profile.moveActive = false;
        axes[i].profile.currentPhase = MotionProfile::IDLE;
        axes[i].stepsToGo = 0;
        axes[i].moving = false;
        axes[i].state = AxisConfig::IDLE;
    }
    
    if (wasActive) {
        Serial.println("=== TEST SEQUENCE STOPPED ===");
        Serial.println("All motion halted immediately");
        Serial.println("Press ENTER to restart test sequence");
        Serial.println("==============================");
    }
}

bool ESP32MotionControl::isTestSequenceActive() {
    return testSequence.active;
}

bool ESP32MotionControl::isTestSequenceCompleted() {
    return testSequence.completed;
}

void ESP32MotionControl::restartTestSequence() {
    // Stop current sequence if active
    if (testSequence.active) {
        stopTestSequence();
        delay(100);  // Small delay to ensure stop completes
    }
    
    // Reset completion flag and start fresh
    testSequence.completed = false;
    startTestSequence();
}

String ESP32MotionControl::getTestSequenceStatus() {
    if (emergencyStop) {
        return "EMERGENCY STOP ACTIVE";
    } else if (testSequence.active) {
        return "RUNNING - Cycle " + String(testSequence.cycleCount + 1) + "/" + String(testSequence.maxCycles) + 
               ", Move " + String(testSequence.currentMove + 1) + "/4";
    } else if (testSequence.completed) {
        return "COMPLETED - Press ENTER to restart";
    } else {
        return "IDLE - Press ENTER to start";
    }
}

void ESP32MotionControl::setPIDGains(int axis, float kP, float kI, float kD) {
    if (axis < 0 || axis >= 2) return;
    
    axes[axis].pid.kP = kP;
    axes[axis].pid.kI = kI;
    axes[axis].pid.kD = kD;
    
    Serial.printf("Axis %d PID gains: P=%.2f, I=%.2f, D=%.2f\n", axis, kP, kI, kD);
}

void ESP32MotionControl::getPIDGains(int axis, float& kP, float& kI, float& kD) {
    if (axis < 0 || axis >= 2) return;
    
    kP = axes[axis].pid.kP;
    kI = axes[axis].pid.kI;
    kD = axes[axis].pid.kD;
}

void ESP32MotionControl::setEmergencyStop(bool stop) {
    emergencyStop = stop;
    
    if (stop) {
        // Stop all motion profiles immediately
        for (int i = 0; i < 2; i++) {
            axes[i].profile.moveActive = false;
            axes[i].profile.currentPhase = MotionProfile::IDLE;
            axes[i].stepsToGo = 0;
            axes[i].moving = false;
            axes[i].state = AxisConfig::IDLE;
        }
        
        // Stop test sequence
        stopTestSequence();
        
        Serial.println("*** EMERGENCY STOP ACTIVATED ***");
    } else {
        Serial.println("Emergency stop released");
    }
}

String ESP32MotionControl::getStatusReport() {
    String report = "Motion Control Status (ClearPath-Enhanced):\n";
    
    for (int i = 0; i < 2; i++) {
        char axisName = (i == 0) ? 'X' : 'Z';
        report += String(axisName) + "-axis: ";
        report += "Pos=" + String(getPosition(i), 2) + "mm ";
        report += "Target=" + String(getTargetPosition(i), 2) + "mm ";
        report += "Error=" + String(getPositionError(i), 2) + "mm ";
        report += "Steps=" + String(axes[i].stepCount) + " ";
        report += "Vel=" + String(getProfileVelocity(i), 1) + "mm/s ";
        
        // Motion phase
        switch (axes[i].profile.currentPhase) {
            case MotionProfile::IDLE: report += "IDLE"; break;
            case MotionProfile::ACCELERATION: report += "ACCEL"; break;
            case MotionProfile::CONSTANT_VELOCITY: report += "CONST"; break;
            case MotionProfile::DECELERATION: report += "DECEL"; break;
            case MotionProfile::COMPLETED: report += "DONE"; break;
        }
        report += "\n";
    }
    
    if (testSequence.active) {
        report += "Test: Cycle " + String(testSequence.cycleCount + 1) + "/" + String(testSequence.maxCycles);
        report += " Move " + String(testSequence.currentMove + 1) + "/4\n";
    }
    
    return report;
}

void ESP32MotionControl::printDiagnostics() {
    Serial.println("=== ESP32 Motion Control Diagnostics ===");
    Serial.print(getStatusReport());
    
    // PID status
    for (int i = 0; i < 2; i++) {
        char axisName = (i == 0) ? 'X' : 'Z';
        Serial.printf("%c-axis PID: P=%.2f I=%.2f D=%.2f\n", 
                     axisName, axes[i].pid.kP, axes[i].pid.kI, axes[i].pid.kD);
    }
    
    Serial.println("==========================================");
}

void ESP32MotionControl::update() {
    // Main update called from loop() - most work done in FreeRTOS task
    // No auto-start - waiting for manual control via keyboard
}