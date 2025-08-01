# Tech Stack: nanoELS-flow

## Core Platform
- **ESP32-S3** - Main microcontroller (H5 variant)
- **Arduino Framework** - Development environment
- **C++** - Primary programming language

## Hardware Components
- **Teknic ClearPath servos** - Motion control motors
- **Quadrature encoders** - Position feedback (600 PPR)
- **Nextion touch screen** - Local user interface
- **PS2 keyboard** - Direct control input
- **WiFi module** - Web interface connectivity

## Libraries & Dependencies
- **WebSockets** by Markus Sattler - Real-time web communication
- **PS2KeyAdvanced** by Paul Carpenter - PS2 keyboard interface
- **ESP32 PCNT** - Hardware pulse counter for encoders

## Development Tools
- **Arduino IDE 2.3.6+** - Primary development environment
- **arduino-cli** - Command-line build and upload tool
- **Git** - Version control

## Key Architecture Patterns
- **Modular design** - Separate classes for motion, display, web interface
- **State machine** - Operation management and workflow control
- **Real-time safety** - Emergency stop and safety interlocks
- **Hardware abstraction** - Clean separation of hardware and software layers

## Communication Protocols
- **WebSocket** - Real-time web interface communication
- **Serial** - Nextion display communication
- **PS2** - Keyboard input protocol
- **Pulse/Direction** - Motor control signals

## File Organization
- **nanoELS-flow.ino** - Main application entry point
- **SetupConstants.h/.cpp** - Hardware configuration and constants
- **MinimalMotionControl.h/.cpp** - Core motion control algorithms
- **OperationManager.h/.cpp** - State machine and workflow management
- **NextionDisplay.h/.cpp** - Display interface
- **WebInterface.h/.cpp** - Web server and WebSocket handling
- **StateMachine.h/.cpp** - State management 