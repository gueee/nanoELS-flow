#ifndef SETUPCONSTANTS_H
#define SETUPCONSTANTS_H

/* ====================================================================== */
/* User-editable setup constants from main .ino file                    */
/* Change values in this section to suit your hardware.                 */
/* ====================================================================== */

// Hardware definition (do not change for H5 variant)
#define HARDWARE_VERSION 5
#define SOFTWARE_VERSION 1

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