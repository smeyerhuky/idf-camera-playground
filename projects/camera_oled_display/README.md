# Camera OLED Display Project

This project implements a real-time camera preview display using an ESP32S3 XIAO Sense board with a SparkFun Qwiic OLED display. The system captures images from the onboard camera and displays them on a 128x64 monochrome OLED screen using LVGL graphics library.

## Features

- Real-time camera preview on OLED display
- SSD1306 128x64 monochrome OLED support
- LVGL-based graphics rendering
- Optimized for ESP32S3 XIAO Sense hardware
- I2C communication via Qwiic connector
- 10 FPS refresh rate
- Automatic image downsampling and conversion

## Hardware Requirements

### Primary Components

- **XIAO ESP32S3 Sense** - Main microcontroller with built-in camera
- **SparkFun Qwiic OLED 1.3" (LCD-23453)** - 128x64 white-on-black OLED display

### OLED Display Specifications

- **Controller**: SSD1306
- **Resolution**: 128 x 64 pixels
- **Color**: Monochrome (white on black)
- **Interface**: I2C via Qwiic connector
- **I2C Address**: 0x3D (default), 0x3C (alternate via jumper)
- **Voltage**: 3.3V (no onboard regulation)
- **Connection**: Qwiic-compatible I2C

## Wiring Diagram

### XIAO ESP32S3 Sense to SparkFun Qwiic OLED

```txt
XIAO ESP32S3 Sense          SparkFun Qwiic OLED
┌─────────────────┐         ┌─────────────────┐
│                 │         │                 │
│  Pin 5 (SDA)    │◄────────┤ SDA (Blue)      │
│  Pin 6 (SCL)    │◄────────┤ SCL (Yellow)    │
│  3V3            │◄────────┤ VCC (Red)       │
│  GND            │◄────────┤ GND (Black)     │
│                 │         │                 │
└─────────────────┘         └─────────────────┘
```

### Qwiic Connector Pinout

The Qwiic connector uses a standard 4-pin JST SH connector:

```txt
Qwiic Connector (JST SH 1.0mm pitch)
┌─────┬─────┬─────┬─────┐
│  1  │  2  │  3  │  4  │
│BLACK│ RED │BLUE │YELL │
│ GND │VCC  │SDA  │SCL  │
│     │3.3V │     │     │
└─────┴─────┴─────┴─────┘
```

### Physical Connection

Connect the OLED display to the XIAO using a Qwiic cable:

1. **Qwiic Cable**: Use a standard Qwiic cable (4-wire JST SH to JST SH)
2. **XIAO Connection**: Connect to the Qwiic connector on the XIAO ESP32S3 Sense
3. **OLED Connection**: Connect to the Qwiic connector on the SparkFun OLED display

**Important Notes:**
- The XIAO ESP32S3 Sense has a built-in Qwiic connector
- No external pullup resistors needed (included on OLED board)
- Maximum I2C frequency: 400kHz
- Power consumption: ~20mA for OLED display

## Pin Configuration

### Camera Pins (Built-in to XIAO ESP32S3 Sense)

```c
#define CAM_PIN_PWDN    -1      // Power down (not used)
#define CAM_PIN_RESET   -1      // Reset (not used)
#define CAM_PIN_XCLK    10      // External clock
#define CAM_PIN_SIOD    40      // SDA for camera
#define CAM_PIN_SIOC    39      // SCL for camera
#define CAM_PIN_D7      48      // Data pin 7
#define CAM_PIN_D6      11      // Data pin 6
#define CAM_PIN_D5      12      // Data pin 5
#define CAM_PIN_D4      14      // Data pin 4
#define CAM_PIN_D3      16      // Data pin 3
#define CAM_PIN_D2      18      // Data pin 2
#define CAM_PIN_D1      17      // Data pin 1
#define CAM_PIN_D0      15      // Data pin 0
#define CAM_PIN_VSYNC   38      // Vertical sync
#define CAM_PIN_HREF    47      // Horizontal reference
#define CAM_PIN_PCLK    13      // Pixel clock
```

