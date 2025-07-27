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
    , numPasses(1)
    , coneRatio(0.0f)
    , currentPass(0)
    , passDepth(0)
    , opDuprSign(1)
    , opDupr(0)
    , spindleSyncPos(0)
    , startOffset(0)
{
    // Initialize numpad digits array
    for (int i = 0; i < 20; i++) {
        numpadDigits[i] = 0;
    }
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
        // Convert screw pitch from deci-microns to microns for h5.ino compatibility
        float screwPitchMicrons = SCREW_Z_DU / 10.0f;
        newPos = (long)(spindlePos * MOTOR_STEPS_Z / screwPitchMicrons / encoderSteps * dupr * starts);
    } else {
        // Convert screw pitch from deci-microns to microns for h5.ino compatibility
        float screwPitchMicrons = SCREW_X_DU / 10.0f;
        newPos = (long)(spindlePos * MOTOR_STEPS_X / screwPitchMicrons / encoderSteps * dupr * starts);
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
        // Convert screw pitch from deci-microns to microns for h5.ino compatibility
        float screwPitchMicrons = SCREW_Z_DU / 10.0f;
        return (long)(pos * screwPitchMicrons * encoderSteps / MOTOR_STEPS_Z / (dupr * starts));
    } else {
        // Convert screw pitch from deci-microns to microns for h5.ino compatibility
        float screwPitchMicrons = SCREW_X_DU / 10.0f;
        return (long)(pos * screwPitchMicrons * encoderSteps / MOTOR_STEPS_X / (dupr * starts));
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

String OperationManager::getNumpadDisplayText() {
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
        
        // Set default feed rate for turning (0.1mm/rev = 1000 deci-microns per revolution)
        // Sign will be adjusted based on direction when operation starts
        if (motionControl) {
            motionControl->setThreadPitch(1000, 1);  // 0.1mm/rev feed rate
        }
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
                    startParameterEntry(STATE_SETUP_PASSES);  // Start numpad entry
                    break;
                case STATE_SETUP_PASSES:
                    startParameterEntry(STATE_SETUP_CONE);    // Start numpad entry
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
            if (currentMode == MODE_THREAD || currentMode == MODE_CONE) {
                currentState = STATE_SETUP_CONE;
            } else {
                currentState = STATE_SETUP_PASSES;
            }
            break;
        case STATE_SETUP_CONE:
            currentState = STATE_SETUP_PASSES;
            break;
        case STATE_SETUP_PASSES:
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
    
    // Save current pitch for consistency
    opDupr = motionControl->getDupr();
    
    // For TURN mode, adjust dupr sign based on direction
    if (currentMode == MODE_TURN && opDupr != 0) {
        // Ensure dupr sign matches the desired direction
        // Right-to-left (default) needs negative dupr, left-to-right needs positive
        if (isLeftToRight && opDupr < 0) {
            opDupr = -opDupr;
            motionControl->setThreadPitch(opDupr, motionControl->getStarts());
        } else if (!isLeftToRight && opDupr > 0) {
            opDupr = -opDupr;
            motionControl->setThreadPitch(opDupr, motionControl->getStarts());
        }
    }
    
    opDuprSign = (opDupr >= 0) ? 1 : -1;
    
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
    
    // Calculate target spindle position for synchronization
    long currentSpindlePos = motionControl->getSpindlePosition();
    long spindleDiff = currentSpindlePos - spindleSyncPos;
    
    // Add start offset for multi-start threads
    if (currentPass > 0 && motionControl->getStarts() > 1) {
        spindleDiff -= startOffset * currentPass;
    }
    
    // Wait for spindle to reach sync position (within 1 encoder count)
    int encoderSteps = ENCODER_PPR * 2;
    spindleDiff = spindleDiff % encoderSteps;
    if (spindleDiff < 0) spindleDiff += encoderSteps;
    
    return (spindleDiff < 1 || spindleDiff > encoderSteps - 1);
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
                float targetDiameter;
                
                if (isInternalOperation) {
                    // Internal: diameter increases
                    targetDiameter = touchOffXCoord + 2.0f * depthMm;
                } else {
                    // External: diameter decreases
                    targetDiameter = touchOffXCoord - 2.0f * depthMm;
                }
                
                // Convert diameter to motor position (X axis moves radially, so divide by 2)
                targetX = touchOffX + mmToSteps((targetDiameter - touchOffXCoord) / 2.0f, AXIS_X);
                motionControl->setTargetPosition(AXIS_X, targetX);
                
                // Use h5.ino-style spindle following for Z
                long spindlePos = motionControl->getSpindlePosition();
                long deltaSpindle = spindlePos - spindleSyncPos;
                
                // Direction is handled by the sign of dupr
                long deltaZ = posFromSpindle(AXIS_Z, deltaSpindle, true);
                targetZ = touchOffZ + deltaZ;
                
                // Apply cone ratio if threading (X movement per Z movement)
                if (currentMode == MODE_THREAD && coneRatio != 0) {
                    float zMovementMm = stepsToMm(deltaZ, AXIS_Z);
                    targetX += mmToSteps(zMovementMm * coneRatio / 2.0f, AXIS_X);  // Radial movement
                }
                
                motionControl->setTargetPosition(AXIS_Z, targetZ);
                
                // Check if we've reached the cut length
                if (abs(deltaZ) >= abs(cutLength)) {
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
                long deltaX = posFromSpindle(AXIS_X, spindlePos - spindleSyncPos, true);
                
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
                long deltaZ = posFromSpindle(AXIS_Z, spindlePos - spindleSyncPos, true);
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
    long targetZ = posFromSpindle(AXIS_Z, spindlePos, true);
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
    
    // Handle other modes
    if (isPassMode()) {
        // Other pass modes can use similar structure
        return "Pass mode setup";
    } else if (currentMode == MODE_CONE) {
        if (inNumpadInput && numpadIndex > 0 && setupIndex == 1) {
            float ratio = getNumpadResult() / 100000.0f;
            return "Use ratio " + String(ratio, 5) + "?";
        } else if (setupIndex == 1) {
            return "Use ratio " + String(coneRatio, 5) + "?";
        } else if (setupIndex == 2) {
            return isInternalOperation ? "Internal?" : "External?";
        } else if (setupIndex >= getLastSetupIndex()) {
            return "Go?";
        }
    }
    
    // Handle numpad input for parameter setup
    if (inNumpadInput && numpadIndex > 0) {
        long deciMicrons = numpadToDeciMicrons();
        return "Use " + formatDeciMicrons(deciMicrons, 5) + "?";
    }
    
    // Fallback to state-based prompts for touch-off and special states
    if (currentState == STATE_TOUCHOFF_X || currentState == STATE_TOUCHOFF_Z) {
        return getTouchOffDisplayText();
    } else if (currentState == STATE_PARKING_SETUP) {
        return getParkingDisplayText();
    } else if (currentState == STATE_TARGET_DIAMETER || currentState == STATE_TARGET_LENGTH) {
        return getTargetDisplayText();
    }
    
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
    
    // Target length is the distance to cut, not a target position
    cutLength = mmToSteps(targetLengthMm, AXIS_Z);
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
            return 4;
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