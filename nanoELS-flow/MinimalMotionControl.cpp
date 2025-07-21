#include "MinimalMotionControl.h"

// Global instance
MinimalMotionControl motionControl;

// Static instance pointer for ISR access
MinimalMotionControl* MinimalMotionControl::instance = nullptr;

MinimalMotionControl::MinimalMotionControl() {
    instance = this;
    emergencyStop = false;
    
    // Initialize spindle tracker
    spindle.position = 0;
    spindle.positionAvg = 0;
    spindle.lastCount = 0;
    spindle.threadPitch = 0;
    spindle.threadStarts = 1;
    spindle.threadingActive = false;
    
    // Initialize MPG trackers (h5.ino style)
    for (int i = 0; i < 2; i++) {
        mpg[i].lastCount = 0;
        mpg[i].fractionalPos = 0.0;     // h5.ino style fractional position
        mpg[i].pcntUnit = (i == AXIS_X) ? PCNT_UNIT_2 : PCNT_UNIT_1;
        mpg[i].stepSize = 10000;  // Default 1mm step size
        mpg[i].active = false;
    }
    
    // Initialize axes with h5.ino-compatible defaults
    for (int i = 0; i < 2; i++) {
        MinimalAxis& axis = axes[i];
        
        // Hardware pins (will be set in initialize())
        axis.stepPin = (i == AXIS_X) ? X_STEP : Z_STEP;
        axis.dirPin = (i == AXIS_X) ? X_DIR : Z_DIR;
        axis.enablePin = (i == AXIS_X) ? X_ENA : Z_ENA;
        axis.invertDirection = (i == AXIS_X) ? INVERT_X : INVERT_Z;
        axis.invertEnable = (i == AXIS_X) ? INVERT_X_ENABLE : INVERT_Z_ENABLE;
        
        // Position
        axis.position = 0;
        axis.targetPosition = 0;
        axis.moving = false;
        
        // Motion parameters (from SetupConstants)
        axis.currentSpeed = (i == AXIS_X) ? SPEED_START_X : SPEED_START_Z;
        axis.maxSpeed = (i == AXIS_X) ? SPEED_MANUAL_MOVE_X : SPEED_MANUAL_MOVE_Z;
        axis.startSpeed = (i == AXIS_X) ? SPEED_START_X : SPEED_START_Z;
        axis.acceleration = (i == AXIS_X) ? ACCELERATION_X : ACCELERATION_Z;
        
        // Hardware specs (from SetupConstants)
        axis.motorSteps = (i == AXIS_X) ? MOTOR_STEPS_X : MOTOR_STEPS_Z;
        axis.screwPitch = (i == AXIS_X) ? SCREW_X_DU : SCREW_Z_DU;
        
        // Timing
        axis.lastStepTime = 0;
        axis.direction = false;
        
        // Safety (start with no limits)
        axis.leftStop = LONG_MAX;
        axis.rightStop = LONG_MIN;
        
        // Status
        axis.enabled = false;
    }
}

MinimalMotionControl::~MinimalMotionControl() {
    shutdown();
}

bool MinimalMotionControl::initialize() {
    initializeEncoders();
    initializeGPIO();
    
    // Reset spindle tracking
    resetSpindlePosition();
    
    return true;
}

