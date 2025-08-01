#include "OperationManager.h"
#include "MinimalMotionControl.h"
#include "SetupConstants.h"
#include <cmath>

extern MinimalMotionControl motionControl;

OperationManager::OperationManager()
    : motionControl(nullptr)
    , currentMode(MODE_NORMAL)
    , currentState(STATE_IDLE)
    , passSubState(SUBSTATE_MOVE_TO_START)
    , setupIndex(0)
    , touchOffXCoord(0.0f)
    , touchOffZCoord(0.0f)
    , touchOffXValid(false)
    , touchOffZValid(false)
    , inNumpadInput(false)
    , numpadIndex(0)
    , currentMeasure(MEASURE_METRIC)
    , touchOffAxis(0)
    , arrowKeyMode(ARROW_MOTION_MODE)
    , isInternalOperation(false)
    , isLeftToRight(false)
    , touchOffX(0)
    , touchOffZ(0)
    , touchOffComplete(false)
    , parkingPositionX(0)
    , parkingPositionZ(0)
    , parkingPositionSet(false)
    , targetDiameter(0)
    , targetZLength(0)
    , cutLength(0)
    , cutDepth(0)
    , numPasses(3)
    , coneRatio(0.0f)
    , currentPass(0)
    , passDepth(0)
    , opDuprSign(1)
    , opDupr(0)
    , spindleSyncPos(0)
    , startOffset(0)
    , cuttingParamsMaterial(MATERIAL_MILD_STEEL)
    , cuttingParamsTool(TOOL_CARBIDE_COATED)
    , cuttingParamsOperation(OP_ROUGH_TURNING)
    , cuttingParamsDiameter(25.0f)
    , cuttingParamsStep(0)
{
    // Initialize numpad digits array
    for (int i = 0; i < 20; i++) {
        numpadDigits[i] = 0;
    }
    
    // Initialize cutting parameters result
    cuttingParamsResult.isValid = false;
    cuttingParamsResult.rpm = 0;
    cuttingParamsResult.cuttingSpeed = 0.0f;
    cuttingParamsResult.recommendation = "";
}

void OperationManager::init(MinimalMotionControl* mc) {
    motionControl = mc;
}

long OperationManager::mmToSteps(float mm, int axis) {
    if (!motionControl) return 0;
    
    if (axis == AXIS_X) {
        return round(mm * MOTOR_STEPS_X / (SCREW_X_DU / 10000.0f));
    } else {
        return round(mm * MOTOR_STEPS_Z / (SCREW_Z_DU / 10000.0f));
    }
}

float OperationManager::stepsToMm(long steps, int axis) const {
    if (!motionControl) return 0;
    
    if (axis == AXIS_X) {
        return steps * (SCREW_X_DU / 10000.0f) / MOTOR_STEPS_X;
    } else {
        return steps * (SCREW_Z_DU / 10000.0f) / MOTOR_STEPS_Z;
    }
}

// H5.ino-style spindle position to axis position calculation
long OperationManager::posFromSpindle(int axis, long spindlePos, bool respectLimits) {
    if (!motionControl) return 0;
    
    long dupr = motionControl->getDupr();
    int starts = motionControl->getStarts();
    float encoderSteps = ENCODER_PPR * 2.0f; // Quadrature encoding
    
    long newPos = 0;
    if (axis == AXIS_Z) {
        newPos = spindlePos * MOTOR_STEPS_Z / (SCREW_Z_DU / 10000.0f) / encoderSteps * dupr * starts;
    } else {
        newPos = spindlePos * MOTOR_STEPS_X / (SCREW_X_DU / 10000.0f) / encoderSteps * dupr * starts;
    }
    
    // TODO: Implement limit checking when limits are added to MinimalMotionControl
    
    return newPos;
}

long OperationManager::spindleFromPos(int axis, long pos) {
    if (!motionControl) return 0;
    
    long dupr = motionControl->getDupr();
    int starts = motionControl->getStarts();
    float encoderSteps = ENCODER_PPR * 2.0f;
    
    if (axis == AXIS_Z) {
        return pos * (SCREW_Z_DU / 10000.0f) * encoderSteps / MOTOR_STEPS_Z / (dupr * starts);
    } else {
        return pos * (SCREW_X_DU / 10000.0f) * encoderSteps / MOTOR_STEPS_X / (dupr * starts);
    }
}

// h5.ino-style numpad functions
void OperationManager::numpadPress(int digit) {
    if (!inNumpadInput) {
        numpadIndex = 0;
    }
    inNumpadInput = true;
    
    if (digit >= 0 && digit <= 9) {
        numpadDigits[numpadIndex] = digit;
        if (numpadIndex < 19) {  // Keep within array bounds
            numpadIndex++;
        } else {
            // Shift array left when full
            for (int i = 0; i < 19; i++) {
                numpadDigits[i] = numpadDigits[i + 1];
            }
            numpadDigits[19] = digit;
        }
    }
}

void OperationManager::numpadBackspace() {
    if (inNumpadInput && numpadIndex > 0) {
        numpadIndex--;
        numpadDigits[numpadIndex] = 0;
    }
}

void OperationManager::resetNumpad() {
    numpadIndex = 0;
    inNumpadInput = false;
    for (int i = 0; i < 20; i++) {
        numpadDigits[i] = 0;
    }
}

long OperationManager::getNumpadResult() {
    long result = 0;
    for (int i = 0; i < numpadIndex; i++) {
        result += numpadDigits[i] * pow(10, numpadIndex - 1 - i);
    }
    return result;
}

long OperationManager::numpadToDeciMicrons() {
    long result = getNumpadResult();
    if (result == 0) {
        return 0;
    }
    
    if (currentMeasure == MEASURE_INCH) {
        result = result * 25.4;  // With fixed decimal at 1/10000, 10000 = 1.0000" = 254000 deci-microns
    } else if (currentMeasure == MEASURE_TPI) {
        result = round(254000.0 / result);  // TPI to pitch in deci-microns
    } else { // MEASURE_METRIC
        result = result * 10;  // With fixed decimal point at 1/1000, 123000 = 123.000mm = 1,230,000 deci-microns
    }
    
    return result;
}

void OperationManager::cycleMeasure() {
    if (currentMeasure == MEASURE_METRIC) {
        currentMeasure = MEASURE_INCH;
    } else if (currentMeasure == MEASURE_INCH) {
        currentMeasure = MEASURE_TPI;
    } else {
        currentMeasure = MEASURE_METRIC;
    }
    
    // Update pitch based on new measurement unit if in threading mode
    if (currentMode == MODE_THREAD && motionControl) {
        // Use touch-off diameter if available, otherwise use default
        float diameter = 10.0f;  // Default 10mm
        if (currentMeasure == MEASURE_INCH || currentMeasure == MEASURE_TPI) {
            diameter = 0.4f;  // Default 0.4"
        }
        
        if (touchOffXValid) {
            diameter = touchOffXCoord;  // Use actual touch-off diameter
        }
        
        // Only update if pitch is not locked
        if (isPitchChangeAllowed()) {
            long defaultPitch = getDefaultPitchForDiameter(diameter, currentMeasure);
            motionControl->setThreadPitch(defaultPitch);
        }
    }
}

// h5.ino-style formatting functions
String OperationManager::formatDeciMicrons(long deciMicrons, int precisionPointsMax) {
    if (deciMicrons == 0) {
        return "0";
    }
    
    bool imperial = currentMeasure != MEASURE_METRIC;
    
    // Always show fixed decimal places to match numpad input format
    int points = imperial ? 4 : 3;  // 4 decimal places for inches, 3 for metric
    
    return String(deciMicrons / (imperial ? 254000.0 : 10000.0), points) + (imperial ? "\"" : "mm");
}

String OperationManager::formatDupr(long value) {
    if (currentMeasure != MEASURE_TPI) {
        return formatDeciMicrons(value, 5);
    }
    
    // TPI formatting with rounding
    float tpi = 254000.0 / value;
    const float TPI_ROUND_EPSILON = 0.03;
    
    String result = "";
    if (abs(tpi - round(tpi)) < TPI_ROUND_EPSILON) {
        result = String(int(round(tpi)));
    } else {
        int tpi100 = round(tpi * 100);
        int points = 0;
        if ((tpi100 % 10) != 0) {
            points = 2;
        } else if ((tpi100 % 100) != 0) {
            points = 1;
        }
        result = String(tpi, points);
    }
    
    return result + "tpi";
}

