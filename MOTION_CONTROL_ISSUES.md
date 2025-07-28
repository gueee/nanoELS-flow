# ⚠️ CRITICAL: TURN Mode Operation Issues

## ⚠️ **WARNING: TURN MODE HAS CRITICAL BUGS**

Current status of TURN mode implementation in OperationManager.cpp:

## 🚨 **1. Cut Distance Scaling - 60x TOO SMALL**

### Current Problem:
- **User sets**: 123mm cut length
- **System cuts**: ~2mm actual distance  
- **Error ratio**: 60x too small

### Root Cause (Under Investigation):
```cpp
// OperationManager.cpp:788 - Completion check
if (abs(deltaZ) >= abs(cutLength)) {
    return true;  // Triggers too early
}
```

### Suspected Issues:
- `posFromSpindle()` calculation scaling error
- Integer division truncation (partially fixed with floating point)
- Incorrect completion threshold comparison

## 🔄 **2. Pass Advancement - BROKEN**

### Current Problem:
- TURN mode stays at "Pass 1/3" 
- Never advances to Pass 2/3 or 3/3
- Operation repeats first pass until manually stopped

### Root Cause:
- Pass completion detection triggers prematurely due to distance scaling issue
- `performCuttingPass()` returns `true` after ~2mm instead of 123mm
- Pass advancement logic works correctly, but gets wrong completion signal

## ✅ **3. Working Components**

### Functional Systems:
- ✅ **TURN mode setup workflow**: Direction, touch-off, targets, passes
- ✅ **Pitch control**: User can set positive/negative values
- ✅ **Manual movement**: Arrow keys work when enabled
- ✅ **Nextion display**: All prompts and status display correctly
- ✅ **Emergency stop**: ESC key stops operation immediately

## 🔧 **Current Architecture**

### File Structure:
```
nanoELS-flow/
├── nanoELS-flow.ino          # Main application
├── OperationManager.cpp/.h   # TURN mode implementation
├── MinimalMotionControl.cpp/.h # Motor control
├── NextionDisplay.cpp/.h     # Touch screen interface
├── SetupConstants.cpp/.h     # Hardware configuration
└── WebInterface.cpp/.h       # HTTP/WebSocket server
```

### Key Functions (OperationManager.cpp):
- `posFromSpindle()` - Line 75: Spindle to motor step conversion
- `performCuttingPass()` - Line 743: Cut execution and completion check
- `executeTurnMode()` - Line 930: Pass state machine

## 🛠️ **Required Fixes**

### Priority 1 - Critical:
1. **Fix distance scaling**: Identify why 123mm → 2mm
2. **Fix pass advancement**: Ensure proper completion detection

### Investigation Needed:
- Verify `cutLength` calculation (should be 98,400 steps for 123mm)
- Check `deltaZ` values from `posFromSpindle()`
- Analyze completion condition `abs(deltaZ) >= abs(cutLength)`

## ⚠️ **Testing Status**

- ✅ **Setup workflow**: Complete and functional
- ✅ **User interface**: All prompts work correctly  
- ✅ **Manual operations**: Arrow keys, touch-off working
- ❌ **Cut distance**: 60x scaling error
- ❌ **Pass progression**: Stuck at first pass
- ❌ **Operation completion**: Premature termination

---

**Last Updated**: 2025-07-28  
**Status**: ⚠️ **CRITICAL BUGS - DISTANCE & PASS ADVANCEMENT BROKEN** ⚠️