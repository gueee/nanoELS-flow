#ifndef OPERATION_MANAGER_H
#define OPERATION_MANAGER_H

#include <Arduino.h>

// h5.ino-compatible measurement units
#define MEASURE_METRIC 0
#define MEASURE_INCH 1
#define MEASURE_TPI 2

// h5.ino-compatible move step constants
#define MOVE_STEP_1 10000   // 1mm in deci-microns
#define MOVE_STEP_2 1000    // 0.1mm in deci-microns
#define MOVE_STEP_3 100     // 0.01mm in deci-microns

#define MOVE_STEP_IMP_1 25400   // 1/10" in deci-microns
#define MOVE_STEP_IMP_2 2540    // 1/100" in deci-microns
#define MOVE_STEP_IMP_3 254     // 1/1000" in deci-microns

// Maximum precision allowed
#define DUPR_MAX 254000  // No more than 1 inch pitch

// Operation modes (h5.ino compatible mode structure)
enum OperationMode {
    MODE_NORMAL = 0,    // Normal gearbox mode
    MODE_TURN = 1,      // Turning operation
    MODE_FACE = 2,      // Facing operation
    MODE_THREAD = 3,    // Threading operation
    MODE_CONE = 4,      // Cone cutting
    MODE_CUT = 5,       // Cut-off operation
    MODE_ASYNC = 6,     // Asynchronous mode
    MODE_ELLIPSE = 7,   // Elliptical cutting mode
    MODE_GCODE = 8      // G-code execution mode
};

// Arrow key mode control for safety
enum ArrowKeyMode {
    ARROW_MOTION_MODE,      // Normal mode - arrows control axes movement
    ARROW_NAVIGATION_MODE   // Setup mode - arrows navigate options
};

// Operation state
enum OperationState {
    STATE_IDLE,             // No operation active
    STATE_DIRECTION_SETUP,  // Setting operation direction (internal/external, left-to-right)
    STATE_TOUCHOFF_X,       // Setting X touch-off coordinate
    STATE_TOUCHOFF_Z,       // Setting Z touch-off coordinate
    STATE_PARKING_SETUP,    // Setting parking position
    STATE_TARGET_DIAMETER,  // Setting target diameter via numpad
    STATE_TARGET_LENGTH,    // Setting target length via numpad
    STATE_SETUP_PASSES,     // Setting number of passes
    STATE_SETUP_STARTS,     // Setting number of starts (threading mode)
    STATE_SETUP_CONE,       // Setting cone ratio (thread mode only)
    STATE_READY,            // Ready to start operation
    STATE_RUNNING,          // Operation in progress
    STATE_PARKING,          // Moving to parking position
    STATE_NEXT_PASS         // Preparing next pass
};

// Pass sub-states for multi-pass operations
enum PassSubState {
    SUBSTATE_MOVE_TO_START,   // Moving to starting position
    SUBSTATE_SYNC_SPINDLE,    // Waiting for spindle sync
    SUBSTATE_CUTTING,         // Making the cut
    SUBSTATE_RETRACTING,      // Retracting to safe position
    SUBSTATE_RETURNING        // Returning for next pass
};

class MinimalMotionControl; // Forward declaration

class OperationManager {
private:
    MinimalMotionControl* motionControl;
    
    // Current state
    OperationMode currentMode;
    OperationState currentState;
    PassSubState passSubState;
    
    // h5.ino-style setup progression
    int setupIndex;                    // Current setup step index (0, 1, 2, 3...)
    
    // Touch-off coordinates (user-defined reference points)
    float touchOffXCoord;  // X coordinate value (diameter in mm)
    float touchOffZCoord;  // Z coordinate value (position in mm)
    bool touchOffXValid;
    bool touchOffZValid;
    
    // h5.ino-style numpad input state
    bool inNumpadInput;
    int numpadDigits[20];  // Array to store entered digits
    int numpadIndex;       // Current position in digit array
    int currentMeasure;    // Current measurement unit (MEASURE_METRIC/INCH/TPI)
    int touchOffAxis;      // 0=X, 1=Z
    
    // Arrow key mode control for safety
    ArrowKeyMode arrowKeyMode;
    
    // Direction control for turning operations
    bool isInternalOperation;   // false = external, true = internal (affects X logic)
    bool isLeftToRight;        // false = right-to-left, true = left-to-right (affects Z logic)
    
    // Single touch-off point storage
    long touchOffX;            // Touch-off X position in steps
    long touchOffZ;            // Touch-off Z position in steps
    bool touchOffComplete;     // Whether touch-off has been performed
    
