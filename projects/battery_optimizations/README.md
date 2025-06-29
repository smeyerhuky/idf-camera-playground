# Battery Optimizations v2.0 - Thermal Camera + WiFi

**Thermal-optimized camera and WiFi application for ESP32-S3 with adaptive capture rates, temperature monitoring, and comprehensive power management.**

> 🔥 **Thermal Issue Solved**: This project addresses the critical 60°C+ overheating issue by implementing:
> - **Adaptive Camera Capture**: Automatically reduces photo frequency when hot
> - **Temperature-Tagged Photos**: Each photo filename includes current temperature
> - **Comprehensive Power Management**: CPU, WiFi, and PSRAM optimizations

## Key Optimizations vs. hello_wifi

### 🔋 Power Management
- **CPU Frequency**: Reduced from 240MHz to 80MHz (`CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ_80=y`)
- **Power Management**: Enabled automatic frequency scaling (`CONFIG_PM_ENABLE=y`)
- **SPIRAM Speed**: Reduced from 80MHz to 40MHz for lower power consumption

### 📡 WiFi Optimizations
- **TX Power**: Reduced from 19.5dBm to 11dBm (WiFi transmission power)
- **Buffer Pool**: Reduced WiFi buffers from 32 to 16 (TX/RX)
- **Window Size**: Reduced aggregation windows from 6 to 4

### ⚡ Task Scheduling
- **LED Task**: Reduced from 10ms to 100ms update frequency
- **Main Loop**: Reduced logging from 10s to 30s intervals
- **Temperature Sensor**: Singleton pattern prevents install/uninstall overhead

### 🌡️ Expected Results
- **Temperature Drop**: 15-20°C reduction (from 60°C+ to ~40-45°C)
- **Battery Life**: 2-3x improvement in battery-powered scenarios
- **Performance**: Minor reduction acceptable for thermal gains

## Features

### 📸 **Thermal-Aware Camera System**
- **OV2640 Camera**: 1600x1200 JPEG capture with optimized quality
- **Adaptive Capture Rate**: 10s normal, 30s when temperature > 50°C
- **Temperature-Tagged Photos**: `photo_0001_42C.jpg` format for monitoring
- **Automatic Throttling**: Reduces camera activity during thermal stress

### 🌐 **Optimized WiFi & Web Server**
- WiFi connectivity with reduced power consumption (11dBm TX power)
- HTTP server with `/stats` endpoint including camera and thermal status
- Real-time temperature monitoring and reporting

### 💾 **Enhanced SD Card Operations**
- **Proven GPIO Configuration**: Uses verified XIAO ESP32S3 Sense pinout
- **Photo Storage**: JPEG files with temperature info
- **System Logging**: IP address, thermal status, and configuration data
- **Modular Design**: Robust sdcard_module with proper error handling

### 📊 **System Monitoring**
- CPU information (optimized 80MHz frequency)
- Memory usage (heap and PSRAM)
- Temperature sensor reading (singleton pattern)
- Flash size, uptime, and photo statistics
- Power status and thermal optimization indicators

## Configuration

WiFi credentials are configured in `main/main.c`:
- SSID: `pianomaster`
- Password: `happyness`

## SD Card Setup

The application expects an SD card connected via SPI:
- MOSI: GPIO 15
- MISO: GPIO 2
- CLK: GPIO 14
- CS: GPIO 13

## API Endpoints

### GET /stats
Returns JSON with system statistics including thermal optimization status:
```json
{
  "chip": "ESP32-S3",
  "cores": 2,
  "revision": 0,
  "flash_size_mb": 4,
  "heap_free": 123456,
  "heap_total": 345678,
  "psram_free": 8388608,
  "psram_total": 8388608,
  "temperature_c": 42.3,
  "uptime_ms": 123456789,
  "usb_powered": true
}
```

## Build Instructions

```bash
# From repository root
./build.sh build battery_optimizations

# Or from project directory
./tools/build.sh build
```

## Full Workflow

```bash
# Setup target
./build.sh build battery_optimizations setup

# Build project
./build.sh build battery_optimizations build

# Flash to device
./build.sh build battery_optimizations flash

# Monitor output
./build.sh build battery_optimizations monitor

# Build and flash in one command
./build.sh build battery_optimizations all
```

## Files Created on SD Card

### 📸 **Camera Captures**
- `photo_0000_42C.jpg` - JPEG photos with temperature in filename
- `photo_0001_45C.jpg` - Temperature indicates thermal conditions during capture
- `photo_0002_38C.jpg` - Adaptive timing based on thermal status

### 📝 **System Information**
- `ip_address.txt` - Network info, server URL, camera status, and power details
- `stats.json` - Real-time system statistics updated via `/stats` endpoint

## Usage

1. Flash the optimized application to ESP32-S3
2. Insert SD card (optional, for logging)
3. Device will connect to WiFi network with reduced power consumption
4. Check serial monitor for IP address
5. Access `http://<IP_ADDRESS>/stats` for system information
6. Monitor temperature - should be significantly lower than original

## Technical Changes Summary

### Configuration Changes (`sdkconfig.defaults`)
```bash
# Power Management - THERMAL OPTIMIZATION
CONFIG_PM_ENABLE=y
CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ_80=y

# WiFi Configuration - OPTIMIZED FOR LOWER POWER
CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM=16  # Was 32
CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM=16  # Was 32

# PSRAM - REDUCED SPEED FOR THERMAL OPTIMIZATION
CONFIG_SPIRAM_SPEED_40M=y  # Was 80M
```

### Code Changes (`main/main.c`)
- WiFi TX power reduced: `esp_wifi_set_max_tx_power(44)` (11dBm vs 19.5dBm)
- LED task frequency: 100ms intervals (was 10ms)
- Main loop: 30s logging intervals (was 10s)
- Temperature sensor: Singleton pattern for efficiency
- Updated logging to indicate thermal optimization status

## Original Project Reference

This project is based on and optimized from: `projects/hello_wifi/`

**Problem Solved**: Original project reached dangerous 60°C+ temperatures
**Solution Applied**: Comprehensive thermal and power optimizations
**Expected Outcome**: 15-20°C temperature reduction with minimal performance impact