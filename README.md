# ESP-IDF Camera Development Framework

A comprehensive ESP32 camera development environment combining ESP-IDF v5.5-dev with ESP-WHO computer vision capabilities. This repository provides a complete platform for building camera-based applications with AI-powered image processing on ESP32 microcontrollers.

## Features

- **Advanced Camera Support**: Built-in support for DVP and CSI cameras with ESP32-S3 and ESP32-P4
- **Computer Vision**: Face detection, face recognition, pedestrian detection, and QR code recognition
- **AI Integration**: Asynchronous camera/AI processing for higher FPS performance
- **Modern UI**: LVGL graphics library support for rich user interfaces
- **Automated Build System**: Central project registry with automated discovery and management
- **Multi-Target Support**: Supports ESP32-S3-EYE and ESP32-P4-Function-EV-Board

## Quick Start

### Prerequisites

- **Hardware**: ESP32-S3-EYE or ESP32-P4-Function-EV-Board development kit
- **Software**: Linux/macOS/Windows with Python 3.8+, Git, CMake (optional)

### Setup

```bash
# Clone the repository
git clone <repository-url>
cd idf-camera

# Initialize development environment (downloads ESP-IDF and ESP-WHO)
./prepare.sh

# Activate the environment (required for each session)
source ./activate.sh

# List available projects
./build.sh list

# Build and flash a project (example: hello_world)
./build.sh build hello_world setup
./build.sh build hello_world all
```

### Submodule Management

This repository uses git submodules to manage ESP-IDF and ESP-WHO dependencies:

```bash
# Update to latest versions
./prepare.sh update

# Manual submodule operations
git submodule update --remote esp-idf    # Update ESP-IDF
git submodule update --remote esp-who    # Update ESP-WHO
git submodule update --remote            # Update all

# Clean and reset environment
./prepare.sh clean
```

## Available Projects

Use `./build.sh list` to see all discovered projects. Each project includes:

- **Target chip** (esp32s3, esp32p4)
- **Description** and key features
- **Automated build commands**

### Example Projects

- **hello_world**: Basic ESP32-S3 application template
- **Camera applications**: Various camera-based demos (see esp-who/examples/)
- **Face detection**: Terminal and LCD display versions
- **Face recognition**: Interactive LVGL-based interface
- **Pedestrian detection**: Real-time people detection

## Build System

### Central Build Script

The `build.sh` script provides centralized project management:

```bash
# Project Management
./build.sh list                    # List all projects
./build.sh create <name>           # Create new project
./build.sh config <project>        # Interactive configuration

# Build Commands
./build.sh build <project> setup   # Configure target chip
./build.sh build <project> build   # Build project
./build.sh build <project> flash   # Flash to device
./build.sh build <project> monitor # Serial monitor
./build.sh build <project> all     # Build and flash

# Analysis Commands
./build.sh build <project> size    # Analyze build size
./build.sh build <project> menuconfig # ESP-IDF configuration

# Cleaning
./build.sh build <project> clean   # Clean build files
./build.sh build <project> fullclean # Complete clean
```

### Project Structure

```bash
projects/
├── <project_name>/
│   ├── main/                  # Main application code
│   ├── components/            # Project components
│   ├── tools/build.sh        # Project-specific build script
│   ├── CMakeLists.txt        # Build configuration
│   ├── project.json          # Project metadata
│   └── README.md             # Project documentation
```

### Project Metadata (project.json)

```json
{
  "name": "project_name",
  "description": "Project description",
  "target": "esp32s3",
  "version": "1.0.0",
  "build": {
    "commands": ["setup", "build", "flash", "monitor", "all"],
    "default_command": "build"
  },
  "features": ["Feature1", "Feature2"]
}
```

## ESP-WHO Computer Vision

### Supported Models

- **Human Face Detection**: High-performance face detection with bounding boxes
- **Human Face Recognition**: Face enrollment, recognition, and deletion
- **Pedestrian Detection**: Real-time people detection
- **QR Code Recognition**: Code scanning and decoding

### Development Boards

