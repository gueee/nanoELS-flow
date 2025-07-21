#ifndef SETUPCONSTANTS_H
#define SETUPCONSTANTS_H

/* ====================================================================== */
/* User-editable setup constants from main .ino file                    */
/* Change values in this section to suit your hardware.                 */
/* ====================================================================== */

// Hardware definition (do not change for H5 variant)
#define HARDWARE_VERSION 5
#define SOFTWARE_VERSION 1

/* ====================================================================== */
/* ESP32-S3 Hardware Pin Definitions - H5 Variant                       */
/* CRITICAL: These pin assignments are PERMANENT and must never change   */
/* ====================================================================== */

// Spindle encoder pins (PCNT_UNIT_0)
#define ENC_A 13
#define ENC_B 14

// Z-axis stepper motor pins
#define Z_ENA 41
#define Z_DIR 42
#define Z_STEP 35

// Z-axis MPG pins (PCNT_UNIT_1)
#define Z_PULSE_A 18
#define Z_PULSE_B 8

// X-axis stepper motor pins
#define X_ENA 16
#define X_DIR 15
#define X_STEP 7

// X-axis MPG pins (PCNT_UNIT_2)
#define X_PULSE_A 47
#define X_PULSE_B 21

// Keyboard interface pins
#define KEY_DATA 37
#define KEY_CLOCK 36

// Nextion display pins (Serial1)
#define NEXTION_TX 43
#define NEXTION_RX 44

/* ====================================================================== */
/* PS2 Keyboard Key Mappings - Personal Development Configuration        */
/* ====================================================================== */

// Movement Controls
#define B_LEFT 68      // Left arrow - Z axis left movement
#define B_RIGHT 75     // Right arrow - Z axis right movement
#define B_UP 85        // Up arrow - X axis towards centerline (away from operator)
#define B_DOWN 72      // Down arrow - X axis away from centerline (towards operator)
#define B_MINUS 73     // Numpad minus - decrement pitch/passes
#define B_PLUS 87      // Numpad plus - increment pitch/passes

// Operation Controls
#define B_ON 50        // Enter - start operation/mode
#define B_OFF 145      // ESC - stop operation/mode

// Stop Controls
#define B_STOPL 83     // 'a' - set left stop
#define B_STOPR 91     // 'd' - set right stop
#define B_STOPU 78     // 'w' - set forward stop
#define B_STOPD 89     // 's' - set rear stop

// Display and System Controls
#define B_DISPL 102    // Win - change bottom line display
#define B_STEP 94      // Tilda - change movement step size
#define B_SETTINGS 74  // Context menu - not used currently
#define B_MEASURE 66   // 'm' - metric/imperial/TPI toggle
#define B_REVERSE 148  // 'r' - thread direction (left/right)
#define B_DIAMETER 22  // 'o' - set X0 to diameter centerline

// Number Entry Keys (Top Row)
#define B_0 84         // '0' top row - number entry
#define B_1 71         // '1' top row
#define B_2 26         // '2' top row
#define B_3 27         // '3' top row
#define B_4 70         // '4' top row
#define B_5 20         // '5' top row
#define B_6 65         // '6' top row
#define B_7 82         // '7' top row
#define B_8 19         // '8' top row
#define B_9 81         // '9' top row
#define B_BACKSPACE 29 // Backspace - remove last entered number

// Mode Selection Keys (F-keys)
#define B_MODE_GEARS 31    // F1 - gearbox mode
#define B_MODE_TURN 24     // F2 - turning mode
#define B_MODE_FACE 147    // F3 - facing mode
#define B_MODE_CONE 17     // F4 - cone mode
#define B_MODE_CUT 92      // F5 - cutting mode
#define B_MODE_THREAD 18   // F6 - threading mode
#define B_MODE_ASYNC 101   // F7 - async mode
#define B_MODE_ELLIPSE 10  // F8 - ellipse mode
#define B_MODE_GCODE 28    // F9 - G-code mode
#define B_MODE_Y 88        // F10 - Y mode (not implemented)

// Axis Control Keys
#define B_X 139        // 'x' - zero X axis
#define B_Z 99         // 'z' - zero Z axis
#define B_X_ENA 93     // 'c' - enable/disable X axis
#define B_Z_ENA 74     // 'q' - enable/disable Z axis

