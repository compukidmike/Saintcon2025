# SAINTCON 2025 Attract Player

Minimal ESP32-S3 firmware for the Saintcon 2025 badge: plays the attract reel on the ST7789 and drives the 20-LED wrench ring. No LVGL, Wi‑Fi, or badge game stack.

## Easy flash (no IDF required)

```bash
cd Firmware/attract_player
chmod +x flash.sh
./flash.sh /dev/cu.usbmodem1101   # your serial port
```

That writes `dist/attract_player_flash.bin` (bootloader + app + SPIFFS video) in one shot. Needs [esptool](https://pypi.org/project/esptool/): `pip install esptool`.

## Rebuild / repack the flash image

```bash
cd Firmware/attract_player
# Optional: regenerate video pack from preview MP4
python3 scripts/pack_attract.py
source $IDF_PATH/export.sh
idf.py build
./scripts/make_flash_image.sh
./flash.sh
```

## Hardware

- ESP32-S3 badge, 240×320 ST7789 (I80), WS2812 ×20 on GPIO 3
- Boots straight into a 60s @ 12fps MJPEG loop with synced LED cues
- Backlight stays full (no idle dim)
