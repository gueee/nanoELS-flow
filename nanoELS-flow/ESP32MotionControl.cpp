#include "ESP32MotionControl.h"

// Static instance pointer for ISR access
ESP32MotionControl* ESP32MotionControl::instance = nullptr;

// Global instance
ESP32MotionControl esp32Motion;

// Quadrature decoding lookup table for optimized encoder handling
static const int8_t QUADRATURE_TABLE[16] = {
    0, -1,  1,  0,   // 00xx
    1,  0,  0, -1,   // 01xx
   -1,  0,  0,  1,   // 10xx
    0,  1, -1,  0    // 11xx
};

ESP32MotionControl::ESP32MotionControl() {
    instance = this;
    emergencyStop = false;
    motionTaskHandle = nullptr;
    
    // Initialize axis configurations
    axes[0] = { // X-axis
        .stepPin = X_STEP,
        .dirPin = X_DIR,
        .enablePin = X_ENA,
        .position = 0,
        .targetPosition = 0,
        .currentSpeed = 0,
        .targetSpeed = 50000,    // Higher default speed for responsive control
        .maxSpeed = 200000,      // Higher max speed for fast movements
        .acceleration = 20000,   // Reduced acceleration for smoother response
        .stepInterval = 20,      // Faster default interval (50kHz)
        .lastStepTime = 0,
        .enabled = false,
        .moving = false,
        .inverted = true,  // X-axis inverted per project config
        .state = AxisConfig::IDLE
    };
    
    axes[1] = { // Z-axis  
        .stepPin = Z_STEP,
        .dirPin = Z_DIR,
        .enablePin = Z_ENA,
        .position = 0,
        .targetPosition = 0,
        .currentSpeed = 0,
        .targetSpeed = 50000,    // Higher default speed for responsive control
        .maxSpeed = 200000,      // Higher max speed for fast movements
        .acceleration = 20000,   // Reduced acceleration for smoother response
        .stepInterval = 20,      // Faster default interval (50kHz)
        .lastStepTime = 0,
        .enabled = false,
        .moving = false,
        .inverted = false, // Z-axis not inverted
        .state = AxisConfig::IDLE
    };
    
    // Initialize encoder configurations
    encoders[0] = { // Spindle encoder
        .pinA = ENC_A,
        .pinB = ENC_B,
        .pcntUnit = PCNT_UNIT_0,
        .count = 0,
        .lastCount = 0,
        .offset = 0,
        .errorCount = 0,
        .name = "Spindle",
        .lastChangeTime = 0,
        .velocity = 0,
        .lastVelocity = 0,
        .velocityUpdateTime = 0,
        .usePCNT = true
    };
    
    encoders[1] = { // Z-axis MPG
        .pinA = Z_PULSE_A,
        .pinB = Z_PULSE_B,
        .pcntUnit = PCNT_UNIT_1,
        .count = 0,
        .lastCount = 0,
        .offset = 0,
        .errorCount = 0,
        .name = "Z-MPG",
        .lastChangeTime = 0,
        .velocity = 0,
        .lastVelocity = 0,
        .velocityUpdateTime = 0,
        .usePCNT = true
    };
    
    encoders[2] = { // X-axis MPG
        .pinA = X_PULSE_A,
        .pinB = X_PULSE_B,
        .pcntUnit = PCNT_UNIT_2,
        .count = 0,
        .lastCount = 0,
        .offset = 0,
        .errorCount = 0,
        .name = "X-MPG",
        .lastChangeTime = 0,
        .velocity = 0,
        .lastVelocity = 0,
        .velocityUpdateTime = 0,
        .usePCNT = true
    };
}

ESP32MotionControl::~ESP32MotionControl() {
    shutdown();
}

bool ESP32MotionControl::initialize() {
    Serial.println("Initializing ESP32-S3 Motion Control System (Task-Based)");
    
    // Initialize GPIO
    initializeGPIO();
    
    // Initialize encoders
    if (!initializeEncoders()) {
        Serial.println("ERROR: Failed to initialize encoders");
        return false;
    }
    
    // Create motion control task
    xTaskCreatePinnedToCore(
        motionControlTask,
        "MotionControl",
        8192,               // Stack size
        this,               // Parameter
        2,                  // Priority (higher than main loop)
        &motionTaskHandle,  // Task handle
        1                   // Core 1 (Core 0 runs Arduino loop)
    );
    
    if (motionTaskHandle == nullptr) {
        Serial.println("ERROR: Failed to create motion control task");
        return false;
    }
    
    Serial.println("✓ ESP32-S3 Motion Control System initialized successfully");
    return true;
}