String OperationManager::getNumpadDisplayText() const {
    if (!inNumpadInput) {
        return "";
    }
    
    if (numpadIndex == 0) {
        // Show default format when no digits entered yet
        if (currentMeasure == MEASURE_METRIC) {
            return "0.000mm";
        } else if (currentMeasure == MEASURE_INCH) {
            return "0.0000\"";
        } else { // MEASURE_TPI
            return "0tpi";
        }
    }
    
    // Build display string with right-to-left entry
    String display = "";
    for (int i = 0; i < numpadIndex; i++) {
        display += String(numpadDigits[i]);
    }
    
    // Add decimal point based on measurement unit
    if (currentMeasure == MEASURE_METRIC) {
        // Format as XXX.XXX mm (3 decimal places, fixed decimal at 1/1000)
        if (display.length() <= 3) {
            display = "0." + String("000").substring(0, 3 - display.length()) + display;
        } else {
            display = display.substring(0, display.length() - 3) + "." + display.substring(display.length() - 3);
        }
        display += "mm";
    } else if (currentMeasure == MEASURE_INCH) {
        // Format as X.XXXX" (4 decimal places)  
        if (display.length() <= 4) {
            display = "0." + String("0000").substring(0, 4 - display.length()) + display;
        } else {
            display = display.substring(0, display.length() - 4) + "." + display.substring(display.length() - 4);
        }
        display += "\"";
    } else { // MEASURE_TPI
        display += "tpi";
    }
    
    return display;
}

void OperationManager::setMode(OperationMode mode) {
    if (currentState != STATE_IDLE) {
        stopOperation();
    }
    
    currentMode = mode;
    currentState = STATE_IDLE;
    setupIndex = 0;  // h5.ino-style: reset to first setup step
    
    // Reset parameters for new mode
    currentPass = 0;
    spindleSyncPos = 0;
    
    // Reset workflow-specific values for turn mode
    if (mode == MODE_TURN) {
        // Clear previous touch-off values
        clearTouchOff();
        // Clear targets
        clearTargets();
        // Clear parking position
        clearParkingPosition();
        // Reset numpad
        resetNumpad();
        // Default values
        numPasses = 3;
        isInternalOperation = false;  // Default to external
        isLeftToRight = false;        // Default to R→L
        
        // Set default feed rate for turning (0.1mm or 0.004" per rev) - always positive
        if (motionControl) {
            long defaultFeedRate = (currentMeasure == MEASURE_METRIC) ? 1000 : 1016; // 0.1mm or 0.004"
            motionControl->setThreadPitch(defaultFeedRate);  // Always positive, direction from isLeftToRight
        }
    }
    
    // Reset workflow-specific values for facing mode
    if (mode == MODE_FACE) {
        // Clear previous values (same as turning)
        clearTouchOff();
        clearTargets();
        clearParkingPosition();
        resetNumpad();
        // Default values
        numPasses = 3;
        isInternalOperation = false;
        isLeftToRight = false;
        
        // Set default feed rate for facing (0.1mm or 0.004" per rev) - always positive
        if (motionControl) {
            long defaultFeedRate = (currentMeasure == MEASURE_METRIC) ? 1000 : 1016; // 0.1mm or 0.004"
            motionControl->setThreadPitch(defaultFeedRate);  // Always positive, direction from isLeftToRight
        }
    }
    
    // Reset workflow-specific values for threading mode
    if (mode == MODE_THREAD) {
        // Clear previous touch-off values
        clearTouchOff();
        // Clear targets
        clearTargets();
        // Clear parking position
        clearParkingPosition();
        // Reset numpad
        resetNumpad();
        // Default values
        numPasses = 3;
        isInternalOperation = false;  // Default to external
        isLeftToRight = false;        // Default to R→L
        // Set default starts to 1 (single-start thread)
        if (motionControl) {
            motionControl->setStarts(1);
            
            // Set intelligent default pitch based on current measurement unit
            // Use a reasonable default diameter (10mm or 0.4") if no touch-off is set
            float defaultDiameter = 10.0f;  // 10mm default
            if (currentMeasure == MEASURE_INCH || currentMeasure == MEASURE_TPI) {
                defaultDiameter = 0.4f;  // 0.4" default for imperial
            }
            
            long defaultPitch = getDefaultPitchForDiameter(defaultDiameter, currentMeasure);
            motionControl->setThreadPitch(defaultPitch);
        }
    }
    
    // Initialize cutting parameters mode
    if (mode == MODE_CUTTING_PARAMS) {
        // Reset to initial cutting parameters state
        currentState = STATE_CUTTING_PARAMS_MATERIAL;
        cuttingParamsStep = 0;
        cuttingParamsMaterial = MATERIAL_MILD_STEEL;  // Default material
        cuttingParamsTool = TOOL_CARBIDE_COATED;      // Default tool
        cuttingParamsOperation = OP_ROUGH_TURNING;    // Default operation
        cuttingParamsDiameter = 25.0f;               // Default diameter
        resetNumpad();
        inNumpadInput = false;
    }
}

void OperationManager::startTouchOffX() {
    // Store current motor position
    touchOffX = motionControl->getAxisPosition(AXIS_X);
    
    // Enter touch-off state
    currentState = STATE_TOUCHOFF_X;
    touchOffAxis = 0;
    
    // Initialize numpad for value entry
    numpadIndex = 0;
    for (int i = 0; i < 20; i++) {
        numpadDigits[i] = 0;
    }
    inNumpadInput = true;  // Enable numpad input
}

void OperationManager::startTouchOffZ() {
    // Store current motor position
    touchOffZ = motionControl->getAxisPosition(AXIS_Z);
    
    // Enter touch-off state
    currentState = STATE_TOUCHOFF_Z;
    touchOffAxis = 1;
    
    // Initialize numpad for value entry
    numpadIndex = 0;
    for (int i = 0; i < 20; i++) {
        numpadDigits[i] = 0;
    }
    inNumpadInput = true;  // Enable numpad input
}

void OperationManager::clearTouchOff() {
    touchOffX = 0;
    touchOffZ = 0;
    touchOffXCoord = 0.0f;
    touchOffZCoord = 0.0f;
    touchOffXValid = false;
    touchOffZValid = false;
}

void OperationManager::handleNumpadInput(char digit) {
    // Convert char digit to integer and use h5.ino numpad system
    if (digit >= '0' && digit <= '9') {
        // Enable numpad input for turn mode passes entry (setupIndex 1 and 4)
        if (currentMode == MODE_TURN && (setupIndex == 1 || setupIndex == 4) && !inNumpadInput) {
            inNumpadInput = true;
            numpadIndex = 0;
        }
        
        // Enable numpad input for threading mode starts and passes entry (setupIndex 1, 4, and 5)
        if (currentMode == MODE_THREAD && (setupIndex == 1 || setupIndex == 4 || setupIndex == 5) && !inNumpadInput) {
            inNumpadInput = true;
            numpadIndex = 0;
        }
        
        if (!inNumpadInput) return;
        numpadPress(digit - '0');
    }
    // Note: decimal point and minus signs are handled by fixed positioning in h5.ino style
}

void OperationManager::handleNumpadBackspace() {
    if (!inNumpadInput) return;
    numpadBackspace();  // Use h5.ino-style backspace
}

void OperationManager::confirmTouchOffValue() {
    if (!inNumpadInput || numpadIndex == 0) return;
    
    // Convert numpad result to millimeters using h5.ino conversion
    long deciMicrons = numpadToDeciMicrons();
    float value = deciMicrons / 10000.0f;  // Convert deci-microns to mm
    
    if (currentState == STATE_TOUCHOFF_X) {
        touchOffXCoord = value;  // Store as diameter
        touchOffXValid = true;
        resetNumpad();
        inNumpadInput = false;
        currentState = STATE_IDLE;
        
        // Update pitch based on touch-off diameter for intelligent defaults
        updatePitchFromTouchOffDiameter();
        
        // Don't auto-advance - let the user press ENTER to advance
    } else if (currentState == STATE_TOUCHOFF_Z) {
        touchOffZCoord = value;  // Store as position
        touchOffZValid = true;
        resetNumpad();
        inNumpadInput = false;
        currentState = STATE_IDLE;
        
        // Don't auto-advance - let the user press ENTER to advance
    }
}

// h5.ino-style parameter entry functions
void OperationManager::startParameterEntry(OperationState parameterState) {
    if (parameterState == STATE_TARGET_DIAMETER || parameterState == STATE_TARGET_LENGTH || 
        parameterState == STATE_SETUP_PASSES || parameterState == STATE_SETUP_CONE) {
        currentState = parameterState;
        setArrowKeyMode(ARROW_NAVIGATION_MODE);  // Disable motion during entry
        resetNumpad();
        inNumpadInput = true;
    }
}

void OperationManager::confirmParameterValue() {
    if (!inNumpadInput || numpadIndex == 0) return;
    
    switch (currentState) {
        case STATE_TARGET_LENGTH:
            if (currentMode == MODE_FACE) {
                setCutDepthFromNumpad();
            } else {
                setCutLengthFromNumpad();
            }
            break;
            
        case STATE_SETUP_PASSES:
            setNumPassesFromNumpad();
            break;
            
        case STATE_SETUP_STARTS:
            setStartsFromNumpad();
            break;
            
        case STATE_SETUP_CONE:
            setConeRatioFromNumpad();
            break;
            
        default:
            break;
    }
    
    resetNumpad();
    nextSetupStep(); // Advance to next setup state
}

