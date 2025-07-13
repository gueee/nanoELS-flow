# nanoELS-flow Codebase Analysis

## Project Overview

**nanoELS-flow** is a complete rewrite of the original nanoELS Electronic Lead Screw controller for metal lathes. This ESP32-S3-based system provides enhanced user experience while maintaining hardware compatibility with the original designs.

### Key Information
- **Target Platform**: ESP32-S3 Dev Module (H5 variant)
- **Original Project**: Based on nanoELS by Maxim Kachurovskiy
- **License**: GPL-3.0 (original was MIT)
- **Development Status**: 🚧 Work in Progress

## Architecture Overview

### Core Components

The system follows a modular architecture with four main modules:

1. **MotionControl** - Stepper motor control and positioning
2. **WebInterface** - WiFi connectivity and web-based control
3. **NextionDisplay** - Touch screen interface management
4. **Main Controller** - Central coordination and keyboard input

### Hardware Configuration (H5 Variant)

#### Stepper Motors
- **X-axis**: Step=7, Dir=15, Enable=16
- **Z-axis**: Step=35, Dir=42, Enable=41

#### Encoders (PCNT Units)
- **Spindle**: A=13, B=14 (PCNT_UNIT_0)
- **Z-MPG**: A=18, B=8 (PCNT_UNIT_1)
- **X-MPG**: A=47, B=21 (PCNT_UNIT_2)

#### Interface
- **PS2 Keyboard**: Data=37, Clock=36
- **Nextion Display**: Serial1 (built-in)

## Key Features

### Motion Control System
- **FastAccelStepper integration** - High-performance stepper control
- **Real-time command queuing** - Smooth motion execution
- **Safety systems** - Emergency stop, software limits
- **Spindle synchronization** - For threading operations (partially implemented)
- **Multiple axis control** - Independent X and Z axis management

### Web Interface
- **Dual WiFi modes**: Access Point or home network connection
- **WebSocket communication** - Real-time control and status updates
- **G-code file management** - Upload, store, and execute G-code
- **Motion status monitoring** - Live position and status updates
- **Responsive design** - Modern web interface

### Display System
- **Nextion touchscreen support** - 4 display areas (T0-T3)
- **State management** - Boot, WiFi, normal, emergency states
- **Priority messaging** - Layered information display
- **Hash-based optimization** - Efficient screen updates
- **Initialization progress** - Startup status indication

### User Interface
- **PS2 keyboard control** - Extensive key mapping for all functions
- **Manual positioning** - Arrow keys for axis movement
- **Step size control** - Variable movement increments (1, 10, 100, 1000)
- **Mode selection** - F-keys for different operation modes
- **Emergency controls** - Immediate stop and axis enable/disable

## Code Structure

### Main Files
```
nanoELS-flow/
├── nanoELS-flow.ino          # Main controller (355 lines)
├── MotionControl.h/.cpp      # Motor control (159/515 lines)
├── WebInterface.h/.cpp       # Web features (72/570 lines)
├── NextionDisplay.h/.cpp     # Touch screen (112/??? lines)
├── MyHardware.h              # Pin definitions (86 lines)
├── indexhtml.h               # Web UI (340 lines)
└── platformio.ini            # Build configuration
```

### Key Classes

#### MotionControl Class
- **FastAccelStepper integration** - Professional stepper motor control
- **Command queue system** - Real-time motion execution
- **Safety features** - Emergency stop, limits, axis enable/disable
- **Position tracking** - Absolute and relative positioning
- **Spindle encoder support** - For synchronized operations

#### WebInterface Class
- **WiFi management** - Station mode or Access Point
- **HTTP server** - Status, G-code management
- **WebSocket server** - Real-time communication
- **File system** - G-code storage and retrieval

#### NextionDisplay Class
- **Multi-area display** - T0-T3 text areas
- **State management** - Different display modes
- **Message queuing** - Priority-based information display
- **Hash optimization** - Efficient update detection

## Development Environment

