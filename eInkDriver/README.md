

# E-Ink Smart Display Driver

A comprehensive Arduino firmware for an ESP32-based 7.5-inch e-ink display with Home Assistant integration, battery monitoring, and multi-page support.

## Overview

This project drives a large e-ink display with WiFi connectivity, Home Assistant MQTT support, and local image serving. The display supports grayscale dithering, battery-aware deep sleep, and button-controlled page navigation.

## Hardware Requirements

- **Microcontroller**: ESP32 (tested on variant with ADC)
- **Display**: 7.5-inch e-ink panel (800×480 resolution, 4-level grayscale) https://www.seeedstudio.com/TRMNL-7-5-Inch-OG-DIY-Kit-p-6481.html
- **Power**: 4.2V battery with voltage divider to ADC
- **Input**: 3 push buttons on GPIO pins
- **GPIO Pins**:
    - `BUTTON_1_PIN` (D1): Next page
    - `BUTTON_2_PIN` (D2): Refresh current page
    - `BUTTON_3_PIN` (D4): Previous page
    - `BATTERY_PIN` (GPIO1): Battery voltage ADC input
    - `ADC_EN_PIN` (GPIO6): ADC enable control
## Installation Example:
<img width="682" height="748" alt="image" src="https://github.com/user-attachments/assets/0463f124-52ee-4d37-9273-436bdb84fe69" />
<img width="1296" height="1081" alt="image" src="https://github.com/user-attachments/assets/5a092b0b-cf0b-4783-8531-a94d558e57ff" />


## Software Dependencies

### Arduino Libraries
- `WiFi.h` – Network connectivity
- `HTTPClient.h` – Download images and check updates
- `TFT_eSPI.h` – Display driver library
- `PubSubClient.h` – MQTT client
- `Preferences.h` – Persistent storage (ESP32 NVS)

### Custom Libraries
- `HaMqttEntities.h` – Home Assistant MQTT discovery & entities
- `driver.h` – E-ink display abstraction layer
- `icons.h` – Battery indicator icons (4bpp format)
- `secrets.h` – WiFi and MQTT credentials

## Configuration

### secrets.h Template
Create a `secrets.h` file in the sketch directory:

```cpp
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"

#define MQTT_SERVER "192.168.1.100"
#define MQTT_PORT 1883
#define MQTT_USER "mqtt_user"
#define MQTT_PASSWORD "mqtt_password"

#define URL1 "http://your-server/path1" 
#define URL2 "http://your-server/path2"
#define URL3 "http://your-server/path3" (This URL is used to determine if there is an updated image; it should return a BMP image counter as plain text. The name should be updated eventually.)
#define URL4 "http://your-server/path4"

#define HA_URL_PREFIX "http://your-server/"
#define HA_URL_SUFFIX ""
#define HA_URL_DATA "default-page"
```

### Display Rotation
Modify `ROTATION` constant (0–3) to adjust display orientation.

## Features

### Multi-Page Display System
Three configurable pages:
- **Page 0 (Image)**: BMP image from server with periodic refresh
- **Page 1 (Home Assistant)**: Dashboard from HA instance
- **Page 2 (Auxiliary)**: Custom URL via MQTT

### Advanced Image Dithering
- **Ordered Bayer dithering** for smooth grayscale representation
- **Gamma correction** with 256-entry lookup table
- **Snap-to-level** logic prevents noise in flat UI areas
- Configurable dither strength (default 85/255)

### Battery Management
- Real-time voltage monitoring (30-sample averaging)
- Battery state classification:
    - `BATT_FULL` (≥4.0V)
    - `BATT_GOOD` (≥3.7V)
    - `BATT_MEDIUM` (≥3.5V)
    - `BATT_LOW` (≥3.2V)
    - `BATT_CRITICAL` (<3.2V)
- Battery icon overlay on display
- Automatic deep sleep when voltage <4.1V

### Deep Sleep & Power Management
- **Deep sleep duration**: 300 seconds (Image page), 200 seconds (others)
- **Wake triggers**: Button press or timer interrupt
- **Power conservation**: ADC disabled between reads, WiFi disconnects during sleep
- **Startup delay**: 20-second minimum awake time before sleep

### Home Assistant Integration
MQTT entities for:
- `page_select` – Page selection dropdown
- `aux_url` – Dynamic auxiliary URL
- `refresh_button` – Manual refresh trigger
- `battery_voltage` – Real-time voltage (V)
- `battery_state` – Battery percentage (%)

Device ID: `smart_display_5`

### Button Controls
- **Button 1**: Cycle to next page
- **Button 2**: Force refresh current page
- **Button 3**: Cycle to previous page
- **Debounce**: 250ms delay between presses

## Image Processing

### BMP Download & Conversion
1. Stream 24-bit BMP from HTTP server
2. Convert to grayscale per-pixel
3. Apply ordered dithering with gamma correction
4. Pack into 4-bit-per-pixel format
5. Push rows to display in reverse order

### Image Server Integration
- Checks server image counter for new content
- Only re-downloads when counter changes
- Image count stored in NVS to persist across reboots

## Building & Uploading

1. Install Arduino IDE with ESP32 board support
2. Add required libraries via Library Manager
3. Create `secrets.h` with your credentials
4. Configure pin assignments if different
5. Select appropriate ESP32 board variant
6. Upload sketch

## Persistent Storage (NVS)

- `displayIndex` – Current page number
- `imageCount` – Last-downloaded image counter

Access via `Preferences` API (namespace: `"myapp"`).

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Display crashes on startup | Ensure e-ink init before WiFi init |
| WiFi timeout | Check 15-second timeout threshold |
| Dither artifacts | Adjust `SNAP_THRESH` or `DITHER_STRENGTH` |
| Battery reading unstable | Verify voltage divider resistor values |
| MQTT connection fails | Confirm broker address, user, and password |

## Future Enhancements

- Partial display updates for faster refresh
- Compressed image support
- Scheduled updates vs. timer-only
- Home Assistant automations (auto-switch pages)