### I2C Pins for OLED

```c
#define I2C_SDA_PIN     5       // Qwiic SDA (Blue wire)
#define I2C_SCL_PIN     6       // Qwiic SCL (Yellow wire)
#define I2C_PORT        0       // I2C port number
#define I2C_FREQ_HZ     400000  // 400kHz I2C frequency
```

## Software Architecture

### Component Structure

```txt
camera_oled_display/
├── components/
│   ├── ssd1306_driver/     # Low-level OLED driver
│   ├── lvgl_port/          # LVGL integration layer
│   └── camera_preview/     # Camera to display interface
├── main/                   # Main application
└── README.md
```

### Key Components

1. **SSD1306 Driver** (`ssd1306_driver/`)
   - Low-level I2C communication with OLED
   - Hardware initialization and configuration
   - Framebuffer management
   - Display update routines

2. **LVGL Port** (`lvgl_port/`)
   - Integration between LVGL and SSD1306 driver
   - Display callbacks and event handling
   - Monochrome color conversion
   - Task and timer management

3. **Camera Preview** (`camera_preview/`)
   - Camera frame capture and processing
   - Image downsampling and format conversion
   - LVGL UI components for display
   - Refresh rate control

## Build Instructions

### Prerequisites

```bash
# Activate ESP-IDF environment
source ./activate.sh
```

### Build Commands

```bash
# Configure for ESP32S3
./build.sh build camera_oled_display setup

# Build the project
./build.sh build camera_oled_display build

# Flash to device
./build.sh build camera_oled_display flash

# Monitor serial output
./build.sh build camera_oled_display monitor
```

### Build All at Once

```bash
./build.sh build camera_oled_display all
```

## Configuration

### OLED I2C Address

The OLED display uses I2C address `0x3D` by default. To change to `0x3C`:

1. Locate the address jumper on the back of the OLED board
2. Solder the jumper closed
3. Update the I2C address in code:

```c
ssd1306_config_t oled_config = {
    .i2c_addr = SSD1306_I2C_ADDR_SECONDARY,  // 0x3C
    // ... other config
};
```

### Camera Settings

The camera is configured for optimal OLED display:

- **Resolution**: QQVGA (160x120) - downsampled to 128x64
- **Format**: RGB565 - converted to monochrome
- **Frame Rate**: ~10 FPS
- **Brightness/Contrast**: Optimized for monochrome conversion

### Display Settings

- **Refresh Rate**: 100ms (10 FPS)
- **Color Conversion**: RGB565 → Grayscale → Monochrome
- **Threshold**: 50% brightness for black/white conversion

## Troubleshooting

### Common Issues

1. **OLED Not Detected**
   - Check I2C connections
   - Verify I2C address (0x3D or 0x3C)
   - Ensure 3.3V power supply

2. **Camera Not Working**
   - Check if camera module is properly seated
   - Verify PSRAM is enabled in configuration
   - Check camera pin definitions

3. **Display Flickering**
   - Reduce refresh rate
   - Check I2C frequency (lower if needed)
   - Verify power supply stability

4. **Poor Image Quality**
   - Adjust camera brightness/contrast settings
   - Modify monochrome conversion threshold
   - Check lighting conditions

### Debug Commands

```bash
# Check I2C devices
i2cdetect -y 0

# Monitor ESP32 logs
./build.sh build camera_oled_display monitor
```

## Performance

- **Memory Usage**: ~150KB RAM, uses PSRAM for camera buffers
- **CPU Usage**: ~15% at 10 FPS
- **Power Consumption**: ~200mA total (camera + OLED + ESP32S3)
- **Boot Time**: ~2 seconds to first frame

## Extension Ideas

- Add touch controls via capacitive touch pins
- Implement image capture and SD card storage
- Add multiple display modes (normal, inverted, etc.)
- Implement motion detection and alerts
- Add WiFi streaming capabilities
- Create configuration menu via button controls
