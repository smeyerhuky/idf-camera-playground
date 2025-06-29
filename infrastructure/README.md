# Timelapse Monitoring Infrastructure

## Quick Start

**Start the complete monitoring stack:**
```bash
cd infrastructure
docker-compose up -d
```

**Access dashboards:**
- **Grafana**: http://localhost:3000 (admin/admin123)
- **Node-RED**: http://localhost:1880
- **InfluxDB**: http://localhost:8086 (admin/password123)

## Architecture

```
[ESP32-S3 Devices] --ESP-NOW--> [ESP32-S3 Gateway] --MQTT--> [Docker Stack]
                                                              |
                                                              v
┌─────────────────────────────────────────────────────────────────┐
│                        Docker Stack                            │
├─────────────────┬─────────────────┬─────────────────┬─────────┤
│   Mosquitto     │    Telegraf     │    InfluxDB     │ Grafana │
│  MQTT Broker    │ MQTT→InfluxDB   │  Time-series    │Dashboard│
│   Port: 1883    │     Bridge      │   Database      │Port:3000│
└─────────────────┴─────────────────┴─────────────────┴─────────┘
```

## What You'll Monitor

**Device Metrics:**
- Battery voltage/current
- WiFi signal strength
- SD card usage
- Memory consumption
- Capture timing
- Error rates

**System Events:**
- Boot/sleep cycles
- Capture successes/failures
- Network connectivity
- Storage issues

**Performance:**
- Power consumption trends
- Capture frequency
- Data transfer rates
- System uptime

## Current Status

⚠️ **Phase 1**: ESP32-S3 devices with stub implementations
✅ **Infrastructure**: Docker stack ready
⏳ **Phase 2**: Need ESP32-S3 gateway device
⏳ **Phase 3**: Full ESP-NOW + MQTT integration

## Manual Testing

**View MQTT messages:**
```bash
# Subscribe to all timelapse topics
docker exec -it timelapse-mqtt mosquitto_sub -t "timelapse/+/+"

# Publish test message
docker exec -it timelapse-mqtt mosquitto_pub -t "timelapse/device01/metrics" -m '{"battery_mv":3800,"temp_c":24.5}'
```