void ESP32MotionControl::initializeGPIO() {
    // Configure stepper GPIO
    for (int i = 0; i < 2; i++) {
        pinMode(axes[i].stepPin, OUTPUT);
        pinMode(axes[i].dirPin, OUTPUT);
        pinMode(axes[i].enablePin, OUTPUT);
        
        // Set initial states
        digitalWrite(axes[i].stepPin, LOW);
        digitalWrite(axes[i].dirPin, LOW);
        digitalWrite(axes[i].enablePin, HIGH); // Disabled (active low)
    }
    
    // Configure encoder GPIO
    for (int i = 0; i < 3; i++) {
        pinMode(encoders[i].pinA, INPUT_PULLUP);
        pinMode(encoders[i].pinB, INPUT_PULLUP);
        
        // Hardware PCNT handles encoder state automatically
    }
    
    Serial.println("✓ GPIO initialized");
}

bool ESP32MotionControl::initializeEncoders() {
    // Initialize hardware PCNT for all encoders
    for (int i = 0; i < 3; i++) {
        EncoderConfig& enc = encoders[i];
        
        if (enc.usePCNT) {
            // Configure PCNT unit
            pcnt_config_t pcnt_config = {
                .pulse_gpio_num = enc.pinA,
                .ctrl_gpio_num = enc.pinB,
                .lctrl_mode = PCNT_MODE_REVERSE,    // Reverse counting direction
                .hctrl_mode = PCNT_MODE_KEEP,       // Keep counting direction
                .pos_mode = PCNT_COUNT_INC,         // Count up on positive edge
                .neg_mode = PCNT_COUNT_DEC,         // Count down on negative edge
                .counter_h_lim = 32767,             // High limit
                .counter_l_lim = -32768,            // Low limit
                .unit = enc.pcntUnit,
                .channel = PCNT_CHANNEL_0,
            };
            
            // Initialize PCNT unit
            if (pcnt_unit_config(&pcnt_config) != ESP_OK) {
                Serial.printf("Failed to configure PCNT unit %d\n", enc.pcntUnit);
                return false;
            }
            
            // Set glitch filter (debouncing)
            pcnt_set_filter_value(enc.pcntUnit, 100);  // 100 APB clock cycles
            pcnt_filter_enable(enc.pcntUnit);
            
            // Clear counter
            pcnt_counter_clear(enc.pcntUnit);
            
            // Start PCNT unit
            pcnt_counter_resume(enc.pcntUnit);
            
            Serial.printf("✓ PCNT initialized for %s (Unit %d)\n", enc.name, enc.pcntUnit);
        }
    }
    
    Serial.println("✓ Hardware PCNT encoders initialized");
    return true;
}

void ESP32MotionControl::motionControlTask(void* parameter) {
    ESP32MotionControl* motion = static_cast<ESP32MotionControl*>(parameter);
    
    const TickType_t xDelay = pdMS_TO_TICKS(1); // 1ms task period
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (true) {
        // CRITICAL: Check emergency stop first - highest priority
        if (motion->emergencyStop) {
            // Stop all motion immediately
            for (int i = 0; i < 2; i++) {
                motion->axes[i].moving = false;
                motion->axes[i].targetPosition = motion->axes[i].position;
                motion->axes[i].state = AxisConfig::IDLE;
            }
            // Skip all other processing during emergency stop
            vTaskDelayUntil(&xLastWakeTime, xDelay);
            continue;
        }
        
        // Update axis motion (only if not in emergency stop)
        for (int axis = 0; axis < 2; axis++) {
            motion->updateAxisMotion(axis);
        }
        
        // Process motion queue (only if not in emergency stop)
        motion->processMotionQueue();
        
        // Wait for next cycle
        vTaskDelayUntil(&xLastWakeTime, xDelay);
    }
}