bool OperationManager::isInParameterEntry() const {
    return inNumpadInput && (currentState == STATE_TARGET_DIAMETER ||
                           currentState == STATE_TARGET_LENGTH || 
                           currentState == STATE_SETUP_PASSES || 
                           currentState == STATE_SETUP_STARTS ||
                           currentState == STATE_SETUP_CONE ||
                           currentState == STATE_TOUCHOFF_X ||
                           currentState == STATE_TOUCHOFF_Z);
}

void OperationManager::setCutLengthFromNumpad() {
    long deciMicrons = numpadToDeciMicrons();
    float mm = deciMicrons / 10000.0f;
    setCutLength(mm);
}

void OperationManager::setCutDepthFromNumpad() {
    long deciMicrons = numpadToDeciMicrons();
    float mm = deciMicrons / 10000.0f;
    setCutDepth(mm);
}

void OperationManager::setNumPassesFromNumpad() {
    long passes = getNumpadResult();
    setNumPasses(passes);
}

void OperationManager::setConeRatioFromNumpad() {
    // Cone ratio is entered as a decimal value (e.g., 12500 = 1.2500)
    long result = getNumpadResult();
    float ratio = result / 10000.0f;  // Convert to decimal (4 decimal places)
    setConeRatio(ratio);
}

void OperationManager::setStartsFromNumpad() {
    // Number of starts is entered as a simple integer
    long result = getNumpadResult();
    int starts = max(1, min((int)result, 99)); // Limit to reasonable range (1-99)
    if (motionControl) {
        motionControl->setStarts(starts);
    }
}

void OperationManager::setCutLength(float mm) {
    cutLength = mmToSteps(mm, AXIS_Z);
}

void OperationManager::setCutDepth(float mm) {
    cutDepth = mmToSteps(mm, AXIS_X);
}

void OperationManager::setNumPasses(int passes) {
    numPasses = max(1, min(passes, 999)); // Limit to reasonable range
}

void OperationManager::setConeRatio(float ratio) {
    coneRatio = ratio;
}


void OperationManager::nextSetupStep() {
    // Touch-off states handle their own progression in confirmTouchOffValue()
    // Don't skip them here, let them advance properly
    
    // If in parameter entry, just return - parameters advance setup automatically
    if (inNumpadInput && (currentState == STATE_TARGET_LENGTH || 
                         currentState == STATE_SETUP_PASSES || 
                         currentState == STATE_SETUP_STARTS ||
                         currentState == STATE_SETUP_CONE)) {
        return;
    }
    
    switch (currentMode) {
        case MODE_NORMAL:
            // No setup needed
            currentState = STATE_READY;
            break;
            
        case MODE_TURN:
        case MODE_FACE:
        case MODE_CUT:
            switch (currentState) {
                case STATE_IDLE:
                    // Check if touch-off is complete before advancing
                    if (!hasTouchOff()) {
                        return;  // Can't advance without touch-off
                    }
                    startParameterEntry(STATE_TARGET_LENGTH);  // Start numpad entry
                    break;
                case STATE_TARGET_LENGTH:
                    startParameterEntry(STATE_SETUP_PASSES);  // Start numpad entry
                    break;
                case STATE_SETUP_PASSES:
                    currentState = STATE_READY;
                    break;
                default:
                    break;
            }
            break;
            
        case MODE_THREAD:
            switch (currentState) {
                case STATE_IDLE:
                    // Check if touch-off is complete before advancing
                    if (!hasTouchOff()) {
                        return;  // Can't advance without touch-off
                    }
                    startParameterEntry(STATE_TARGET_LENGTH);  // Start numpad entry
                    break;
                case STATE_TARGET_LENGTH:
                    startParameterEntry(STATE_SETUP_STARTS);   // Start numpad entry for starts
                    break;
                case STATE_SETUP_STARTS:
                    startParameterEntry(STATE_SETUP_PASSES);   // Start numpad entry for passes
                    break;
                case STATE_SETUP_PASSES:
                    startParameterEntry(STATE_SETUP_CONE);     // Start numpad entry for cone ratio
                    break;
                case STATE_SETUP_CONE:
                    currentState = STATE_READY;
                    break;
                default:
                    break;
            }
            break;
            
        case MODE_CONE:
            switch (currentState) {
                case STATE_IDLE:
                    // Check if touch-off is complete before advancing
                    if (!hasTouchOff()) {
                        return;  // Can't advance without touch-off
                    }
                    startParameterEntry(STATE_SETUP_CONE);    // Start numpad entry
                    break;
                case STATE_SETUP_CONE:
                    currentState = STATE_READY;
                    break;
                default:
                    break;
            }
            break;
    }
}

void OperationManager::previousSetupStep() {
    switch (currentState) {
        case STATE_READY:
            if (currentMode == MODE_THREAD) {
                currentState = STATE_SETUP_CONE;
            } else if (currentMode == MODE_CONE) {
                currentState = STATE_SETUP_CONE;
            } else {
                currentState = STATE_SETUP_PASSES;
            }
            break;
        case STATE_SETUP_CONE:
            if (currentMode == MODE_THREAD) {
                currentState = STATE_SETUP_PASSES;
            } else {
                currentState = STATE_SETUP_PASSES;
            }
            break;
        case STATE_SETUP_PASSES:
            if (currentMode == MODE_THREAD) {
                currentState = STATE_SETUP_STARTS;
            } else {
                currentState = STATE_TARGET_LENGTH;
            }
            break;
        case STATE_SETUP_STARTS:
            currentState = STATE_TARGET_LENGTH;
            break;
        case STATE_TARGET_LENGTH:
            currentState = STATE_IDLE;
            break;
        default:
            break;
    }
}

bool OperationManager::startOperation() {
    if (!motionControl || !hasTouchOff() || currentState != STATE_READY) {
        return false;
    }
    
    // Calculate cut parameters from targets and touch-off
    calculateOperationParameters();
    
    // Check required parameters
    if (currentMode != MODE_NORMAL && currentMode != MODE_CONE) {
        if (cutLength == 0 || cutDepth == 0) {
            return false;
        }
    }
    
    // Initialize operation
    currentState = STATE_RUNNING;
    passSubState = SUBSTATE_MOVE_TO_START;
    currentPass = 0;
    
    // Get the absolute pitch value
    long absPitch = abs(motionControl->getDupr());
    
    // Determine direction based on operation settings
    if (currentMode == MODE_TURN || currentMode == MODE_FACE || currentMode == MODE_CUT) {
        // For turning/facing/cutoff: direction from operation settings
        opDuprSign = isLeftToRight ? 1 : -1;
        // Set the pitch with the correct sign for motion control
        motionControl->setThreadPitch(absPitch * opDuprSign);
    } else if (currentMode == MODE_THREAD) {
        // For threading: direction determines thread handedness
        // L→R = right-hand thread (positive), R→L = left-hand thread (negative)
        // This matches the physics: spindle rotation direction is constant,
        // cutting direction determines which way the thread spirals
        opDuprSign = isLeftToRight ? 1 : -1;
        // Set the pitch with the correct sign for threading
        motionControl->setThreadPitch(absPitch * opDuprSign);
    } else {
        opDuprSign = 1; // Default
    }
    
    // Enable spindle sync for all cutting operations
    if (currentMode == MODE_TURN || currentMode == MODE_FACE || 
        currentMode == MODE_THREAD || currentMode == MODE_CUT || 
        currentMode == MODE_CONE) {
        motionControl->startThreading(); // Enable spindle sync
    }
    
    // Save the absolute pitch for display
    opDupr = absPitch;
    
    // Calculate start offset for multi-start threads
    int starts = motionControl->getStarts();
    startOffset = (starts == 1) ? 0 : round((ENCODER_PPR * 2.0f) / starts);
    
    // Mark current spindle position for sync
    spindleSyncPos = motionControl->getSpindlePosition();
    
    return true;
}

void OperationManager::stopOperation() {
    currentState = STATE_IDLE;
    currentPass = 0;
    passSubState = SUBSTATE_MOVE_TO_START;
    
    // Stop all axis movement
    if (motionControl) {
        motionControl->setTargetPosition(AXIS_X, motionControl->getAxisPosition(AXIS_X));
        motionControl->setTargetPosition(AXIS_Z, motionControl->getAxisPosition(AXIS_Z));
        
        // Disable spindle sync
        motionControl->stopThreading();
    }
    
    // Re-enable manual movement when operation stops
    setArrowKeyMode(ARROW_MOTION_MODE);
}

void OperationManager::pauseOperation() {
    // TODO: Implement pause functionality
}

void OperationManager::resumeOperation() {
    // TODO: Implement resume functionality
}

void OperationManager::advancePass() {
    if (currentState == STATE_RUNNING && currentPass < numPasses - 1) {
        currentPass++;
        passSubState = SUBSTATE_MOVE_TO_START;
    }
}

