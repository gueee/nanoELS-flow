#ifndef MYHARDWARE_H
#define MYHARDWARE_H

// Hardware pin definitions for ESP32-S3-dev board (H5 variant)
// CRITICAL: These pin assignments are PERMANENT and must never be changed

// Encoder configuration
#define ENCODER_PPR 600
#define PULSE_PER_REVOLUTION 600

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

// Keyboard key definitions (personal development configuration)
#define B_LEFT 68      // Left arrow - Z axis left movement
#define B_RIGHT 75     // Right arrow - Z axis right movement
#define B_UP 85        // Up arrow - X axis forward movement
#define B_DOWN 72      // Down arrow - X axis backward movement
#define B_MINUS 73     // Numpad minus - decrement pitch/passes
#define B_PLUS 87      // Numpad plus - increment pitch/passes
#define B_ON 50        // Enter - start operation/mode
#define B_OFF 145      // ESC - stop operation/mode
#define B_STOPL 83     // 'a' - set left stop
#define B_STOPR 91     // 'd' - set right stop
#define B_STOPU 78     // 'w' - set forward stop
#define B_STOPD 89     // 's' - set rear stop
#define B_DISPL 102    // Win - change bottom line display
#define B_STEP 94      // Tilda - change movement step size
#define B_SETTINGS 74  // Context menu - not used currently
#define B_MEASURE 66   // 'm' - metric/imperial/TPI toggle
#define B_REVERSE 148  // 'r' - thread direction (left/right)
#define B_DIAMETER 22  // 'o' - set X0 to diameter centerline
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

// Mode selection keys (F-keys)
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

// Axis control keys
#define B_X 139        // 'x' - zero X axis
#define B_Z 99         // 'z' - zero Z axis
#define B_X_ENA 93     // 'c' - enable/disable X axis
#define B_Z_ENA 74     // 'q' - enable/disable Z axis

#endif // MYHARDWARE_H