void MinimalMotionControl::initializeEncoders() {
    // Configure PCNT for spindle encoder (h5.ino style)
    pcnt_config_t pcnt_config;
    pcnt_config.pulse_gpio_num = ENC_A;
    pcnt_config.ctrl_gpio_num = ENC_B;
    pcnt_config.channel = PCNT_CHANNEL_0;
    pcnt_config.unit = PCNT_UNIT_0;
    pcnt_config.pos_mode = PCNT_COUNT_INC;
    pcnt_config.neg_mode = PCNT_COUNT_DEC;
    pcnt_config.lctrl_mode = PCNT_MODE_REVERSE;
    pcnt_config.hctrl_mode = PCNT_MODE_KEEP;
    pcnt_config.counter_h_lim = 30000;     // h5.ino PCNT_LIM
    pcnt_config.counter_l_lim = -30000;
    
    pcnt_unit_config(&pcnt_config);
    pcnt_set_filter_value(PCNT_UNIT_0, ENCODER_FILTER);
    pcnt_filter_enable(PCNT_UNIT_0);
    pcnt_counter_pause(PCNT_UNIT_0);
    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);
    
    // Configure PCNT for MPG encoders (h5.ino style)
    for (int i = 0; i < 2; i++) {
        pcnt_config_t mpg_config;
        mpg_config.pulse_gpio_num = (i == AXIS_X) ? X_PULSE_A : Z_PULSE_A;
        mpg_config.ctrl_gpio_num = (i == AXIS_X) ? X_PULSE_B : Z_PULSE_B;
        mpg_config.channel = PCNT_CHANNEL_0;
        mpg_config.unit = mpg[i].pcntUnit;
        mpg_config.pos_mode = PCNT_COUNT_INC;
        mpg_config.neg_mode = PCNT_COUNT_DEC;
        mpg_config.lctrl_mode = PCNT_MODE_REVERSE;
        mpg_config.hctrl_mode = PCNT_MODE_KEEP;
        mpg_config.counter_h_lim = MPG_PCNT_LIM;
        mpg_config.counter_l_lim = -MPG_PCNT_LIM;
        
        pcnt_unit_config(&mpg_config);
        pcnt_set_filter_value(mpg[i].pcntUnit, MPG_PCNT_FILTER);
        pcnt_filter_enable(mpg[i].pcntUnit);
        pcnt_counter_pause(mpg[i].pcntUnit);
        pcnt_counter_clear(mpg[i].pcntUnit);
        pcnt_counter_resume(mpg[i].pcntUnit);
    }
}

void MinimalMotionControl::initializeGPIO() {
    for (int i = 0; i < 2; i++) {
        MinimalAxis& axis = axes[i];
        
        // Configure step/dir/enable pins
        pinMode(axis.stepPin, OUTPUT);
        pinMode(axis.dirPin, OUTPUT);
        pinMode(axis.enablePin, OUTPUT);
        
        // Set initial states
        digitalWrite(axis.stepPin, HIGH);   // Idle high
        digitalWrite(axis.dirPin, axis.invertDirection ? HIGH : LOW);
        digitalWrite(axis.enablePin, axis.invertEnable ? HIGH : LOW);  // Disabled initially
    }
}

// Core h5.ino algorithm: Calculate stepper position from spindle position
int32_t MinimalMotionControl::positionFromSpindle(int axis, int32_t spindlePos) {
    MinimalAxis& a = axes[axis];
    
    // Exact h5.ino formula adapted for our 600 PPR encoder
    int32_t newPos = spindlePos * a.motorSteps / a.screwPitch / ENCODER_STEPS_FLOAT 
                     * spindle.threadPitch * spindle.threadStarts;
    
    // Respect software limits (h5.ino style)
    if (newPos < a.rightStop) newPos = a.rightStop;
    if (newPos > a.leftStop) newPos = a.leftStop;
    
    return newPos;
}

// Core h5.ino algorithm: Calculate spindle position from stepper position
int32_t MinimalMotionControl::spindleFromPosition(int axis, int32_t axisPos) {
    MinimalAxis& a = axes[axis];
    
    // h5.ino inverse formula
    return axisPos * a.screwPitch * ENCODER_STEPS_FLOAT / a.motorSteps / (spindle.threadPitch * spindle.threadStarts);
}

