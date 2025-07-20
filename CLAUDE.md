# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

nanoELS-flow is a complete rewrite of the nanoELS Electronic Lead Screw controller for metal lathes. This project focuses on improved workflow and usability while maintaining hardware compatibility with the original nanoELS designs.

### Hardware Targets
- **H2**: Arduino Nano-based ELS controller
- **H4**: ESP32-S3-based 4-axis CNC controller  
- **H5**: ESP32-S3-based 3-axis controller with touch screen

### Project Status
🚧 **Active Development** - Core motion control implemented with native ESP32-S3 system, Nextion display, and PS2 keyboard interface. MPG (Manual Pulse Generator) control working with smooth velocity-based step scaling (0.01mm to 0.5mm per detent).

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

The project supports both Arduino IDE and PlatformIO development environments:

#### Arduino IDE Structure (Primary - `/nanoELS-flow/`)
```
nanoELS-flow/                      # Arduino IDE project folder
├── nanoELS-flow.ino              # Main application file
├── SetupConstants.cpp/.h         # User-editable hardware configuration
├── ESP32MotionControl.h/.cpp     # Native ESP32-S3 motion control
├── NextionDisplay.h/.cpp         # Touch screen interface  
├── WebInterface.h/.cpp           # HTTP server and WebSocket
├── StateMachine.h/.cpp           # Non-blocking state machine
├── CircularBuffer.h              # Real-time circular buffer
├── MyHardware.h/.txt            # Pin definitions (authoritative)
└── indexhtml.h                  # Embedded web interface
```

#### PlatformIO Structure (Alternative - `/src/`)
```
src/                              # PlatformIO source folder
├── nanoELS-flow.ino              # Main application (mirrored)
├── [same files as Arduino IDE]  # All source files mirrored
└── platformio.ini                # PlatformIO configuration
```

**Important**: Both structures contain identical code. Arduino IDE structure is primary and authoritative.

**Key Files:**
- **nanoELS-flow.ino**: Main application file with state machine coordination
- **SetupConstants.cpp**: User-editable hardware configuration (motor steps, encoders, limits)
- **SetupConstants.h**: Header declarations for Arduino IDE compatibility
- **MyHardware.txt**: Authoritative pin definitions and keyboard mappings

## Code Architecture

### Core System Overview
The application follows a modular architecture with three main subsystems:

1. **ESP32MotionControl** (`ESP32MotionControl.h/.cpp`): Native ESP32-S3 motion control system with task-based stepper control, interrupt-based encoder counting, and smooth MPG control
2. **WebInterface** (`WebInterface.h/.cpp`): HTTP server and WebSocket communication for remote control and monitoring
3. **NextionDisplay** (`NextionDisplay.h/.cpp`): Touch screen interface with state-based display system

### Key Architectural Patterns
- **Native ESP32-S3 Implementation**: No external stepper libraries - uses ESP32 hardware timers, GPIO, and FreeRTOS tasks
- **Task-Based Motion Control**: FreeRTOS task on Core 1 for real-time motion processing
- **Servo-Style Position Following**: Motion controller follows target position variables like a simple servo
- **Simple Target Position Updates**: Arrow keys and MPGs update global position variables, motion controller follows
- **Hardware Abstraction**: Pin definitions centralized in `MyHardware.txt` and `MyHardware.h`
- **Emergency Stop Integration**: Immediate stop capability throughout all motion systems

### Motion Control Architecture (CRITICAL)
**Major Change**: FastAccelStepper has been completely removed and replaced with native ESP32-S3 implementation:

- **FreeRTOS Task**: Motion control runs on Core 1 with 1ms update rate
- **Hardware Timer ISR**: 2kHz step generation using ESP32 hardware timers
- **Multi-Step ISR**: Can generate up to 50 steps per ISR cycle for high-speed movement
- **Target Position Following**: Motion controller follows `targetPositionX` and `targetPositionZ` variables
- **Emergency Stop**: Highest priority - checked on every step and task cycle

### Manual Control System
**Simple Architecture**: 
- **Global Variables**: `targetPositionX` and `targetPositionZ` hold target positions
- **Arrow Keys**: Simply update target position variables by step size amount
- **Motion Controller**: Continuously follows target positions using motion profiles
- **Update Detection**: Only sends new targets when position variables actually change

### Main Application Flow
The main application (`nanoELS-flow.ino`) coordinates all subsystems:
```cpp
void loop() {
  scheduler.update();  // Non-blocking state machine at ~100kHz
}

void taskMotionUpdate() {  // Runs at 200Hz
  esp32Motion.update();
  // Only update when target positions change
  if (targetPositionX != lastTargetX) esp32Motion.setTargetPosition(0, targetPositionX);
  if (targetPositionZ != lastTargetZ) esp32Motion.setTargetPosition(1, targetPositionZ);
}
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

### Alternative: PlatformIO (Available but not primary)
```bash
# Build with PlatformIO (project structure also supports this)
pio run