bool OperationManager::moveToStartPosition() {
    if (!motionControl) return false;
    
    // Calculate starting positions based on touch-off coordinates
    long startX, startZ;
    
    switch (currentMode) {
        case MODE_TURN:
        case MODE_THREAD:
            // Start at touch-off diameter and Z position
            startX = touchOffX;  // At touch-off diameter
            startZ = touchOffZ;  // At touch-off Z position
            break;
            
        case MODE_FACE:
            // Start at parking position if set, otherwise at touch-off
            if (parkingPositionSet) {
                startX = parkingPositionX;
                startZ = parkingPositionZ;
            } else {
                startX = touchOffX;
                startZ = touchOffZ;
            }
            break;
            
        case MODE_CUT:
            // Start at touch-off diameter, ready to plunge
            startX = touchOffX;
            startZ = touchOffZ;
            break;
            
        default:
            startX = touchOffX;
            startZ = touchOffZ;
            break;
    }
    
    // Move to start position
    motionControl->setTargetPosition(AXIS_X, startX);
    motionControl->setTargetPosition(AXIS_Z, startZ);
    
    // Check if we've reached the position
    if (abs(motionControl->getAxisPosition(AXIS_X) - startX) < 5 &&
        abs(motionControl->getAxisPosition(AXIS_Z) - startZ) < 5) {
        return true;
    }
    
    return false;
}

bool OperationManager::waitForSpindleSync() {
    if (!motionControl) return false;
    
    // h5.ino style: Just proceed immediately when spindlePosSync == 0
    // The spindle sync happens in real-time during cutting, not as a wait state
    return true;
}

bool OperationManager::performCuttingPass() {
    if (!motionControl) return false;
    
    // Calculate pass depth (incremental for each pass)
    long currentDepth = (cutDepth * (currentPass + 1)) / numPasses;
    
    // Calculate target positions based on touch-off coordinates
    long targetX, targetZ;
    
    switch (currentMode) {
        case MODE_TURN:
        case MODE_THREAD:
            {
                // Calculate target diameter based on pass depth
                float depthMm = stepsToMm(currentDepth, AXIS_X);
                float targetDiameter = touchOffXCoord - 2.0f * depthMm;  // Remove material on diameter
                
                // Convert diameter to motor position (X axis moves radially, so divide by 2)
                targetX = touchOffX + mmToSteps((targetDiameter - touchOffXCoord) / 2.0f, AXIS_X);
                motionControl->setTargetPosition(AXIS_X, targetX);
                
                // h5.ino style: Set Z target based on current spindle position
                long spindlePos = motionControl->getSpindlePosition();
                long targetZFromSpindle = motionControl->positionFromSpindle(AXIS_Z, spindlePos);
                
                motionControl->setTargetPosition(AXIS_Z, targetZFromSpindle);
                
                // Track movement for completion checking
                long deltaZ = targetZFromSpindle - motionControl->positionFromSpindle(AXIS_Z, spindleSyncPos);
                
                // Check if we've reached the cut length
                // For turning operations, check actual Z position vs target
                long actualZMovement = motionControl->getPosition(AXIS_Z) - touchOffZ;
                if (abs(actualZMovement) >= abs(cutLength)) {
                    return true;
                }
            }
            break;
            
        case MODE_FACE:
            {
                // Calculate Z position based on pass depth
                float depthMm = stepsToMm(currentDepth, AXIS_Z);
                float targetZCoord = touchOffZCoord - depthMm;  // Move into material
                targetZ = touchOffZ + mmToSteps(targetZCoord - touchOffZCoord, AXIS_Z);
                
                // Calculate X movement (from touch-off diameter towards center)
                float targetDiameter = touchOffXCoord - 2.0f * stepsToMm(cutLength, AXIS_X);
                targetX = touchOffX + mmToSteps((targetDiameter - touchOffXCoord) / 2.0f, AXIS_X);
                
                motionControl->setTargetPosition(AXIS_Z, targetZ);
                motionControl->setTargetPosition(AXIS_X, targetX);
                
                // Check if we've reached the target diameter
                if (abs(motionControl->getAxisPosition(AXIS_X) - targetX) < 5) {
                    return true;
                }
            }
            break;
            
        case MODE_CUT:
            {
                // X follows spindle for cut-off (plunging towards center)
                long spindlePos = motionControl->getSpindlePosition();
                long targetXFromSpindle = motionControl->positionFromSpindle(AXIS_X, spindlePos);
                long deltaX = targetXFromSpindle - motionControl->positionFromSpindle(AXIS_X, spindleSyncPos);
                
                // Calculate target diameter (moving towards center)
                float deltaXMm = stepsToMm(deltaX, AXIS_X);
                float targetDiameter = touchOffXCoord - 2.0f * deltaXMm;
                targetX = touchOffX + mmToSteps((targetDiameter - touchOffXCoord) / 2.0f, AXIS_X);
                
                // Limit to cut depth (final diameter)
                float finalDiameter = touchOffXCoord - 2.0f * stepsToMm(cutDepth, AXIS_X);
                long finalX = touchOffX + mmToSteps((finalDiameter - touchOffXCoord) / 2.0f, AXIS_X);
                
                if ((cutDepth > 0 && targetX > finalX) || (cutDepth < 0 && targetX < finalX)) {
                    targetX = finalX;
                }
                
                motionControl->setTargetPosition(AXIS_X, targetX);
                
                // Check if we've reached the final diameter
                if (targetX == finalX) {
                    return true;
                }
            }
            break;
            
        case MODE_CONE:
            {
                // Both axes follow spindle with cone ratio
                long spindlePos = motionControl->getSpindlePosition();
                long targetZFromSpindle = motionControl->positionFromSpindle(AXIS_Z, spindlePos);
                long deltaZ = targetZFromSpindle - motionControl->positionFromSpindle(AXIS_Z, spindleSyncPos);
                targetZ = touchOffZ + deltaZ;
                
                // Calculate X movement based on Z movement and cone ratio
                float zMovementMm = stepsToMm(deltaZ, AXIS_Z);
                float diameterChange = zMovementMm * coneRatio * 2.0f;  // Cone ratio is per radius, multiply by 2 for diameter
                float targetDiameter = touchOffXCoord + diameterChange;
                targetX = touchOffX + mmToSteps((targetDiameter - touchOffXCoord) / 2.0f, AXIS_X);
                
                motionControl->setTargetPosition(AXIS_Z, targetZ);
                motionControl->setTargetPosition(AXIS_X, targetX);
            }
            break;
    }
    
    return false;
}

bool OperationManager::retractTool() {
    if (!motionControl) return false;
    
    // Move to parking position if set, otherwise retract to touch-off position
    long safeX = parkingPositionSet ? parkingPositionX : touchOffX;
    
    motionControl->setTargetPosition(AXIS_X, safeX);
    
    // Check if we've reached safe position
    return (abs(motionControl->getAxisPosition(AXIS_X) - safeX) < 5);
}

bool OperationManager::returnToStart() {
    if (!motionControl) return false;
    
    // Return Z to start position
    motionControl->setTargetPosition(AXIS_Z, touchOffZ);
    
    // Check if we've returned
    return (abs(motionControl->getAxisPosition(AXIS_Z) - touchOffZ) < 5);
}

void OperationManager::update() {
    if (!motionControl || currentState != STATE_RUNNING) {
        return;
    }
    
    // Execute based on current mode
    switch (currentMode) {
        case MODE_NORMAL:
            executeNormalMode();
            break;
        case MODE_TURN:
            executeTurnMode();
            break;
        case MODE_FACE:
            executeFaceMode();
            break;
        case MODE_THREAD:
            executeThreadMode();
            break;
        case MODE_CONE:
            executeConeMode();
            break;
        case MODE_CUT:
            executeCutMode();
            break;
        case MODE_ASYNC:
            executeAsyncMode();
            break;
        case MODE_ELLIPSE:
            executeEllipseMode();
            break;
        case MODE_GCODE:
            executeGcodeMode();
            break;
    }
}

void OperationManager::executeNormalMode() {
    // Normal gearbox mode - Z follows spindle
    long spindlePos = motionControl->getSpindlePosition();
    long targetZ = motionControl->positionFromSpindle(AXIS_Z, spindlePos);
    motionControl->setTargetPosition(AXIS_Z, targetZ);
}

void OperationManager::executeTurnMode() {
    switch (passSubState) {
        case SUBSTATE_MOVE_TO_START:
            if (moveToStartPosition()) {
                passSubState = SUBSTATE_SYNC_SPINDLE;
            }
            break;
            
        case SUBSTATE_SYNC_SPINDLE:
            if (waitForSpindleSync()) {
                // Reset spindle reference for this pass
                spindleSyncPos = motionControl->getSpindlePosition();
                passSubState = SUBSTATE_CUTTING;
            }
            break;
            
        case SUBSTATE_CUTTING:
            if (performCuttingPass()) {
                passSubState = SUBSTATE_RETRACTING;
            }
            break;
            
        case SUBSTATE_RETRACTING:
            if (retractTool()) {
                passSubState = SUBSTATE_RETURNING;
            }
            break;
            
        case SUBSTATE_RETURNING:
            if (returnToStart()) {
                if (currentPass < numPasses - 1) {
                    currentPass++;
                    passSubState = SUBSTATE_MOVE_TO_START;
                } else {
                    // Operation complete
                    stopOperation();
                }
            }
            break;
    }
}