| SoC | Development Board | Features |
|-----|------------------|----------|
| ESP32-S3 | ESP-S3-EYE | Camera, LCD, buttons, microphone |
| ESP32-P4 | ESP32-P4-Function-EV-Board | High-performance AI, camera, touch display |

### Example Applications

```bash
# Face detection with terminal output
./build.sh build face_detect_terminal all

# Face detection with LCD display (high FPS)
./build.sh build face_detect_lcd all

# Interactive face recognition with LVGL
./build.sh build face_recognition all

# Pedestrian detection
./build.sh build pedestrian_detect all
```

## ESP-IDF Framework

### Version Information

- **ESP-IDF**: v5.5-dev (latest development branch)
- **Target Support**: ESP32, ESP32-S2, ESP32-C3, ESP32-S3, ESP32-C6, ESP32-H2, ESP32-P4, ESP32-C5, ESP32-C61
- **Key Components**:
  - Camera drivers (esp_driver_cam)
  - ISP (Image Signal Processor)
  - JPEG encode/decode
  - LCD display support
  - WiFi and Bluetooth connectivity

### Hardware Features

- **Camera Interface**: DVP (Digital Video Port) and CSI (Camera Serial Interface)
- **Image Processing**: Hardware ISP with auto-exposure, auto-white-balance
- **Display**: LCD controller with DMA support
- **Connectivity**: WiFi 6, Bluetooth 5.0, ESP-NOW
- **Storage**: SD card, SPI flash, external RAM support

## Development Workflow

### 1. Create New Project

```bash
./build.sh create my_camera_app
cd projects/my_camera_app
```

### 2. Configure Hardware

```bash
# Set target chip
./build.sh build my_camera_app setup

# Configure peripherals
./build.sh build my_camera_app menuconfig
```

### 3. Develop Application

Edit files in `main/` directory:
- `main.c` - Main application logic
- `CMakeLists.txt` - Build dependencies

### 4. Build and Test

```bash
# Build project
./build.sh build my_camera_app build

# Flash and monitor
./build.sh build my_camera_app all
./build.sh build my_camera_app monitor
```

### 5. Optimization

```bash
# Analyze build size
./build.sh build my_camera_app size

# Component analysis
./build.sh build my_camera_app size-components
```

## Advanced Features

### Web UI Integration

Projects can include web-based configuration interfaces:

```bash
# UI is automatically built if ui/ directory exists
# with build.js and package.json files
./build.sh build <project> build
```

### Encryption Support

```bash
# Flash encrypted firmware
./build.sh build <project> encrypted-flash
```

### Partition Management

```bash
# Flash specific partitions
./build.sh build <project> bootloader-flash
./build.sh build <project> partition-table-flash
```

## Performance Optimization

### Camera Performance

- **High FPS**: Asynchronous camera/AI processing
- **Memory Management**: Efficient buffer allocation
- **Hardware Acceleration**: ISP and JPEG hardware support

### AI Model Performance

- **Quantization**: 8-bit integer models for faster inference
- **Memory Optimization**: Optimized memory allocation
- **Parallel Processing**: Camera capture during model inference

## Troubleshooting

### Common Issues

1. **Build Errors**: Ensure ESP-IDF environment is properly sourced
2. **Flash Errors**: Check USB connection and chip target
3. **Camera Issues**: Verify camera module connections
4. **Memory Issues**: Adjust partition table or enable PSRAM

### Debug Commands

```bash
# Full clean and rebuild
./build.sh build <project> fullclean
./build.sh build <project> all

# Erase flash completely
./build.sh build <project> erase-flash

# Monitor with custom baud rate
idf.py monitor -p /dev/ttyUSB0 -b 921600
```

## Contributing

1. Create projects in the `projects/` directory
2. Include proper `project.json` metadata
3. Follow ESP-IDF coding standards
4. Test on supported hardware platforms

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [ESP-WHO Documentation](https://github.com/espressif/esp-who)
- [ESP32-S3-EYE User Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-eye/)
- [ESP32-P4 Function EV Board Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/)
- [LVGL Documentation](https://docs.lvgl.io/)

## License

This project follows the ESP-IDF and ESP-WHO licensing terms. See individual component licenses for details.