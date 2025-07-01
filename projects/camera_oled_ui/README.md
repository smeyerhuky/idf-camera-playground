# Camera OLED UI Demo Project

This project demonstrates how to use the OLED UI component with an LVGL-based interface for camera control on the XIAO ESP32S3 Sense board.

## Hardware Setup

### Required Components
- XIAO ESP32S3 Sense development board
- SparkFun Qwiic OLED 1.3" (128x64) display  
- Qwiic cable or jumper wires

### Connections
```
XIAO ESP32S3 Sense    SparkFun Qwiic OLED
------------------    -------------------
3V3            -----> VCC (3.3V)
GND            -----> GND
GPIO 5 (SDA)   -----> SDA
GPIO 6 (SCL)   -----> SCL
GPIO 0 (Boot)  -----> (Button input for demo)
```

## Features Demonstrated

### Camera Control Interface
- Photo/Video/Timelapse mode switching
- Camera settings adjustment (brightness, contrast, saturation, exposure, JPEG quality)
- Video settings configuration
- Audio settings (microphone on/off)
- Real-time status display

### Input Methods
- **Button Press (GPIO 0):** Navigate and select menu items
- **Long Press (1 second):** Switch between camera modes
- **Encoder Support:** Optional rotary encoder for navigation (GPIO 1/2)

### Display Features
- 128x64 monochrome OLED with crisp white-on-black display
- LVGL-based user interface with smooth navigation
- Real-time status updates
- Multi-level menu system

## Building and Running

### Prerequisites
```bash
# Set up environment (run once per session)
source ./activate.sh
```

### Build Commands
```bash
# Configure for ESP32S3 target
./build.sh build camera_oled_ui setup

# Build the project
./build.sh build camera_oled_ui build

# Flash to device
./build.sh build camera_oled_ui flash

# Monitor serial output
./build.sh build camera_oled_ui monitor

# Build and flash in one command
./build.sh build camera_oled_ui all
```

### Configuration
```bash
# Open ESP-IDF configuration menu
./build.sh build camera_oled_ui menuconfig
```

## Usage Instructions

### Basic Navigation
1. **Power on:** Display shows main menu with current camera mode
2. **Button press:** Select highlighted menu item
3. **Navigate:** Use button to move through menu options
4. **Mode switch:** Long press button (1 second) to cycle camera modes

### Menu Structure
```
Main Menu
├── Take Photo/Video    (Action based on current mode)
├── Camera Settings
│   ├── Brightness      (-100 to +100)
│   ├── Contrast        (-100 to +100) 
│   ├── Saturation      (-100 to +100)
│   ├── Exposure        (-100 to +100)
│   └── JPEG Quality    (10 to 100)
├── Video Settings
│   ├── Resolution
│   ├── Frame Rate
│   ├── Bitrate
│   └── Duration
├── Audio Settings
│   ├── Microphone      (ON/OFF)
│   ├── Volume
│   └── Audio Format
└── System Settings
```

### Status Bar Information
- Current microphone status (ON/OFF)
- JPEG quality setting
- Camera mode indicator in header

## Code Structure

### Main Components
- `main.c`: Application entry point and task creation
- `input_handler.c`: GPIO input processing and event generation
- `oled_ui` component: Display driver and LVGL integration
- `camera_control_ui.c`: UI screens and camera settings management

### Key Functions
```c
// Initialize OLED display and UI
esp_err_t oled_ui_init(const ssd1306_config_t *config, oled_ui_handle_t **handle);

// Send input events to UI
esp_err_t oled_ui_send_event(const ui_event_t *event);

// Get current camera settings
camera_settings_t* camera_control_ui_get_settings(void);
```

## Customization

### Adding New Menu Items
1. Add item to appropriate menu array in `camera_control_ui.c`
2. Update menu handling logic in `handle_select()`
3. Add setting adjustment in `adjust_setting()`

### Changing GPIO Pins
Modify GPIO assignments in `main.c`:
```c
ssd1306_config_t oled_config = {
    .sda_gpio = 5,   // Change SDA pin
    .scl_gpio = 6,   // Change SCL pin
    .address = SSD1306_I2C_ADDRESS
};

input_handler_config_t input_config = {
    .button_gpio = 0,     // Change button pin
    .encoder_a_gpio = 1,  // Change encoder A pin
    .encoder_b_gpio = 2,  // Change encoder B pin
};
```

### Adding Rotary Encoder
Connect a rotary encoder to GPIO 1 and 2, then the input handler will automatically detect rotation and send encoder events to the UI.

## Troubleshooting

### Display Issues
- **Blank screen:** Check I2C connections and 3.3V power
- **Garbled display:** Verify I2C address (0x3D) and bus speed (400kHz)
- **UI not responding:** Check LVGL task is running and event queue creation

### Input Issues  
- **Button not working:** Verify GPIO 0 connection and pull-up configuration
- **Long press not detected:** Check timer creation and button debounce timing

### Build Issues
- **Component not found:** Ensure `EXTRA_COMPONENT_DIRS` is set correctly
- **LVGL errors:** Verify LVGL version compatibility (v8.x required)

## Performance Notes

- UI refresh rate: ~100Hz (10ms task delay)
- LVGL timer handler: 5ms intervals
- Button debounce: 50ms
- Long press detection: 1000ms
- I2C frequency: 400kHz
- Memory usage: ~8KB for LVGL buffers

## Integration with Camera

To integrate with actual camera functionality:

1. Include camera component in CMakeLists.txt
2. Apply settings from `camera_control_ui_get_settings()` to camera driver
3. Implement actual photo/video capture in menu action handlers
4. Add camera status feedback to UI

Example integration:
```c
camera_settings_t *settings = camera_control_ui_get_settings();
camera_sensor_set_brightness(settings->brightness);
camera_sensor_set_quality(settings->jpeg_quality);
```