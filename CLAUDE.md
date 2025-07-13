# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

nanoELS-flow is a complete rewrite of the nanoELS Electronic Lead Screw controller for metal lathes. This project focuses on improved workflow and usability while maintaining hardware compatibility with the original nanoELS designs.

### Hardware Targets
- **H2**: Arduino Nano-based ELS controller
- **H4**: ESP32-S3-based 4-axis CNC controller  
- **H5**: ESP32-S3-based 3-axis controller with touch screen

### Project Status
🚧 **Active Development** - Core architecture implemented with motion control, web interface, and display modules. Hardware abstraction layer and stepper control functional.

## Development Environment Setup

### Build System Commands
- **Build**: `pio run -e esp32-s3-devkitc-1`
- **Upload**: `pio run -e esp32-s3-devkitc-1 --target upload`
- **Clean**: `pio run --target clean`
- **Monitor**: `pio device monitor --baud 115200`
- **Check/Lint**: `pio check` (uses cppcheck)
- **Build and Upload**: `pio run -e esp32-s3-devkitc-1 --target upload --target monitor`
- **Dependency Update**: `pio pkg update`

### Development Setup
- **Platform**: PlatformIO with ESP32-S3 support
- **Target Board**: esp32-s3-devkitc-1
- **Framework**: Arduino framework for ESP32
- **Upload Port**: /dev/ttyUSB0 (modify in platformio.ini if needed)

### Current Project Structure
```
/
├── nanoELS-flow/           # Arduino IDE compatible folder
│   └── nanoELS-flow.ino   # Main application file
├── *.h / *.cpp            # Project modules (root level)
├── platformio.ini         # PlatformIO configuration
├── MyHardware.txt         # Pin definitions (authoritative)
└── MyHardware.h          # C++ header for pin definitions
```

## Code Architecture

### Core System Overview
The application follows a modular architecture with three main subsystems:

1. **MotionControl** (`MotionControl.h/.cpp`): Manages stepper motors via FastAccelStepper, handles encoder inputs, processes motion commands through a real-time queue system
2. **WebInterface** (`WebInterface.h/.cpp`): Provides HTTP server and WebSocket communication for remote control and monitoring
3. **NextionDisplay** (`NextionDisplay.h/.cpp`): Manages touch screen interface with state-based display system

### Key Architectural Patterns
- **Hardware Abstraction**: Pin definitions centralized in `MyHardware.txt` and `MyHardware.h`
- **Real-time Motion Queue**: Motion commands queued and executed with precise timing
- **Event-driven Updates**: Each subsystem has `update()` method called from main loop
- **Global Object Pattern**: Core subsystems instantiated as global objects for real-time access

### Main Application Flow
The main application (`nanoELS-flow.ino`) coordinates all subsystems:
```cpp
void loop() {
  motionControl.update();    // Process motion queue and encoder data
  webInterface.update();     // Handle HTTP/WebSocket requests  
  nextionDisplay.update();   // Update display and process touch events
  handleKeyboard();          // Process PS2 keyboard input
  processMovement();         // Handle movement logic and status updates
}
```

## WiFi Configuration

The system supports two WiFi modes (set `WIFI_MODE` in main .ino file):

### Mode 0: Access Point
- **SSID**: "nanoELS-flow"  
- **Password**: "nanoels123"
- **Web Interface**: http://192.168.4.1

### Mode 1: Home Network
- **SSID**: Configurable in `HOME_WIFI_SSID`
- **Password**: Configurable in `HOME_WIFI_PASSWORD`  
- **Fallback**: Automatically creates AP if connection fails
- **Web Interface**: IP shown in serial monitor

## Debugging and Testing

### Serial Monitor Output
- **Baud Rate**: 115200
- **Motion Status**: Printed every 5 seconds when moving
- **Diagnostics**: Use keyboard 'Win' key or call `motionControl.printDiagnostics()`

### Motion Control Testing
- **Test Function**: `testMotionControl()` in main file (commented out during startup)
- **Manual Control**: Use arrow keys for axis movement
- **Emergency Stop**: ESC key (keyboard) or touch interface

### Development Testing Flow
1. **Hardware Verification**: Uncomment `testMotionControl()` to verify motor connections
2. **Keyboard Testing**: Use defined key mappings from `MyHardware.txt`
3. **Web Interface**: Access via IP address shown in serial output
4. **Display Testing**: Touch screen interactions via NextionDisplay module

