# largeEink — 7.5" E‑Paper Smart Display

Small project to drive a 7.5" grayscale e‑ink panel from an ESP32. Includes:
- An ESP32 firmware (eInkDriver.c) that fetches BMP images, converts to 4‑level grayscale with ordered dither + gamma LUT, and displays them on an e‑paper module.
- A Python helper (convertImage.py) to convert PNG/JPEG images into 4bpp C arrays for embedding.
- MQTT/Home Assistant integration for remote control.
<img width="1828" height="1376" alt="image" src="https://github.com/user-attachments/assets/7144d0a4-91ab-4f0e-b830-c2c852780632" />
<img width="1036" height="1376" alt="image" src="https://github.com/user-attachments/assets/760267c7-dc92-4262-bfc6-22c8949bd38a" />
<img width="1828" height="1376" alt="image" src="https://github.com/user-attachments/assets/ed91a1c4-4f1f-450d-97d3-73b86186e7c0" />
<img width="1828" height="1376" alt="image" src="https://github.com/user-attachments/assets/65e2f375-635c-4912-90c1-255b1c8ad63b" />

Subscript: I don't like writing docs, so genAI helped with this page. If you're reading this, and you're a human a) I'm surprised you found my project b) I'm happy to help you implement this on your project. 

## Repository layout
- eInkDriver/  
  - eInkDriver.c — main ESP32 firmware
  - convertImage.py — Python tool to convert images to 4bpp C arrays
  - icons.h, secrets.h, other project files (expected)

## Prerequisites
- Hardware: ESP32 board compatible with the e‑paper module used (pins in firmware), 7.5" e‑paper display.
- Toolchain: Arduino/PlatformIO or preferred ESP32 toolchain to compile and flash the C firmware.
- Python 3 + Pillow to run the conversion script.
  - Install: pip install pillow

## Configuration
- secrets.h — add WIFI_SSID, WIFI_PASS, MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD and any URL constants (URL1, URL2, URL3, etc.).
- Adjust pin constants in eInkDriver.c for your board if needed (battery ADC pin, ADC_EN, button pins).
- Verify TFT / ePaper definitions (TFT_eSPI / epaper library) match your hardware.

## Using the Python image converter
Script: convertImage.py
Usage:
python3 convertImage.py <inputDirectory> <OutputFilename> <width> <height>

- inputDirectory: folder with .png/.jpg/.jpeg files.
- OutputFilename: path to the generated C header (appends arrays).
- width/height: desired target size to resize each image to.

The script writes PROGMEM C arrays and width/height constants for each image.

## Firmware behavior
- Downloads BMP image(s) from configured URLs, converts streamed BMP rows to 4bpp via a dithered/gamma LUT, and pushes rows to the display.
- Maintains persistent preferences (imageCount, displayIndex), Reducing unecessary flickering after deep sleep
- Exposes MQTT/Home Assistant entities for page selection, aux URL, refresh, and battery sensors (via HA-MQTT integration).
- Supports deep sleep wake via buttons and timer.

## MQTT / Home Assistant
- Device ID and friendly name defined in eInkDriver.c (HA_DEVICE_ID, HA_DEVICE_FRIENDLY_NAME).
- Provided entities include page selection, aux URL, refresh button and battery sensors. Configure your Home Assistant MQTT discovery or use the included HA‑MQTT helper to register entities.

## Troubleshooting
- BMP download: server must serve uncompressed BMP with expected resolution/pixel format.
- The driver must receive exactly 480x800 pixels image, in 8bit BMP format.

## Tips
- Make sure you setup your secrets file with the correct URLS and credentials for MQTT etc

## Future Work
- It would be cool to do partial updates or less flickering.
-- For example an indicator showing that it's thinking or working. 