void ESP32MotionControl::generateStepPulse(int axis) {
    if (axis < 0 || axis >= 2) return;
    
    AxisConfig& axisConfig = axes[axis];
    
    if (!axisConfig.enabled || !axisConfig.moving) return;
    
    uint32_t currentTime = micros();
    
    // Check if it's time for the next step
    if (currentTime - axisConfig.lastStepTime >= axisConfig.stepInterval) {
        // Set direction
        bool forward = (axisConfig.targetPosition > axisConfig.position);
        if (axisConfig.inverted) forward = !forward;
        
        digitalWrite(axisConfig.dirPin, forward ? HIGH : LOW);
        
        // Generate step pulse
        digitalWrite(axisConfig.stepPin, HIGH);
        delayMicroseconds(2);  // Step pulse width
        digitalWrite(axisConfig.stepPin, LOW);
        
        // Update position
        if (forward) {
            axisConfig.position++;
        } else {
            axisConfig.position--;
        }
        
        axisConfig.lastStepTime = currentTime;
    }
}

void ESP32MotionControl::updateAxisMotion(int axis) {
    if (axis < 0 || axis >= 2) return;
    
    AxisConfig& axisConfig = axes[axis];
    
    // Check if move complete
    if (axisConfig.position == axisConfig.targetPosition) {
        axisConfig.moving = false;
        axisConfig.state = AxisConfig::IDLE;
        return;
    }
    
    // Update acceleration/deceleration
    calculateAcceleration(axis);
    
    // Update step interval
    axisConfig.stepInterval = calculateStepInterval(axis);
    
    // Generate step pulse
    generateStepPulse(axis);
}

void ESP32MotionControl::calculateAcceleration(int axis) {
    AxisConfig& axisConfig = axes[axis];
    
    uint32_t remainingSteps = abs(axisConfig.targetPosition - axisConfig.position);
    uint32_t decelerationSteps = (axisConfig.currentSpeed * axisConfig.currentSpeed) / (2 * axisConfig.acceleration);
    
    switch (axisConfig.state) {
        case AxisConfig::ACCELERATING:
            if (axisConfig.currentSpeed < axisConfig.targetSpeed) {
                axisConfig.currentSpeed += axisConfig.acceleration / 1000; // Per ms
                if (axisConfig.currentSpeed >= axisConfig.targetSpeed) {
                    axisConfig.currentSpeed = axisConfig.targetSpeed;
                    axisConfig.state = AxisConfig::CONSTANT_SPEED;
                }
            }
            
            // Check if we need to start decelerating
            if (remainingSteps <= decelerationSteps) {
                axisConfig.state = AxisConfig::DECELERATING;
            }
            break;
            
        case AxisConfig::CONSTANT_SPEED:
            if (remainingSteps <= decelerationSteps) {
                axisConfig.state = AxisConfig::DECELERATING;
            }
            break;
            
        case AxisConfig::DECELERATING:
            if (axisConfig.currentSpeed > 100) { // Minimum speed
                axisConfig.currentSpeed -= axisConfig.acceleration / 1000;
                if (axisConfig.currentSpeed < 100) {
                    axisConfig.currentSpeed = 100;
                }
            }
            break;
            
        default:
            break;
    }
}

uint32_t ESP32MotionControl::calculateStepInterval(int axis) {
    if (axis < 0 || axis >= 2) return 0;
    
    AxisConfig& axisConfig = axes[axis];
    
    if (axisConfig.currentSpeed == 0) return 1000000; // Very slow
    
    return 1000000 / axisConfig.currentSpeed; // Microseconds
}

// Optimized encoder ISR handlers
// ISR functions removed - using hardware PCNT instead

bool ESP32MotionControl::moveRelative(int axis, int32_t steps, bool blocking) {
    if (axis < 0 || axis >= 2) return false;
    
    AxisConfig& axisConfig = axes[axis];
    
    if (!axisConfig.enabled || emergencyStop) return false;
    
    axisConfig.targetPosition = axisConfig.position + steps;
    axisConfig.moving = true;
    axisConfig.state = AxisConfig::ACCELERATING;
    axisConfig.currentSpeed = 100; // Start at minimum speed
    
    if (blocking) {
        while (axisConfig.moving && !emergencyStop) {
            delay(1);
        }
    }
    
    return true;
}