void OperationManager::executeFaceMode() {
    // Similar to turn mode but with axes swapped
    executeTurnMode();
}

void OperationManager::executeThreadMode() {
    // Same as turn mode but with cone ratio applied
    executeTurnMode();
}

void OperationManager::executeConeMode() {
    // Continuous cone cutting
    performCuttingPass();
}

void OperationManager::executeCutMode() {
    switch (passSubState) {
        case SUBSTATE_MOVE_TO_START:
            if (moveToStartPosition()) {
                passSubState = SUBSTATE_SYNC_SPINDLE;
            }
            break;
            
        case SUBSTATE_SYNC_SPINDLE:
            // Set spindle sync position
            spindleSyncPos = motionControl->getSpindlePosition();
            passSubState = SUBSTATE_CUTTING;
            break;
            
        case SUBSTATE_CUTTING:
            if (performCuttingPass()) {
                passSubState = SUBSTATE_RETURNING;
            }
            break;
            
        case SUBSTATE_RETURNING:
            // Retract X axis
            motionControl->setTargetPosition(AXIS_X, touchOffX);
            if (abs(motionControl->getAxisPosition(AXIS_X) - touchOffX) < 5) {
                if (currentPass < numPasses - 1) {
                    currentPass++;
                    passSubState = SUBSTATE_MOVE_TO_START;
                } else {
                    stopOperation();
                }
            }
            break;
    }
}

String OperationManager::getStatusText() {
    switch (currentState) {
        case STATE_IDLE:
            switch (currentMode) {
                case MODE_NORMAL: return "GEAR OFF";
                case MODE_TURN: return "TURN OFF";
                case MODE_FACE: return "FACE OFF";
                case MODE_THREAD: return "THRD OFF";
                case MODE_CONE: return "CONE OFF";
                case MODE_CUT: return "CUT OFF";
                case MODE_ASYNC: return "ASYNC OFF";
                case MODE_ELLIPSE: return "ELLI OFF";
                case MODE_GCODE: return "GCODE OFF";
            }
            break;
            
        case STATE_DIRECTION_SETUP:
            return "Direction";  // 9 chars
            
        case STATE_TOUCHOFF_X:
            return "Touch X";
            
        case STATE_TOUCHOFF_Z:
            return "Touch Z";
            
        case STATE_PARKING_SETUP:
            return "Parking";  // 7 chars
            
        case STATE_TARGET_DIAMETER:
            return "Target X";  // 8 chars
            
        case STATE_TARGET_LENGTH:
            return "Target L";  // 8 chars
            
        case STATE_SETUP_PASSES:
            return "Set passes";
            
        case STATE_SETUP_CONE:
            return "Set ratio";
            
        case STATE_READY:
            return "Ready";
            
        case STATE_RUNNING:
            {
                String status = "";
                switch (currentMode) {
                    case MODE_NORMAL: status = "GEAR ON"; break;
                    case MODE_TURN: status = "TURN ON"; break;
                    case MODE_FACE: status = "FACE ON"; break;
                    case MODE_THREAD: status = "THRD ON"; break;
                    case MODE_CONE: status = "CONE ON"; break;
                    case MODE_CUT: status = "CUT ON"; break;
                    case MODE_ASYNC: status = "ASYNC ON"; break;
                    case MODE_ELLIPSE: status = "ELLI ON"; break;
                    case MODE_GCODE: status = "GCODE ON"; break;
                }
                
                if (numPasses > 1) {
                    status += " " + String(currentPass + 1) + "/" + String(numPasses);
                }
                return status;
            }
            
        default:
            return "Unknown";
    }
}

String OperationManager::getPromptText() {
    // Turn mode workflow as per Forkflow-example-turning.md
    if (currentMode == MODE_TURN) {
        switch (setupIndex) {
            case 0:  // Step 1: Direction selection
                {
                    String direction = isLeftToRight ? "L→R" : "R→L";
                    String type = isInternalOperation ? "INT" : "EXT";
                    return direction + " " + type + " ←→↑↓";
                }
            
            case 1:  // Step 2: Touch-off both axes (any order)
                if (currentState == STATE_TOUCHOFF_X || currentState == STATE_TOUCHOFF_Z) {
                    return getTouchOffDisplayText();
                }
                if (touchOffXValid && touchOffZValid) {
                    // Both values confirmed, show them and prompt to continue
                    return "X" + formatDeciMicrons(touchOffXCoord * 10000, 1) + " Z" + formatDeciMicrons(touchOffZCoord * 10000, 1);
                }
                return "Touch off";
            
            case 2:  // Step 3: Move to parking position
                return "Move to parking pos";
            
            case 3:  // Step 4: Set target diameter and length
                if (currentState == STATE_TARGET_DIAMETER) {
                    if (inNumpadInput && numpadIndex > 0) {
                        return "X:" + getNumpadDisplayText();
                    }
                    return "Target X (final)";
                } else if (currentState == STATE_TARGET_LENGTH) {
                    if (inNumpadInput && numpadIndex > 0) {
                        return "L:" + getNumpadDisplayText();
                    }
                    return "Cut length";
                } else {
                    // Show current values if set
                    if (targetDiameter > 0 && targetZLength > 0) {
                        return "X" + formatDeciMicrons(targetDiameter, 1) + " L" + formatDeciMicrons(targetZLength, 1);
                    } else if (targetDiameter > 0) {
                        return "X" + formatDeciMicrons(targetDiameter, 1) + " →L?";
                    } else {
                        return "Set target X & L";
                    }
                }
            
            case 4:  // Step 5: Set number of passes
                if (inNumpadInput && numpadIndex > 0) {
                    long passes = getNumpadResult();
                    return String(passes) + " passes";
                }
                return String(numPasses) + " passes";
            
            case 5:  // Step 6: Sanity check and ready
                {
                    // Show summary of operation parameters
                    if (targetDiameter > 0 && targetZLength > 0) {
                        // Calculate operation type indicator
                        String opType = (isInternalOperation ? "INT" : "EXT") + String(" ") + (isLeftToRight ? "L→R" : "R→L");
                        // Show final diameter and passes
                        return opType + " X" + formatDeciMicrons(targetDiameter, 0) + " GO?";
                    }
                    return "Ready? Press ENTER";
                }
            
            default:
                return "Ready";
        }
    }
    
    // Threading mode workflow - exact copy of turning but with starts prompt
    if (currentMode == MODE_THREAD) {
        switch (setupIndex) {
            case 0:  // Step 1: Direction selection
                {
                    String direction = isLeftToRight ? "L→R" : "R→L";
                    String type = isInternalOperation ? "INT" : "EXT";
                    return direction + " " + type + " ←→↑↓";
                }
            
            case 1:  // Step 2: Touch-off both axes (any order)
                if (currentState == STATE_TOUCHOFF_X || currentState == STATE_TOUCHOFF_Z) {
                    return getTouchOffDisplayText();
                }
                if (touchOffXValid && touchOffZValid) {
                    // Both values confirmed, show them and prompt to continue
                    return "X" + formatDeciMicrons(touchOffXCoord * 10000, 1) + " Z" + formatDeciMicrons(touchOffZCoord * 10000, 1);
                }
                return "Touch off";
            
            case 2:  // Step 3: Move to parking position
                return "Move to parking pos";
            
            case 3:  // Step 4: Set target diameter and length
                if (currentState == STATE_TARGET_DIAMETER) {
                    if (inNumpadInput && numpadIndex > 0) {
                        return "X:" + getNumpadDisplayText();
                    }
                    return "Target X (final)";
                } else if (currentState == STATE_TARGET_LENGTH) {
                    if (inNumpadInput && numpadIndex > 0) {
                        return "L:" + getNumpadDisplayText();
                    }
                    return "Cut length";
                } else {
                    // Show current values if set
                    if (targetDiameter > 0 && targetZLength > 0) {
                        return "X" + formatDeciMicrons(targetDiameter, 1) + " L" + formatDeciMicrons(targetZLength, 1);
                    } else if (targetDiameter > 0) {
                        return "X" + formatDeciMicrons(targetDiameter, 1) + " →L?";
                    } else {
                        return "Set target X & L";
                    }
                }
            
            case 4:  // Step 5: Set number of starts
                if (inNumpadInput && numpadIndex > 0) {
                    long starts = getNumpadResult();
                    return String(starts) + " starts";
                }
                return String(motionControl->getStarts()) + " starts";
            
            case 5:  // Step 6: Set number of passes
                if (inNumpadInput && numpadIndex > 0) {
                    long passes = getNumpadResult();
                    return String(passes) + " passes";
                }
                return String(numPasses) + " passes";
            
            default:
                return "Ready";
        }
    }
    
    // Handle other modes
    if (isPassMode()) {
        switch (setupIndex) {
            case 0:  // Step 1: Pass count
                if (inNumpadInput && numpadIndex > 0) {
                    long passes = getNumpadResult();
                    return String(passes) + " passes";
                }
                return String(numPasses) + " passes";
            case 1:  // Step 2: Direction selection
                {
                    String direction = isLeftToRight ? "L→R" : "R→L";
                    String type = isInternalOperation ? "INT" : "EXT";
                    return direction + " " + type + " ←→↑↓";
                }
            case 2:  // Step 3: Touch-off both axes (any order)
                if (currentState == STATE_TOUCHOFF_X || currentState == STATE_TOUCHOFF_Z) {
                    return getTouchOffDisplayText();
                }
                if (touchOffXValid && touchOffZValid) {
                    // Both values confirmed, show them and prompt to continue
                    return "X" + formatDeciMicrons(touchOffXCoord * 10000, 1) + " Z" + formatDeciMicrons(touchOffZCoord * 10000, 1);
                }
                return "Touch off";
            case 3:  // Step 4: Move to parking position
                return "Move to parking pos";
            case 4:  // Step 5: Set target diameter and length
                if (currentState == STATE_TARGET_DIAMETER) {
                    if (inNumpadInput && numpadIndex > 0) {
                        return "X:" + getNumpadDisplayText();
                    }
                    return "Target X (final)";
                } else if (currentState == STATE_TARGET_LENGTH) {
                    if (inNumpadInput && numpadIndex > 0) {
                        return "L:" + getNumpadDisplayText();
                    }
                    return "Cut length";
                } else {
                    // Show current values if set
                    if (targetDiameter > 0 && targetZLength > 0) {
                        return "X" + formatDeciMicrons(targetDiameter, 1) + " L" + formatDeciMicrons(targetZLength, 1);
                    } else if (targetDiameter > 0) {
                        return "X" + formatDeciMicrons(targetDiameter, 1) + " →L?";
                    } else {
                        return "Set target X & L";
                    }
                }
            default:
                return "Ready";
        }
    }
    
    // Handle cutting parameters mode
    if (currentMode == MODE_CUTTING_PARAMS) {
        return getCuttingParamsPrompt();
    }
    
    // Default for other modes
    return "Ready";
}

