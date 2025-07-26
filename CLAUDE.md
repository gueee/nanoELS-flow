# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

nanoELS-flow is a complete rewrite of the nanoELS Electronic Lead Screw controller for metal lathes. This project focuses on improved workflow and usability while maintaining hardware compatibility with the original nanoELS designs.

### Hardware Targets
- **H2**: Arduino Nano-based ELS controller
- **H4**: ESP32-S3-based 4-axis CNC controller  
- **H5**: ESP32-S3-based 3-axis controller with touch screen (CURRENT TARGET)

### Project Status
🚧 **Active Development** - Modular architecture with h5.ino-inspired precision algorithms. Arduino IDE-only development environment implementing professional threading precision with clean, maintainable code structure.

## Development Environment Setup

### Arduino IDE Setup (Primary Development Environment)
- **Arduino IDE Version**: 2.3.6 or later
- **Board**: ESP32S3 Dev Module
- **Framework**: Arduino framework for ESP32
- **Upload Port**: /dev/ttyUSB0 (select appropriate port in IDE)

### ESP32 Board Manager Configuration
Add to Arduino IDE Board Manager URLs:
```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

### Board Settings (ESP32S3 Dev Module)
- **CPU Frequency**: 240MHz
- **Flash Mode**: QIO  
- **Flash Size**: 16MB
- **Partition Scheme**: Huge APP (3MB No OTA/1MB SPIFFS)
- **PSRAM**: Enabled
- **Upload Speed**: 921600
- **USB Mode**: Hardware CDC and JTAG

### Required Libraries
Install via Arduino IDE Library Manager:
- **WebSockets by Markus Sattler**: For WebSocket communication
- **PS2KeyAdvanced by Paul Carpenter**: For PS2 keyboard interface

## Project Structure

**Arduino IDE Only** - PlatformIO support has been completely removed for simplicity:

```
nanoELS-flow/                          # Arduino IDE project folder
├── nanoELS-flow.ino                  # Main application file
├── SetupConstants.cpp/.h             # Hardware configuration + pin definitions
├── MinimalMotionControl.h/.cpp       # h5.ino-inspired minimal motion control
├── OperationManager.h/.cpp           # Operation state management and workflows
├── NextionDisplay.h/.cpp             # Touch screen interface  
├── WebInterface.h/.cpp               # HTTP server and WebSocket
├── StateMachine.h/.cpp               # Non-blocking state machine
├── CircularBuffer.h                  # Real-time circular buffer
├── indexhtml.h                       # Embedded web interface
├── MyHardware.txt                   # Pin reference documentation
└── schematics.png                   # Hardware reference schematic
```

## Code Architecture

### Core System Overview
The application follows a modular architecture inspired by h5.ino proven algorithms:

1. **MinimalMotionControl** (`MinimalMotionControl.h/.cpp`): h5.ino-inspired motion controller with professional precision
2. **OperationManager** (`OperationManager.h/.cpp`): State-based operation management with h5.ino-compatible workflows
3. **WebInterface** (`WebInterface.h/.cpp`): HTTP server and WebSocket communication for remote control
4. **NextionDisplay** (`NextionDisplay.h/.cpp`): Touch screen interface for status display and control
5. **StateMachine** (`StateMachine.h/.cpp`): Non-blocking state machine for UI coordination

### Key Architectural Patterns
- **h5.ino Compatibility**: Uses proven algorithms and constants from original implementation
- **Modular Design**: Clean separation of concerns with well-defined interfaces
- **Hardware Abstraction**: Pin definitions centralized in `SetupConstants.h`
- **Real-time Safety**: Emergency stop integration throughout all systems
- **State-Based Operations**: Clear state management for complex operations

### Motion Control Architecture (CRITICAL)
**h5.ino-Inspired Implementation**: Based on proven h5.ino algorithms:

- **Encoder Tracking**: ESP32 PCNT for spindle encoder (600 PPR = 1200 steps quadrature)
- **Backlash Compensation**: 3-step deadband filter from h5.ino
- **Position Following**: Direct position calculations using h5.ino formulas
- **Speed Ramping**: Simple acceleration/deceleration like h5.ino
- **Emergency Stop**: Highest priority safety system

### Operation Management System
**State-Based Workflow**: Operations progress through defined states:

- **Setup States**: Direction setup, touch-off coordinates, target input
- **Execution States**: Running operations, pass management, parking
- **Safety Integration**: Emergency stop handling throughout all states

## Development Commands

### Build and Upload (Arduino IDE - Primary)
```bash
# Arduino IDE 2.3.6+
# Board: ESP32S3 Dev Module
# Upload Speed: 921600
# Flash Size: 16MB
# Partition: Huge APP (3MB No OTA/1MB SPIFFS)
# USB CDC On Boot: Enabled
# CPU Frequency: 240MHz (WiFi/BT)
# Flash Mode: QIO
# PSRAM: OPI PSRAM (if available)
```

### Build Testing with arduino-cli (Optional)
```bash
# Test compilation without opening IDE
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc nanoELS-flow/