bool ESP32MotionControl::moveAbsolute(int axis, int32_t position, bool blocking) {
    if (axis < 0 || axis >= 2) return false;
    
    AxisConfig& axisConfig = axes[axis];
    int32_t steps = position - axisConfig.position;
    
    return moveRelative(axis, steps, blocking);
}

// CRITICAL: Direct MPG control for immediate response - NO LIMITS!
void ESP32MotionControl::moveDirectMPG(int axis, int32_t steps) {
    if (axis < 0 || axis >= 2) return;
    
    AxisConfig& axisConfig = axes[axis];
    
    if (!axisConfig.enabled || emergencyStop) return;
    
    // Stop any existing motion immediately
    axisConfig.moving = false;
    axisConfig.state = AxisConfig::IDLE;
    
    // Execute steps immediately - NO SPEED/ACCELERATION LIMITS for MPG!
    bool forward = (steps > 0);
    if (axisConfig.inverted) forward = !forward;
    
    int32_t stepsToMove = abs(steps);
    
    // Set direction
    digitalWrite(axisConfig.dirPin, forward ? HIGH : LOW);
    delayMicroseconds(1); // Minimal direction setup time
    
    // Execute each step with MAXIMUM SPEED for immediate MPG response
    for (int32_t i = 0; i < stepsToMove && !emergencyStop; i++) {
        // Check for emergency stop on every step
        if (emergencyStop) break;
        
        // Generate step pulse - FASTEST POSSIBLE for MPG synchronization
        digitalWrite(axisConfig.stepPin, HIGH);
        delayMicroseconds(1);  // Minimal step pulse width
        digitalWrite(axisConfig.stepPin, LOW);
        delayMicroseconds(10); // Minimal step interval - 100kHz for immediate response
        
        // Update position immediately
        if (forward) {
            axisConfig.position++;
        } else {
            axisConfig.position--;
        }
    }
}

// Smooth MPG control with velocity-based step scaling and acceleration
void ESP32MotionControl::moveSmoothMPG(int axis, int32_t steps, int32_t velocity) {
    if (axis < 0 || axis >= 2) return;
    
    AxisConfig& axisConfig = axes[axis];
    
    if (!axisConfig.enabled || emergencyStop) return;
    
    // Stop any existing motion immediately
    axisConfig.moving = false;
    axisConfig.state = AxisConfig::IDLE;
    
    // Calculate velocity-based step scaling (0.01mm to 0.05mm based on velocity)
    float stepScale = calculateMPGStepScale(velocity);
    int32_t scaledSteps = (int32_t)(steps * stepScale);
    
    if (scaledSteps == 0) return; // No movement needed
    
    // Execute smooth movement with acceleration/deceleration
    bool forward = (scaledSteps > 0);
    if (axisConfig.inverted) forward = !forward;
    
    int32_t stepsToMove = abs(scaledSteps);
    
    // Set direction
    digitalWrite(axisConfig.dirPin, forward ? HIGH : LOW);
    delayMicroseconds(2); // Direction setup time
    
    // Calculate smooth timing - gentle acceleration profile  
    uint32_t baseInterval = 80; // 80μs base = 12.5kHz (moderate response)
    uint32_t minInterval = 40;  // 40μs min = 25kHz (gentler maximum speed)
    uint32_t maxInterval = 120; // 120μs max = 8.3kHz (slower start)
    
    // Execute steps with emergency stop checking
    for (int32_t i = 0; i < stepsToMove; i++) {
        // CRITICAL: Check emergency stop before each step
        if (emergencyStop) {
            Serial.println("Emergency stop detected during MPG movement");
            break;
        }
        
        // Calculate very smooth step interval with gentle acceleration curve
        uint32_t stepInterval;
        if (stepsToMove <= 5) {
            // Single-step moves - very slow and smooth
            stepInterval = 500; // 500μs = 2kHz (very gentle)
        } else if (stepsToMove <= 20) {
            // Short moves - moderate speed
            stepInterval = 200; // 200μs = 5kHz (smoother than base)
        } else if (i < stepsToMove / 3) {
            // Gentle acceleration phase (first 33%)
            float accelRatio = (float)i / (stepsToMove / 3);
            // Use exponential curve for smoother start
            accelRatio = accelRatio * accelRatio; // Square for gentler start
            stepInterval = maxInterval - (uint32_t)((maxInterval - minInterval) * accelRatio);
        } else if (i > (2 * stepsToMove) / 3) {
            // Gentle deceleration phase (last 33%)
            float decelRatio = (float)(stepsToMove - i) / (stepsToMove / 3);
            // Use exponential curve for smoother stop
            decelRatio = decelRatio * decelRatio; // Square for gentler stop
            stepInterval = minInterval + (uint32_t)((maxInterval - minInterval) * (1.0f - decelRatio));
        } else {
            // Constant speed phase (middle 33%)
            stepInterval = minInterval;
        }
        
        // Apply velocity-based timing adjustment
        if (abs(velocity) > 100) {
            stepInterval = stepInterval * 2 / 3; // Faster for high velocity
        }
        
        // Generate step pulse
        digitalWrite(axisConfig.stepPin, HIGH);
        delayMicroseconds(2);  // Step pulse width
        digitalWrite(axisConfig.stepPin, LOW);
        
        // Use delay with emergency stop checking for larger intervals
        if (stepInterval > 100) {
            // For long delays, break into smaller chunks to allow emergency stop
            uint32_t chunks = stepInterval / 50;
            for (uint32_t c = 0; c < chunks && !emergencyStop; c++) {
                delayMicroseconds(50);
            }
            delayMicroseconds(stepInterval % 50);
        } else {
            delayMicroseconds(stepInterval);
        }
        
        // Update position immediately
        if (forward) {
            axisConfig.position++;
        } else {
            axisConfig.position--;
        }
    }
}

