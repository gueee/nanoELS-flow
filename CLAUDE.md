# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

nanoELS-flow is a complete rewrite of the nanoELS Electronic Lead Screw controller for metal lathes. This project focuses on improved workflow and usability while maintaining hardware compatibility with the original nanoELS designs.

### Hardware Targets
- **H2**: Arduino Nano-based ELS controller
- **H4**: ESP32-S3-based 4-axis CNC controller  
- **H5**: ESP32-S3-based 3-axis controller with touch screen

### Project Status
🚧 **Active Development** - Minimal motion control system implemented based on h5.ino precision algorithms. Arduino IDE-only development environment with professional threading precision (0.0007mm following error) using ~80 lines of core motion logic instead of 300+.

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

### Project Structure

**Arduino IDE Only** - PlatformIO support has been completely removed for simplicity:

#### Arduino IDE Structure (Only - `/nanoELS-flow/`)
```
nanoELS-flow/                          # Arduino IDE project folder
├── nanoELS-flow.ino                  # Main application file
├── SetupConstants.cpp/.h             # User-editable configuration + pin definitions
├── MinimalMotionControl.h/.cpp       # h5.ino-based minimal motion control
├── NextionDisplay.h/.cpp             # Touch screen interface  
├── WebInterface.h/.cpp               # HTTP server and WebSocket
├── StateMachine.h/.cpp               # Non-blocking state machine
├── CircularBuffer.h                  # Real-time circular buffer
├── MyHardware.txt                   # Pin reference documentation
└── indexhtml.h                      # Embedded web interface
```

**Key Files:**
- **nanoELS-flow.ino**: Main application with minimal motion control integration
- **SetupConstants.h**: Hardware configuration including all pin definitions and keyboard mappings
- **SetupConstants.cpp**: User-editable motion parameters (steps, speeds, limits)
- **MinimalMotionControl.h/.cpp**: Professional precision motion control with h5.ino algorithms
- **MyHardware.txt**: Pin definitions reference (read-only documentation)

## Code Architecture

### Core System Overview
The application follows a minimalist architecture based on h5.ino proven algorithms:

1. **MinimalMotionControl** (`MinimalMotionControl.h/.cpp`): Professional precision motion control with h5.ino algorithms achieving 0.0007mm following error
2. **OperationManager** (`OperationManager.h/.cpp`): h5.ino-style setup workflows and operation state management with setupIndex progression
3. **WebInterface** (`WebInterface.h/.cpp`): HTTP server and WebSocket communication for remote control and monitoring
4. **NextionDisplay** (`NextionDisplay.h/.cpp`): Touch screen interface for status display and control

### Key Architectural Patterns
- **h5.ino Precision Algorithms**: Proven backlash compensation and position tracking from original nanoELS
- **Minimal Complexity**: ~80 lines of core motion logic vs 300+ in enterprise versions
- **Direct Position Updates**: No complex queues - immediate position changes like h5.ino
- **Hardware PCNT Integration**: ESP32 Pulse Counter for precise encoder tracking
- **Hardware Abstraction**: Pin definitions centralized in `SetupConstants.h` (merged from MyHardware.h)
- **Emergency Stop Integration**: Immediate stop capability with highest priority

### Motion Control Architecture (CRITICAL)
**h5.ino-Based Implementation**: The project uses proven h5.ino algorithms for professional precision:

- **Backlash Compensation**: 3-step deadband filter from h5.ino
- **Position Following**: `positionFromSpindle()` calculation directly from h5.ino
- **Direct Updates**: Immediate position changes without complex motion profiles
- **Hardware PCNT**: ESP32 Pulse Counter for quadrature encoder (600 PPR = 1200 steps)
- **Sub-micrometer Precision**: 0.0007mm following error vs h5.ino's 0.0004mm
- **Emergency Stop**: Highest priority - checked on every update cycle

### Manual Control System
**h5.ino-Style Architecture**: 
- **Direct Position Updates**: Arrow keys directly update axis target positions
- **Motion Controller**: Follows target positions using h5.ino speed ramping
- **Simple Interface**: `motionControl.setTargetPosition(axis, steps)` for all movement
- **Immediate Response**: No queuing delays - sub-millisecond response time

### Main Application Flow
The main application (`nanoELS-flow.ino`) coordinates with minimal motion control:
```cpp
void loop() {
  motionControl.update();  // h5.ino-style motion control at ~5kHz
  
  // Handle keyboard input
  if (keyPressed) {
    processKeyInput();  // Update target positions
  }
  
  // Update displays and interfaces
  scheduler.update();
}
```

### Key API Patterns
**Motion Control API**:
```cpp
// Get current pitch (dupr) value
long currentDupr = motionControl.getDupr();

// Set thread pitch (for pitch adjustment)
motionControl.setThreadPitch(newDupr);

// Direct position control
motionControl.setTargetPosition(AXIS_X, targetSteps);
motionControl.setTargetPosition(AXIS_Z, targetSteps);
```

