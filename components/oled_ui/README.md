# OLED UI Component for ESP32-S3 Camera Interface

This component provides an LVGL-based user interface for controlling camera settings on a 128x64 SSD1306 OLED display connected via I2C.

## Hardware Connections

### SparkFun Qwiic OLED 1.3" to XIAO ESP32S3 Sense

```
SparkFun Qwiic OLED    XIAO ESP32S3 Sense
------------------     ------------------
VCC (3.3V)      -----> 3V3
GND             -----> GND  
SDA             -----> GPIO 5 (I2C SDA)
SCL             -----> GPIO 6 (I2C SCL)
```

**CRITICAL NOTES:**
- The Qwiic OLED operates at 3.3V only - do NOT connect to 5V
- I2C address is 0x3D by default (can be changed to 0x3C by soldering jumper)
- Built-in 2.2kΩ pull-up resistors on SDA/SCL lines
- Maximum I2C frequency: 400kHz

### Schematic Diagram

```
                    XIAO ESP32S3 Sense
                   ┌─────────────────────┐
                   │                     │
                   │  GPIO 5 (SDA) ──────┼──── SDA
                   │  GPIO 6 (SCL) ──────┼──── SCL  
                   │  3V3 ───────────────┼──── VCC
                   │  GND ───────────────┼──── GND
                   │                     │
                   └─────────────────────┘
                              │
                              │ Qwiic Cable
                              │
                   ┌─────────────────────┐
                   │  SparkFun Qwiic     │
                   │  OLED 1.3" 128x64   │
                   │  (SSD1306)          │
                   │                     │
                   │  I2C Address: 0x3D  │
                   └─────────────────────┘
```

## Features

### Camera Control Interface
- **Photo/Video/Timelapse modes**
- **Camera Settings:** Brightness, Contrast, Saturation, Exposure, JPEG Quality
- **Video Settings:** Resolution, Frame Rate, Bitrate, Duration  
- **Audio Settings:** Microphone On/Off, Volume, Audio Format
- **System Settings:** General system configuration

### User Interface
- **Menu Navigation:** Encoder/button-based navigation
- **Real-time Status:** Current mode, microphone status, quality settings
- **Setting Adjustment:** In-place value modification
- **Mode Switching:** Long-press to cycle camera modes

### Input Methods
- **Button Press:** Select/confirm actions
- **Encoder Turn:** Navigate menus or adjust values
- **Long Press:** Switch camera modes

## Usage

### Include in Your Project

Add to your main CMakeLists.txt:
```cmake
set(EXTRA_COMPONENT_DIRS "components/oled_ui")
```

### Basic Implementation

```c
#include "oled_ui.h"

void app_main(void)
{
    // Configure OLED display
    ssd1306_config_t oled_config = {
        .sda_gpio = 5,
        .scl_gpio = 6,
        .address = SSD1306_I2C_ADDRESS
    };
    
    // Initialize OLED UI
    oled_ui_handle_t *ui_handle;
    esp_err_t ret = oled_ui_init(&oled_config, &ui_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("APP", "Failed to initialize OLED UI");
        return;
    }
    
    // Create UI task
    xTaskCreate(oled_ui_task, "oled_ui", 4096, NULL, 5, NULL);
    
    // Send events to UI
    ui_event_t event = {
        .type = UI_EVENT_BUTTON_PRESSED,
        .value = 1
    };
    oled_ui_send_event(&event);
}
```

### Event Handling

```c
// Button press event
ui_event_t button_event = {
    .type = UI_EVENT_BUTTON_PRESSED,
    .value = 1
};
oled_ui_send_event(&button_event);

// Encoder rotation event  
ui_event_t encoder_event = {
    .type = UI_EVENT_ENCODER_TURNED,
    .value = 1  // positive for clockwise, negative for counter-clockwise
};
oled_ui_send_event(&encoder_event);

// Long press event
ui_event_t long_press_event = {
    .type = UI_EVENT_LONG_PRESS,
    .value = 1
};
oled_ui_send_event(&long_press_event);
```

### Get Camera Settings

```c
camera_settings_t *settings = camera_control_ui_get_settings();
printf("Brightness: %d\n", settings->brightness);
printf("JPEG Quality: %d\n", settings->jpeg_quality);
printf("Microphone: %s\n", settings->microphone_enabled ? "ON" : "OFF");
```

## Dependencies

- ESP-IDF v5.x
- LVGL v8.x
- ESP LCD component
- Driver component (I2C)

Add to your main component's CMakeLists.txt:
```cmake
REQUIRES driver esp_lcd lvgl oled_ui
```

## Display Specifications

- **Resolution:** 128x64 pixels
- **Controller:** SSD1306  
- **Interface:** I2C (400kHz)
- **Colors:** Monochrome (white on black)
- **Supply Voltage:** 3.0-3.3V only
- **Active Area:** 29.42 x 14.7 mm

## Troubleshooting

### Common Issues

1. **Display not initializing:**
   - Check I2C connections (SDA/SCL)
   - Verify 3.3V power supply
   - Confirm I2C address (0x3D default)

2. **Garbled display:**
   - Check I2C frequency (max 400kHz)  
   - Verify buffer allocation in DMA-capable memory
   - Ensure proper LVGL flush callback

3. **No response to input:**
   - Verify UI event queue creation
   - Check task priorities
   - Confirm event structure format

### Debug Commands

```bash
# Check I2C bus
i2cdetect -y 0

# Monitor ESP logs
idf.py monitor

# Check memory usage
idf.py size
```