// Calculate step scale based on MPG velocity (0.01mm to 0.05mm)
float ESP32MotionControl::calculateMPGStepScale(int32_t velocity) {
    float absVelocity = abs(velocity);
    
    // Velocity thresholds (counts per second)
    const float slowThreshold = 10.0f;   // Below this = 0.01mm per detent
    const float fastThreshold = 200.0f;  // Above this = 0.05mm per detent
    
    if (absVelocity <= slowThreshold) {
        return 1.0f; // 0.01mm per detent (5 steps * 0.002mm = 0.01mm)
    } else if (absVelocity >= fastThreshold) {
        return 25.0f; // 0.05mm per detent (25 steps * 0.002mm = 0.05mm)
    } else {
        // Smooth interpolation between 1.0 and 25.0
        float ratio = (absVelocity - slowThreshold) / (fastThreshold - slowThreshold);
        return 1.0f + (24.0f * ratio);
    }
}

// Update MPG velocity for smooth scaling
void ESP32MotionControl::updateMPGVelocity(int encoderIndex) {
    if (encoderIndex < 1 || encoderIndex > 2) return; // Only MPG encoders
    
    EncoderConfig& enc = encoders[encoderIndex];
    uint32_t currentTime = micros();
    
    // Update velocity every 50ms for smooth response
    if (currentTime - enc.velocityUpdateTime > 50000) {
        int32_t deltaCount = enc.count - enc.lastCount;
        uint32_t deltaTime = currentTime - enc.velocityUpdateTime;
        
        if (deltaTime > 0) {
            // Calculate velocity in counts per second
            enc.velocity = (deltaCount * 1000000) / deltaTime;
            
            // Smooth velocity with simple filter
            enc.velocity = (enc.velocity + enc.lastVelocity) / 2;
            enc.lastVelocity = enc.velocity;
        }
        
        enc.velocityUpdateTime = currentTime;
    }
}

bool ESP32MotionControl::enableAxis(int axis) {
    if (axis < 0 || axis >= 2) return false;
    
    AxisConfig& axisConfig = axes[axis];
    axisConfig.enabled = true;
    digitalWrite(axisConfig.enablePin, LOW); // Active low
    
    Serial.println("Axis " + String(axis) + " enabled");
    return true;
}

bool ESP32MotionControl::disableAxis(int axis) {
    if (axis < 0 || axis >= 2) return false;
    
    AxisConfig& axisConfig = axes[axis];
    axisConfig.enabled = false;
    axisConfig.moving = false;
    digitalWrite(axisConfig.enablePin, HIGH); // Active low
    
    Serial.println("Axis " + String(axis) + " disabled");
    return true;
}