/* ====================================================================== */
/* User-Editable Motor and System Configuration                          */
/* ====================================================================== */

// Spindle rotary encoder setup
extern const int ENCODER_PPR;      // Pulses per revolution of spindle encoder
extern const int ENCODER_BACKLASH; // Number of pulses encoder can issue without spindle movement

// WiFi Configuration
extern const bool WIFI_ENABLED;
extern const char* HOME_WIFI_SSID;           // Your WiFi network name
extern const char* HOME_WIFI_PASSWORD;       // Your WiFi password

// Main lead screw (Z-axis) parameters
extern const long SCREW_Z_DU;           // Lead screw pitch in deci-microns (5mm = 50000 du)
extern const long MOTOR_STEPS_Z;        // Motor steps per revolution
extern const long SPEED_START_Z;        // Initial speed, steps/second
extern const long ACCELERATION_Z;       // Acceleration, steps/second²
extern const long SPEED_MANUAL_MOVE_Z;  // Max manual speed, steps/second
extern const bool INVERT_Z;             // Invert direction if carriage moves wrong way
extern const bool INVERT_Z_ENABLE;      // Enable pin inversion (true = active-LOW)
extern const bool INVERT_Z_STEP;        // Step pin inversion for level shifting
extern const bool NEEDS_REST_Z;         // Set false for closed-loop drivers
extern const long MAX_TRAVEL_MM_Z;      // Maximum Z travel in mm
extern const long BACKLASH_DU_Z;        // Backlash compensation in deci-microns

// Cross-slide lead screw (X-axis) parameters  
extern const long SCREW_X_DU;           // Lead screw pitch in deci-microns (4mm = 40000 du)
extern const long MOTOR_STEPS_X;        // Motor steps per revolution
extern const long SPEED_START_X;        // Initial speed, steps/second
extern const long ACCELERATION_X;       // Acceleration, steps/second²
extern const long SPEED_MANUAL_MOVE_X;  // Max manual speed, steps/second
extern const bool INVERT_X;             // Invert direction if carriage moves wrong way
extern const bool INVERT_X_ENABLE;      // Enable pin inversion (true = active-LOW)
extern const bool INVERT_X_STEP;        // Step pin inversion for level shifting
extern const bool NEEDS_REST_X;         // Set false for closed-loop drivers
extern const long MAX_TRAVEL_MM_X;      // Maximum X travel in mm
extern const long BACKLASH_DU_X;        // Backlash compensation in deci-microns

// Manual stepping configuration
extern const long STEP_TIME_MS;           // Time for one manual step in milliseconds
extern const long DELAY_BETWEEN_STEPS_MS; // Pause between manual steps in milliseconds

// MPG (Manual Pulse Generator) configuration - h5.ino style
extern const float PULSE_PER_REVOLUTION;  // MPG pulses per revolution (100 PPR encoder = 400 quadrature counts)
extern const int MPG_PCNT_FILTER;         // Encoder filter value (1-1023 clock cycles)
extern const int MPG_PCNT_LIM;            // PCNT limit for overflow detection
extern const int MPG_PCNT_CLEAR;          // Reset PCNT when reaching this value
extern const float MPG_SCALE_DIVISOR;     // How many pulses equals one full step size movement

// Motion control limits (converted to our system internally)
extern const float MAX_VELOCITY_X_USER;    // Maximum X velocity in mm/s
extern const float MAX_VELOCITY_Z_USER;    // Maximum Z velocity in mm/s
extern const float MAX_ACCELERATION_X_USER; // Maximum X acceleration in mm/s²
extern const float MAX_ACCELERATION_Z_USER; // Maximum Z acceleration in mm/s²

// Advanced settings (normally no need to change)
extern const long INCOMING_BUFFER_SIZE;    // WebSocket input buffer size
extern const long OUTGOING_BUFFER_SIZE;    // WebSocket output buffer size
extern const long SAVE_DELAY_US;           // Wait 5s before auto-saving preferences
extern const long DIRECTION_SETUP_DELAY_US; // Delay after direction change
extern const long STEPPED_ENABLE_DELAY_MS;  // Delay after enable before stepping

// PID controller tuning (for fine positioning)
extern const float PID_KP;    // Proportional gain
extern const float PID_KI;    // Integral gain  
extern const float PID_KD;    // Derivative gain

#endif // SETUPCONSTANTS_H