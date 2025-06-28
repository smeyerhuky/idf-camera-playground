# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

**Environment Setup** (required for each session):

```bash
source ./activate.sh  # Sources ESP-IDF and sets environment variables
```

**Project Management**:

```bash
./build.sh list                    # List all discovered projects
./build.sh create <name>           # Create new project from template
./build.sh config <project>        # Interactive project configuration
```

**Build Commands**:

```bash
./build.sh build <project> setup   # Configure target chip (esp32s3)
./build.sh build <project> build   # Build project (includes UI if present)
./build.sh build <project> flash   # Flash to device
./build.sh build <project> monitor # Serial monitor
./build.sh build <project> all     # Build and flash
```

**Testing Commands**:

```bash
# No unified test command - testing varies by component
# For ESP-IDF component tests: cd esp-idf && python -m pytest <component>/test_apps/
# For hardware testing: requires pytest-embedded with actual hardware
```

**Linting Commands**:

```bash
# Python linting in ESP-IDF
cd esp-idf && python -m ruff check .
cd esp-idf && python -m ruff format .
```

**Analysis Commands**:

```bash
./build.sh build <project> size              # Analyze build size
./build.sh build <project> size-components   # Component size analysis
./build.sh build <project> menuconfig        # ESP-IDF configuration menu
```

## Architecture

This is an **ESP-IDF Camera Development Framework** combining ESP-IDF v5.5-dev with ESP-WHO computer vision capabilities for ESP32-S3 microcontrollers.

### Core Components

**Repository Structure**:

- `esp-idf/` - ESP-IDF framework (git submodule, v5.4 release branch)
- `esp-who/` - ESP-WHO computer vision library (git submodule, master branch)
- `projects/` - User projects with standardized metadata (`project.json`)
- `build.sh` - Central build system with auto-discovery
- `activate.sh` - Environment setup script

**Project Discovery System**:

- Projects auto-discovered via `project.json` metadata files
- Centralized build system supports all ESP32 variants
- Each project specifies target chip, features, and available commands

### Development Workflow

**Environment Management**:

1. Run `./prepare.sh` once to initialize submodules and install ESP-IDF tools
2. Run `source ./activate.sh` at start of each development session
3. Use `./build.sh` commands for all project operations

**Project Structure**:

```txt
projects/<project_name>/
├── main/                  # Main application code
├── components/            # Project-specific components
├── tools/build.sh        # Project build script (delegates to main)
├── CMakeLists.txt        # ESP-IDF build configuration
├── project.json          # Project metadata (required)
├── sdkconfig             # ESP-IDF configuration
└── README.md
```

**Web UI Integration**:

- If project has `ui/` directory with `build.js`, web interface is auto-built
- UI build creates C header files embedded in firmware
- Supports React-based interfaces with automated asset generation

### Hardware Targets

**Supported Development Boards**:

- ESP32S3 XIAO Sense (OV2640/OV5640 camera, digital microphone, 8MB PSRAM, SD card slot)

**Camera Features**:

- OV2640/OV5640 camera modules with up to 1600x1200 resolution
- JPEG, RGB565, and grayscale pixel formats
- Frame buffer storage in 8MB PSRAM
- Multiple frame buffer support for smooth video capture
- Face detection, recognition, pedestrian detection, QR code scanning

### Important Notes

**Project Metadata Required**:
Every project must have `project.json` with:

```json
{
  "name": "project_name",
  "description": "Project description", 
  "target": "esp32s3",
  "build": {
    "commands": ["setup", "build", "flash", "monitor", "all"]
  }
}
```

**Build System Features**:

- Automatic ESP-IDF environment sourcing
- Multi-target chip support (esp32, esp32s3, etc.)
- Interactive configuration via `menuconfig`
- Build size analysis and optimization tools
- Encryption and OTA update support

**Testing Infrastructure**:

- Unity testing framework for unit tests
- pytest-embedded for hardware-in-the-loop testing
- Component-specific test applications in ESP-IDF

**Submodule Management**:

- ESP-IDF and ESP-WHO managed as git submodules
- Use `./prepare.sh update` to update all submodules
- Manual updates: `git submodule update --remote esp-idf`

## Team Expertise: XIAO ESP32S3 Sense Development

### Specialized Development Expertise
- **Core Focus**: ESP-IDF development for XIAO ESP32S3 Sense platform
- **Key Domains**:
  * ESP-IDF Framework Architecture
  * XIAO Hardware Integration
  * Computer Vision & Audio Processing
  * Wireless Connectivity
  * Low-Power Systems Engineering
  * Security & Encryption
  * Embedded Storage Optimization

### Technical Capabilities
- **Hardware Optimization**:
  * Camera (OV2640) integration
  * Digital microphone configuration
  * PSRAM utilization
  * Power management strategies
  * GPIO and peripheral configuration

- **Software Development**:
  * FreeRTOS task management
  * Real-time processing
  * Wireless protocol implementation
  * Edge AI and machine learning inference
  * Secure boot and encryption

- **Development Tools**:
  * ESP-IDF v5.x
  * ESP-WHO library
  * CMake build system
  * JTAG debugging
  * OTA update mechanisms
  * Performance profiling tools

### Supported Libraries & Frameworks
- ESP-IDF Core Libraries
- ESP-WHO Computer Vision
- TensorFlow Lite Micro
- FreeRTOS
- LittleFS/SPIFFS
- ESP-NOW
- ESP-MESH
- ESP Cryptographic Libraries

### Development Principles
- Hardware-aware design
- Power-efficient architecture
- Security-first implementation
- Production-ready code
- Modular and maintainable solutions