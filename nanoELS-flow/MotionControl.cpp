#include "MotionControl.h"

// Global instance
MotionControl motionControl;

// Spindle encoder data (volatile for ISR access)
volatile SpindleData spindleData = {0, 0, 0, false};

// MPG interrupt handlers (static for ISR compatibility)
static volatile int32_t mpgXPulseCount = 0;
static volatile int32_t mpgZPulseCount = 0;
static volatile uint32_t mpgXLastPulseTime = 0;
static volatile uint32_t mpgZLastPulseTime = 0;

MotionControl::MotionControl() {
  engine = nullptr;
  stepperX = nullptr;
  stepperZ = nullptr;
  emergencyStop = false;
  limitsEnabled = true;
  
  // Initialize axis configurations with safe defaults
  axisX = {X_STEP, X_DIR, X_ENA, 2000, 4000, 0, -100000, 100000, false, false, nullptr};
  axisZ = {Z_STEP, Z_DIR, Z_ENA, 2000, 4000, 0, -100000, 100000, false, false, nullptr};
  
  // Initialize MPG configurations (assign members individually)
  mpgX.pulsePinA = X_PULSE_A;
  mpgX.pulsePinB = X_PULSE_B;
  mpgX.axis = 0;
  mpgX.pulseCount = 0;
  mpgX.lastPulseCount = 0;
  mpgX.lastPulseTime = 0;
  mpgX.stepRatio = 1.0;
  mpgX.enabled = false;
  mpgX.operationActive = false;

  mpgZ.pulsePinA = Z_PULSE_A;
  mpgZ.pulsePinB = Z_PULSE_B;
  mpgZ.axis = 1;
  mpgZ.pulseCount = 0;
  mpgZ.lastPulseCount = 0;
  mpgZ.lastPulseTime = 0;
  mpgZ.stepRatio = 1.0;
  mpgZ.enabled = false;
  mpgZ.operationActive = false;
  
  // Link MPG to axes
  axisX.mpg = &mpgX;
  axisZ.mpg = &mpgZ;
  
  // Initialize spindle data
  spindle = {0, 0, 0, false};
  
  // Initialize operation setup
  operation = {1.5, 1, false, 0.0, 1, 0.1, false};
}

MotionControl::~MotionControl() {
  shutdown();
  // Note: engine is a static instance, so we don't delete it
}

bool MotionControl::initialize() {
  Serial.println("Initializing MotionControl...");
  
  // Initialize FastAccelStepper engine
  // Use global instance approach common in Arduino libraries
  static FastAccelStepperEngine staticEngine;
  engine = &staticEngine;
  engine->init();
  
  // Connect steppers to pins (permanent pin assignments per project rules)
  stepperX = engine->stepperConnectToPin(X_STEP);
  stepperZ = engine->stepperConnectToPin(Z_STEP);
  
  if (!stepperX || !stepperZ) {
    Serial.println("ERROR: Failed to connect steppers to pins");
    return false;
  }
  
  // Initialize both axes
  initializeAxis(0);  // X-axis
  initializeAxis(1);  // Z-axis
  
  // Initialize MPG systems
  initializeMPG(0);   // X-axis MPG
  initializeMPG(1);   // Z-axis MPG
  
  // Initialize spindle encoder
  initializeSpindleEncoder();
  
  Serial.println("✓ MotionControl initialized successfully");
  return true;
}

void MotionControl::initializeAxis(uint8_t axis) {
  FastAccelStepper* stepper = (axis == 0) ? stepperX : stepperZ;
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  
  if (!stepper) return;
  
  // Configure stepper pins
  stepper->setDirectionPin(config->dirPin);
  stepper->setEnablePin(config->enablePin);
  stepper->setAutoEnable(true);
  
  // Set default speed and acceleration
  stepper->setSpeedInHz(config->maxSpeed);
  stepper->setAcceleration(config->maxAccel);
  
  // Configure direction inversion if needed
  if (config->inverted) {
    stepper->setDirectionPin(config->dirPin, true);
  }
  
  // Enable axis by default
  enableAxis(axis);
  
  Serial.printf("✓ %c-axis initialized (Step:%d, Dir:%d, Enable:%d)\n", 
                getCharFromAxis(axis), config->stepPin, config->dirPin, config->enablePin);
}

