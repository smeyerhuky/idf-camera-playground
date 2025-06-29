# Timelapse Camera with Audio Recording

Advanced timelapse camera system for XIAO ESP32-S3 Sense with synchronized audio recording, automatic time synchronization, and organized file storage.

## Features

- **3-Second Timelapse Clips**: Captures video frames every 10 seconds for 3-second duration
- **Audio Recording**: Synchronized WAV audio recording using built-in PDM microphone
- **WiFi Time Sync**: Automatic NTP time synchronization on startup and daily resync
- **Organized Storage**: Files stored in `timelapse_data/YYYY/MM/DD/` structure
- **JSON Manifest**: Complete recording manifest at `timelapse_data/manifest.json`
- **Power Efficient**: WiFi disconnects after time sync to save power
- **Robust Operation**: Continues recording even without WiFi/time sync

## Hardware Requirements

- XIAO ESP32-S3 Sense development board
- MicroSD card (up to 32GB)
- WiFi network for time synchronization

## Configuration

Edit `main/wifi_config.h` with your network credentials:

```c
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
#define NTP_SERVER     "pool.ntp.org"
#define GMT_OFFSET_SEC 0              // UTC offset in seconds
#define DAYLIGHT_OFFSET_SEC 0         // Daylight saving offset
```

## Build and Flash

```bash
# From project root
./build.sh build burst_cam setup
./build.sh build burst_cam build
./build.sh build burst_cam flash
./build.sh build burst_cam monitor
```

## File Organization

```
SD Card Structure:
├── timelapse_data/
│   ├── manifest.json              # Master index of all recordings
│   ├── 2024/                     # Year folders
│   │   ├── 01/                   # Month folders
│   │   │   ├── 15/               # Day folders
│   │   │   │   ├── clip_143052_0001.jpg    # Video frame
│   │   │   │   ├── audio_143052_0001.wav   # Audio file
│   │   │   │   ├── clip_143102_0002.jpg    # Next clip
│   │   │   │   └── audio_143102_0002.wav
│   │   │   └── 16/
│   │   └── 02/
│   └── no_time/                  # Fallback when time sync fails
│       ├── clip_0001_frame_000.jpg
│       └── audio_0001.wav
```

## Recording Pattern

- **Capture Interval**: 10 seconds between sessions
- **Capture Duration**: 3 seconds per session
- **Video**: Single JPEG frame per 3-second session (at start)
- **Audio**: Continuous WAV recording during 3-second session
- **Time Sync**: Every 24 hours or on startup

## Technical Details

### Audio Recording
- **Microphone**: Built-in PDM microphone (MSM261D3526H1CPM)
- **Sample Rate**: 16 kHz
- **Format**: 16-bit mono WAV
- **Interface**: I2S PDM (GPIO 42/41)

### Camera Settings
- **Resolution**: VGA (640x480)
- **Format**: JPEG
- **Quality**: 12 (configurable)

### Time Synchronization
- **NTP Server**: pool.ntp.org (configurable)
- **Sync Frequency**: Every 24 hours
- **Fallback**: Sequential naming without time sync

## System Architecture

### Components

1. **camera_module**: OV2640 camera interface
2. **sdcard_module**: SD card file operations
3. **time_sync**: WiFi and NTP time synchronization
4. **audio_recorder**: PDM microphone I2S interface
5. **manifest_manager**: JSON-based file indexing

### Main Application Flow

1. Initialize NVS, camera, SD card, audio, and time sync
2. Connect to WiFi and perform initial time synchronization
3. Disconnect WiFi to save power
4. Start timelapse capture task:
   - Create timestamped directory structure
   - Record 3-second audio+video session
   - Save files with timestamp naming
   - Update manifest
   - Sleep for 7 seconds
   - Repeat

## Power Management

- WiFi enabled only during time synchronization
- Deep sleep between captures (when implemented)
- Efficient I2S audio recording
- Minimal camera power usage

## Manifest Format

```json
{
  "videos": [
    {
      "filename": "clip_143052_0001.jpg",
      "path": "2024/01/15/clip_143052_0001.jpg",
      "timestamp": 1705328652,
      "size": 25340,
      "duration_ms": 3000
    }
  ],
  "total_count": 1,
  "created_timestamp": 1705328652
}
```

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

### Audio Pins (PDM)
- PDM CLK: GPIO 42
- PDM DATA: GPIO 41

## Troubleshooting

### Time Sync Issues
- Check WiFi credentials in `wifi_config.h`
- Verify network connectivity
- System continues recording without time sync

### Audio Issues
- Ensure J1/J2 jumpers are connected (enables microphone)
- Check I2S GPIO configuration
- Audio recording is optional - system works without it

### SD Card Issues
- Format SD card as FAT32
- Check SPI connections
- Verify card capacity (max 32GB)

## Future Enhancements

- Video encoding (MP4 with audio)
- Motion detection triggers
- Web interface for configuration
- Battery monitoring
- Remote download capabilities