int32_t ESP32MotionControl::getEncoderCount(int encoderIndex) {
    if (encoderIndex < 0 || encoderIndex >= 3) return 0;
    
    EncoderConfig& enc = encoders[encoderIndex];
    
    if (enc.usePCNT) {
        // Read from hardware PCNT
        int16_t pcntCount = 0;
        pcnt_get_counter_value(enc.pcntUnit, &pcntCount);
        return (int32_t)pcntCount + enc.offset;
    } else {
        // Fallback to software count
        return enc.count + enc.offset;
    }
}

void ESP32MotionControl::resetEncoderCount(int encoderIndex) {
    if (encoderIndex < 0 || encoderIndex >= 3) return;
    
    encoders[encoderIndex].count = 0;
    encoders[encoderIndex].lastCount = 0;
    encoders[encoderIndex].offset = 0;
}

void ESP32MotionControl::processMPGInputs() {
    // IMMEDIATE MPG control with hardware PCNT (no debouncing needed)
    for (int i = 1; i < 3; i++) { // Encoders 1 and 2 are MPGs
        EncoderConfig& enc = encoders[i];
        
        // Update velocity tracking
        updateMPGVelocity(i);
        
        int32_t currentCount = getEncoderCount(i);
        int32_t delta = currentCount - enc.lastCount;
        
        if (delta != 0) {
            int axis = (i == 1) ? 1 : 0; // Encoder 1 = Z-axis, Encoder 2 = X-axis
            
            // Convert pulses to movement immediately (hardware PCNT eliminates noise)
            int32_t steps = delta * 5; // 5 steps per pulse (0.01mm per detent / 4 pulses)
            
            if (axes[axis].enabled && !emergencyStop && delta != 0) {
                // Get current velocity for scaling
                int32_t velocity = enc.velocity;
                float stepScale = calculateMPGStepScale(velocity);
                
                Serial.printf("MPG: Axis %d, delta=%d, vel=%d, scale=%.2f, steps=%d\n", 
                             axis, delta, velocity, stepScale, steps);
                
                // IMMEDIATE MPG CONTROL with velocity-based scaling
                moveSmoothMPG(axis, steps, velocity);
            }
            
            enc.lastCount = currentCount;
        }
    }
}

void ESP32MotionControl::update() {
    // CRITICAL: Check emergency stop first
    if (emergencyStop) {
        // During emergency stop, reset MPG tracking to prevent unwanted movement
        for (int i = 1; i < 3; i++) {
            encoders[i].lastCount = getEncoderCount(i);
        }
        return; // Skip all other processing during emergency stop
    }
    
    // Process MPG inputs (motion control runs in separate task)
    processMPGInputs();
}

void ESP32MotionControl::processMotionQueue() {
    while (!motionQueue.empty() && !emergencyStop) {
        MotionCommand cmd;
        if (!motionQueue.front(cmd)) break;
        
        // Check if command should be executed now
        if (cmd.timestamp == 0 || micros() >= cmd.timestamp) {
            motionQueue.pop(cmd);
            
            // Execute command
            switch (cmd.type) {
                case MotionCommand::MOVE_RELATIVE:
                    moveRelative(cmd.axis, cmd.value, cmd.blocking);
                    break;
                    
                case MotionCommand::MOVE_ABSOLUTE:
                    moveAbsolute(cmd.axis, cmd.value, cmd.blocking);
                    break;
                    
                case MotionCommand::STOP_AXIS:
                    stopAxis(cmd.axis);
                    break;
                    
                case MotionCommand::ENABLE_AXIS:
                    enableAxis(cmd.axis);
                    break;
                    
                case MotionCommand::DISABLE_AXIS:
                    disableAxis(cmd.axis);
                    break;
                    
                default:
                    break;
            }
        } else {
            break; // Wait for timestamp
        }
    }
}