# Upload if compilation succeeds
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3 nanoELS-flow/
```

### Hardware Configuration
Edit motion parameters in `SetupConstants.cpp`:
- Motor steps per revolution: `MOTOR_STEPS_X`, `MOTOR_STEPS_Z`
- Lead screw pitch: `SCREW_X_DU`, `SCREW_Z_DU` (in deci-microns)
- Manual move speeds: `SPEED_MANUAL_MOVE_X`, `SPEED_MANUAL_MOVE_Z`
- Acceleration rates: `ACCELERATION_X`, `ACCELERATION_Z`

Pin definitions are centralized in `SetupConstants.h`.

## Key API Patterns

### MinimalMotionControl API
```cpp
// Initialize motion control system
bool motionControl.initialize();

// Position control
void motionControl.setTargetPosition(int axis, long steps);
long motionControl.getPosition(int axis);

// Thread pitch control (h5.ino compatible)
void motionControl.setDupr(long dupr);
long motionControl.getDupr();

// Threading parameters
void motionControl.setStarts(int starts);
int motionControl.getStarts();

// Emergency stop
void motionControl.emergencyStop();
bool motionControl.isEmergencyStop();
```

### OperationManager API
```cpp
// Operation mode control
void operationManager.setMode(OperationMode mode);
OperationMode operationManager.getCurrentMode();

// State management
OperationState operationManager.getCurrentState();
String operationManager.getPromptText();

// Measurement system (h5.ino compatible)
void operationManager.setMeasure(int measure); // MEASURE_METRIC, MEASURE_INCH, MEASURE_TPI
int operationManager.getCurrentMeasure();

// Operation parameters
void operationManager.setInternalOperation(bool internal);
void operationManager.setLeftToRight(bool leftToRight);
bool operationManager.isInternalOperation();
bool operationManager.isLeftToRight();
```

### NextionDisplay API
```cpp
// Direct Nextion commands (h5.ino style)
Serial1.print("t3.txt=\"message\""); 
Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);