void MotionControl::initializeMPG(uint8_t axis) {
  MPGConfig* mpg = (axis == 0) ? &mpgX : &mpgZ;
  
  // Configure MPG pins as inputs with pullups
  pinMode(mpg->pulsePinA, INPUT_PULLUP);
  pinMode(mpg->pulsePinB, INPUT_PULLUP);
  
  // Attach interrupts for quadrature decoding
  if (axis == 0) {
    attachInterrupt(digitalPinToInterrupt(mpg->pulsePinA), mpgXISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(mpg->pulsePinB), mpgXISR, CHANGE);
  } else {
    attachInterrupt(digitalPinToInterrupt(mpg->pulsePinA), mpgZISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(mpg->pulsePinB), mpgZISR, CHANGE);
  }
  
  // Enable MPG by default
  mpg->enabled = true;
  
  Serial.printf("✓ %c-axis MPG initialized (A:%d, B:%d)\n", 
                getCharFromAxis(axis), mpg->pulsePinA, mpg->pulsePinB);
}

// MPG interrupt handlers
void IRAM_ATTR MotionControl::mpgXISR() {
  static uint8_t lastState = 0;
  uint8_t currentState = (digitalRead(X_PULSE_A) << 1) | digitalRead(X_PULSE_B);
  
  // Simple quadrature decoding
  if (lastState == 0b00 && currentState == 0b01) mpgXPulseCount++;
  else if (lastState == 0b01 && currentState == 0b11) mpgXPulseCount++;
  else if (lastState == 0b11 && currentState == 0b10) mpgXPulseCount++;
  else if (lastState == 0b10 && currentState == 0b00) mpgXPulseCount++;
  else if (lastState == 0b01 && currentState == 0b00) mpgXPulseCount--;
  else if (lastState == 0b11 && currentState == 0b01) mpgXPulseCount--;
  else if (lastState == 0b10 && currentState == 0b11) mpgXPulseCount--;
  else if (lastState == 0b00 && currentState == 0b10) mpgXPulseCount--;
  
  lastState = currentState;
  mpgXLastPulseTime = micros();
}

void IRAM_ATTR MotionControl::mpgZISR() {
  static uint8_t lastState = 0;
  uint8_t currentState = (digitalRead(Z_PULSE_A) << 1) | digitalRead(Z_PULSE_B);
  
  // Simple quadrature decoding
  if (lastState == 0b00 && currentState == 0b01) mpgZPulseCount++;
  else if (lastState == 0b01 && currentState == 0b11) mpgZPulseCount++;
  else if (lastState == 0b11 && currentState == 0b10) mpgZPulseCount++;
  else if (lastState == 0b10 && currentState == 0b00) mpgZPulseCount++;
  else if (lastState == 0b01 && currentState == 0b00) mpgZPulseCount--;
  else if (lastState == 0b11 && currentState == 0b01) mpgZPulseCount--;
  else if (lastState == 0b10 && currentState == 0b11) mpgZPulseCount--;
  else if (lastState == 0b00 && currentState == 0b10) mpgZPulseCount--;
  
  lastState = currentState;
  mpgZLastPulseTime = micros();
}

void MotionControl::initializeSpindleEncoder() {
  // Simple interrupt-based encoder (avoiding PCNT conflicts)
  // Configure encoder pins as inputs with pullups
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  
  // For now, we'll implement a basic interrupt-based counter
  // TODO: Implement quadrature decoding with interrupts
  // This avoids the PCNT API conflicts with FastAccelStepper
  
  Serial.printf("✓ Spindle encoder pins configured (A:%d, B:%d)\n", ENC_A, ENC_B);
  Serial.println("  NOTE: Using simplified encoder interface (PCNT conflicts avoided)");
}

bool MotionControl::enableAxis(uint8_t axis) {
  if (axis > 1) return false;
  
  FastAccelStepper* stepper = (axis == 0) ? stepperX : stepperZ;
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  
  if (stepper) {
    stepper->enableOutputs();
    config->enabled = true;
    Serial.printf("✓ %c-axis enabled\n", getCharFromAxis(axis));
    return true;
  }
  return false;
}

bool MotionControl::disableAxis(uint8_t axis) {
  if (axis > 1) return false;
  
  FastAccelStepper* stepper = (axis == 0) ? stepperX : stepperZ;
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  
  if (stepper) {
    stepper->disableOutputs();
    config->enabled = false;
    Serial.printf("✓ %c-axis disabled\n", getCharFromAxis(axis));
    return true;
  }
  return false;
}

