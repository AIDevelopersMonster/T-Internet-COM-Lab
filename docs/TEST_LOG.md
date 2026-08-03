# Test log

## 2026-08-03 — Stage 1 board check

### Hardware state

- LILYGO T-Internet-COM ESP32;
- modem module not installed;
- microSD card not installed;
- USB connection active;
- Arduino IDE port: COM15.

### Software configuration

- Arduino IDE;
- ESP32 by Espressif Systems 3.3.11;
- board: ESP32 Dev Module;
- CPU frequency: 240 MHz (WiFi/BT);
- flash frequency: 80 MHz;
- flash mode: QIO;
- flash size: 4 MB (32 Mb);
- partition scheme: Huge APP (3 MB No OTA / 1 MB SPIFFS);
- PSRAM: Enabled;
- upload speed: 921600;
- Adafruit NeoPixel installed.

### Result

The firmware compiled, uploaded and started successfully after adapting the code to the Arduino-ESP32 3.3.11 API.

Confirmed:

- ESP32 boot;
- Serial Monitor output at 115200 baud;
- RGB initialization;
- Ethernet driver initialization;
- Serial command prompt;
- correct message when no microSD card is installed;
- operation with modem support disabled.

### Serial Monitor output

```text
load:0x40080400,len:3500
entry 0x400805b4

T-Internet-COM Lab - board check
Stage 1: Ethernet, microSD and RGB. Modem disabled.
microSD not detected. This is acceptable if no card is installed.
Ethernet driver started.
Commands:
  HELP       - show this list
  STATUS     - complete board report
  INFO       - ESP32 information
  ETH        - Ethernet state
  SD         - microSD state
  SD LIST    - list files in SD root
  LED RED    - RGB red
  LED GREEN  - RGB green
  LED BLUE   - RGB blue
  LED WHITE  - RGB white
  LED OFF    - turn RGB off
  REBOOT     - restart ESP32
>
```

### Compatibility fixes applied

- renamed the project constant `RGB_BRIGHTNESS` to `RGB_LED_BRIGHTNESS` because Arduino-ESP32 3.x defines `RGB_BRIGHTNESS` as a macro;
- added `Network.h`;
- replaced `WiFi.onEvent(...)` with `Network.onEvent(...)`;
- updated the `ETH.begin(...)` argument order for Arduino-ESP32 3.x.

### Pending checks

- install and test a microSD card;
- connect an Ethernet cable and record DHCP, MAC, link speed and duplex;
- test every RGB command;
- test `STATUS`, `INFO`, `ETH`, `SD` and `REBOOT`;
- add modem tests after compatible Mini PCIe modules arrive.