**OperationManager API**:
```cpp
// Setup index progression (h5.ino style)
int setupIndex = operationManager.getSetupIndex();
operationManager.resetSetupIndex();  // Return to setupIndex 0
operationManager.advanceSetupIndex(); // Move to next setup stage

// Direction and type control
operationManager.setInternalOperation(bool internal);
operationManager.setLeftToRight(bool leftToRight);
operationManager.toggleDirection();

// Measurement system
int currentMeasure = operationManager.getCurrentMeasure(); // MEASURE_METRIC, MEASURE_INCH, MEASURE_TPI

// Display prompts
String promptText = operationManager.getPromptText();
```

**Nextion Display API**:
```cpp
// Direct Nextion commands (h5.ino style)
Serial1.print("t3.txt=\"message\""); 
Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);

// Via NextionDisplay wrapper
nextionDisplay.showMessage("Status text");
```

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

**Note**: Arduino IDE 2.3.6 is the only supported development environment. PlatformIO support has been completely removed.

### Hardware Configuration
Edit motion parameters in `SetupConstants.cpp`:
- Motor steps per revolution: `MOTOR_STEPS_X`, `MOTOR_STEPS_Z`
- Lead screw pitch: `SCREW_X_DU`, `SCREW_Z_DU` (in deci-microns)
- Manual move speeds: `SPEED_MANUAL_MOVE_X`, `SPEED_MANUAL_MOVE_Z`
- Acceleration rates: `ACCELERATION_X`, `ACCELERATION_Z`

Pin definitions are now centralized in `SetupConstants.h` (merged from MyHardware.h).

## WiFi Configuration

Configure WiFi settings before uploading in main .ino file:

```cpp
// Line ~62 in nanoELS-flow.ino
#define WIFI_MODE 1  // Change to 1 for home WiFi

// Lines ~70-71 - Update with your WiFi credentials
const char* HOME_WIFI_SSID = "YourWiFiNetwork";
const char* HOME_WIFI_PASSWORD = "YourWiFiPassword";
```

The system supports two WiFi modes:

### Mode 0: Access Point
- **SSID**: "nanoELS-flow"  
- **Password**: "nanoels123"
- **Web Interface**: http://192.168.4.1