bool MotionControl::isAxisEnabled(uint8_t axis) {
  if (axis > 1) return false;
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  return config->enabled;
}

// MPG control methods
bool MotionControl::enableMPG(uint8_t axis) {
  if (axis > 1) return false;
  MPGConfig* mpg = (axis == 0) ? &mpgX : &mpgZ;
  mpg->enabled = true;
  Serial.printf("✓ %c-axis MPG enabled\n", getCharFromAxis(axis));
  return true;
}

bool MotionControl::disableMPG(uint8_t axis) {
  if (axis > 1) return false;
  MPGConfig* mpg = (axis == 0) ? &mpgX : &mpgZ;
  mpg->enabled = false;
  Serial.printf("✓ %c-axis MPG disabled\n", getCharFromAxis(axis));
  return true;
}

bool MotionControl::isMPGEnabled(uint8_t axis) {
  if (axis > 1) return false;
  MPGConfig* mpg = (axis == 0) ? &mpgX : &mpgZ;
  return mpg->enabled;
}

void MotionControl::setMPGRatio(uint8_t axis, float ratio) {
  if (axis > 1) return;
  MPGConfig* mpg = (axis == 0) ? &mpgX : &mpgZ;
  mpg->stepRatio = ratio;
  Serial.printf("✓ %c-axis MPG ratio set to %.2f\n", getCharFromAxis(axis), ratio);
}

float MotionControl::getMPGRatio(uint8_t axis) {
  if (axis > 1) return 0.0;
  MPGConfig* mpg = (axis == 0) ? &mpgX : &mpgZ;
  return mpg->stepRatio;
}

int32_t MotionControl::getMPGPulseCount(uint8_t axis) {
  if (axis == 0) return mpgXPulseCount;
  if (axis == 1) return mpgZPulseCount;
  return 0;
}

int32_t MotionControl::getXMPGPulseCount() const {
  return mpgX.pulseCount;
}

int32_t MotionControl::getZMPGPulseCount() const {
  return mpgZ.pulseCount;
}

bool MotionControl::queueCommand(const MotionCommand& cmd) {
  if (emergencyStop) {
    Serial.println("WARNING: Emergency stop active, command rejected");
    return false;
  }
  
  commandQueue.push(cmd);
  return true;
}

bool MotionControl::executeImmediate(const MotionCommand& cmd) {
  if (emergencyStop) {
    Serial.println("WARNING: Emergency stop active, command rejected");
    return false;
  }
  
  executeCommand(cmd);
  return true;
}

void MotionControl::executeCommand(const MotionCommand& cmd) {
  if (cmd.axis > 1) return;
  
  FastAccelStepper* stepper = (cmd.axis == 0) ? stepperX : stepperZ;
  AxisConfig* config = (cmd.axis == 0) ? &axisX : &axisZ;
  
  if (!stepper || !config->enabled) return;
  
  switch (cmd.type) {
    case CMD_MOVE_RELATIVE:
      if (checkLimits(cmd.axis, config->position + cmd.value)) {
        stepper->move(cmd.value);
        config->position += cmd.value;
      }
      break;
      
    case CMD_MOVE_ABSOLUTE:
      if (checkLimits(cmd.axis, cmd.value)) {
        int32_t steps = cmd.value - config->position;
        stepper->move(steps);
        config->position = cmd.value;
      }
      break;
      
    case CMD_MPG_MOVE:
      // Direct MPG-controlled movement
      if (config->mpg && config->mpg->enabled) {
        int32_t steps = (int32_t)(cmd.value * config->mpg->stepRatio);
        if (checkLimits(cmd.axis, config->position + steps)) {
          stepper->move(steps);
          config->position += steps;
        }
      }
      break;
      
    case CMD_SET_SPEED:
      stepper->setSpeedInHz(cmd.value);
      break;
      
    case CMD_SET_ACCELERATION:
      stepper->setAcceleration(cmd.value);
      break;
      
    case CMD_STOP:
      stepper->forceStopAndNewPosition(stepper->getCurrentPosition());
      break;
      
    case CMD_ENABLE_AXIS:
      enableAxis(cmd.axis);
      break;
      
    case CMD_DISABLE_AXIS:
      disableAxis(cmd.axis);
      break;
      
    case CMD_SYNC_POSITION:
      // Future implementation for spindle sync
      break;
      
    case CMD_SYNC_SPEED:
      // Future implementation for spindle sync
      break;
  }
}