# Upload to device
pio run --target upload

# Serial monitor
pio device monitor
```

**Note**: Arduino IDE is the primary development environment. PlatformIO support exists but Arduino IDE is recommended for consistency with documentation.

### Hardware Configuration
Edit motion parameters in `SetupConstants.cpp`:
- Motor steps per revolution: `MOTOR_STEPS_X`, `MOTOR_STEPS_Z`
- Lead screw pitch: `SCREW_X_DU`, `SCREW_Z_DU` (in deci-microns)
- Velocity limits: `MAX_VELOCITY_X_USER`, `MAX_VELOCITY_Z_USER`
- Acceleration: `MAX_ACCELERATION_X_USER`, `MAX_ACCELERATION_Z_USER`

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
- Native ESP32-S3 motion control system (no external libraries)
- Target position following with motion profiles
- Task-based motion control with FreeRTOS (Core 1)
- Multi-step ISR for high-speed movement (up to 100,000 steps/sec)
- Nextion display with direct Serial1 communication
- PS2 keyboard interface with full key mapping
- Web interface with HTTP and WebSocket support

**🔧 Current Functionality**:
- **Manual Mode**: Arrow keys update target positions, motion controller follows
- **Emergency Stop**: Immediate response to ESC key
- **Motion Control**: Task-based stepper control with acceleration profiles
- **Display**: Real-time status and position updates

### Library Dependencies
Required libraries (install via Arduino IDE Library Manager):
- **WebSockets by Markus Sattler**: For WebSocket communication
- **PS2KeyAdvanced by Paul Carpenter**: For PS2 keyboard interface
- **Native ESP32-S3**: No external stepper libraries - uses ESP32 hardware directly

**IMPORTANT NOTE**: The ARDUINO_SETUP.md mentions FastAccelStepper but this has been completely removed from the current implementation. Do NOT install FastAccelStepper - the project now uses native ESP32-S3 hardware control.

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

**HARDWARE CONFIGURATION FILE**: `MyHardware.txt` contains all pin definitions and keyboard mappings.

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
**CRITICAL**: The project uses a native ESP32-S3 implementation:
- **NO FastAccelStepper**: Completely removed from the project
- **Task-Based Control**: FreeRTOS task on Core 1 for motion processing
- **Multi-Step ISR**: Hardware timer ISR can generate up to 50 steps per cycle
- **Target Position Following**: Simple servo-style architecture
- **Emergency Stop Priority**: Highest priority safety system

### Code Architecture Rules
1. **Native ESP32-S3 Only**: No external stepper libraries allowed
2. **Task-Based Motion**: All motion control via FreeRTOS tasks
3. **Target Position Variables**: Global `targetPositionX` and `targetPositionZ` 
4. **Simple Updates**: Arrow keys and MPGs only update position variables
5. **Emergency Stop Integration**: Must be checked throughout all motion systems

### Development Constraints
- **No Y-Axis**: Completely ignore Y-axis functionality
- **ESP32-S3 Only**: Do not consider other microcontrollers
- **No External Stepper Libraries**: Use native ESP32-S3 implementation only
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
- B_OFF (145): ESC - **IMMEDIATE EMERGENCY STOP**
- B_STEP (86): Tilda (~) - cycle step size (0.01/0.1/1.0/10.0mm)
- B_X_ENA (67): c - Enable/disable X axis
- B_Z_ENA (113): q - Enable/disable Z axis

**Critical Safety Note**: B_OFF (ESC key) toggles emergency stop - press once to activate, press again to clear.

### User Interface and Feedback
- **Primary Interface**: Nextion display provides all user feedback and status information
- **NO SERIAL MONITOR**: All debugging must go to Nextion display
- **Keyboard Controls**: PS2 keyboard for all manual control and emergency stop
- **Emergency Stop Flow**: ESC activates emergency stop → ESC again clears → system ready
- **Target Position Updates**: Arrow keys update global position variables
- **Step Size Control**: Tilda (~) key cycles through step sizes

## Project Memories and Notes

### Critical System Constraints
- **Pulse Generation Rule**: Pulse generation must not be blocked or delayed at any time, this is a high priority task!

### Emergency Stop Constraints
- **Emergency stop must be recognized under any circumstance!**

### Memory: Schematics Reference
- `@nanoELS-flow/schematics.png`: Reference schematic for project hardware configuration

### Project Memory
- **Primary Development**: Arduino IDE is the primary development environment
- **PlatformIO**: Available as alternative but Arduino IDE preferred for consistency
- **Serial Monitor**: Only for initial setup and debugging - primary interface is Nextion display
- **No FastAccelStepper**: Completely removed - native ESP32-S3 implementation only

### Project Memory
- **Reference Guideline**: Always refer to @original-h5.ino/h5.ino for sanity check, all of what's in there is known to work! Don't blindly copy anything though, just use it as a debug reference for functionality not for hardware reference!

### Project Memory
- All axis are enabled, no need to check that, that works!!!!