// Core h5.ino algorithm: Spindle tracking with backlash compensation
void MinimalMotionControl::updateSpindleTracking() {
    // Read hardware pulse counter
    int16_t count;
    pcnt_get_counter_value(PCNT_UNIT_0, &count);
    int32_t delta = count - spindle.lastCount;
    
    if (delta == 0) {
        return;  // No movement
    }
    
    // Handle counter overflow (h5.ino PCNT_CLEAR logic)
    if (count >= 30000 || count <= -30000) {
        pcnt_counter_clear(PCNT_UNIT_0);
        spindle.lastCount = 0;
    } else {
        spindle.lastCount = count;
    }
    
    // Update raw position
    spindle.position += delta;
    
    // h5.ino backlash compensation algorithm (THE KEY!)
    if (spindle.position > spindle.positionAvg) {
        // Forward motion: immediate follow (0 error)
        spindle.positionAvg = spindle.position;
    } else if (spindle.position < spindle.positionAvg - ENCODER_BACKLASH) {
        // Reverse motion: 3-step deadband filter
        spindle.positionAvg = spindle.position + ENCODER_BACKLASH;
    }
    // Else: within deadband, maintain current positionAvg
    
    spindle.lastUpdateTime = micros();
}

// Core update loop (call from main loop at ~5kHz)
void MinimalMotionControl::update() {
    if (emergencyStop) {
        return;  // Emergency stop overrides everything
    }
    
    // Update spindle tracking with backlash compensation
    updateSpindleTracking();
    
    // Update MPG tracking (h5.ino style continuous monitoring)
    updateMPGTracking();
    
    // Update each axis
    for (int axis = 0; axis < 2; axis++) {
        if (axes[axis].enabled) {
            // Process MPG movement (takes priority over threading)
            processMPGMovement(axis);
            
            // Calculate target position from spindle (if threading and MPG not active)
            if (spindle.threadingActive && spindle.threadPitch != 0 && !mpg[axis].active) {
                int32_t newTarget = positionFromSpindle(axis, spindle.positionAvg);
                axes[axis].targetPosition = newTarget;
            }
            
            // Update axis motion
            updateAxisMotion(axis);
        }
    }
}

// Axis motion update (h5.ino style)
void MinimalMotionControl::updateAxisMotion(int axis) {
    MinimalAxis& a = axes[axis];
    int32_t stepsToGo = a.targetPosition - a.position;
    
    if (stepsToGo == 0) {
        a.moving = false;
        // Decelerate if moving
        if (a.currentSpeed > a.startSpeed) {
            a.currentSpeed--;
        }
        return;
    }
    
    a.moving = true;
    
    // Update speed (simple h5.ino ramping)
    updateSpeed(axis);
    
    // Check if it's time for next step
    uint32_t now = micros();
    uint32_t stepInterval = 1000000 / a.currentSpeed;  // Convert Hz to microseconds
    
    if (now - a.lastStepTime >= stepInterval) {
        generateStepPulse(axis);
        a.lastStepTime = now;
    }
}

// Simple speed ramping (h5.ino style)
void MinimalMotionControl::updateSpeed(int axis) {
    MinimalAxis& a = axes[axis];
    
    // Simple acceleration ramping
    if (a.currentSpeed < a.maxSpeed) {
        a.currentSpeed += a.acceleration / a.currentSpeed;  // Acceleration curve
        if (a.currentSpeed > a.maxSpeed) {
            a.currentSpeed = a.maxSpeed;
        }
    }
}

// Step pulse generation (direct GPIO)
void MinimalMotionControl::generateStepPulse(int axis) {
    MinimalAxis& a = axes[axis];
    int32_t stepsToGo = a.targetPosition - a.position;
    
    if (stepsToGo == 0) {
        return;
    }
    
    // Set direction
    bool newDirection = stepsToGo > 0;
    if (newDirection != a.direction) {
        a.direction = newDirection;
        digitalWrite(a.dirPin, newDirection ^ a.invertDirection);
        delayMicroseconds(DIRECTION_SETUP_DELAY_US);
    }
    
    // Generate step pulse
    digitalWrite(a.stepPin, LOW);
    delayMicroseconds(STEP_PULSE_WIDTH_US);
    digitalWrite(a.stepPin, HIGH);
    
    // Update position
    if (stepsToGo > 0) {
        a.position++;
    } else {
        a.position--;
    }
}

