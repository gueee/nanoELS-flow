# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

nanoELS-flow is a complete rewrite of the nanoELS Electronic Lead Screw controller for metal lathes. This project focuses on improved workflow and usability while maintaining hardware compatibility with the original nanoELS designs.

### Hardware Targets
- **H2**: Arduino Nano-based ELS controller
- **H4**: ESP32-S3-based 4-axis CNC controller  
- **H5**: ESP32-S3-based 3-axis controller with touch screen

### Project Status
🚧 **Early Development Phase** - The repository currently contains only documentation. The development environment, build system, and source code structure have not yet been established.

## Development Environment Setup

Since this project targets embedded hardware, the development setup will likely require:

### For ESP32-S3 Targets (H4, H5)
- **PlatformIO**: Recommended for ESP32-S3 development
- **Configuration File**: `platformio.ini` (to be created)
- **Framework**: Arduino or ESP-IDF
- **Build Command**: `pio run`
- **Upload Command**: `pio run --target upload`

### For Arduino Nano Target (H2)
- **Arduino IDE** or **PlatformIO**
- **Board**: Arduino Nano (ATmega328P)
- **Build**: Arduino IDE compile or PlatformIO

### Expected Project Structure
```
/
├── src/                    # Source code
├── include/               # Header files
├── lib/                   # Project libraries
├── test/                  # Unit tests
├── docs/                  # Documentation
├── hardware/              # Hardware designs/schematics
├── platformio.ini         # PlatformIO configuration
└── README.md             # Project documentation
```

## Architecture Goals

Based on the project documentation, the architecture should focus on:

### Core Principles
- **Hardware Compatibility**: Maintain compatibility with original nanoELS hardware
- **Modular Design**: Clean, maintainable codebase with modular architecture
- **Multi-Target Support**: Single codebase supporting H2, H4, and H5 variants
- **Improved Workflow**: Enhanced user interface and operation flow

### Key Components (To Be Implemented)
- **Hardware Abstraction Layer**: Support multiple microcontrollers
- **UI Framework**: Touch screen support for H5, button interface for others
- **Threading Operations**: Lead screw and threading functionality
- **Configuration Management**: Settings and parameter storage
- **Safety Systems**: Limits and error handling
- **Motor Control**: Stepper motor control for lead screw operation

## Development Workflow

### Initial Setup Tasks
1. Create PlatformIO configuration for multi-target builds
2. Establish source code structure
3. Set up testing framework
4. Define hardware abstraction interfaces
5. Implement build system for all hardware variants

### Build Commands (To Be Established)
Commands will depend on the chosen build system:
- **PlatformIO**: `pio run -e <environment>`
- **Testing**: `pio test`
- **Upload**: `pio run -e <environment> --target upload`

### Hardware-Specific Builds
- **H2 (Arduino Nano)**: Target Arduino framework
- **H4 (ESP32-S3 4-axis)**: Target ESP32-S3 with extended GPIO
- **H5 (ESP32-S3 Touch)**: Target ESP32-S3 with display libraries

## Code Organization

### Hardware Abstraction
- Separate hardware-specific code from business logic
- Use conditional compilation for hardware variants
- Common interfaces for motor control, input handling, and display

### Key Modules (Planned)
- **Motor Control**: Stepper motor drivers and motion control
- **User Interface**: Display management and input handling  
- **Threading Engine**: Lead screw calculations and operations
- **Configuration**: Settings management and persistence
- **Safety**: Limit checking and emergency stops
- **Communication**: Serial/USB communication for setup

## Testing Strategy

### Unit Testing
- Hardware abstraction layer testing
- Mathematical calculations (threading, feed rates)
- Configuration management
- Safety system validation

### Hardware-in-Loop Testing
- Motor control validation
- User interface testing on actual hardware
- Timing and real-time performance validation

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