    // User-set parking position storage (no compensation - just store position)
    long parkingPositionX;     // User-set X parking position in steps
    long parkingPositionZ;     // User-set Z parking position in steps
    bool parkingPositionSet;   // Whether parking position has been stored
    
    // Target values entered via numpad
    long targetDiameter;       // Final diameter target in deci-microns
    long targetZLength;        // Cut length target in deci-microns
    
    // Operation parameters (calculated from targets and touch-off)
    long cutLength;     // Z-axis cut length in steps (turning/threading)
    long cutDepth;      // X-axis cut depth in steps (all operations)
    int numPasses;      // Number of passes for the operation
    float coneRatio;    // Cone ratio for threading (X movement per Z movement)
    
    // Current pass tracking
    int currentPass;
    long passDepth;       // Depth for current pass
    int opDuprSign;      // Sign of pitch when operation started
    long opDupr;         // Pitch when operation started
    
    // Synchronization
    long spindleSyncPos;  // Spindle position for synchronization
    int startOffset;      // Multi-start thread offset
    
    // Safe distance for retraction (0.5mm default)
    static const long SAFE_DISTANCE_DU = 5000; // 0.5mm in deci-microns
    
    // Convert between units
    long mmToSteps(float mm, int axis);
    float stepsToMm(long steps, int axis) const;
    
    // h5.ino-style setup progression helpers (moved to public section)
    
    // Spindle synchronization (h5.ino style)
    long posFromSpindle(int axis, long spindlePos, bool respectLimits);
    long spindleFromPos(int axis, long pos);
    
    // Operation execution helpers
    void executeNormalMode();
    void executeTurnMode();
    void executeFaceMode();
    void executeThreadMode();
    void executeConeMode();
    void executeCutMode();
    void executeAsyncMode();
    void executeEllipseMode();
    void executeGcodeMode();
    
    // Multi-pass operation helpers
    bool moveToStartPosition();
    bool waitForSpindleSync();
    bool performCuttingPass();
    bool retractTool();
    bool returnToStart();
    
    // New workflow helper methods
    void processDirectionSetup();       // Handle direction setup state
    void processTouchOffSetup();        // Handle touch-off state
    void processParkingSetup();         // Handle parking setup state
    void processTargetEntry();          // Handle target entry states
    void calculateOperationParameters(); // Calculate cut parameters from targets
    String getDirectionDisplayText();   // Get direction display text (≤21 chars)
    String getTouchOffDisplayText();    // Get touch-off display text (≤21 chars)
    String getParkingDisplayText();     // Get parking display text (≤21 chars)
    String getTargetDisplayText();      // Get target entry display text (≤21 chars)
    
public:
    OperationManager();
    void init(MinimalMotionControl* mc);
    
    // Mode selection
    void setMode(OperationMode mode);
    OperationMode getMode() const { return currentMode; }
    OperationState getState() const { return currentState; }
    
    // Direction control (internal/external and left-to-right/right-to-left)
    void setInternalOperation(bool internal) { isInternalOperation = internal; }
    void setLeftToRight(bool leftToRight) { isLeftToRight = leftToRight; }
    bool getInternalOperation() const { return isInternalOperation; }
    bool getLeftToRight() const { return isLeftToRight; }
    void toggleInternalExternal() { isInternalOperation = !isInternalOperation; }
    void toggleDirection() { isLeftToRight = !isLeftToRight; }
    
    // Touch-off management
    void startTouchOffX();  // Start X touch-off process
    void startTouchOffZ();  // Start Z touch-off process
    void clearTouchOff();   // Clear touch-off coordinates
    bool hasTouchOffX() const { return touchOffXValid; }
    bool hasTouchOffZ() const { return touchOffZValid; }
    bool hasTouchOff() const { return touchOffXValid && touchOffZValid; }
    void setTouchOffAxis(int axis) { touchOffAxis = axis; }
    
    // Parking position management (variable parking system)
    void startParkingSetup();           // Enter parking position setup mode
    void confirmParkingPosition();      // Store current position as parking
    void clearParkingPosition();        // Clear stored parking position
    bool hasParkingPosition() const { return parkingPositionSet; }
    void moveToParkingPosition();       // Move to stored parking position
    void setParkingPosition(long x, long z) { parkingPositionX = x; parkingPositionZ = z; parkingPositionSet = true; }
    
    // Target value management (numpad entry for targets)
    void startTargetDiameterEntry();    // Start diameter target entry
    void startTargetLengthEntry();      // Start length target entry
    void confirmTargetValue();          // Confirm numpad-entered target
    bool hasTargetDiameter() const { return targetDiameter > 0; }
    bool hasTargetLength() const { return targetZLength > 0; }
    void clearTargets();                // Clear target values
    