### Library Dependencies and Versions
Key libraries specified in `platformio.ini`:
- **FastAccelStepper**: `gin66/FastAccelStepper@^0.30.0` (mandatory for all motor control)
- **WebSockets**: For WebSocket communication
- **PS2KeyAdvanced**: For PS2 keyboard interface

### File Organization Rules
- **Main application**: `nanoELS-flow/nanoELS-flow.ino` (Arduino IDE compatible)
- **Hardware definitions**: `nanoELS-flow/MyHardware.txt` (authoritative pin mappings)
- **Core modules**: `nanoELS-flow/*.h` and `nanoELS-flow/*.cpp` (modular architecture)
- **Web assets**: `nanoELS-flow/indexhtml.h` (embedded web interface)

## PROJECT RULES - MANDATORY FOR ALL DEVELOPMENT

### Primary Target Hardware
- **CURRENT TARGET**: H5 variant ONLY (ESP32-S3-dev board)
- **Board**: ESP32-S3-dev board
- **Framework**: Arduino framework for ESP32-S3
- **PlatformIO Environment**: esp32-s3-devkitc-1

### Hardware Specifications - H5 Variant
- **MCU**: ESP32-S3 with dual-core CPU
- **Display**: Touch screen interface
- **Axes**: 3-axis controller (X, Z, Spindle) - Y axis NOT implemented
- **Interface**: Touch screen + physical controls

### Pin Definitions - NEVER CHANGE
**CRITICAL RULE**: Pin definitions must remain constant throughout the entire project. Any pin assignment made initially is PERMANENT and cannot be modified.

**HARDWARE CONFIGURATION FILE**: `MyHardware.txt` contains all pin definitions and keyboard mappings.

#### Fixed Pin Assignments (ESP32-S3-dev board)
**Spindle Encoder** (PCNT_UNIT_0):
- ENC_A: GPIO 13
- ENC_B: GPIO 14

**Z-Axis Stepper Motor**:
- Z_ENA: GPIO 41 (Enable)
- Z_DIR: GPIO 42 (Direction)
- Z_STEP: GPIO 35 (Step)

**Z-Axis MPG** (PCNT_UNIT_1):
- Z_PULSE_A: GPIO 18
- Z_PULSE_B: GPIO 8

**X-Axis Stepper Motor**:
- X_ENA: GPIO 16 (Enable)
- X_DIR: GPIO 15 (Direction)
- X_STEP: GPIO 7 (Step)

**X-Axis MPG** (PCNT_UNIT_2):
- X_PULSE_A: GPIO 47
- X_PULSE_B: GPIO 21

**Keyboard Interface**:
- KEY_DATA: GPIO 37
- KEY_CLOCK: GPIO 36

### Encoder Hardware Assignments (MANDATORY)
- **PCNT_UNIT_0**: Spindle encoder (dedicated)
- **PCNT_UNIT_1**: Z-axis MPG (Manual Pulse Generator)
- **PCNT_UNIT_2**: X-axis MPG (Manual Pulse Generator)
- **Y-axis**: NOT IMPLEMENTED - ignore completely

### Required Libraries & Dependencies
- **Motor Control**: https://github.com/gin66/FastAccelStepper
  - Use ONLY FastAccelStepper library for all stepper motor control
  - Use for servo control as well
  - Must be compatible with ESP32-S3 hardware

### ESP32-S3 Hardware Utilization
- **PCNT Units**: Hardware pulse counters for encoders/MPGs
  - Refer to ESP32-S3 datasheet for PCNT capabilities
  - Use hardware features for accurate counting
  - No software polling for encoder counts
- **Timer Units**: Use ESP32 hardware timers for precision timing
- **GPIO**: Utilize ESP32-S3 GPIO capabilities efficiently

### Development Configuration
- **Keyboard Layout**: Use developer's personal keyboard configuration during development
  - Keyboard key definitions can vary per user
  - Current development uses developer's personal layout (defined in MyHardware.txt)
  - Document keyboard mappings for user customization later

#### Personal Keyboard Mapping (Development Configuration)
**Movement Controls**:
- B_LEFT (68): Left arrow - Z axis left movement
- B_RIGHT (75): Right arrow - Z axis right movement  
- B_UP (85): Up arrow - X axis forward movement
- B_DOWN (72): Down arrow - X axis backward movement

