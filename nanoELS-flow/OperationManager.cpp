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
    , touchOffX(0)
    , touchOffZ(0)
    , touchOffXCoord(0.0f)
    , touchOffZCoord(0.0f)
    , touchOffXValid(false)
    , touchOffZValid(false)
    , inNumpadInput(false)
    , numpadIndex(0)
    , currentMeasure(MEASURE_METRIC)
    , touchOffAxis(0)
    , cutLength(0)
    , cutDepth(0)
    , numPasses(1)
    , coneRatio(0.0f)
    , parkingOffsetX(20000)  // 2mm default
    , parkingOffsetZ(50000)  // 5mm default
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
        result = result * 25.4;  // Convert to deci-microns (inch * 254000 / 10000)
    } else if (currentMeasure == MEASURE_TPI) {
        result = round(254000.0 / result);  // TPI to pitch in deci-microns
    } else { // MEASURE_METRIC
        result = result * 10;  // Convert to deci-microns (mm * 10000 / 1000)
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
    long v = imperial ? round(deciMicrons / 25.4) : deciMicrons;
    int points = 0;
    
    if (v == 0 && precisionPointsMax >= 5) {
        points = 5;
    } else if ((v % 10) != 0 && precisionPointsMax >= 4) {
        points = 4;
    } else if ((v % 100) != 0 && precisionPointsMax >= 3) {
        points = 3;
    } else if ((v % 1000) != 0 && precisionPointsMax >= 2) {
        points = 2;
    } else if ((v % 10000) != 0 && precisionPointsMax >= 1) {
        points = 1;
    }
    
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
    if (!inNumpadInput || numpadIndex == 0) {
        return "";
    }
    
    // Build display string with right-to-left entry
    String display = "";
    for (int i = 0; i < numpadIndex; i++) {
        display += String(numpadDigits[i]);
    }
    
    // Add decimal point based on measurement unit
    if (currentMeasure == MEASURE_METRIC) {
        // Format as X.XXX mm (3 decimal places)
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
    
    // Reset parameters for new mode
    currentPass = 0;
    spindleSyncPos = 0;
}

void OperationManager::startTouchOffX() {
    if (currentState != STATE_IDLE && currentState != STATE_TOUCHOFF_X && currentState != STATE_TOUCHOFF_Z) {
        return;
    }
    
    // Store current motor position
    touchOffX = motionControl->getAxisPosition(AXIS_X);
    
    // Enter touch-off state
    currentState = STATE_TOUCHOFF_X;
    touchOffAxis = 0;
    resetNumpad();  // Initialize h5.ino-style numpad
    inNumpadInput = true;
}

void OperationManager::startTouchOffZ() {
    if (currentState != STATE_IDLE && currentState != STATE_TOUCHOFF_X && currentState != STATE_TOUCHOFF_Z) {
        return;
    }
    
    // Store current motor position
    touchOffZ = motionControl->getAxisPosition(AXIS_Z);
    
    // Enter touch-off state
    currentState = STATE_TOUCHOFF_Z;
    touchOffAxis = 1;
    resetNumpad();  // Initialize h5.ino-style numpad
    inNumpadInput = true;
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
    if (!inNumpadInput) return;
    
    // Convert char digit to integer and use h5.ino numpad system
    if (digit >= '0' && digit <= '9') {
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
        currentState = STATE_IDLE;
    } else if (currentState == STATE_TOUCHOFF_Z) {
        touchOffZCoord = value;  // Store as position
        touchOffZValid = true;
        resetNumpad();
        currentState = STATE_IDLE;
    }
}

// h5.ino-style parameter entry functions
void OperationManager::startParameterEntry(OperationState parameterState) {
    if (parameterState == STATE_SETUP_LENGTH || parameterState == STATE_SETUP_PASSES || 
        parameterState == STATE_SETUP_CONE) {
        currentState = parameterState;
        resetNumpad();
        inNumpadInput = true;
    }
}

void OperationManager::confirmParameterValue() {
    if (!inNumpadInput || numpadIndex == 0) return;
    
    switch (currentState) {
        case STATE_SETUP_LENGTH:
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
    return inNumpadInput && (currentState == STATE_SETUP_LENGTH || 
                           currentState == STATE_SETUP_PASSES || 
                           currentState == STATE_SETUP_CONE);
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

void OperationManager::setParkingOffsetX(float mm) {
    parkingOffsetX = mmToSteps(mm, AXIS_X);
}

void OperationManager::setParkingOffsetZ(float mm) {
    parkingOffsetZ = mmToSteps(mm, AXIS_Z);
}

void OperationManager::nextSetupStep() {
    // Skip touch-off states when advancing
    if (currentState == STATE_TOUCHOFF_X || currentState == STATE_TOUCHOFF_Z) {
        return;
    }
    
    // If in parameter entry, just return - parameters advance setup automatically
    if (inNumpadInput && (currentState == STATE_SETUP_LENGTH || 
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
                    startParameterEntry(STATE_SETUP_LENGTH);  // Start numpad entry
                    break;
                case STATE_SETUP_LENGTH:
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
                    startParameterEntry(STATE_SETUP_LENGTH);  // Start numpad entry
                    break;
                case STATE_SETUP_LENGTH:
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
            currentState = STATE_SETUP_LENGTH;
            break;
        case STATE_SETUP_LENGTH:
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
            // Start at parking offset from touch-off diameter
            {
                float parkingDiameter = touchOffXCoord + 2.0f * stepsToMm(parkingOffsetX, AXIS_X);
                startX = touchOffX + mmToSteps((parkingDiameter - touchOffXCoord) / 2.0f, AXIS_X);
                startZ = touchOffZ;  // At touch-off Z (face) position
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
                float targetDiameter = touchOffXCoord - 2.0f * depthMm;  // Remove material on diameter
                
                // Convert diameter to motor position (X axis moves radially, so divide by 2)
                targetX = touchOffX + mmToSteps((targetDiameter - touchOffXCoord) / 2.0f, AXIS_X);
                motionControl->setTargetPosition(AXIS_X, targetX);
                
                // Use h5.ino-style spindle following for Z
                long spindlePos = motionControl->getSpindlePosition();
                long deltaZ = posFromSpindle(AXIS_Z, spindlePos - spindleSyncPos, true);
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
    
    // Calculate safe retraction position
    long safeX = touchOffX - parkingOffsetX;
    
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
            
        case STATE_TOUCHOFF_X:
            return "Touch X";
            
        case STATE_TOUCHOFF_Z:
            return "Touch Z";
            
        case STATE_SETUP_LENGTH:
            return currentMode == MODE_FACE ? "Set depth" : "Set length";
            
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
    switch (currentState) {
        case STATE_IDLE:
            if (!touchOffXValid && !touchOffZValid) {
                return "Touch off X and Z";
            } else if (!touchOffXValid) {
                return "Touch off X axis";
            } else if (!touchOffZValid) {
                return "Touch off Z axis";
            }
            return "";
            
        case STATE_TOUCHOFF_X:
            if (inNumpadInput && numpadIndex > 0) {
                return "Use " + getNumpadDisplayText() + "?";
            } else {
                return "Enter X diameter";
            }
            
        case STATE_TOUCHOFF_Z:
            if (inNumpadInput && numpadIndex > 0) {
                return "Use " + getNumpadDisplayText() + "?";
            } else {
                return "Enter Z position";
            }
            
        case STATE_SETUP_LENGTH:
            if (inNumpadInput && numpadIndex > 0) {
                return "Use " + getNumpadDisplayText() + "?";
            } else if (inNumpadInput) {
                return currentMode == MODE_FACE ? "Enter depth" : "Enter length";
            } else if (currentMode == MODE_FACE) {
                return "Depth: " + String(stepsToMm(cutDepth, AXIS_Z), 2) + "mm";
            } else {
                return "Length: " + String(stepsToMm(cutLength, AXIS_Z), 2) + "mm";
            }
            
        case STATE_SETUP_PASSES:
            if (inNumpadInput && numpadIndex > 0) {
                long passes = getNumpadResult();
                return "Use " + String(passes) + " passes?";
            } else if (inNumpadInput) {
                return "Enter number of passes";
            } else {
                return "Passes: " + String(numPasses);
            }
            
        case STATE_SETUP_CONE:
            if (inNumpadInput && numpadIndex > 0) {
                float ratio = getNumpadResult() / 10000.0f;
                return "Use ratio " + String(ratio, 4) + "?";
            } else if (inNumpadInput) {
                return "Enter cone ratio";
            } else {
                return "Ratio: " + String(coneRatio, 4);
            }
            
        case STATE_READY:
            {
                String prompt = "Press ON to start";
                if (cutLength != 0 || cutDepth != 0) {
                    prompt = "L:" + String(stepsToMm(cutLength, AXIS_Z), 1) + 
                             " D:" + String(stepsToMm(cutDepth, AXIS_X), 1);
                    if (numPasses > 1) {
                        prompt += " P:" + String(numPasses);
                    }
                }
                return prompt;
            }
            
        default:
            return "";
    }
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