    // Arrow key mode control for safety
    void setArrowKeyMode(ArrowKeyMode mode) { arrowKeyMode = mode; }
    ArrowKeyMode getArrowKeyMode() const { return arrowKeyMode; }
    bool isArrowMotionEnabled() const { return arrowKeyMode == ARROW_MOTION_MODE; }
    
    // Clear/cancel functionality
    void clearCurrentInput();           // Clear current numpad/state
    void cancelOperation();             // Cancel and return to idle
    
    // h5.ino-style numpad functions
    void numpadPress(int digit);           // Add digit to numpad (0-9)
    void numpadBackspace();                // Remove last digit
    void resetNumpad();                    // Clear numpad input
    long getNumpadResult();                // Get current numeric result
    long numpadToDeciMicrons();           // Convert to deci-microns based on measure
    void cycleMeasure();                   // Cycle through measurement units
    String formatDeciMicrons(long deciMicrons, int precisionPoints); // h5.ino-style formatting
    String formatDupr(long value);         // Format pitch value based on measure
    
    // Touch-off coordinate entry (updated for h5.ino compatibility)
    void handleNumpadInput(char digit);    // Legacy function - calls numpadPress
    void handleNumpadBackspace();          // Legacy function - calls numpadBackspace
    void confirmTouchOffValue();           // Confirm entered coordinate value
    bool isInNumpadInput() const { return inNumpadInput; }
    String getNumpadDisplayText();         // Get current numpad display text
    
    // Operation setup (original float-based functions)
    void setCutLength(float mm);  // Set Z-axis cut length
    void setCutDepth(float mm);   // Set X-axis cut depth
    void setNumPasses(int passes);
    void setConeRatio(float ratio);
    
    // h5.ino-style parameter setup using numpad
    void startParameterEntry(OperationState parameterState);  // Start numpad entry for parameter
    void confirmParameterValue();  // Confirm numpad-entered parameter
    bool isInParameterEntry() const;  // Check if entering a parameter
    void setCutLengthFromNumpad();   // Set cut length from current numpad value
    void setCutDepthFromNumpad();    // Set cut depth from current numpad value  
    void setNumPassesFromNumpad();   // Set passes from current numpad value
    void setConeRatioFromNumpad();   // Set cone ratio from current numpad value
    void setStartsFromNumpad();      // Set starts from current numpad value
    
    // Operation control
    bool startOperation();    // Start the current operation
    void stopOperation();     // Stop and reset
    void pauseOperation();    // Pause at safe point
    void resumeOperation();   // Resume from pause
    void advancePass();       // Move to next pass (manual advance)
    
    // Setup state machine
    void nextSetupStep();     // Advance setup state
    void previousSetupStep(); // Go back in setup
    
    // h5.ino-style setup progression
    int getSetupIndex() const { return setupIndex; }
    void resetSetupIndex() { setupIndex = 0; }  // Reset to first setup step
    void advanceSetupIndex();  // Move to next setup step
    bool isSetupComplete() const;  // Check if all setup steps complete
    int getLastSetupIndex() const;     // Get final setup index for current mode
    bool isPassMode() const;           // Check if mode uses passes
    bool needsZStops() const;          // Check if mode needs Z-axis stops
    
    // Main update function (call from loop)
    void update();
    
    // Status information
    String getStatusText();   // Get current status for display
    String getPromptText();   // Get setup prompt for display
    float getProgress();      // Get operation progress (0-1)
    int getCurrentPass() const { return currentPass; }
    int getTotalPasses() const { return numPasses; }
    
    // Safety checks
    bool isPitchChangeAllowed() const;  // Check if pitch changes are allowed (not during threading)
    
    // Intelligent pitch defaults
    long getDefaultPitchForDiameter(float diameter, int measure) const;  // Get default pitch based on diameter and unit
    void updatePitchFromTouchOffDiameter();  // Update pitch based on touch-off diameter
    
    // Operation parameter getters
    float getCutLengthMm() const { return stepsToMm(cutLength, 1); }  // Z axis
    float getCutDepthMm() const { return stepsToMm(cutDepth, 0); }    // X axis
    float getConeRatio() const { return coneRatio; }
    float getTouchOffXCoord() const { return touchOffXCoord; }
    float getTouchOffZCoord() const { return touchOffZCoord; }
    bool isRunning() const { return currentState == STATE_RUNNING; }
    int getCurrentMeasure() const { return currentMeasure; }  // Get current measurement unit
};

#endif // OPERATION_MANAGER_H