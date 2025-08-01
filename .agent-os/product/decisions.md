# Key Decisions: nanoELS-flow

## Architecture Decisions

### Motion Control Approach
**Decision**: Use spindle-synchronized motion control with direct position following
**Rationale**: Provides precise threading and turning operations by having tools follow spindle rotation
**Impact**: Eliminates need for complex speed/acceleration algorithms in automatic mode

### Hardware Platform
**Decision**: ESP32-S3 (H5 variant) with Arduino framework
**Rationale**: Provides sufficient processing power, WiFi, and GPIO for motion control
**Impact**: Enables web interface and real-time control capabilities

### Motor Control Interface
**Decision**: Pulse/Direction signals for stepper/servo control
**Rationale**: Standard interface compatible with most motion control drivers
**Impact**: Simplified hardware integration and driver compatibility

## Technical Decisions

### Spindle Tracking
**Decision**: Use ESP32 PCNT (Pulse Counter) for encoder reading
**Rationale**: Hardware-based counting provides accurate real-time position tracking
**Impact**: Reliable position feedback without CPU overhead

### User Interface
**Decision**: Nextion touch screen + PS2 keyboard + web interface
**Rationale**: Provides multiple control options (local touch, keyboard, remote web)
**Impact**: Flexible user interaction but increased complexity

### Communication Protocol
**Decision**: WebSocket for real-time web communication
**Rationale**: Enables bidirectional real-time updates between web interface and ESP32
**Impact**: Responsive web interface with minimal latency

## Code Organization Decisions

### Modular Architecture
**Decision**: Separate classes for motion, display, web interface, and operation management
**Rationale**: Maintains clean separation of concerns and enables independent development
**Impact**: Easier testing, debugging, and feature development

### State Machine Pattern
**Decision**: Use state machine for operation management
**Rationale**: Provides clear workflow control and prevents invalid state transitions
**Impact**: Reliable operation sequencing and safety management

### Hardware Abstraction
**Decision**: Centralize hardware configuration in SetupConstants files
**Rationale**: Makes hardware changes easier and reduces configuration errors
**Impact**: Simplified hardware adaptation for different setups

## Safety Decisions

### Emergency Stop
**Decision**: Immediate hardware-level stop with software confirmation
**Rationale**: Ensures safety even if software fails
**Impact**: Reliable emergency response but requires careful state management

### Motion Validation
**Decision**: Validate all motion commands before execution
**Rationale**: Prevents dangerous movements and protects hardware
**Impact**: Safer operation but requires comprehensive validation logic

## Performance Decisions

### Update Frequency
**Decision**: 5kHz update rate for motion control
**Rationale**: Provides smooth motion while maintaining real-time responsiveness
**Impact**: Good performance but requires efficient code execution

### Speed Control
**Decision**: Bypass speed ramping in spindle-synchronized mode
**Rationale**: Spindle speed determines feed rate, not arbitrary speed limits
**Impact**: More accurate motion but requires careful manual mode handling 