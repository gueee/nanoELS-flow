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
    
    // Update each axis
    for (int axis = 0; axis < 2; axis++) {
        if (axes[axis].enabled) {
            // Calculate target position from spindle (if threading)
            if (spindle.threadingActive && spindle.threadPitch != 0) {
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

void MinimalMotionControl::shutdown() {
    setEmergencyStop(true);
    
    for (int i = 0; i < 2; i++) {
        disableAxis(i);
    }
}