**Operation Controls**:
- B_ON (50): Enter - starts operation/mode
- B_OFF (145): ESC - stops operation/mode
- B_PLUS (87): Numpad plus - increment pitch/passes
- B_MINUS (73): Numpad minus - decrement pitch/passes

**Stop Settings**:
- B_STOPL (83): 'a' key - sets left stop
- B_STOPR (91): 'd' key - sets right stop
- B_STOPU (78): 'w' key - sets forward stop
- B_STOPD (89): 's' key - sets rear stop

**Mode Selection** (F-keys):
- B_MODE_GEARS (31): F1 - gearbox mode
- B_MODE_TURN (24): F2 - turning mode
- B_MODE_FACE (147): F3 - facing mode
- B_MODE_CONE (17): F4 - cone mode
- B_MODE_CUT (92): F5 - cutting mode
- B_MODE_THREAD (18): F6 - threading mode
- B_MODE_ASYNC (101): F7 - async mode
- B_MODE_ELLIPSE (10): F8 - ellipse mode
- B_MODE_GCODE (28): F9 - G-code mode

**Axis Controls**:
- B_X (139): 'x' key - zero X axis
- B_Z (99): 'z' key - zero Z axis
- B_X_ENA (93): 'c' key - enable/disable X axis
- B_Z_ENA (74): 'q' key - enable/disable Z axis

**Number Entry** (Top row 0-9):
- B_0 through B_9: Number input for parameters
- B_BACKSPACE (29): Remove last entered number

**Additional Functions**:
- B_MEASURE (66): 'm' key - metric/imperial/TPI toggle
- B_REVERSE (148): 'r' key - thread direction (left/right)
- B_DIAMETER (22): 'o' key - set X0 to diameter centerline
- B_STEP (94): Tilda - change movement step size
- B_DISPL (102): Win key - change bottom line display info

### Code Architecture Rules
1. **Hardware Abstraction**: All pin definitions in centralized header files
2. **PCNT Integration**: Direct hardware PCNT usage, no software alternatives
3. **FastAccelStepper Integration**: All motor control through this library only
4. **ESP32-S3 Specific**: Code optimized for ESP32-S3 capabilities
5. **Touch Interface**: Primary UI through touch screen on H5

### Build System Requirements
- **Platform**: PlatformIO with ESP32-S3 support
- **Target**: esp32-s3-devkitc-1 board definition
- **Framework**: Arduino framework for ESP32
- **Libraries**: FastAccelStepper as primary dependency

### Development Constraints
- **No Y-Axis**: Completely ignore Y-axis functionality
- **ESP32-S3 Only**: Do not consider other microcontrollers
- **Hardware PCNT**: Must use ESP32 hardware pulse counters
- **Fixed Pins**: Pin assignments are permanent once set
- **Touch Primary**: Touch screen is primary interface

### Testing Requirements
- **Hardware-Specific**: All testing on ESP32-S3-dev board
- **PCNT Validation**: Test hardware pulse counting accuracy
- **FastAccelStepper**: Validate motor control library integration
- **Touch Interface**: Test touch screen responsiveness

### Documentation Standards
- **Pin Mappings**: Maintain permanent pin assignment documentation (see MyHardware.txt)
- **PCNT Usage**: Document hardware counter configurations
- **Keyboard Config**: Document current keyboard layout for development (defined in MyHardware.txt)
- **Library Usage**: Document FastAccelStepper implementation patterns
- **Hardware File**: MyHardware.txt is the authoritative source for all pin definitions and key mappings

### Reference Documentation
- **ESP32-S3 Datasheet**: Primary reference for hardware capabilities
- **PCNT Documentation**: ESP32-S3 pulse counter specifications
- **FastAccelStepper**: Library documentation and examples

**CRITICAL REMINDERS**:
1. Target is ESP32-S3-dev board (H5 variant) ONLY
2. Pin definitions are PERMANENT once assigned
3. Use PCNT hardware units as specified above
4. FastAccelStepper library is MANDATORY for motor control
5. Y-axis is NOT implemented - ignore completely
6. Always refer to ESP32-S3 datasheet for hardware capabilities

## License

This project is licensed under GPL-3.0, maintaining compatibility with the original nanoELS project while allowing for the complete rewrite approach.