// Position control interface
void MinimalMotionControl::setTargetPosition(int axis, int32_t steps) {
    if (axis >= 0 && axis < 2) {
        axes[axis].targetPosition = steps;
    }
}

void MinimalMotionControl::moveRelative(int axis, int32_t steps) {
    if (axis >= 0 && axis < 2) {
        axes[axis].targetPosition += steps;
    }
}

void MinimalMotionControl::stopAxis(int axis) {
    if (axis >= 0 && axis < 2) {
        axes[axis].targetPosition = axes[axis].position;
        axes[axis].moving = false;
    }
}

void MinimalMotionControl::stopAllAxes() {
    for (int i = 0; i < 2; i++) {
        stopAxis(i);
    }
}

// Threading control
void MinimalMotionControl::setThreadPitch(int32_t dupr, int32_t starts) {
    spindle.threadPitch = dupr;
    spindle.threadStarts = starts;
}

void MinimalMotionControl::startThreading() {
    spindle.threadingActive = true;
}

void MinimalMotionControl::stopThreading() {
    spindle.threadingActive = false;
}

// Axis control
void MinimalMotionControl::enableAxis(int axis) {
    if (axis >= 0 && axis < 2) {
        axes[axis].enabled = true;
        digitalWrite(axes[axis].enablePin, axes[axis].invertEnable ? LOW : HIGH);
    }
}

void MinimalMotionControl::disableAxis(int axis) {
    if (axis >= 0 && axis < 2) {
        axes[axis].enabled = false;
        digitalWrite(axes[axis].enablePin, axes[axis].invertEnable ? HIGH : LOW);
    }
}

bool MinimalMotionControl::isAxisEnabled(int axis) {
    return (axis >= 0 && axis < 2) ? axes[axis].enabled : false;
}

// Speed control
void MinimalMotionControl::setMaxSpeed(int axis, uint32_t speed) {
    if (axis >= 0 && axis < 2) {
        axes[axis].maxSpeed = speed;
    }
}

// Safety
void MinimalMotionControl::setEmergencyStop(bool stop) {
    emergencyStop = stop;
    if (stop) {
        stopAllAxes();
        stopThreading();
    }
}

void MinimalMotionControl::setSoftLimits(int axis, int32_t leftLimit, int32_t rightLimit) {
    if (axis >= 0 && axis < 2) {
        axes[axis].leftStop = leftLimit;
        axes[axis].rightStop = rightLimit;
    }
}

void MinimalMotionControl::getSoftLimits(int axis, int32_t& leftLimit, int32_t& rightLimit) {
    if (axis >= 0 && axis < 2) {
        leftLimit = axes[axis].leftStop;
        rightLimit = axes[axis].rightStop;
    }
}

// Spindle interface
void MinimalMotionControl::resetSpindlePosition() {
    spindle.position = 0;
    spindle.positionAvg = 0;
    spindle.lastCount = 0;
    pcnt_counter_clear(PCNT_UNIT_0);
}

void MinimalMotionControl::zeroAxis(int axis) {
    if (axis >= 0 && axis < 2) {
        // Set current position as new zero origin (like h5.ino markAxis0)
        axes[axis].position = 0;
        axes[axis].targetPosition = 0;
        // No physical movement - just resets coordinate system
    }
}

// Status and diagnostics
float MinimalMotionControl::getFollowingError(int axis) {
    if (!spindle.threadingActive || axis < 0 || axis >= 2) {
        return 0.0;
    }
    
    // Calculate expected position from spindle
    int32_t expectedPos = positionFromSpindle(axis, spindle.positionAvg);
    int32_t actualPos = axes[axis].position;
    int32_t errorSteps = expectedPos - actualPos;
    
    // Convert to micrometers
    float errorMM = stepsToMM(axis, errorSteps);
    return errorMM * 1000.0;  // Return in micrometers
}