void MotionControl::processCommandQueue() {
  while (!commandQueue.empty() && !emergencyStop) {
    MotionCommand cmd = commandQueue.front();
    
    // Check if command should be executed now
    if (cmd.timestamp == 0 || micros() >= cmd.timestamp) {
      executeCommand(cmd);
      commandQueue.pop();
      
      // If blocking command, wait for completion
      if (cmd.blocking) {
        while (isMoving(cmd.axis) && !emergencyStop) {
          delay(1);
        }
      }
    } else {
      break; // Wait for timestamp
    }
  }
}

void MotionControl::clearCommandQueue() {
  while (!commandQueue.empty()) {
    commandQueue.pop();
  }
  Serial.println("✓ Command queue cleared");
}

bool MotionControl::checkLimits(uint8_t axis, int32_t targetPosition) {
  if (!limitsEnabled) return true;
  
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  
  if (targetPosition < config->minLimit || targetPosition > config->maxLimit) {
    Serial.printf("WARNING: %c-axis limit exceeded (target: %d, limits: %d to %d)\n",
                  getCharFromAxis(axis), targetPosition, config->minLimit, config->maxLimit);
    return false;
  }
  return true;
}

int32_t MotionControl::getPosition(uint8_t axis) {
  if (axis > 1) return 0;
  
  FastAccelStepper* stepper = (axis == 0) ? stepperX : stepperZ;
  if (stepper) {
    return stepper->getCurrentPosition();
  }
  return 0;
}

bool MotionControl::setPosition(uint8_t axis, int32_t position) {
  if (axis > 1) return false;
  
  FastAccelStepper* stepper = (axis == 0) ? stepperX : stepperZ;
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  
  if (stepper) {
    stepper->setCurrentPosition(position);
    config->position = position;
    return true;
  }
  return false;
}

bool MotionControl::isMoving(uint8_t axis) {
  if (axis > 1) return false;
  
  FastAccelStepper* stepper = (axis == 0) ? stepperX : stepperZ;
  if (stepper) {
    return stepper->isRunning();
  }
  return false;
}

bool MotionControl::isMoving() {
  return isMoving(0) || isMoving(1);
}

bool MotionControl::moveRelative(uint8_t axis, int32_t steps, bool blocking) {
  MotionCommand cmd = createMoveCommand(axis, steps, true);
  cmd.blocking = blocking;
  return executeImmediate(cmd);
}

bool MotionControl::moveAbsolute(uint8_t axis, int32_t position, bool blocking) {
  MotionCommand cmd = createMoveCommand(axis, position, false);
  cmd.blocking = blocking;
  return executeImmediate(cmd);
}

bool MotionControl::stopAxis(uint8_t axis) {
  MotionCommand cmd = createStopCommand(axis);
  return executeImmediate(cmd);
}

bool MotionControl::stopAll() {
  bool success = true;
  success &= stopAxis(0);  // X-axis
  success &= stopAxis(1);  // Z-axis
  return success;
}

int32_t MotionControl::getSpindlePosition() {
  // Simple encoder reading (avoiding PCNT API conflicts)
  // TODO: Implement proper quadrature decoding
  static int32_t simulatedPosition = 0;
  
  // For now, return a simulated position that increments slowly
  // This will be replaced with proper encoder reading later
  simulatedPosition += (millis() % 100 == 0) ? 1 : 0;
  
  return simulatedPosition;
}

int32_t MotionControl::getSpindleRPM() {
  // Return calculated RPM from spindle data
  return spindle.rpm;
}

void MotionControl::updateSpindleData() {
  static int32_t lastPosition = 0;
  static uint32_t lastTime = 0;
  
  int32_t currentPosition = getSpindlePosition();
  uint32_t currentTime = millis();
  
  if (currentTime - lastTime >= 100) { // Update every 100ms
    int32_t deltaPosition = currentPosition - lastPosition;
    uint32_t deltaTime = currentTime - lastTime;
    
    // Calculate RPM (assuming ENCODER_PPR pulses per revolution)
    if (deltaTime > 0) {
      spindle.rpm = (deltaPosition * 60000) / (ENCODER_PPR * deltaTime);
    }
    
    spindle.position = currentPosition;
    spindle.lastUpdate = currentTime;
    
    lastPosition = currentPosition;
    lastTime = currentTime;
  }
}

