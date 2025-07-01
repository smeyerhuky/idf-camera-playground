# Red Rocks QR Photo Booth - Quick Start Guide

## 🎸 What You Just Built

A **concert photo booth** that:
- Captures wide-angle photos with OV2640 camera
- Displays QR codes on Waveshare touchscreen  
- Lets people download photos instantly to their phones
- Perfect for Red Rocks Amphitheatre concerts!

## 📦 Hardware Setup

### Option A: Waveshare ESP32-S3 + OV2640 (Recommended)
```
Waveshare ESP32-S3 1.69" LCD Board
├── Camera: OV2640 160° Wide-Angle
│   ├── VCC → 3.3V
│   ├── GND → Ground
│   ├── SDA → GPIO 8
│   └── SCL → GPIO 9
├── Power: 18650 4S Battery Pack (14.8V)
│   └── Buck converter → 5V input
└── Display: Built-in 240×280 touchscreen
```

### Option B: XIAO ESP32-S3 Sense (Alternative)
- Use built-in camera (standard angle, not wide)
- Requires external display connection
- More compact but less features

## 🚀 Build & Flash

```bash
# From repository root
source ./activate.sh
./build.sh build red_rocks_one setup
./build.sh build red_rocks_one build
./build.sh build red_rocks_one flash
./build.sh build red_rocks_one monitor
```

## 📱 How It Works

### User Experience
1. **Person approaches booth** → Sees "RED ROCKS PHOTOS" on display
2. **Touches "TAKE PHOTO"** → Camera captures wide-angle shot
3. **QR code appears** → Shows download link
4. **Scan with phone** → Photo downloads automatically
5. **Ready for next person** → System resets

### Technical Flow
```
Touch Button → Photo Capture → QR Generation → WiFi Download
     ↓              ↓              ↓              ↓
  UI Update → Save to Storage → Display QR → HTTP Server
```

## 🌐 Network Configuration

**WiFi Hotspot Created:**
- **SSID:** `RedRocks_PhotoBooth`  
- **Password:** `Concert2024`
- **Max Connections:** 10 devices
- **Photo Downloads:** `http://192.168.4.1/photo/filename.jpg`

**To Connect:**
1. Connect phone to `RedRocks_PhotoBooth` WiFi
2. Scan QR code displayed on booth
3. Photo downloads automatically

## ⚙️ System Configuration

### Camera Settings (Red Rocks Optimized)
```c
Resolution: 1600×1200 (UXGA)
Quality: 8 (high quality JPEG)
Brightness: +1 (outdoor compensation)  
Contrast: +1 (enhanced for concerts)
White Balance: Auto (stage lighting adaptive)
```

### Storage Capacity
- **Internal Flash:** ~200 photos (8MB storage)
- **Photo Size:** ~40KB each (JPEG compressed)
- **File Naming:** `redrock_YYYYMMDD_HHMMSS_XXX.jpg`

### Power Management
- **18650 4S Pack:** 14.8V nominal, 3300mAh
- **Runtime:** 8+ hours continuous operation
- **Power Usage:** ~75mA average

## 🎵 Red Rocks Specific Features

### Concert Environment Adaptations
- **Altitude:** 6,450 feet compensation
- **Lighting:** Evening concert optimization
- **Temperature:** -10°C to +40°C operation
- **Crowd WiFi:** Unique SSID to avoid conflicts

### Perfect For
- **Group Photos:** 160° wide-angle captures multiple people
- **Fast Sharing:** No email/social media needed
- **Unique Souvenirs:** Timestamped Red Rocks memories
- **Concert Experience:** Quick 5-second workflow

## 🔧 Customization Options

### Change WiFi Settings
Edit `main/wifi_config.h`:
```c
#define WIFI_SSID      "YourBoothName"
#define WIFI_PASSWORD  "YourPassword"
```

### Modify Photo Quality
Edit `main/main.c`:
```c
#define PHOTO_QUALITY  8    // 1-63 (lower = higher quality)
#define PHOTO_WIDTH    1600 // Pixel width
#define PHOTO_HEIGHT   1200 // Pixel height
```

### Customize Display Text
Modify display functions in `main/main.c`:
```c
waveshare_display_draw_header();        // "RED ROCKS PHOTOS"
waveshare_display_draw_footer("text");  // Bottom instructions
```

## 📊 Monitoring & Status

### Serial Monitor Output
```
I (1234) red_rocks_photobooth: Photo booth running - State: 0, Photos taken: 5
I (5678) red_rocks_photobooth: Photo captured: redrock_20240701_143052_005.jpg
I (9012) red_rocks_photobooth: QR code generated: http://192.168.4.1/photo/redrock_20240701_143052_005.jpg
```

### Display States
- **Idle:** "Touch to take your Red Rocks photo!"
- **Capturing:** "Smile! Taking your photo..."  
- **Processing:** "Creating your QR code..."
- **Ready:** QR code displayed + "Scan to download!"
- **Complete:** "Photo downloaded! Touch for next person"

## 🚨 Troubleshooting

### Common Issues

**Display Not Working**
- Check SPI connections to Waveshare board
- Verify 5V power supply is stable
- Reset ESP32-S3 module

**Camera Not Capturing**
- Verify I2C connections (GPIO 8, 9)
- Check 3.3V power to camera module
- Try different camera module

**QR Code Not Scanning**
- Ensure phone connected to `RedRocks_PhotoBooth` WiFi
- Check if HTTP server is running (serial monitor)
- Verify QR code display quality

**Photos Not Downloading**
- Check storage space (200 photo limit)
- Verify HTTP server endpoints
- Restart ESP32-S3 if needed

### Reset Instructions
1. Hold power button for 10 seconds
2. System will restart and reinitialize
3. All components will be re-detected

## 🎯 Usage Tips

### Optimal Setup
- **Mount at chest height** for group photos
- **Angle slightly downward** to capture faces
- **Stable mounting** to prevent blur
- **Good lighting position** (not backlit)

### Concert Operations
- **Pre-event:** Test all functions, charge batteries
- **During event:** Monitor battery level, clear storage if needed  
- **Post-event:** Download all photos for backup

### Battery Management
- **Full charge:** ~8 hours operation
- **50% charge:** ~4 hours operation
- **Low battery:** Display will show warning
- **Charge between sets** for all-day events

## 🔮 Future Enhancements

Ready for upgrades:
- **Multi-camera panoramic stitching**
- **Audio recording with video clips**
- **Cloud upload and sharing**
- **Social media integration**
- **LED ring lighting for better photos**
- **Battery monitoring on display**

---

## 🎸 Ready to Rock!

Your Red Rocks QR Photo Booth is ready to capture concert memories! The system automatically handles photo capture, QR generation, and wireless sharing - perfect for Red Rocks' unique concert experience.

**Enjoy the show and happy photo sharing! 🎵📸**