// Via NextionDisplay wrapper
nextionDisplay.showMessage("Status text");
nextionDisplay.updatePosition(float x, float z);
```

## Hardware Configuration - H5 Variant

### Pin Definitions - NEVER CHANGE
**CRITICAL RULE**: Pin definitions must remain constant throughout the entire project.

#### Fixed Pin Assignments (ESP32-S3-dev board)
**Spindle Encoder**:
- ENC_A: GPIO 13
- ENC_B: GPIO 14

**Z-Axis Stepper Motor**:
- Z_ENA: GPIO 41 (Enable)
- Z_DIR: GPIO 42 (Direction)
- Z_STEP: GPIO 35 (Step)

**Z-Axis MPG**:
- Z_PULSE_A: GPIO 18
- Z_PULSE_B: GPIO 8

**X-Axis Stepper Motor**:
- X_ENA: GPIO 16 (Enable)
- X_DIR: GPIO 15 (Direction)
- X_STEP: GPIO 7 (Step)

**X-Axis MPG**:
- X_PULSE_A: GPIO 47
- X_PULSE_B: GPIO 21

**Keyboard Interface**:
- KEY_DATA: GPIO 37
- KEY_CLOCK: GPIO 36

**Nextion Display**:
- NEXTION_TX: GPIO 43
- NEXTION_RX: GPIO 44

### Motion Configuration
- **Spindle Encoder**: 600 PPR (1200 steps with quadrature)
- **Z-axis Motor**: 4000 steps per revolution, 5mm lead screw = 800 steps/mm
- **X-axis Motor**: 4000 steps per revolution, 4mm lead screw = 1000 steps/mm

## TURN Mode Workflow (Based on h5.ino)

**CRITICAL**: The actual h5.ino TURN mode workflow is:

### F2 Key Press → MODE_TURN
1. **setupIndex 0**: Initial state
   - **Requirement**: Must set all stops first (Z left/right, X up/down)
   - **Display**: "TURN off" + stops status
   - **Action**: ENTER advances when stops are set

2. **setupIndex 1**: Pass count selection
   - **Display**: "3 passes?" (shows current `turnPasses`)
   - **Input**: Numpad entry or +/- keys to adjust
   - **Action**: ENTER advances to next step

3. **setupIndex 2**: Internal/External selection
   - **Display**: "External?" or "Internal?" (based on `auxForward`)
   - **Input**: Left/Right arrows toggle `auxForward`
   - **Action**: ENTER advances to final step

4. **setupIndex 3**: Final confirmation
   - **Display**: "Go Z<offset> X<offset>?" (calculated starting positions)
   - **Action**: ENTER starts operation (`isOn = true`)

### Operation Execution
Once started, `modeTurn(&z, &x)` executes with:
- `opIndex`: Current pass number
- `opSubIndex`: Sub-operation within pass
- Automatic positioning and cutting cycles

## Debugging and Development

### Critical Debugging Rules
- **Primary Interface**: Nextion display for all user feedback
- **Serial Monitor**: Available for debugging but not primary interface
- **Display Objects**: t0, t1, t2, t3 available for debug output on Nextion
- **Reference Implementation**: Check `original-h5.ino/h5.ino` for proven algorithms

### Emergency Stop System
- **ESC Key**: Immediate emergency stop activation
- **Integration**: Emergency stop checking throughout all motion systems
- **Recovery**: Clear emergency stop with second ESC press

## PROJECT RULES - MANDATORY FOR ALL DEVELOPMENT

### Primary Target Hardware
- **CURRENT TARGET**: H5 variant ONLY (ESP32-S3-dev board)
- **Board**: ESP32S3 Dev Module (Arduino IDE)
- **Framework**: Arduino framework for ESP32-S3

### Motion Control System Requirements
**CRITICAL**: The project uses h5.ino-inspired minimal motion control:
- **h5.ino Algorithms**: Proven backlash compensation and position tracking
- **Hardware PCNT**: ESP32 Pulse Counter for precise encoder tracking
- **Emergency Stop Priority**: Highest priority safety system
- **State-Based Operations**: Clear state progression for complex operations

### Development Constraints
- **ESP32-S3 Only**: Do not consider other microcontrollers
- **h5.ino Compatibility**: Maintain compatibility with proven h5.ino algorithms
- **Arduino IDE Only**: PlatformIO support completely removed
- **Fixed Pins**: Pin assignments are permanent once set
- **Nextion Primary**: Nextion display is primary interface

### Code Architecture Rules
1. **Modular Design**: Maintain clean separation between motion control, operations, and display
2. **h5.ino Compatibility**: Use proven algorithms and constants from original
3. **State Management**: Use clear state machines for complex operations
4. **Safety Integration**: Emergency stop must be checked throughout all systems
5. **Hardware Abstraction**: Keep hardware configuration centralized

## Reference Implementation

### Critical Reference Files
- **`original-h5.ino/h5.ino`**: Authoritative reference for proven algorithms and workflows
- **Usage**: Use for understanding working implementations, not hardware reference
- **Rule**: Don't blindly copy code, but use as reference for functionality verification

### Known Working Systems
- All axes are enabled and functional
- MPG (Manual Pulse Generator) support for both X and Z axes
- Emergency stop system with immediate response
- Threading precision with sub-micrometer accuracy

## Project Memories and Notes

### Critical System Constraints
- **Pulse Generation Rule**: Pulse generation must not be blocked or delayed at any time
- **Emergency Stop Rule**: Emergency stop must be recognized under any circumstance
- **h5.ino Compatibility**: Maintain compatibility with proven algorithms and workflows

### Development Environment
- **Arduino IDE Only**: Arduino IDE 2.3.6 is the only supported development environment
- **No PlatformIO**: PlatformIO support completely removed for simplicity
- **h5.ino Reference**: Always available at `original-h5.ino/h5.ino` for sanity checking

### Architecture Notes
- **Modular Rewrite**: Complete rewrite with modular architecture while maintaining h5.ino compatibility
- **State-Based Operations**: Operations use clear state machines instead of complex setup indices
- **Clean Interfaces**: Well-defined APIs between modules for maintainability