void MotionControl::update() {
  // Process command queue
  processCommandQueue();
  
  // Process MPG input
  processMPGInput(0);  // X-axis
  processMPGInput(1);  // Z-axis
  
  // Update spindle data
  updateSpindleData();
  
  // Check emergency stop
  if (emergencyStop) {
    stopAll();
  }
}

void MotionControl::processMPGInput(uint8_t axis) {
  if (axis > 1) return;
  
  MPGConfig* mpg = (axis == 0) ? &mpgX : &mpgZ;
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  
  if (!mpg->enabled || !config->enabled) return;
  
  // Get current pulse count
  int32_t currentPulseCount = (axis == 0) ? mpgXPulseCount : mpgZPulseCount;
  int32_t pulseDelta = currentPulseCount - mpg->lastPulseCount;
  
  if (pulseDelta != 0) {
    // Create MPG movement command
    MotionCommand cmd;
    cmd.type = CMD_MPG_MOVE;
    cmd.axis = axis;
    cmd.value = pulseDelta;
    cmd.timestamp = micros();
    cmd.blocking = false;
    cmd.mpgRatio = mpg->stepRatio;
    
    // Execute immediately for real-time response
    executeImmediate(cmd);
    
    // Update last pulse count
    mpg->lastPulseCount = currentPulseCount;
    mpg->lastPulseTime = micros();
  }
}

// Operation setup methods
void MotionControl::setThreadPitch(float pitch) {
  operation.threadPitch = pitch;
  Serial.printf("✓ Thread pitch set to %.2f mm\n", pitch);
}

float MotionControl::getThreadPitch() {
  return operation.threadPitch;
}

void MotionControl::setThreadStarts(int starts) {
  operation.threadStarts = starts;
  Serial.printf("✓ Thread starts set to %d\n", starts);
}

int MotionControl::getThreadStarts() {
  return operation.threadStarts;
}

void MotionControl::setThreadLeftHand(bool leftHand) {
  operation.threadLeftHand = leftHand;
  Serial.printf("✓ Thread direction set to %s\n", leftHand ? "left-hand" : "right-hand");
}

bool MotionControl::getThreadLeftHand() {
  return operation.threadLeftHand;
}

void MotionControl::setTaperAngle(float angle) {
  operation.taperAngle = angle;
  Serial.printf("✓ Taper angle set to %.2f degrees\n", angle);
}

float MotionControl::getTaperAngle() {
  return operation.taperAngle;
}

void MotionControl::setOperationPasses(int passes) {
  operation.operationPasses = passes;
  Serial.printf("✓ Operation passes set to %d\n", passes);
}

int MotionControl::getOperationPasses() {
  return operation.operationPasses;
}

void MotionControl::setFeedRate(float feedRate) {
  operation.feedRate = feedRate;
  Serial.printf("✓ Feed rate set to %.3f mm/rev\n", feedRate);
}

float MotionControl::getFeedRate() {
  return operation.feedRate;
}

void MotionControl::startOperation() {
  operation.operationActive = true;
  Serial.println("✓ Operation started");
}

void MotionControl::stopOperation() {
  operation.operationActive = false;
  Serial.println("✓ Operation stopped");
}

bool MotionControl::isOperationActive() {
  return operation.operationActive;
}

void MotionControl::setEmergencyStop(bool stop) {
  emergencyStop = stop;
  if (stop) {
    stopAll();
    clearCommandQueue();
    Serial.println("EMERGENCY STOP ACTIVATED");
  } else {
    Serial.println("Emergency stop released");
  }
}

bool MotionControl::getEmergencyStop() {
  return emergencyStop;
}

bool MotionControl::setSpeed(uint8_t axis, uint32_t speed) {
  if (axis > 1) return false;
  
  FastAccelStepper* stepper = (axis == 0) ? stepperX : stepperZ;
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  
  if (stepper) {
    stepper->setSpeedInHz(speed);
    config->maxSpeed = speed;  // Store the speed for retrieval
    return true;
  }
  return false;
}