String MinimalMotionControl::getStatusReport() {
    String report = "MinimalMotionControl Status:\n";
    report += "Threading: " + String(spindle.threadingActive ? "ACTIVE" : "INACTIVE") + "\n";
    report += "Spindle: " + String(spindle.positionAvg) + " (raw: " + String(spindle.position) + ")\n";
    
    for (int i = 0; i < 2; i++) {
        char axisName = (i == AXIS_X) ? 'X' : 'Z';
        report += String(axisName) + ": pos=" + String(axes[i].position);
        report += " target=" + String(axes[i].targetPosition);
        report += " speed=" + String(axes[i].currentSpeed);
        report += " " + String(axes[i].enabled ? "EN" : "DIS");
        report += " " + String(axes[i].moving ? "MOV" : "STOP") + "\n";
    }
    
    return report;
}

void MinimalMotionControl::printDiagnostics() {
    Serial.println(getStatusReport());
    
    for (int i = 0; i < 2; i++) {
        float followingError = getFollowingError(i);
        char axisName = (i == AXIS_X) ? 'X' : 'Z';
        Serial.println(String(axisName) + " Following Error: " + String(followingError, 3) + " μm");
    }
}

void MinimalMotionControl::printMPGDiagnostics() {
    Serial.println("=== MPG Diagnostics ===");
    
    for (int i = 0; i < 2; i++) {
        char axisName = (i == AXIS_X) ? 'X' : 'Z';
        int16_t count;
        esp_err_t err = pcnt_get_counter_value(mpg[i].pcntUnit, &count);
        
        Serial.printf("%c-axis MPG (PCNT_UNIT_%d):\n", axisName, mpg[i].pcntUnit);
        Serial.printf("  Active: %s\n", mpg[i].active ? "YES" : "NO");
        Serial.printf("  Step Size: %d du (%.3f mm)\n", mpg[i].stepSize, mpg[i].stepSize / 10000.0);
        Serial.printf("  PCNT Read: %s (count=%d)\n", err == ESP_OK ? "OK" : "ERROR", count);
        Serial.printf("  Last Count: %d\n", mpg[i].lastCount);
        Serial.printf("  Fractional Pos: %.3f\n", mpg[i].fractionalPos);
        Serial.println();
    }
}

// Utility functions
float MinimalMotionControl::stepsToMM(int axis, int32_t steps) {
    if (axis < 0 || axis >= 2) return 0.0;
    MinimalAxis& a = axes[axis];
    return (float)steps * a.screwPitch / a.motorSteps / 10000.0;  // Convert from deci-microns
}

int32_t MinimalMotionControl::mmToSteps(int axis, float mm) {
    if (axis < 0 || axis >= 2) return 0;
    MinimalAxis& a = axes[axis];
    return (int32_t)(mm * 10000.0 * a.motorSteps / a.screwPitch);  // Convert to deci-microns
}

// ======================================================================
// MPG (Manual Pulse Generator) Implementation - h5.ino style
// ======================================================================

// Read MPG encoder delta (h5.ino algorithm)
int32_t MinimalMotionControl::getMPGDelta(int axis) {
    if (axis < 0 || axis >= 2) return 0;
    
    int16_t count;
    esp_err_t err = pcnt_get_counter_value(mpg[axis].pcntUnit, &count);
    if (err != ESP_OK) {
        Serial.printf("MPG[%d] PCNT read error: %d\n", axis, err);
        return 0;
    }
    
    int32_t delta = count - mpg[axis].lastCount;
    
    if (delta == 0) return 0;
    
    // Apply inversion if configured (similar to stepper inversion)
    if ((axis == AXIS_Z && INVERT_MPG_Z) || (axis == AXIS_X && INVERT_MPG_X)) {
        delta = -delta;
    }
    
    // Debug output for non-zero deltas
    Serial.printf("MPG[%d] delta=%d (count=%d, last=%d)\n", axis, delta, count, mpg[axis].lastCount);
    
    // Handle PCNT overflow (h5.ino style)
    if (count >= MPG_PCNT_CLEAR || count <= -MPG_PCNT_CLEAR) {
        pcnt_counter_clear(mpg[axis].pcntUnit);
        mpg[axis].lastCount = 0;
    } else {
        mpg[axis].lastCount = count;
    }
    
    return delta;
}