### Required Libraries
- **FastAccelStepper** by gin66 - Core motor control
- **WebSockets** by Markus Sattler - Real-time communication
- **PS2KeyAdvanced** by Paul Carpenter - Keyboard interface

### Board Configuration
- **Board**: ESP32S3 Dev Module
- **Flash Size**: 16MB
- **Partition**: Huge APP (3MB No OTA/1MB SPIFFS)
- **CPU**: 240MHz (WiFi/BT enabled)
- **PSRAM**: OPI PSRAM enabled

### WiFi Configuration
```cpp
#define WIFI_MODE 1  // 0=AP mode, 1=Home WiFi
const char* HOME_WIFI_SSID = "YourNetwork";
const char* HOME_WIFI_PASSWORD = "YourPassword";
```

## Implementation Highlights

### Motion Control Features
- **Non-blocking operations** - Smooth concurrent operations
- **Software limits** - Configurable axis boundaries
- **Speed/acceleration control** - Tunable motion parameters
- **Emergency stop system** - Immediate halt capability
- **Position synchronization** - Coordinated multi-axis movement

### Web Interface Capabilities
- **Real-time status** - Live position and system information
- **G-code management** - Upload, store, delete G-code files
- **Command interface** - Direct motion control via web
- **Mobile responsive** - Works on phones and tablets

### Display System Features
- **Efficient updates** - Hash-based change detection
- **Priority messaging** - Important messages override routine display
- **State transitions** - Smooth UI state changes
- **Initialization feedback** - Clear startup progress indication

## Notable Implementation Details

### FastAccelStepper Integration
- Uses static engine instance for Arduino compatibility
- Proper pin configuration with enable/disable support
- Speed and acceleration control per axis
- Non-blocking movement execution

### PCNT Conflict Resolution
- Avoids ESP32 IDF v5.x PCNT conflicts with FastAccelStepper
- Simplified encoder interface to prevent compilation issues
- Future-ready for proper quadrature decoding

### Memory Management
- PROGMEM storage for web interface HTML
- Efficient string handling in display updates
- Queue-based command processing to prevent memory leaks

### Safety Systems
- Emergency stop with immediate motor disable
- Software limit checking before movement
- Axis enable/disable state management
- Command rejection during emergency states

## Current Status & Limitations

### Implemented Features ✅
- Basic motion control (X/Z axes)
- PS2 keyboard interface
- Nextion display management
- WiFi connectivity (AP and Station modes)
- Web interface with real-time communication
- G-code file storage
- Emergency stop system
- Manual positioning

### Partially Implemented 🚧
- Spindle encoder integration (pins configured, full quadrature pending)
- Threading operations (framework exists)
- Synchronized movements (command structure ready)

### Future Enhancements 🔮
- Complete spindle synchronization
- Threading operation modes
- Advanced G-code execution
- Improved error handling
- Enhanced safety features

## Development Notes

### Hardware Compatibility
- **Maintains original nanoELS H5 hardware compatibility**
- **Pin assignments are permanent** - clearly documented
- **Supports existing Nextion display interface**
- **Compatible with original keyboard mappings**

### Code Quality
- **Modern C++ practices** - Clean class structure
- **Comprehensive error handling** - Graceful failure modes
- **Modular design** - Well-separated concerns
- **Extensive documentation** - Clear setup instructions

### Testing Considerations
- Motion control tests available but disabled during startup
- Serial monitoring provides extensive diagnostic information
- Web interface allows remote testing and monitoring
- Emergency stop testing is critical for safety

## Conclusion

The nanoELS-flow project represents a significant modernization of the original nanoELS controller while maintaining hardware compatibility. The codebase demonstrates solid engineering practices with a focus on safety, modularity, and user experience. The current implementation provides a strong foundation for a full-featured electronic lead screw system.

The project is well-structured for continued development, with clear separation of concerns and comprehensive documentation. The use of industry-standard libraries (FastAccelStepper) and modern web technologies positions it well for future enhancements.