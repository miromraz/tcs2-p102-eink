# E-Ink Dashboard: Victron & Weather Monitoring

Low-power always-on dashboard displayed on a 10.2" e-ink screen, powered by an ESP32-S3 with WiFi.

## Overview

This project uses the XIAO ESP32-S3 Sense to fetch data from a Victron energy system and weather services, then renders a dashboard on a large 1024x1280 e-ink display. E-ink is ideal for this: zero power draw to maintain the image, readable in direct sunlight, and a paper-like aesthetic.

## Planned Features

### Victron System Monitoring
- Battery state of charge (%) and voltage
- Solar PV input power and daily yield
- AC load consumption
- Inverter/charger status
- Historical graphs (daily/weekly energy flow)

### Weather
- Current conditions (temperature, humidity, wind)
- Multi-day forecast
- Sunrise/sunset times

### Dashboard Layout
- 1024x1280 pixels at 160 DPI — room for dense, well-typeset information
- Partial refresh (~1s) for frequent data updates without full-screen flash
- Full refresh on a schedule to prevent ghosting

## Architecture

```
┌──────────────┐     WiFi      ┌──────────────────┐
│  Victron GX  │◄─────────────►│                  │
│  (VRM API)   │               │  XIAO ESP32-S3   │
└──────────────┘               │                  │     SPI      ┌─────────────┐
                               │  - Fetch data    │─────────────►│  TCS2-P102  │
┌──────────────┐               │  - Render layout │              │  Controller │
│ Weather API  │◄─────────────►│  - Manage sleep  │              └──────┬──────┘
│ (OpenWeather │               │                  │                     │
│  / yr.no)    │               └──────────────────┘              ┌──────┴──────┐
└──────────────┘                                                 │  10.2" EPD  │
                                                                 │ 1024x1280   │
                                                                 └─────────────┘
```

## Development Phases

### Phase 1: Display Driver (current)
Get the e-ink display working on the ESP32-S3. Checkerboard test pattern and serial image upload.

**Status**: Firmware built, awaiting hardware test.

### Phase 2: Dashboard
- WiFi connectivity and NTP time sync
- Victron VRM API integration (or local MQTT from GX device)
- Weather API integration
- On-device framebuffer rendering (text, numbers, simple charts)
- Refresh scheduling with deep sleep between updates

## Hardware

- Seeed Studio XIAO ESP32-S3 Sense
- MpicoSys TCS2-P102-231 timing controller
- EZ102CT011 10.2" e-ink panel (1024x1280, 160 DPI)

See [README.md](README.md) for wiring and build instructions.
