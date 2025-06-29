# SD Card Tester

A simple ESP-IDF project for testing SD card functionality on the XIAO ESP32S3 Sense board.

## Features

- **SD Card Initialization**: Properly configures SPI interface with verified pin mappings
- **Statistics Collection**: Gathers comprehensive SD card information (size, usage, free space)
- **File Operations**: Creates timestamped stats files on the SD card
- **Full SD Card Handling**: Automatically handles full SD cards by listing files and cleaning up old stats
- **Periodic Monitoring**: Continuously monitors and logs SD card status

## Hardware Configuration

**Verified SD Card Pin Configuration for XIAO ESP32S3 Sense:**
- MISO: GPIO 8
- MOSI: GPIO 9  
- SCLK: GPIO 7
- CS: GPIO 21
- SPI Frequency: 10MHz (conservative for reliability)

## Usage

### Build and Flash
```bash
./build.sh build sd_card_tester setup    # Configure for ESP32-S3
./build.sh build sd_card_tester build    # Build the project  
./build.sh build sd_card_tester flash    # Flash to device
./build.sh build sd_card_tester monitor  # Monitor serial output
```

### Alternatively, using standard ESP-IDF commands:
```bash
cd projects/sd_card_tester
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## Output

The application will:

1. Initialize the SD card and display detailed information
2. List all files currently on the SD card
3. Display comprehensive statistics (size, usage, free space)
4. Attempt to create a timestamped stats file
5. If the SD card is full, automatically clean up old stats files and retry
6. Continue monitoring and logging free space every 10 seconds

## Error Handling

- **Full SD Card**: Automatically detects when SD card is full and attempts cleanup
- **Missing SD Card**: Gracefully handles cases where no SD card is present
- **Write Failures**: Provides detailed error messages with troubleshooting information

## Files Created

The application creates files named: `sd_stats_<timestamp>.txt`

Example content:
```
SD Card Statistics
==================
Timestamp: 1672531200
Card Present: Yes
Initialized: Yes
Card Size: 15193 MB
Total Bytes: 15923150848
Used Bytes: 15923118080
Free Bytes: 32768
Usage Percentage: 100.00%
```

## Project Structure

```
sd_card_tester/
├── main/
│   ├── main.c          # Main application logic
│   ├── sd_tester.h     # SD card tester header
│   ├── sd_tester.c     # SD card implementation
│   └── CMakeLists.txt  # Component configuration
├── tools/build.sh      # Build script
├── project.json        # Project metadata
├── CMakeLists.txt      # Project configuration
└── README.md          # This file
```

## Troubleshooting

- **SD Card Not Detected**: Verify connections and try reformatting the SD card (FAT32)
- **Write Failures**: Check if SD card is write-protected or full
- **Build Errors**: Ensure ESP-IDF environment is properly activated with `source ./activate.sh`