# Hello WiFi

WiFi-enabled ESP32-S3 application with system stats API and SD card logging.

## Features

- WiFi connectivity (connects to configured SSID/password)
- HTTP server with `/stats` endpoint
- SD card logging of IP address for debugging
- System statistics including:
  - CPU information
  - Memory usage (heap and PSRAM)
  - Temperature sensor reading
  - Flash size
  - Uptime

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
Returns JSON with system statistics:
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
  "temperature_c": 45.2,
  "uptime_ms": 123456789
}
```

## Build Instructions

```bash
# From repository root
./build.sh build hello_wifi

# Or from project directory
./tools/build.sh build
```

## Full Workflow

```bash
# Setup target
./build.sh build hello_wifi setup

# Build project
./build.sh build hello_wifi build

# Flash to device
./build.sh build hello_wifi flash

# Monitor output
./build.sh build hello_wifi monitor

# Build and flash in one command
./build.sh build hello_wifi all
```

## Files Created

- `/sdcard/ip_address.txt` - Contains the assigned IP address and SSID

## Usage

1. Flash the application to ESP32-S3
2. Insert SD card (optional, for IP logging)
3. Device will connect to WiFi network
4. Check serial monitor for IP address
5. Access `http://<IP_ADDRESS>/stats` for system information