// h5.ino: calculateStepScale() not needed - uses direct formula in processing

// Update MPG tracking for all axes (h5.ino exact algorithm)
void MinimalMotionControl::updateMPGTracking() {
    for (int axis = 0; axis < 2; axis++) {
        if (!mpg[axis].active) continue;
        
        int32_t pulseDelta = getMPGDelta(axis);
        if (pulseDelta == 0) continue;
        
        MinimalAxis& a = axes[axis];
        
        // Convert MPG pulses to motor steps using configured step size
        // Each MPG pulse = stepSize movement in deci-microns
        float fractionalDelta = (float)pulseDelta * mpg[axis].stepSize / a.screwPitch * a.motorSteps / MPG_SCALE_DIVISOR + mpg[axis].fractionalPos;
        int32_t deltaSteps = round(fractionalDelta);
        
        // h5.ino fractional accumulation - don't lose fractional steps
        mpg[axis].fractionalPos = fractionalDelta - deltaSteps;
        
        // Apply movement directly to target position (h5.ino style)
        if (deltaSteps != 0) {
            axes[axis].targetPosition += deltaSteps;
        }
    }
}

// Process MPG movement for one axis (h5.ino style - soft limit checking)
void MinimalMotionControl::processMPGMovement(int axis) {
    if (axis < 0 || axis >= 2 || !mpg[axis].active) return;
    
    // h5.ino approach: movement already applied in updateMPGTracking()
    // This function now just enforces soft limits
    MinimalAxis& a = axes[axis];
    
    // Respect soft limits (h5.ino style limit enforcement)
    if (a.targetPosition > a.leftStop) {
        a.targetPosition = a.leftStop;
    } else if (a.targetPosition < a.rightStop) {
        a.targetPosition = a.rightStop;
    }
}

// MPG control interface methods
void MinimalMotionControl::enableMPG(int axis, bool enable) {
    if (axis >= 0 && axis < 2) {
        mpg[axis].active = enable;
        Serial.printf("MPG[%d] %s (stepSize=%d du)\n", axis, enable ? "ENABLED" : "DISABLED", mpg[axis].stepSize);
        
        if (enable) {
            // Reset tracking when enabling (h5.ino style)
            mpg[axis].fractionalPos = 0.0;
            pcnt_counter_clear(mpg[axis].pcntUnit);
            mpg[axis].lastCount = 0;
            Serial.printf("MPG[%d] tracking reset\n", axis);
        }
    }
}

void MinimalMotionControl::setMPGStepSize(int axis, int32_t stepSizeDU) {
    if (axis >= 0 && axis < 2) {
        mpg[axis].stepSize = stepSizeDU;
    }
}

// Float interface for OperationManager compatibility
void MinimalMotionControl::setMPGStepSize(int axis, float mm) {
    if (axis >= 0 && axis < 2) {
        mpg[axis].stepSize = mmToSteps(axis, mm);
    }
}

float MinimalMotionControl::getMPGStepSize(int axis) const {
    if (axis >= 0 && axis < 2) {
        return mpg[axis].stepSize / 10000.0f;  // Convert deci-microns to mm
    }
    return 0.0f;
}

void MinimalMotionControl::shutdown() {
    setEmergencyStop(true);
    
    for (int i = 0; i < 2; i++) {
        disableAxis(i);
        enableMPG(i, false);  // Disable MPG tracking
    }
}