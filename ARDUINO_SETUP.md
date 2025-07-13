# Arduino IDE Setup for nanoELS-flow

## Required Libraries Installation

**IMPORTANT**: You must install these libraries before the sketch will compile.

### 1. Install Required Libraries via Arduino Library Manager

Open Arduino IDE → Tools → Manage Libraries, then search and install:

#### Core Libraries (Required)
1. **FastAccelStepper** by gin66
   - Search: "FastAccelStepper"
   - Install the latest version
   - GitHub: https://github.com/gin66/FastAccelStepper

2. **WebSockets** by Markus Sattler  
   - Search: "WebSockets"
   - Install by Markus Sattler (NOT other WebSocket libraries)
   - GitHub: https://github.com/Links2004/arduinoWebSockets

3. **PS2KeyAdvanced** by Paul Carpenter
   - Search: "PS2KeyAdvanced" 
   - Install by Paul Carpenter
   - GitHub: https://github.com/PaulStoffregen/PS2Keyboard

### 2. ESP32 Board Package Installation

If not already installed:

1. **Add ESP32 Board URL**:
   - File → Preferences
   - Additional Board Manager URLs: 
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

2. **Install ESP32 Boards**:
   - Tools → Board → Boards Manager
   - Search: "ESP32"
   - Install "ESP32 by Espressif Systems"

### 3. Board Configuration

Select the correct board and settings:

- **Board**: "ESP32S3 Dev Module"
- **USB CDC On Boot**: "Enabled" 
- **CPU Frequency**: "240MHz (WiFi/BT)"
- **Flash Mode**: "QIO"
- **Flash Size**: "16MB (128Mb)"
- **Partition Scheme**: "Huge APP (3MB No OTA/1MB SPIFFS)"
- **PSRAM**: "OPI PSRAM" (if available)

### 4. WiFi Configuration

Before uploading, configure your WiFi settings in the sketch:

```cpp
// Line ~62 in nanoELS-flow.ino
#define WIFI_MODE 1  // Change to 1 for home WiFi

// Lines ~70-71 - Update with your WiFi credentials
const char* HOME_WIFI_SSID = "YourWiFiNetwork";
const char* HOME_WIFI_PASSWORD = "YourWiFiPassword";
```

**WiFi Modes**:
- `WIFI_MODE 0`: Creates Access Point "nanoELS-flow" (password: "nanoels123")
- `WIFI_MODE 1`: Connects to your home WiFi network

### 5. Hardware Connections

Pin connections are defined in `MyHardware.h` and are **PERMANENT**:

#### Stepper Motors
- **X-axis**: Step=7, Dir=15, Enable=16
- **Z-axis**: Step=35, Dir=42, Enable=41

#### Encoders (PCNT Units)
- **Spindle**: A=13, B=14 (PCNT_UNIT_0)
- **Z-MPG**: A=18, B=8 (PCNT_UNIT_1)  
- **X-MPG**: A=47, B=21 (PCNT_UNIT_2)

#### Interface
- **Keyboard**: Data=37, Clock=36
- **Nextion Display**: Serial1 (built-in)

### 6. Common Compilation Issues

**"FastAccelStepper.h: No such file or directory"**
- Install FastAccelStepper library via Library Manager

**"WebSocketsServer.h: No such file or directory"**
- Install WebSockets library by Markus Sattler

**"PS2KeyAdvanced.h: No such file or directory"**
- Install PS2KeyAdvanced library

**Board not found**
- Install ESP32 board package
- Select "ESP32S3 Dev Module"

**Compilation errors with WiFi**
- Ensure ESP32 board package is latest version
- Check board configuration settings

### 7. Upload Process

1. **Connect ESP32-S3** via USB
2. **Select correct COM port** in Tools → Port
3. **Put ESP32 in download mode** (may require holding BOOT button)
4. **Upload sketch** 
5. **Monitor Serial** at 115200 baud for status

### 8. First Run

After successful upload:

1. **Open Serial Monitor** (115200 baud)
2. **Watch initialization** sequence
3. **Note WiFi status** and IP address
4. **Access web interface** at displayed IP address
5. **Test basic motion** with keyboard commands

### 9. Troubleshooting

**WiFi Connection Issues**:
- Check SSID and password spelling
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- Watch Nextion display for status updates
- Check Serial Monitor for detailed error messages

**Motion Control Issues**:
- Verify stepper motor wiring
- Check power supply connections
- Test with emergency stop released (ENTER key)

**Display Issues**:
- Ensure Nextion display connected to Serial1
- Check baud rate (9600)
- Verify display is compatible with original h5 interface

## File Structure

Arduino IDE compatible structure:
```
nanoELS-flow/
├── nanoELS-flow.ino          # Main sketch
├── MotionControl.h           # Motion control header
├── MotionControl.cpp         # Motion control implementation  
├── WebInterface.h            # Web interface header
├── WebInterface.cpp          # Web interface implementation
├── NextionDisplay.h          # Display header
├── NextionDisplay.cpp        # Display implementation
├── MyHardware.h              # Pin definitions
├── indexhtml.h               # Web page content
└── ARDUINO_SETUP.md          # This file
```

All files must be in the same folder as the .ino file for Arduino IDE compatibility.