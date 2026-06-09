# HwMonitorKey — Wireless USB HID Bridge

A hardware project built on dual **ESP32-S3** microcontrollers that wirelessly bridges a USB keyboard to a PC, with a real-time web monitoring dashboard.

## Overview

Two ESP32-S3 boards communicate over **ESP-NOW** (Wi-Fi based, no router needed):

| Board | Role |
|-------|------|
| **Host** | Reads USB keyboard via USB OTG → transmits HID reports over ESP-NOW |
| **Device** | Receives HID reports → forwards to PC as native USB keyboard + hosts web monitor |

## Features

- 🔌 **USB HID Passthrough** — Transparent relay; the PC sees a native keyboard
- 📡 **ESP-NOW Wireless** — Low-latency, peer-to-peer, no Wi-Fi router required
- 🌐 **Web Monitor Dashboard** — Real-time keystroke stream via WebSocket
- 📊 **Live Statistics** — Keys/min, session duration, most-pressed key
- 📁 **CSV Export** — Download full keystroke log
- ⌨️ **Remote Inject** — Send keystrokes to PC directly from browser textarea
- 🔔 **Audio Feedback** — Optional beep on key detection
- 🛡️ **Watchdog Timer** — Auto-releases all keys on connection timeout

## Hardware Requirements

- 2× ESP32-S3 DevKitC-1
- USB OTG cable (for Host board)
- USB cable (for Device board → PC)

## Tech Stack

- **Platform:** PlatformIO + Arduino framework
- **Wireless:** ESP-NOW (Espressif)
- **USB Host:** EspUsbHost library
- **USB Device:** TinyUSB HID
- **Web:** Async HTTP + WebSocket server

## Architecture

```
[USB Keyboard]
      │ USB OTG
[ESP32-S3 Host] ──ESP-NOW──▶ [ESP32-S3 Device] ──USB──▶ [PC]
                                      │
                               Wi-Fi AP + WebSocket
                                      │
                              [Browser Dashboard]
```

## Getting Started

1. Clone the repo and open in PlatformIO
2. Flash `host_main.cpp` to the **Host** board
3. Flash `device_main.cpp` to the **Device** board
4. Connect Device board to PC via USB
5. Open browser → `http://192.168.4.1` for the live dashboard

## Project Status

> EducationTerm project — built for learning ESP-NOW, USB HID, and embedded web servers on ESP32-S3.

## License

MIT