float OperationManager::getProgress() {
    if (currentState != STATE_RUNNING || numPasses == 0) {
        return 0.0f;
    }
    
    // Calculate progress based on current pass and position
    float passProgress = 0.0f;
    
    if (currentMode == MODE_TURN || currentMode == MODE_THREAD) {
        long currentZ = motionControl->getAxisPosition(AXIS_Z);
        passProgress = abs(currentZ - touchOffZ) / float(abs(cutLength));
    } else if (currentMode == MODE_FACE) {
        long currentX = motionControl->getAxisPosition(AXIS_X);
        passProgress = abs(currentX - touchOffX) / float(abs(cutLength));
    } else if (currentMode == MODE_CUT) {
        long currentX = motionControl->getAxisPosition(AXIS_X);
        passProgress = abs(currentX - touchOffX) / float(abs(cutDepth));
    }
    
    passProgress = constrain(passProgress, 0.0f, 1.0f);
    
    // Total progress including completed passes
    return (currentPass + passProgress) / float(numPasses);
}

void OperationManager::executeAsyncMode() {
    // TODO: Implement async mode functionality
    // Basic stub - allows manual operation when on
    // In async mode, stepper motor moves continuously based on dupr setting
}

void OperationManager::executeEllipseMode() {
    // TODO: Implement elliptical cutting functionality
    // Requires complex geometry calculations for elliptical path generation
    // Similar to turn mode but with elliptical motion profile
}

void OperationManager::executeGcodeMode() {
    // TODO: Implement G-code execution functionality
    // Requires file management and G-code parser integration
    // Should coordinate with web interface for program selection
}

// ===== NEW SIMPLIFIED VARIABLE PARKING WORKFLOW METHODS =====

// Parking position management
void OperationManager::startParkingSetup() {
    if (currentState == STATE_IDLE || currentState == STATE_DIRECTION_SETUP || 
        currentState == STATE_TOUCHOFF_X || currentState == STATE_TOUCHOFF_Z) {
        currentState = STATE_PARKING_SETUP;
        setArrowKeyMode(ARROW_MOTION_MODE);  // Enable motion for positioning
    }
}

void OperationManager::confirmParkingPosition() {
    if (currentState == STATE_PARKING_SETUP && motionControl) {
        parkingPositionX = motionControl->getAxisPosition(AXIS_X);
        parkingPositionZ = motionControl->getAxisPosition(AXIS_Z);
        parkingPositionSet = true;
        currentState = STATE_IDLE;  // Return to idle after storing
    }
}

void OperationManager::clearParkingPosition() {
    parkingPositionX = 0;
    parkingPositionZ = 0;
    parkingPositionSet = false;
}

void OperationManager::moveToParkingPosition() {
    if (parkingPositionSet && motionControl) {
        motionControl->setTargetPosition(AXIS_X, parkingPositionX);
        motionControl->setTargetPosition(AXIS_Z, parkingPositionZ);
    }
}

// Target value management
void OperationManager::startTargetDiameterEntry() {
    currentState = STATE_TARGET_DIAMETER;
    setArrowKeyMode(ARROW_NAVIGATION_MODE);  // Disable motion during entry
    resetNumpad();
    inNumpadInput = true;
}

void OperationManager::startTargetLengthEntry() {
    currentState = STATE_TARGET_LENGTH;
    setArrowKeyMode(ARROW_NAVIGATION_MODE);  // Disable motion during entry
    resetNumpad();
    inNumpadInput = true;
}

void OperationManager::confirmTargetValue() {
    if (!inNumpadInput || numpadIndex == 0) return;
    
    long deciMicrons = numpadToDeciMicrons();
    
    switch (currentState) {
        case STATE_TARGET_DIAMETER:
            targetDiameter = deciMicrons;
            resetNumpad();
            currentState = STATE_IDLE;
            
            // For turn mode, auto-advance to length entry after diameter
            if (currentMode == MODE_TURN && setupIndex == 3 && !hasTargetLength()) {
                // Auto-advance to length entry
                startTargetLengthEntry();
                return;  // Skip STATE_IDLE since we're starting length entry
            }
            break;
            
        case STATE_TARGET_LENGTH:
            targetZLength = deciMicrons;
            resetNumpad();
            currentState = STATE_IDLE;
            
            // For turn mode, calculate operation parameters
            if (currentMode == MODE_TURN && setupIndex == 3) {
                calculateOperationParameters();
            }
            break;
            
        default:
            break;
    }
    
    // Don't automatically re-enable motion during TURN mode setup
    // Motion will be re-enabled when operation completes or is cancelled
}

void OperationManager::clearTargets() {
    targetDiameter = 0;
    targetZLength = 0;
}

// Clear/cancel functionality
void OperationManager::clearCurrentInput() {
    resetNumpad();
    
    // Return to appropriate state based on current context
    if (currentState == STATE_TARGET_DIAMETER || currentState == STATE_TARGET_LENGTH ||
        currentState == STATE_TOUCHOFF_X || currentState == STATE_TOUCHOFF_Z ||
        currentState == STATE_PARKING_SETUP) {
        currentState = STATE_IDLE;
        
        // Only re-enable motion if not in TURN mode setupIndex 0 (direction selection)
        if (!(currentMode == MODE_TURN && setupIndex == 0)) {
            setArrowKeyMode(ARROW_MOTION_MODE);
        }
    }
}

void OperationManager::cancelOperation() {
    stopOperation();
    clearCurrentInput();
    currentState = STATE_IDLE;
    setArrowKeyMode(ARROW_MOTION_MODE);
}

// Calculate operation parameters from targets and touch-off
void OperationManager::calculateOperationParameters() {
    if (!touchOffXValid || !touchOffZValid || targetDiameter == 0 || targetZLength == 0) {
        return;
    }
    
    // Convert targets to mm
    float targetDiameterMm = targetDiameter / 10000.0f;
    float targetLengthMm = targetZLength / 10000.0f;
    
    // Calculate cut parameters based on direction settings
    if (isInternalOperation) {
        // Internal: diameter increases, move away from centerline
        float diameterChange = targetDiameterMm - touchOffXCoord;
        cutDepth = mmToSteps(abs(diameterChange) / 2.0f, AXIS_X);  // Radial depth
    } else {
        // External: diameter decreases, move toward centerline  
        float diameterChange = touchOffXCoord - targetDiameterMm;
        cutDepth = mmToSteps(abs(diameterChange) / 2.0f, AXIS_X);  // Radial depth
    }
    
    // Target length is the distance to cut
    // For R→L (negative direction), make length negative
    cutLength = mmToSteps(targetLengthMm, AXIS_Z);
    if (!isLeftToRight) {
        cutLength = -cutLength;
    }
}

