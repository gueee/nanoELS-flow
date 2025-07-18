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

### Build System Commands
**Note**: PlatformIO is located at `~/.platformio/penv/bin/pio`

- **Build**: `~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1`
- **Upload**: `~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 --target upload`
- **Clean**: `~/.platformio/penv/bin/pio run --target clean`
- **Monitor**: `~/.platformio/penv/bin/pio device monitor --baud 115200`
- **Check/Lint**: `~/.platformio/penv/bin/pio check` (uses cppcheck)
- **Build and Upload**: `~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 --target upload --target monitor`

### Quick Development Commands
- **Full cycle**: `~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 --target upload --target monitor`
- **List devices**: `~/.platformio/penv/bin/pio device list`
- **Verbose build**: `~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 -v`
- **Force rebuild**: `~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 --target clean --target upload`

### Development Setup
- **Platform**: PlatformIO with ESP32-S3 support
- **Target Board**: esp32-s3-devkitc-1
- **Framework**: Arduino framework for ESP32
- **Arduino IDE Version**: 2.3.6
- **Upload Port**: /dev/ttyUSB0 (modify in platformio.ini if needed)

### Project Structure
```
/
├── nanoELS-flow/           # Arduino IDE compatible folder
│   ├── nanoELS-flow.ino   # Main application file
│   ├── ESP32MotionControl.h/.cpp  # Native ESP32-S3 motion control
│   ├── NextionDisplay.h/.cpp      # Touch screen interface
│   ├── WebInterface.h/.cpp        # HTTP server and WebSocket
│   ├── CircularBuffer.h           # Real-time circular buffer
│   ├── MyHardware.h/.txt          # Pin definitions (authoritative)
│   └── indexhtml.h               # Embedded web interface
├── src/                    # PlatformIO build folder (mirrors nanoELS-flow/)
├── platformio.ini          # PlatformIO configuration
└── README.md              # Project documentation
```

## Code Architecture

### Core System Overview
The application follows a modular architecture with three main subsystems:

1. **ESP32MotionControl** (`ESP32MotionControl.h/.cpp`): Native ESP32-S3 motion control system with task-based stepper control, interrupt-based encoder counting, and smooth MPG control
2. **WebInterface** (`WebInterface.h/.cpp`): HTTP server and WebSocket communication for remote control and monitoring
3. **NextionDisplay** (`NextionDisplay.h/.cpp`): Touch screen interface with state-based display system

### Key Architectural Patterns
- **Native ESP32-S3 Implementation**: No external stepper libraries - uses ESP32 hardware timers, GPIO, and FreeRTOS tasks
- **Task-Based Motion Control**: FreeRTOS task on Core 1 for real-time motion processing
- **Circular Buffer Queue**: Real-time safe motion command queue with 64-command capacity
- **Smooth MPG Control**: Velocity-based step scaling with acceleration/deceleration profiles
- **Hardware Abstraction**: Pin definitions centralized in `MyHardware.txt` and `MyHardware.h`
- **Emergency Stop Integration**: Immediate stop capability throughout all motion systems

### Main Application Flow
The main application (`nanoELS-flow.ino`) coordinates all subsystems:
```cpp
void loop() {
  esp32Motion.update();      // Process MPG inputs (motion control runs in separate task)
  webInterface.update();     // Handle HTTP/WebSocket requests  
  nextionDisplay.update();   // Update display and process touch events
  handleKeyboard();          // Process PS2 keyboard input
  processMovement();         // Handle movement logic and status updates
}
```

### Motion Control Architecture
**Critical Changes**: FastAccelStepper has been completely removed and replaced with native ESP32-S3 implementation:

- **FreeRTOS Task**: Motion control runs on Core 1 with 1ms update rate
- **Direct MPG Control**: Immediate response without buffering for MPG inputs
- **Smooth MPG Scaling**: Velocity-based step scaling from 0.01mm to 0.5mm per detent
- **Acceleration Profiles**: Smooth acceleration/deceleration with sub-100μs timing
- **Emergency Stop**: Highest priority - checked on every step and task cycle

### MPG Control System
The MPG system features sophisticated velocity-based control:
- **Velocity Detection**: Tracks encoder counts per second for responsive scaling
- **Step Scaling**: Linear interpolation between 0.01mm (slow) and 0.5mm (fast) per detent
- **Smooth Motion**: Acceleration/deceleration profiles with 20-100μs step intervals
- **Emergency Stop**: Immediate stop capability on every step

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
- **Motion Status**: Real-time MPG debugging with velocity and scaling information
- **Diagnostics**: Use keyboard 'Win' key or call `esp32Motion.printDiagnostics()`

### Motion Control Testing
- **MPG Testing**: Rotate X/Z axis MPGs - should show smooth movement with velocity-based scaling
- **Emergency Stop**: ESC key (B_OFF) stops all motion immediately
- **Manual Control**: Use arrow keys for axis movement
- **Velocity Scaling**: Fast MPG rotation increases step size up to 0.5mm per detent

### Development Testing Flow
1. **Hardware Verification**: Enable axes and test basic movement
2. **MPG Testing**: Verify smooth velocity-based scaling (0.01mm to 0.5mm per detent)
3. **Emergency Stop**: Test immediate stop with ESC key
4. **Web Interface**: Access via IP address shown in serial output
5. **Display Testing**: Touch screen interactions via NextionDisplay module

### Current Implementation Status
**✅ Working Features**:
- Native ESP32-S3 motion control system (no external libraries)
- Smooth MPG control with velocity-based step scaling
- Task-based motion control with FreeRTOS
- Real-time circular buffer motion queue
- Immediate emergency stop functionality
- Nextion display with 1300ms splash screen
- PS2 keyboard interface with full key mapping
- Web interface with HTTP and WebSocket support

