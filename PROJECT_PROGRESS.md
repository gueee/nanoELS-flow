# nanoELS-flow Project Progress Summary

**Date:** 2025-07-28  
**Status:** TURN mode implemented with critical scaling issues  

## ✅ Successfully Implemented Features

### 1. **Hardware Setup & Pin Configuration**
- **Target**: ESP32-S3-dev board (H5 variant) 
- **Pin definitions**: Fixed assignments in `SetupConstants.h`
- **Motors**: Z-axis (4000 steps/rev, 5mm pitch), X-axis (4000 steps/rev, 4mm pitch)
- **Encoders**: Spindle encoder (600 PPR) + 2 MPG encoders
- **Display**: Nextion touch screen (primary interface, no Serial Monitor)

### 2. **Development Environment**
- **Arduino IDE 2.3.6+**: Only supported development environment
- **Board**: ESP32S3 Dev Module with specific settings:
  - CPU: 240MHz, Flash: 16MB, Partition: Huge APP
  - PSRAM: Enabled, Upload Speed: 921600

### 3. **TURN Mode Workflow**
- **Complete setup sequence**: Direction → Touch-off → Targets → Passes → GO
- **Direction control**: Internal/External, Left-to-Right/Right-to-Left
- **Touch-off system**: X-diameter and Z-position with coordinate storage
- **Target entry**: Diameter and cut length with numpad input
- **Pass management**: 3-pass default with depth progression

### 4. **User Interface System**
- **Nextion display**: All prompts and status messages
- **Numpad input**: Metric (3 decimal), Inch (4 decimal), TPI support
- **Arrow key control**: Motion/Navigation modes with proper disable/enable
- **Emergency stop**: ESC key immediate stop functionality

## 🚨 **Critical Issues - TURN Mode**

### **1. Cut Distance Scaling (60x ERROR)**
- **Expected**: 123mm cut length → 98,400 motor steps
- **Actual**: ~2mm cut length → ~1,600 motor steps  
- **Error**: Distance 60x too small

### **2. Pass Advancement (BROKEN)**
- **Problem**: Stays at "Pass 1/3", never advances
- **Cause**: Premature completion due to distance scaling issue
- **Impact**: Operation repeats first pass until manually stopped

## 🔧 Current Implementation Status

### **File Structure & Architecture**
```
nanoELS-flow/
├── nanoELS-flow.ino              # Main application loop
├── OperationManager.cpp/.h       # TURN mode workflow and logic
├── MinimalMotionControl.cpp/.h   # Motor control and positioning
├── NextionDisplay.cpp/.h         # Touch screen interface
├── SetupConstants.cpp/.h         # Hardware configuration
├── StateMachine.cpp/.h           # UI state management
├── WebInterface.cpp/.h           # HTTP server (secondary)
└── CircularBuffer.h              # Real-time buffer
```

### **Key Implementation Details**

#### **TURN Mode State Machine** (`OperationManager.cpp`)
```cpp
// Main execution states
SUBSTATE_MOVE_TO_START    // Position to start
SUBSTATE_SYNC_SPINDLE     // Wait for spindle sync
SUBSTATE_CUTTING          // Execute cutting pass
SUBSTATE_RETRACTING       // Retract tool
SUBSTATE_RETURNING        // Return for next pass
```

#### **Distance Calculation** (`OperationManager.cpp:75-96`)
```cpp
long OperationManager::posFromSpindle(int axis, long spindlePos, bool respectLimits) {
    // Conversion from spindle position to motor steps
    // CRITICAL: This has 60x scaling error
    float calc = (float)spindlePos * MOTOR_STEPS_Z * dupr * starts / SCREW_Z_DU / encoderSteps;
    return (long)calc;
}
```

#### **Pass Completion Check** (`OperationManager.cpp:788`)
```cpp
// BROKEN: Triggers too early due to scaling issue  
if (abs(deltaZ) >= abs(cutLength)) {
    return true;  // Should be false until full distance
}
```

## 🎯 Working Test Procedure

### **Current Functionality**
1. **Power on** → ESP32 boots, Nextion display shows ELS interface
2. **F2 Key** → Enter TURN mode setup
3. **Arrow keys** → Select direction (Internal/External, L→R/R→L)  
4. **ENTER** → Advance through setup steps
5. **Touch-off**: X-axis diameter, Z-axis position with numpad entry
6. **Targets**: Final diameter and cut length entry
7. **Passes**: Number of passes (default 3)
8. **GO**: Start operation (currently fails due to scaling)

### **Hardware Settings (SetupConstants.cpp)**
```cpp
// Z-axis: 5mm lead screw, 4000 steps/rev
const long SCREW_Z_DU = 50000;        // 5mm = 50,000 deci-microns  
const long MOTOR_STEPS_Z = 4000;      // Steps per revolution

// X-axis: 4mm lead screw, 4000 steps/rev  
const long SCREW_X_DU = 40000;        // 4mm = 40,000 deci-microns
const long MOTOR_STEPS_X = 4000;      // Steps per revolution

// Spindle encoder
const int ENCODER_PPR = 600;          // Pulses per revolution
```

## 📋 Immediate Priority Issues

### **Critical (Blocking Production Use)**
1. **Fix distance scaling**: 123mm must cut exactly 123mm
2. **Fix pass advancement**: Must progress through all passes
3. **Root cause analysis**: Identify scaling error in `posFromSpindle()`

### **Investigation Areas**
- `cutLength` calculation verification (should be 98,400 steps for 123mm)
- `deltaZ` values from spindle movement  
- Completion threshold comparison logic
- Integer vs floating point precision issues

## 🚀 How to Resume Development

### **Build & Upload**
1. **Open Arduino IDE 2.3.6+**
2. **Load project**: `/nanoELS-flow/nanoELS-flow.ino`
3. **Board settings**: ESP32S3 Dev Module (settings in CLAUDE.md)
4. **Upload**: Use USB cable to ESP32-S3-dev board

### **Debug Approach** 
- **Primary interface**: Nextion display only (no Serial Monitor per project rules)
- **Code analysis**: Trace scaling calculations through code logic
- **Test methodology**: Set known distances, measure actual movement

### **Working Components to Preserve**
- ✅ Complete TURN mode setup workflow
- ✅ Numpad input and display formatting  
- ✅ Touch-off coordinate system
- ✅ Emergency stop functionality
- ✅ Arrow key motion control

---

**Build command**: Compile and upload via Arduino IDE  
**Hardware**: ESP32-S3-dev + stepper drivers + Nextion display + PS2 keyboard + spindle encoder  
**Current status**: ⚠️ **TURN mode workflow complete, distance scaling critical bug** ⚠️