### Mode 1: Home Network
- **SSID**: Configurable in `HOME_WIFI_SSID`
- **Password**: Configurable in `HOME_WIFI_PASSWORD`  
- **Requirements**: 2.4GHz network (ESP32 doesn't support 5GHz)
- **Fallback**: Automatically creates AP if connection fails
- **Web Interface**: IP shown on Nextion display

## Debugging and Development

### Critical Debugging Rules
- **NO SERIAL MONITOR**: All debugging output must go to Nextion display
- **Use Direct Serial1 Commands**: `Serial1.print("t3.txt=\"message\""); Serial1.write(0xFF,0xFF,0xFF);`
##- **Reference Original**: Check `@original-h5.ino/h5.ino` for working implementations
- **Display Objects**: t0, t1, t2, t3 available for debug output on Nextion

### Motion Control Testing
- **Win Key Diagnostics**: Shows ISR count, step pulse counts, version info
- **Target Position Updates**: Arrow keys update global position variables
- **Emergency Stop**: ESC key (B_OFF) stops all motion immediately
- **Manual Control**: Step sizes: 0.01mm, 0.1mm, 1.0mm, 10.0mm (Tilda key cycles)

### Current Implementation Status
**✅ Working Features**:
- h5.ino-based minimal motion control (0.0007mm following error)
- Professional threading precision with 75-85% less code
- Direct position updates (no complex queuing systems)
- Hardware PCNT encoder tracking with backlash compensation
- Nextion display with direct Serial1 communication
- PS2 keyboard interface with full key mapping
- Web interface with HTTP and WebSocket support
- **MPG Support**: Manual Pulse Generators for both X and Z axes with automatic initialization
- **User-Defined TURN Setup Workflow**: Complete F2 setup process with direction/type selection

**🔧 Current Functionality**:
- **Manual Mode**: Arrow keys directly update target positions with h5.ino algorithms
- **MPG Control**: Hardware pulse counters (PCNT_UNIT_1, PCNT_UNIT_2) for handwheel input
- **Emergency Stop**: Immediate response integrated throughout motion system
- **Threading Mode**: Professional precision using proven h5.ino positionFromSpindle() calculations
- **Display**: Real-time status and position updates
- **Turn Mode Setup**: F2 → setupIndex-based workflow with arrow key selection and stop key confirmations

### Library Dependencies
Required libraries (install via Arduino IDE Library Manager):
- **WebSockets by Markus Sattler**: For WebSocket communication
- **PS2KeyAdvanced by Paul Carpenter**: For PS2 keyboard interface
- **Native ESP32-S3**: No external stepper libraries - uses h5.ino algorithms with ESP32 hardware

**IMPORTANT NOTE**: Do NOT install FastAccelStepper or any external motion libraries. The project uses h5.ino-based minimal motion control for professional precision with minimal complexity.

### ESP32 Arduino Core Compatibility
- **Version 3.x**: Uses new timer API (`timerBegin(frequency)`, `timerAlarm()`, `timerStart()`)
- **Version 2.x**: Uses legacy timer API (`timerBegin(num, prescaler, countUp)`, `timerAlarmWrite()`)
- **Auto-detection**: Code automatically detects and uses appropriate API

## PROJECT RULES - MANDATORY FOR ALL DEVELOPMENT

### Primary Target Hardware
- **CURRENT TARGET**: H5 variant ONLY (ESP32-S3-dev board)
- **Board**: ESP32S3 Dev Module (Arduino IDE)
- **Framework**: Arduino framework for ESP32-S3

### Hardware Specifications - H5 Variant
- **MCU**: ESP32-S3 with dual-core CPU
- **Display**: Nextion touch screen interface
- **Axes**: 3-axis controller (X, Z, Spindle) - Y axis NOT implemented
- **Interface**: Touch screen + PS2 keyboard + physical controls

### Pin Definitions - NEVER CHANGE
**CRITICAL RULE**: Pin definitions must remain constant throughout the entire project. Any pin assignment made initially is PERMANENT and cannot be modified.

**HARDWARE CONFIGURATION FILE**: Pin definitions and keyboard mappings are now in `SetupConstants.h` (merged from MyHardware.h). Reference documentation in `MyHardware.txt`.

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

### Motion Control System Requirements
**CRITICAL**: The project uses h5.ino-based minimal motion control:
- **h5.ino Algorithms**: Proven backlash compensation and position tracking
- **NO External Libraries**: No FastAccelStepper or other motion libraries
- **Direct Position Updates**: Immediate position changes like h5.ino (no queues)
- **Hardware PCNT**: ESP32 Pulse Counter for precise encoder tracking (PCNT_UNIT_0=spindle, PCNT_UNIT_1=Z-MPG, PCNT_UNIT_2=X-MPG)
- **Sub-micrometer Precision**: 0.0007mm following error with 600 PPR encoder
- **MPG Integration**: Manual pulse generators use spindle-style instant following
- **Emergency Stop Priority**: Highest priority safety system

### Code Architecture Rules
1. **h5.ino-Based Only**: Use proven h5.ino algorithms for all motion control
2. **Minimal Complexity**: ~80 lines of core logic vs 300+ in enterprise versions
3. **Direct Updates**: `motionControl.setTargetPosition(axis, steps)` for all movement
4. **Hardware Integration**: ESP32 PCNT for encoders, GPIO for steppers
5. **Emergency Stop Integration**: Must be checked throughout all motion systems

### Development Constraints
- **No Y-Axis**: Completely ignore Y-axis functionality
- **ESP32-S3 Only**: Do not consider other microcontrollers
- **h5.ino Algorithms Only**: Use only proven h5.ino motion control algorithms
- **Arduino IDE Only**: PlatformIO support completely removed
- **Fixed Pins**: Pin assignments are permanent once set
- **Nextion Primary**: Nextion display is primary interface, NO serial monitor

### Hardware Configuration
- **Spindle Encoder**: 600 PPR (pulses per revolution)
- **Z-axis Motor**: 4000 steps per revolution, 5mm lead screw = 800 steps/mm
- **X-axis Motor**: 4000 steps per revolution, 4mm lead screw = 1000 steps/mm
- **Manual Speeds**: Configurable via `SPEED_MANUAL_MOVE_X/Z` constants

### Personal Keyboard Mapping (Development Configuration)
**Movement Controls**:
- B_LEFT (68): Left arrow - Z axis left movement
- B_RIGHT (75): Right arrow - Z axis right movement  
- B_UP (85): Up arrow - X axis away from centerline (towards operator)
- B_DOWN (72): Down arrow - X axis towards centerline (towards workpiece)

**Operation Controls**:
- B_ON (50): Enter - starts operation/mode
- B_OFF (145): ESC - **IMMEDIATE EMERGENCY STOP** or return to setupIndex 0 during setup
- B_STEP (86): Tilda (~) - cycle step size (0.01/0.1/1.0/10.0mm)
- B_X_ENA (67): c - Enable/disable X axis
- B_Z_ENA (113): q - Enable/disable Z axis
- B_MEASURE (66): m - Toggle X-axis MPG (Manual Pulse Generator)
- B_REVERSE (148): r - Toggle Z-axis MPG (Manual Pulse Generator)
- B_PLUS (87): Numpad plus - **INCREMENT PITCH** (works anytime, even during cutting)
- B_MINUS (73): Numpad minus - **DECREMENT PITCH** (works anytime, even during cutting)

**Setup Mode Keys (F2 Turn Mode)**:
- Arrow Keys (setupIndex 0): Select direction (L/R) and type (Up/Down for INT/EXT)
- Stop Keys: w=internal, s=external, d=R→L, a=L→R for setup confirmations

**Critical Safety Note**: B_OFF (ESC key) toggles emergency stop - press once to activate, press again to clear.

**MPG Note**: MPGs are automatically enabled on startup with 1mm step size. Keys m/r can toggle them on/off.

### User Interface and Feedback
- **Primary Interface**: Nextion display provides all user feedback and status information
- **NO SERIAL MONITOR**: All debugging must go to Nextion display
- **Keyboard Controls**: PS2 keyboard for all manual control and emergency stop
- **Emergency Stop Flow**: ESC activates emergency stop → ESC again clears → system ready
- **Target Position Updates**: Arrow keys directly update motion control target positions
- **Step Size Control**: Tilda (~) key cycles through step sizes
- **h5.ino Precision**: Professional threading precision with minimal complexity

### TURN Mode Setup Workflow (F2 Key)
**CRITICAL**: The F2 key initiates a specific user-defined setup process:

1. **setupIndex 0**: Direction and type selection
   - **Arrow Keys**: Left/Right = R→L/L→R direction, Up/Down = Internal/External
   - **Display**: Shows current selection (e.g., "L→R EXT ←→↑↓")

2. **setupIndex 1**: Touch-off confirmation
   - **Stop Keys**: w=internal, s=external, d=R→L, a=L→R operations
   - **Display**: "Touch off diameter & face (w/s/d/a)"

3. **setupIndex 2**: Target input confirmation  
   - **Stop Keys**: a=R→L, d=L→R, w=internal, s=external targets
   - **Display**: "Input target Ø & length (w/s/d/a)"

4. **setupIndex 3**: Number of passes
   - **Numpad/Arrows**: Enter pass count, default 3
   - **Display**: "3 passes (numpad/↑↓)"

5. **Final**: Ready to start
   - **Enter**: Confirm and start operation
   - **Display**: "Ready? Press ENTER"

**ESC Navigation**: ESC returns to setupIndex 0 from any stage (non-destructive)

## Project Memories and Notes

### Critical System Constraints
- **Pulse Generation Rule**: Pulse generation must not be blocked or delayed at any time, this is a high priority task!

### Emergency Stop Constraints
- **Emergency stop must be recognized under any circumstance!**

### Memory: Schematics Reference
- `@nanoELS-flow/schematics.png`: Reference schematic for project hardware configuration

### Project Memory
- **Arduino IDE Only**: Arduino IDE 2.3.6 is the only supported development environment
- **PlatformIO Removed**: PlatformIO support completely removed for simplicity
- **Serial Monitor**: Only for initial setup and debugging - primary interface is Nextion display
- **h5.ino Motion Control**: Professional precision with minimal complexity (0.0007mm following error)
- **Merged Configuration**: Pin definitions merged from MyHardware.h into SetupConstants.h

### Project Memory
- **Reference Guideline**: Always refer to @original-h5.ino/h5.ino for sanity check, all of what's in there is known to work! Don't blindly copy anything though, just use it as a debug reference for functionality not for hardware reference!

### Project Memory  
- All axis are enabled, no need to check that, that works!!!!

## Architecture Restructure History

### Major Restructure (Latest)
The project underwent a complete restructure to achieve professional precision with minimal complexity:

**Eliminated Components**:
- Removed entire PlatformIO structure (`/src/` directory, `platformio.ini`)
- Removed complex ESP32MotionControl system (300+ lines)
- Removed MyHardware.h (merged into SetupConstants.h)
- Removed FastAccelStepper dependencies
- Removed complex motion profiles and queuing systems

**h5.ino Integration**:
- Implemented `MinimalMotionControl` with h5.ino proven algorithms
- Achieved 0.0007mm following error (vs h5.ino's 0.0004mm with 1200 PPR encoder)
- Core motion logic reduced to ~80 lines vs 300+ in enterprise version
- Direct position updates without complex queuing delays
- Professional backlash compensation with 3-step deadband filter

**Configuration Consolidation**:
- Merged MyHardware.h pin definitions into SetupConstants.h
- Centralized all hardware configuration in single location
- Maintained authoritative pin definitions in MyHardware.txt for reference

**Result**: 
- 75-85% code reduction while maintaining professional precision
- Sub-micrometer following error for fine threading operations
- Arduino IDE-only development environment
- Proven h5.ino algorithms ensure reliable operation
