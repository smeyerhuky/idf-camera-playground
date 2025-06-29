# Simple Camera Application

A modular ESP32-S3 camera application for XIAO ESP32S3 Sense that captures JPEG images and saves them to an SD card.

## Features

- OV2640 camera support with optimal settings
- SD card storage using SPI interface
- Automatic photo capture every 5 seconds
- JPEG compression with configurable quality
- Modular architecture with separate camera and SD card components
- Free space monitoring

## Hardware Requirements

- Seeed Studio XIAO ESP32S3 Sense
- MicroSD card (FAT32 formatted, up to 32GB)

## Pin Configuration

### Camera Pins (OV2640)
- XCLK: GPIO 10
- SIOD: GPIO 40
- SIOC: GPIO 39
- VSYNC: GPIO 38
- HREF: GPIO 47
- PCLK: GPIO 13
- D0-D7: GPIO 15, 17, 18, 16, 14, 12, 11, 48

### SD Card Pins (SPI)
- MISO: GPIO 8
- MOSI: GPIO 9
- SCLK: GPIO 7
- CS: GPIO 21

## Building and Flashing

```bash
# From project directory
./tools/build.sh setup
./tools/build.sh build
./tools/build.sh flash
./tools/build.sh monitor
```

Or from the root directory:

```bash
./build.sh build simple_camera all
```

## Configuration

The application can be configured through `menuconfig`:

```bash
./tools/build.sh menuconfig
```

Key configuration options:
- Camera frame size (default: UXGA 1600x1200)
- JPEG quality (default: 10, lower is better quality)
- Capture interval (default: 5 seconds)

## Usage

1. Insert a FAT32-formatted microSD card
2. Flash the application
3. The camera will automatically start taking photos
4. Photos are saved as `photo_XXXX.jpg` on the SD card
5. Monitor output shows capture status and SD card free space

## Architecture

The application uses a modular design:

- **camera_module**: Handles camera initialization and image capture
- **sdcard_module**: Manages SD card mounting and file operations
- **main**: Orchestrates the modules and implements the capture loop