String ESP32MotionControl::getStatusReport() {
    String status = "ESP32-S3 Motion Control Status (Task-Based):\n";
    
    // Axis status
    for (int i = 0; i < 2; i++) {
        char axisName = (i == 0) ? 'X' : 'Z';
        status += String(axisName) + "-axis: Pos=" + String(axes[i].position) + 
                 " Speed=" + String(axes[i].currentSpeed) + "Hz ";
        status += axes[i].enabled ? "ENABLED " : "DISABLED ";
        status += axes[i].moving ? "MOVING" : "STOPPED";
        status += "\n";
    }
    
    // Encoder status
    for (int i = 0; i < 3; i++) {
        status += String(encoders[i].name) + ": " + String(getEncoderCount(i)) + " counts";
        if (encoders[i].errorCount > 0) {
            status += " (Errors: " + String(encoders[i].errorCount) + ")";
        }
        status += "\n";
    }
    
    // Queue status
    status += "Queue: " + String(motionQueue.size()) + "/" + String(motionQueue.capacity());
    status += " (" + String(motionQueue.utilization(), 1) + "% util)\n";
    
    // Emergency stop status
    status += "E-Stop: " + String(emergencyStop ? "ACTIVE" : "OK");
    
    return status;
}

void ESP32MotionControl::printDiagnostics() {
    Serial.println("=== ESP32-S3 Motion Control Diagnostics (Task-Based) ===");
    Serial.println(getStatusReport());
    Serial.println("==========================================================");
}

// Implement all the missing methods
bool ESP32MotionControl::setPosition(int axis, int32_t position) {
    if (axis < 0 || axis >= 2) return false;
    axes[axis].position = position;
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

int32_t ESP32MotionControl::getPosition(int axis) {
    if (axis < 0 || axis >= 2) return 0;
    return axes[axis].position;
}

bool ESP32MotionControl::setSpeed(int axis, uint32_t speed) {
    if (axis < 0 || axis >= 2) return false;
    axes[axis].targetSpeed = speed;
    return true;
}

bool ESP32MotionControl::setAcceleration(int axis, uint32_t accel) {
    if (axis < 0 || axis >= 2) return false;
    axes[axis].acceleration = accel;
    return true;
}

bool ESP32MotionControl::stopAxis(int axis) {
    if (axis < 0 || axis >= 2) return false;
    
    // IMMEDIATE STOP - CRITICAL FOR SAFETY
    axes[axis].moving = false;
    axes[axis].targetPosition = axes[axis].position;
    axes[axis].state = AxisConfig::IDLE;
    
    return true;
}

bool ESP32MotionControl::stopAll() {
    for (int i = 0; i < 2; i++) {
        stopAxis(i);
    }
    return true;
}

bool ESP32MotionControl::queueCommand(const MotionCommand& cmd) {
    if (emergencyStop) return false;
    return motionQueue.push(cmd);
}

void ESP32MotionControl::clearMotionQueue() {
    motionQueue.clear();
}

void ESP32MotionControl::setEmergencyStop(bool stop) {
    emergencyStop = stop;
    if (stop) {
        // IMMEDIATE EMERGENCY STOP - CRITICAL SAFETY
        Serial.println("*** EMERGENCY STOP: All motion stopped immediately ***");
        
        // Stop all axes immediately
        for (int i = 0; i < 2; i++) {
            axes[i].moving = false;
            axes[i].targetPosition = axes[i].position;
            axes[i].state = AxisConfig::IDLE;
        }
        
        // Clear motion queue
        motionQueue.clear();
        
        // Reset MPG tracking to prevent unwanted movement
        for (int i = 1; i < 3; i++) {
            encoders[i].lastCount = getEncoderCount(i);
        }
    }
}

// Placeholder methods for turning mode
bool ESP32MotionControl::startTurningMode(float feedRatio, int passes) {
    Serial.println("Starting turning mode: feedRatio=" + String(feedRatio) + ", passes=" + String(passes));
    return true;
}

bool ESP32MotionControl::stopTurningMode() {
    Serial.println("Stopping turning mode");
    return true;
}

bool ESP32MotionControl::isTurningModeActive() {
    return false;
}

void ESP32MotionControl::shutdown() {
    emergencyStop = true;
    
    // Delete motion control task
    if (motionTaskHandle != nullptr) {
        vTaskDelete(motionTaskHandle);
        motionTaskHandle = nullptr;
    }
    
    // Disable all axes
    for (int i = 0; i < 2; i++) {
        disableAxis(i);
    }
    
    // Clear motion queue
    motionQueue.clear();
    
    Serial.println("ESP32-S3 Motion Control shutdown complete");
}