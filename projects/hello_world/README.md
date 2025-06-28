# Hello World

Basic hello world application for ESP32-S3 demonstrating the idf-camera build system.

## Build Instructions

```bash
# From repository root
./build.sh build hello_world

# Or from project directory
./tools/build.sh build
```

## Full Workflow

```bash
# Setup target
./build.sh build hello_world setup

# Build project
./build.sh build hello_world build

# Flash to device
./build.sh build hello_world flash

# Monitor output
./build.sh build hello_world monitor

# Build and flash in one command
./build.sh build hello_world all
```