// Display text methods (≤21 characters as requested)
String OperationManager::getDirectionDisplayText() {
    String text = isInternalOperation ? "Int " : "Ext ";
    text += isLeftToRight ? "L→R" : "R→L";
    return text;  // Max: "Int L→R" = 7 chars, "Ext R→L" = 7 chars
}

String OperationManager::getTouchOffDisplayText() {
    if (currentState == STATE_TOUCHOFF_X) {
        if (inNumpadInput) {
            return "X:" + getNumpadDisplayText();  // "X:" + numpad display
        }
        return "Touch off diameter";  // 18 chars
    } else if (currentState == STATE_TOUCHOFF_Z) {
        if (inNumpadInput) {
            return "Z:" + getNumpadDisplayText();  // "Z:" + numpad display
        }
        return "Touch off face/Z";  // 16 chars
    }
    
    // Status display showing confirmed values
    String status = "";
    if (touchOffXValid) {
        status += "X" + formatDeciMicrons(touchOffXCoord * 10000, 1) + " ";
    }
    if (touchOffZValid) {
        status += "Z" + formatDeciMicrons(touchOffZCoord * 10000, 1);
    }
    return status.length() > 0 ? status : "Touch off X & Z";  // 15 chars
}

String OperationManager::getParkingDisplayText() {
    if (currentState == STATE_PARKING_SETUP) {
        return "Park: arrows+ENTER";  // 19 chars
    }
    return parkingPositionSet ? "Parking ✓" : "No parking";  // 9 or 10 chars
}

String OperationManager::getTargetDisplayText() {
    if (currentState == STATE_TARGET_DIAMETER) {
        if (inNumpadInput && numpadIndex > 0) {
            return "X:" + getNumpadDisplayText();  // "X:" + numpad display
        }
        return "Target diameter";  // 15 chars
    } else if (currentState == STATE_TARGET_LENGTH) {
        if (inNumpadInput && numpadIndex > 0) {
            return "L:" + getNumpadDisplayText();  // "L:" + numpad display
        }
        return "Target length";  // 13 chars
    }
    
    // Status display
    String status = "";
    if (targetDiameter > 0) {
        status += "X" + String(targetDiameter / 10000.0f, 1);
        if (targetZLength > 0) status += " ";
    }
    if (targetZLength > 0) {
        status += "L" + String(targetZLength / 10000.0f, 1);
    }
    
    return status.length() > 0 ? status : "Set targets";  // Variable or 11 chars
}

// Workflow state processors
void OperationManager::processDirectionSetup() {
    // Direction is controlled via toggle methods called by key handler
    // Arrow keys navigate between internal/external and left-to-right options
    // This method handles any automatic advancement if needed
}

void OperationManager::processTouchOffSetup() {
    // Touch-off is handled by startTouchOffX/Z and confirmTouchOffValue methods
    // This method can handle any automatic state transitions
}

void OperationManager::processParkingSetup() {
    // Parking setup is handled by arrow movement + confirmParkingPosition
    // This method handles any automatic state management
}

void OperationManager::processTargetEntry() {
    // Target entry is handled by numpad + confirmTargetValue methods
    // This method handles any validation or state management
    if (targetDiameter > 0 && targetZLength > 0 && 
        touchOffXValid && touchOffZValid) {
        // Auto-calculate operation parameters when we have all data
        calculateOperationParameters();
    }
}

// h5.ino-style setup progression functions
int OperationManager::getLastSetupIndex() const {
    switch (currentMode) {
        case MODE_CONE:
        case MODE_GCODE:
            return 2;
        case MODE_THREAD:
            return 5;  // Threading mode workflow: 6 steps (0-5) - same as turning but with starts prompt
        case MODE_TURN:
        case MODE_FACE:
        case MODE_CUT:
        case MODE_ELLIPSE:
            return 5;  // Turn mode workflow: 6 steps (0-5)
        default:
            return 0;
    }
}

bool OperationManager::isPassMode() const {
    return currentMode == MODE_TURN || currentMode == MODE_FACE || 
           currentMode == MODE_CUT || currentMode == MODE_THREAD || 
           currentMode == MODE_ELLIPSE;
}

bool OperationManager::needsZStops() const {
    return currentMode == MODE_TURN || currentMode == MODE_FACE || 
           currentMode == MODE_THREAD || currentMode == MODE_ELLIPSE;
}

void OperationManager::advanceSetupIndex() {
    if (setupIndex < getLastSetupIndex()) {
        setupIndex++;
        
        // For all pass modes using setupIndex, set STATE_READY when reaching final setup index
        if (isPassMode() && setupIndex == getLastSetupIndex()) {
            currentState = STATE_READY;
        }
    }
}

bool OperationManager::isSetupComplete() const {
    return setupIndex >= getLastSetupIndex();
}

bool OperationManager::isPitchChangeAllowed() const {
    // Pitch changes are not allowed during active threading operations
    if (currentMode == MODE_THREAD && currentState == STATE_RUNNING) {
        return false;
    }
    
    // Pitch changes are allowed in all other cases
    return true;
}

