# ESP32 Hexagons LED Controller

## Overview

Wi-Fi based LED controller for modular hexagonal LED panels.
The ESP32 firmware provides LED control and web interface for configuration.

Project focus on reliable OTA updates. Firmware versions are verified with a local server before installation. After updating, the device runs automatic tests to check if everything works correctly.
If any test fails or the device crashes, the ESP sends an HTTP request to the server and performs a rollback to the previous firmware version.

## Tech Stack

- ESP32
- Embedded C with ESP-IDF
- FreeRTOS
- Wi-Fi
- HTTP server
- NVS
- OTA firmware updates

## Gallery

### Main UI
![Main UI](images/main-ui.jpg)

### Configuration page
![Configuration](images/config-ui.jpg)