bool MotionControl::setAcceleration(uint8_t axis, uint32_t accel) {
  if (axis > 1) return false;
  
  FastAccelStepper* stepper = (axis == 0) ? stepperX : stepperZ;
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  
  if (stepper) {
    stepper->setAcceleration(accel);
    config->maxAccel = accel;  // Store the acceleration for retrieval
    return true;
  }
  return false;
}

uint32_t MotionControl::getSpeed(uint8_t axis) {
  if (axis > 1) return 0;
  
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  return config->maxSpeed;
}

uint32_t MotionControl::getAcceleration(uint8_t axis) {
  if (axis > 1) return 0;
  
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  return config->maxAccel;
}

bool MotionControl::setLimits(uint8_t axis, int32_t minLimit, int32_t maxLimit) {
  if (axis > 1) return false;
  
  AxisConfig* config = (axis == 0) ? &axisX : &axisZ;
  config->minLimit = minLimit;
  config->maxLimit = maxLimit;
  
  Serial.printf("✓ %c-axis limits set: %d to %d\n", 
                getCharFromAxis(axis), minLimit, maxLimit);
  return true;
}

void MotionControl::enableLimits(bool enable) {
  limitsEnabled = enable;
  Serial.printf("✓ Software limits %s\n", enable ? "enabled" : "disabled");
}

bool MotionControl::syncWithSpindle(uint8_t axis, float ratio) {
  // TODO: Implement spindle synchronization
  Serial.printf("TODO: Sync %c-axis with spindle (ratio: %.3f)\n", 
                getCharFromAxis(axis), ratio);
  return false;
}

void MotionControl::stopSync(uint8_t axis) {
  // TODO: Stop spindle synchronization
  Serial.printf("TODO: Stop %c-axis spindle sync\n", getCharFromAxis(axis));
}

String MotionControl::getStatus() {
  String status = "Motion Status:\n";
  status += "X-axis: " + String(getPosition(0)) + " steps, ";
  status += isMoving(0) ? "MOVING" : "STOPPED";
  status += isAxisEnabled(0) ? " (ENABLED)" : " (DISABLED)";
  status += "\n";
  
  status += "Z-axis: " + String(getPosition(1)) + " steps, ";
  status += isMoving(1) ? "MOVING" : "STOPPED";
  status += isAxisEnabled(1) ? " (ENABLED)" : " (DISABLED)";
  status += "\n";
  
  status += "Spindle: " + String(getSpindlePosition()) + " counts, ";
  status += String(spindle.rpm) + " RPM\n";
  
  status += "Queue: " + String(commandQueue.size()) + " commands\n";
  status += "E-Stop: " + String(emergencyStop ? "ACTIVE" : "OK");
  
  return status;
}

void MotionControl::printDiagnostics() {
  Serial.println("=== MotionControl Diagnostics ===");
  Serial.println(getStatus());
  Serial.println("================================");
}

void MotionControl::shutdown() {
  setEmergencyStop(true);
  disableAxis(0);
  disableAxis(1);
  Serial.println("MotionControl shutdown complete");
}

// Helper function implementations
MotionCommand createMoveCommand(uint8_t axis, int32_t steps, bool relative) {
  MotionCommand cmd;
  cmd.type = relative ? CMD_MOVE_RELATIVE : CMD_MOVE_ABSOLUTE;
  cmd.axis = axis;
  cmd.value = steps;
  cmd.timestamp = 0;
  cmd.blocking = false;
  return cmd;
}

MotionCommand createSpeedCommand(uint8_t axis, uint32_t speed) {
  MotionCommand cmd;
  cmd.type = CMD_SET_SPEED;
  cmd.axis = axis;
  cmd.value = speed;
  cmd.timestamp = 0;
  cmd.blocking = false;
  return cmd;
}

MotionCommand createStopCommand(uint8_t axis) {
  MotionCommand cmd;
  cmd.type = CMD_STOP;
  cmd.axis = axis;
  cmd.value = 0;
  cmd.timestamp = 0;
  cmd.blocking = false;
  return cmd;
}

// Enhanced command creation helpers
MotionCommand createMPGCommand(uint8_t axis, int32_t pulses, float ratio) {
  MotionCommand cmd;
  cmd.type = CMD_MPG_MOVE;
  cmd.axis = axis;
  cmd.value = pulses;
  cmd.timestamp = micros();
  cmd.blocking = false;
  cmd.mpgRatio = ratio;
  return cmd;
}