long OperationManager::getDefaultPitchForDiameter(float diameter, int measure) const {
    // Convert diameter to appropriate units for comparison
    float diameterMm = diameter;
    if (measure == MEASURE_INCH) {
        diameterMm = diameter * 25.4f;  // Convert inches to mm
    }
    
    if (measure == MEASURE_METRIC) {
        // ISO Metric Thread Standards (most common pitches by diameter)
        if (diameterMm >= 1.0f && diameterMm < 1.4f) return 250;      // M1.0 - M1.4: 0.25mm pitch
        if (diameterMm >= 1.4f && diameterMm < 2.0f) return 300;      // M1.4 - M2.0: 0.30mm pitch
        if (diameterMm >= 2.0f && diameterMm < 2.5f) return 400;      // M2.0 - M2.5: 0.40mm pitch
        if (diameterMm >= 2.5f && diameterMm < 3.0f) return 450;      // M2.5 - M3.0: 0.45mm pitch
        if (diameterMm >= 3.0f && diameterMm < 4.0f) return 500;      // M3.0 - M4.0: 0.50mm pitch
        if (diameterMm >= 4.0f && diameterMm < 5.0f) return 700;      // M4.0 - M5.0: 0.70mm pitch
        if (diameterMm >= 5.0f && diameterMm < 6.0f) return 800;      // M5.0 - M6.0: 0.80mm pitch
        if (diameterMm >= 6.0f && diameterMm < 8.0f) return 1000;     // M6.0 - M8.0: 1.00mm pitch
        if (diameterMm >= 8.0f && diameterMm < 10.0f) return 1250;    // M8.0 - M10.0: 1.25mm pitch
        if (diameterMm >= 10.0f && diameterMm < 12.0f) return 1500;   // M10.0 - M12.0: 1.50mm pitch
        if (diameterMm >= 12.0f && diameterMm < 16.0f) return 1750;   // M12.0 - M16.0: 1.75mm pitch
        if (diameterMm >= 16.0f && diameterMm < 20.0f) return 2000;   // M16.0 - M20.0: 2.00mm pitch
        if (diameterMm >= 20.0f && diameterMm < 24.0f) return 2500;   // M20.0 - M24.0: 2.50mm pitch
        if (diameterMm >= 24.0f && diameterMm < 30.0f) return 3000;   // M24.0 - M30.0: 3.00mm pitch
        if (diameterMm >= 30.0f && diameterMm < 36.0f) return 3500;   // M30.0 - M36.0: 3.50mm pitch
        if (diameterMm >= 36.0f && diameterMm < 42.0f) return 4000;   // M36.0 - M42.0: 4.00mm pitch
        if (diameterMm >= 42.0f && diameterMm < 48.0f) return 4500;   // M42.0 - M48.0: 4.50mm pitch
        if (diameterMm >= 48.0f && diameterMm < 56.0f) return 5000;   // M48.0 - M56.0: 5.00mm pitch
        if (diameterMm >= 56.0f && diameterMm < 64.0f) return 5500;   // M56.0 - M64.0: 5.50mm pitch
        if (diameterMm >= 64.0f && diameterMm < 72.0f) return 6000;   // M64.0 - M72.0: 6.00mm pitch
        if (diameterMm >= 72.0f && diameterMm < 80.0f) return 6500;   // M72.0 - M80.0: 6.50mm pitch
        if (diameterMm >= 80.0f && diameterMm < 90.0f) return 7000;   // M80.0 - M90.0: 7.00mm pitch
        if (diameterMm >= 90.0f && diameterMm < 100.0f) return 7500;  // M90.0 - M100.0: 7.50mm pitch
        if (diameterMm >= 100.0f) return 8000;                       // M100.0+: 8.00mm pitch
        
        // Default for very small diameters
        return 250;  // 0.25mm pitch default
    }
    
    if (measure == MEASURE_INCH) {
        // Imperial Thread Standards (most common TPI by diameter)
        if (diameter >= 0.25f && diameter < 0.3125f) return 28;       // 1/4" - 5/16": 28 TPI
        if (diameter >= 0.3125f && diameter < 0.375f) return 24;      // 5/16" - 3/8": 24 TPI
        if (diameter >= 0.375f && diameter < 0.4375f) return 20;      // 3/8" - 7/16": 20 TPI
        if (diameter >= 0.4375f && diameter < 0.5f) return 20;        // 7/16" - 1/2": 20 TPI
        if (diameter >= 0.5f && diameter < 0.5625f) return 18;        // 1/2" - 9/16": 18 TPI
        if (diameter >= 0.5625f && diameter < 0.625f) return 18;      // 9/16" - 5/8": 18 TPI
        if (diameter >= 0.625f && diameter < 0.6875f) return 16;      // 5/8" - 11/16": 16 TPI
        if (diameter >= 0.6875f && diameter < 0.75f) return 16;       // 11/16" - 3/4": 16 TPI
        if (diameter >= 0.75f && diameter < 0.8125f) return 14;       // 3/4" - 13/16": 14 TPI
        if (diameter >= 0.8125f && diameter < 0.875f) return 14;      // 13/16" - 7/8": 14 TPI
        if (diameter >= 0.875f && diameter < 0.9375f) return 12;      // 7/8" - 15/16": 12 TPI
        if (diameter >= 0.9375f && diameter < 1.0f) return 12;        // 15/16" - 1": 12 TPI
        if (diameter >= 1.0f && diameter < 1.125f) return 12;         // 1" - 1-1/8": 12 TPI
        if (diameter >= 1.125f && diameter < 1.25f) return 11;        // 1-1/8" - 1-1/4": 11 TPI
        if (diameter >= 1.25f && diameter < 1.375f) return 11;        // 1-1/4" - 1-3/8": 11 TPI
        if (diameter >= 1.375f && diameter < 1.5f) return 10;         // 1-3/8" - 1-1/2": 10 TPI
        if (diameter >= 1.5f && diameter < 1.625f) return 10;         // 1-1/2" - 1-5/8": 10 TPI
        if (diameter >= 1.625f && diameter < 1.75f) return 9;         // 1-5/8" - 1-3/4": 9 TPI
        if (diameter >= 1.75f && diameter < 1.875f) return 9;         // 1-3/4" - 1-7/8": 9 TPI
        if (diameter >= 1.875f && diameter < 2.0f) return 8;          // 1-7/8" - 2": 8 TPI
        if (diameter >= 2.0f && diameter < 2.25f) return 8;           // 2" - 2-1/4": 8 TPI
        if (diameter >= 2.25f && diameter < 2.5f) return 7;           // 2-1/4" - 2-1/2": 7 TPI
        if (diameter >= 2.5f && diameter < 2.75f) return 7;           // 2-1/2" - 2-3/4": 7 TPI
        if (diameter >= 2.75f && diameter < 3.0f) return 6;           // 2-3/4" - 3": 6 TPI
        if (diameter >= 3.0f && diameter < 3.5f) return 6;            // 3" - 3-1/2": 6 TPI
        if (diameter >= 3.5f && diameter < 4.0f) return 5;            // 3-1/2" - 4": 5 TPI
        if (diameter >= 4.0f && diameter < 4.5f) return 5;            // 4" - 4-1/2": 5 TPI
        if (diameter >= 4.5f && diameter < 5.0f) return 4;            // 4-1/2" - 5": 4 TPI
        if (diameter >= 5.0f) return 4;                               // 5"+: 4 TPI
        
        // Default for very small imperial diameters
        return 28;  // 28 TPI default
    }
    
    if (measure == MEASURE_TPI) {
        // TPI mode - same as imperial but with TPI values
        return getDefaultPitchForDiameter(diameter, MEASURE_INCH);
    }
    
    // Default fallback
    return 1000;  // 1.0mm pitch default
}

void OperationManager::updatePitchFromTouchOffDiameter() {
    // Only update pitch if we have a valid touch-off diameter and we're in threading mode
    if (currentMode == MODE_THREAD && touchOffXValid && motionControl) {
        float diameter = touchOffXCoord;  // Touch-off X coordinate is the diameter
        
        // Get intelligent default pitch for this diameter
        long defaultPitch = getDefaultPitchForDiameter(diameter, currentMeasure);
        
        // Only update if pitch is not locked (not during active threading)
        if (isPitchChangeAllowed()) {
            motionControl->setThreadPitch(defaultPitch);
        }
    }
}

// Cutting parameters system implementation
void OperationManager::startCuttingParamsEntry() {
    currentState = STATE_CUTTING_PARAMS_MATERIAL;
    cuttingParamsStep = 0;
    resetNumpad();
    inNumpadInput = false;
}

void OperationManager::setCuttingParamsMaterial(MaterialCategory material) {
    cuttingParamsMaterial = material;
}

void OperationManager::setCuttingParamsTool(ToolType tool) {
    cuttingParamsTool = tool;
}

void OperationManager::setCuttingParamsOperation(OperationType operation) {
    cuttingParamsOperation = operation;
}

void OperationManager::setCuttingParamsDiameter(float diameter) {
    cuttingParamsDiameter = diameter;
}

RPMResult OperationManager::getCuttingParamsResult() const {
    return cuttingParamsResult;
}

void OperationManager::nextCuttingParamsStep() {
    switch (currentState) {
        case STATE_CUTTING_PARAMS_MATERIAL:
            currentState = STATE_CUTTING_PARAMS_TOOL;
            break;
        case STATE_CUTTING_PARAMS_TOOL:
            currentState = STATE_CUTTING_PARAMS_OPERATION;
            break;
        case STATE_CUTTING_PARAMS_OPERATION:
            currentState = STATE_CUTTING_PARAMS_DIAMETER;
            resetNumpad();
            inNumpadInput = true;
            break;
        case STATE_CUTTING_PARAMS_DIAMETER:
            if (inNumpadInput && numpadIndex > 0) {
                // Convert numpad result using standard pattern (like all other inputs)
                long diameterDeciMicrons = numpadToDeciMicrons();
                // Convert deci-microns to mm for cutting parameters
                float diameter = diameterDeciMicrons / 10000.0f; // Convert deci-microns to mm
                setCuttingParamsDiameter(diameter);
                resetNumpad();
                inNumpadInput = false;
            }
            currentState = STATE_CUTTING_PARAMS_RESULT;
            // Calculate result
            cuttingParamsResult = cuttingParams.calculateRPM(
                cuttingParamsMaterial, 
                cuttingParamsTool, 
                cuttingParamsOperation, 
                cuttingParamsDiameter, 
                currentMeasure == MEASURE_METRIC
            );
            break;
        case STATE_CUTTING_PARAMS_RESULT:
            // Return to material selection for new calculation
            currentState = STATE_CUTTING_PARAMS_MATERIAL;
            break;
    }
}

void OperationManager::previousCuttingParamsStep() {
    switch (currentState) {
        case STATE_CUTTING_PARAMS_TOOL:
            currentState = STATE_CUTTING_PARAMS_MATERIAL;
            break;
        case STATE_CUTTING_PARAMS_OPERATION:
            currentState = STATE_CUTTING_PARAMS_TOOL;
            break;
        case STATE_CUTTING_PARAMS_DIAMETER:
            currentState = STATE_CUTTING_PARAMS_OPERATION;
            resetNumpad();
            inNumpadInput = false;
            break;
        case STATE_CUTTING_PARAMS_RESULT:
            currentState = STATE_CUTTING_PARAMS_DIAMETER;
            resetNumpad();
            inNumpadInput = true;
            break;
        case STATE_CUTTING_PARAMS_MATERIAL:
            // Stay at material selection
            break;
    }
}

int OperationManager::getCuttingParamsStep() const {
    return cuttingParamsStep;
}

String OperationManager::getCuttingParamsPrompt() const {
    switch (currentState) {
        case STATE_CUTTING_PARAMS_MATERIAL:
            return "Select Material";
        case STATE_CUTTING_PARAMS_TOOL:
            return "Select Tool";
        case STATE_CUTTING_PARAMS_OPERATION:
            return "Select Operation";
        case STATE_CUTTING_PARAMS_DIAMETER:
            if (inNumpadInput && numpadIndex > 0) {
                return "D:" + getNumpadDisplayText();
            }
            return "Enter Diameter";
        case STATE_CUTTING_PARAMS_RESULT:
            if (cuttingParamsResult.isValid) {
                return cuttingParamsResult.recommendation;
            }
            return "Invalid Parameters";
        default:
            return "Cutting Parameters";
    }
}