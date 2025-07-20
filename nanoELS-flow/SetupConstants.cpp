#include "SetupConstants.h"

/* ====================================================================== */
/* User-editable setup constants - moved from main .ino file           */
/* Change values in this section to suit your hardware.                 */
/* ====================================================================== */

// Spindle rotary encoder setup
const int ENCODER_PPR = 600;       // Pulses per revolution of spindle encoder
const int ENCODER_BACKLASH = 3;    // Number of pulses encoder can issue without spindle movement

// WiFi Configuration
const bool WIFI_ENABLED = true;
const char* HOME_WIFI_SSID = "Holzweg_131";           // Your WiFi network name
const char* HOME_WIFI_PASSWORD = "Holzweg131-mesh";   // Your WiFi password

// Main lead screw (Z-axis) parameters
const long SCREW_Z_DU = 50000;           // Lead screw pitch in deci-microns (5mm = 50000 du)
const long MOTOR_STEPS_Z = 4000;         // Motor steps per revolution
const long SPEED_START_Z = MOTOR_STEPS_Z;     // Initial speed, steps/second
const long ACCELERATION_Z = 25 * MOTOR_STEPS_Z;  // Acceleration, steps/second²
const long SPEED_MANUAL_MOVE_Z = 8 * MOTOR_STEPS_Z;  // Max manual speed, steps/second
const bool INVERT_Z = false;             // Invert direction if carriage moves wrong way
const bool INVERT_Z_ENABLE = true;       // Enable pin inversion (true = active-LOW)
const bool INVERT_Z_STEP = true;         // Step pin inversion for level shifting
const bool NEEDS_REST_Z = false;         // Set false for closed-loop drivers
const long MAX_TRAVEL_MM_Z = 300;        // Maximum Z travel in mm
const long BACKLASH_DU_Z = 0;           // Backlash compensation in deci-microns

// Cross-slide lead screw (X-axis) parameters  
const long SCREW_X_DU = 40000;           // Lead screw pitch in deci-microns (4mm = 40000 du)
const long MOTOR_STEPS_X = 4000;         // Motor steps per revolution
const long SPEED_START_X = MOTOR_STEPS_X;     // Initial speed, steps/second
const long ACCELERATION_X = 25 * MOTOR_STEPS_X;  // Acceleration, steps/second²
const long SPEED_MANUAL_MOVE_X = 8 * MOTOR_STEPS_X;  // Max manual speed, steps/second
const bool INVERT_X = true;              // Invert direction if carriage moves wrong way
const bool INVERT_X_ENABLE = true;       // Enable pin inversion (true = active-LOW)
const bool INVERT_X_STEP = true;         // Step pin inversion for level shifting
const bool NEEDS_REST_X = false;         // Set false for closed-loop drivers
const long MAX_TRAVEL_MM_X = 100;        // Maximum X travel in mm
const long BACKLASH_DU_X = 0;           // Backlash compensation in deci-microns

// Manual stepping configuration
const long STEP_TIME_MS = 500;           // Time for one manual step in milliseconds
const long DELAY_BETWEEN_STEPS_MS = 80;  // Pause between manual steps in milliseconds

// Motion control limits (converted to our system internally)
const float MAX_VELOCITY_X_USER = 200.0;  // Maximum X velocity in mm/s (increased for manual jogging)
const float MAX_VELOCITY_Z_USER = 200.0;  // Maximum Z velocity in mm/s (increased for manual jogging)
const float MAX_ACCELERATION_X_USER = 2000.0;  // Maximum X acceleration in mm/s² (increased for manual jogging)
const float MAX_ACCELERATION_Z_USER = 2000.0;  // Maximum Z acceleration in mm/s² (increased for manual jogging)

// Advanced settings (normally no need to change)
const long INCOMING_BUFFER_SIZE = 100000;  // WebSocket input buffer size
const long OUTGOING_BUFFER_SIZE = 100000;  // WebSocket output buffer size
const long SAVE_DELAY_US = 5000000;        // Wait 5s before auto-saving preferences
const long DIRECTION_SETUP_DELAY_US = 5;   // Delay after direction change
const long STEPPED_ENABLE_DELAY_MS = 100;  // Delay after enable before stepping

// PID controller tuning (for fine positioning)
const float PID_KP = 10.0;    // Proportional gain
const float PID_KI = 0.1;     // Integral gain  
const float PID_KD = 0.05;    // Derivative gain