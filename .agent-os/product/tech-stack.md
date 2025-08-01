# Tech Stack: nanoELS-flow

## Hardware Components

### Core Controller
- **ESP32-S3-dev board (H5 variant)** - Main microcontroller
- **Arduino IDE** - Development environment
- **arduino-cli** - Command-line compilation and upload

### Motion Control
- **Teknic ClearPath servos** - High-precision stepper motors
- **Quadrature encoders (600 PPR)** - Position feedback
- **PS2 keyboard** - User input interface
- **Nextion display** - User interface display

### Communication
- **WiFi** - Network connectivity
- **WebSockets** - Real-time communication
- **Web interface** - Remote control and monitoring

## Software Architecture

### Core Libraries
- **FreeRTOS** - Real-time operating system for ESP32
- **Preferences** - Persistent storage
- **LittleFS** - File system for G-code storage
- **WebSocketsServer** - Real-time communication
- **PS2KeyAdvanced** - Keyboard input handling

### Motion Control System
- **MinimalMotionControl** - Core motion algorithms
- **OperationManager** - High-level operation coordination
- **StateMachine** - State-based operation management

## Spindle Synchronization Implementation

### Core Principles (Based on h5.ino Analysis)

The spindle synchronization system is the heart of the Electronic Lead Screw (ELS) functionality. Here's how it works:

#### 1. **Spindle Position Tracking**
```cpp
// Key variables for spindle tracking
long spindlePos = 0;           // Current spindle position in encoder steps
long spindlePosAvg = 0;        // Spindle position with backlash compensation
long spindlePosGlobal = 0;     // Global spindle position (unaffected by zeroing)
int spindlePosSync = 0;        // Synchronization offset when out of sync
```

#### 2. **Position Calculation Algorithm**
```cpp
// Calculate stepper position from spindle position
long posFromSpindle(Axis* a, long s, bool respectStops) {
  long newPos = s * a->motorSteps / a->screwPitch / ENCODER_STEPS_FLOAT * dupr * starts;
  
  // Respect left/right stops
  if (respectStops) {
    if (newPos < a->rightStop) newPos = a->rightStop;
    else if (newPos > a->leftStop) newPos = a->leftStop;
  }
  return newPos;
}

// Calculate spindle position from stepper position
long spindleFromPos(Axis* a, long p) {
  return p * a->screwPitch * ENCODER_STEPS_FLOAT / a->motorSteps / (dupr * starts);
}
```

#### 3. **Spindle Following Logic**
The system continuously updates axis positions based on spindle movement:

```cpp
void modeGearbox() {
  if (z.movingManually) return;
  z.speedMax = LONG_MAX;
  stepToContinuous(&z, posFromSpindle(&z, spindlePosAvg, true));
}
```

#### 4. **Stopping Mechanisms**

**A. Physical Stops (Left/Right Limits)**
- When an axis reaches its physical stop, the spindle continues rotating
- The system tracks when the axis is "standing on a stop"
- Full spindle turns are discounted to avoid waiting during direction reversals

**B. Synchronization Loss Detection**
```cpp
void leaveStop(Axis* a, long oldStop) {
  if (mode == MODE_NORMAL && a == getPitchAxis() && a->pos == oldStop) {
    // Spindle is out of sync because it was spinning while lead screw was on stop
    spindlePosSync = spindleModulo(spindlePos - spindleFromPos(a, a->pos));
  }
}
```

**C. Synchronization Recovery**
```cpp
if (spindlePosSync != 0) {
  spindlePosSync += delta;
  if (spindlePosSync % ENCODER_STEPS_INT == 0) {
    spindlePosSync = 0;
    Axis* a = getPitchAxis();
    spindlePosAvg = spindlePos = spindleFromPos(a, a->pos);
  }
}
```

#### 5. **Backlash Compensation**
```cpp
// Encoder backlash compensation
if (spindlePos > spindlePosAvg) {
  spindlePosAvg = spindlePos;
} else if (spindlePos < spindlePosAvg - ENCODER_BACKLASH) {
  spindlePosAvg = spindlePos + ENCODER_BACKLASH;
}
```

#### 6. **Full Turn Discounting**
```cpp
void discountFullSpindleTurns() {
  // When standing at stop, ignore full spindle turns
  // This avoids waiting when spindle direction reverses
  if (dupr != 0 && !stepperIsRunning(&z) && (mode == MODE_NORMAL || mode == MODE_CONE)) {
    // Calculate and apply full turn offsets based on current position vs stop position
  }
}
```

### Key Implementation Requirements

#### 1. **Real-time Response**
- Spindle position updates must be processed immediately
- No speed/acceleration limits in spindle-synchronized mode
- Direct step pulse generation based on spindle position

#### 2. **Precision Tracking**
- Maintain exact position relationship between spindle and axis
- Handle encoder backlash and mechanical backlash
- Account for multi-start threads (starts > 1)

#### 3. **Stop Detection**
- Detect when axis reaches physical limits
- Handle synchronization loss when spindle continues while axis is stopped
- Implement recovery mechanisms for re-synchronization

#### 4. **Mode-specific Behavior**
- **Normal Mode**: Continuous spindle following with stop respect
- **Thread Mode**: Multi-pass operations with precise start/stop synchronization
- **Cone Mode**: Coordinated X-Z movement based on spindle position
- **Manual Mode**: Independent axis movement with speed limits

### Critical Design Decisions

1. **No Speed Limits in Spindle Mode**: When `spindle.threadingActive` is true, the system bypasses speed ramping and generates step pulses directly based on spindle position.

2. **Immediate Position Updates**: The system calculates target positions from spindle position and moves immediately, ensuring the axis follows the spindle without delay.

3. **Stop Respect**: Physical stops are respected by clamping the calculated position, but the spindle continues to be tracked.

4. **Synchronization Recovery**: When synchronization is lost (e.g., axis stopped while spindle continued), the system calculates the offset and gradually recovers synchronization.

This implementation ensures that the axis movement is perfectly synchronized with spindle rotation, enabling precise threading, turning, and other spindle-synchronized operations. 