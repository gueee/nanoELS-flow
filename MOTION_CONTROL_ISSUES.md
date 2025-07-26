# ⚠️ CRITICAL: Motion Control System Issues

## ⚠️ **WARNING: MOTION SYSTEM PARTIALLY TESTED**

The motion control system has been significantly improved but still needs verification for production use:

## 🏃 **1. Excessive Speed - Motors Run "As Fast As Possible"**

### Current Speed Settings (SetupConstants.cpp):
```cpp
const long SPEED_MANUAL_MOVE_Z = 8 * MOTOR_STEPS_Z;  // = 8 * 4000 = 32,000 steps/sec
const long SPEED_MANUAL_MOVE_X = 8 * MOTOR_STEPS_X;  // = 8 * 4000 = 32,000 steps/sec
```

### Problems:
- **32,000 steps/second** is extremely fast for most stepper motors
- No proper speed ramping (acceleration formula is incorrect)
- Motors will likely stall, skip steps, or move erratically
- **Dangerous for actual lathe operations**

### Reference (h5.ino working speeds):
```cpp
const long SPEED_MANUAL_MOVE_Z = 8 * 800 = 6,400 steps/sec  // 5x slower
const long SPEED_MANUAL_MOVE_X = 8 * 800 = 6,400 steps/sec  // 5x slower
```

## ✅ **2. Distance Calculations - HARDWARE SPECIFIC (RESOLVED)**

### Current Motor Settings (SetupConstants.cpp):
```cpp
const long MOTOR_STEPS_Z = 4000;  // Steps per revolution - CORRECT for this hardware
const long MOTOR_STEPS_X = 4000;  // Steps per revolution - CORRECT for this hardware
```

### Status: **RESOLVED**
- ✅ **Motor steps verified correct** for this specific hardware configuration
- ✅ **Manual movements tested accurate** - confirms proper calibration
- ✅ **Target length calculation fixed** - now uses cut distance not target position
- ⚠️ **Note**: Different from h5.ino reference (800 steps/rev) due to different hardware

### Verification:
- Manual movement distances: **✅ ACCURATE**
- Touch-off coordinates: **✅ WORKING**
- Target calculation: **✅ FIXED** (was treating target as position, now as cut distance)

## 🔧 **3. Acceleration Algorithm Issues**

### Current Code (MinimalMotionControl.cpp:266):
```cpp
a.currentSpeed += a.acceleration / a.currentSpeed;  // ❌ INCORRECT FORMULA
```

### Problems:
- Mathematically incorrect acceleration curve
- Can cause divide-by-zero or erratic behavior
- No proper deceleration control

## 🛠️ **Required Fixes**

### Immediate Actions Needed:
1. **Fix motor step constants** to match actual hardware
2. **Reduce maximum speeds** to safe levels
3. **Fix acceleration algorithm** using proper physics
4. **Add speed limiting** and validation
5. **Calibrate distance calculations** with real measurements

### Safety Recommendations:
- **DO NOT USE** for actual machining until fixed
- Test with **NO WORKPIECE** and manual observation only
- Keep **emergency stop** readily accessible
- Verify all distances with measurement tools

## 📋 **Testing Status**

- ✅ **Workflow Logic**: TURN mode setup works correctly
- ✅ **User Interface**: Touch-off, target entry, operation start
- ✅ **Distance Calculations**: Verified accurate for this hardware
- ✅ **Target Length Fix**: Now uses cut distance (not target position)
- ✅ **Manual Movement Control**: Properly disabled during setup
- ⚠️ **Motion Control Speeds**: May still need verification for high-speed operations
- ⚠️ **Acceleration Algorithm**: Needs review for production use

## 🔗 **Related Issues**

See GitHub issues for tracking motion control fixes:
- [ ] Issue #X: Fix excessive motion speeds
- [ ] Issue #Y: Correct distance/step calculations  
- [ ] Issue #Z: Implement proper acceleration curves

---

**Last Updated**: 2025-07-26  
**Status**: ⚠️ **PARTIAL - BASIC OPERATIONS WORKING, VERIFY BEFORE PRODUCTION USE** ⚠️