**🔧 Current Functionality**:
- **Manual Mode**: Smooth MPG control with velocity-based scaling
- **Emergency Stop**: Immediate response to ESC key
- **Motion Control**: Task-based stepper control with acceleration profiles
- **Display**: Real-time status and position updates

### Library Dependencies and Versions
Key libraries specified in `platformio.ini`:
- **WebSockets**: For WebSocket communication
- **PS2KeyAdvanced**: For PS2 keyboard interface
- **Native ESP32-S3**: No external stepper libraries - uses ESP32 hardware directly

#### ESP32-S3 Build Configuration
- **Platform**: espressif32
- **Framework**: Arduino
- **CPU Frequency**: 240MHz
- **Flash Mode**: QIO
- **Partitions**: huge_app.csv (for large application)
- **PSRAM**: Enabled with cache fix
- **Upload Speed**: 921600 baud
- **Debug Level**: 0 (production)

### File Organization Rules
- **Main application**: `nanoELS-flow/nanoELS-flow.ino` (Arduino IDE compatible)
- **Hardware definitions**: `nanoELS-flow/MyHardware.txt` (authoritative pin mappings)
- **Core modules**: `nanoELS-flow/*.h` and `nanoELS-flow/*.cpp` (modular architecture)
- **Web assets**: `nanoELS-flow/indexhtml.h` (embedded web interface)
- **PlatformIO build**: `src/` folder (mirrors nanoELS-flow/ for compilation)

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
- **Interrupt-Based Encoders**: Optimized quadrature decoding with lookup tables
- **Smooth MPG Control**: Velocity-based step scaling with acceleration profiles
- **Emergency Stop Priority**: Highest priority safety system

### Code Architecture Rules
1. **Native ESP32-S3 Only**: No external stepper libraries allowed
2. **Task-Based Motion**: All motion control via FreeRTOS tasks
3. **Circular Buffer**: Real-time safe motion queue (no std::queue)
4. **Interrupt-Based Encoders**: Hardware-optimized encoder handling
5. **Emergency Stop Integration**: Must be checked throughout all motion systems

### Development Constraints
- **No Y-Axis**: Completely ignore Y-axis functionality
- **ESP32-S3 Only**: Do not consider other microcontrollers
- **No External Stepper Libraries**: Use native ESP32-S3 implementation only
- **Fixed Pins**: Pin assignments are permanent once set
- **Touch Primary**: Touch screen is primary interface

### Testing Requirements
- **MPG Smoothness**: Verify velocity-based step scaling works smoothly
- **Emergency Stop**: Test immediate stop response (<10ms)
- **Motion Profiles**: Verify smooth acceleration/deceleration
- **Task Performance**: Monitor FreeRTOS task timing and performance

### Documentation Standards
- **Pin Mappings**: Maintain permanent pin assignment documentation (see MyHardware.txt)
- **Motion Control**: Document native ESP32-S3 implementation patterns
- **MPG Behavior**: Document velocity-based scaling parameters
- **Emergency Stop**: Document safety system integration
- **Hardware File**: MyHardware.txt is the authoritative source for all pin definitions and key mappings

### Personal Keyboard Mapping (Development Configuration)
**Movement Controls**:
- B_LEFT (68): Left arrow - Z axis left movement
- B_RIGHT (75): Right arrow - Z axis right movement  
- B_UP (85): Up arrow - X axis forward movement
- B_DOWN (72): Down arrow - X axis backward movement

**Operation Controls**:
- B_ON (50): Enter - starts operation/mode
- B_OFF (145): ESC - **IMMEDIATE EMERGENCY STOP**
- B_PLUS (87): Numpad plus - increment pitch/passes
- B_MINUS (73): Numpad minus - decrement pitch/passes

**Critical Safety Note**: B_OFF (ESC key) triggers immediate emergency stop regardless of mode or operation state.

## Project Memories and Notes

### Critical Architecture Changes
- **FastAccelStepper Removed**: Completely replaced with native ESP32-S3 implementation
- **Task-Based Motion Control**: FreeRTOS task on Core 1 for real-time processing
- **Smooth MPG Control**: Velocity-based step scaling eliminates jerky movement
- **Emergency Stop Priority**: Highest priority safety system integrated throughout

### Motor Movement Mapping
- 4000 steps equals 4mm movement on x and 5mm movement on z
- MPG encoders: Velocity-based scaling from 0.01mm to 0.5mm per detent
- Step resolution: 0.002mm per step (5000 steps/mm)

### System Communication Notes
- Serial output available at 115200 baud
- Nextion display: 1300ms splash screen on boot (mandatory)
- MPG debug output shows velocity and scaling information

### Development Environment
- Arduino IDE version 2.3.6
- PlatformIO located at `~/.platformio/penv/bin/pio`
- Dual folder structure: `nanoELS-flow/` (Arduino IDE) and `src/` (PlatformIO)

**CRITICAL REMINDERS**:
1. Target is ESP32-S3-dev board (H5 variant) ONLY
2. NO external stepper libraries - native ESP32-S3 implementation only
3. Pin definitions are PERMANENT once assigned
4. Task-based motion control with FreeRTOS
5. Emergency stop has highest priority throughout system
6. Y-axis is NOT implemented - ignore completely
7. MPG control uses velocity-based step scaling for smooth operation

## License

This project is licensed under GPL-3.0, maintaining compatibility with the original nanoELS project while allowing for the complete rewrite approach.