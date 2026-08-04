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

## 2026-08-04 — Bluetooth Classic and BLE check

### Hardware and software

- LILYGO T-Internet-COM ESP32;
- Arduino-ESP32 3.3.11;
- Bluetooth diagnostic sketch `ticom_bluetooth_check`;
- Windows PC used as the Bluetooth Classic SPP client.

### Identifiers

- Bluetooth device name: `TICOM-EBF2`;
- Bluetooth MAC: `B0:B2:1C:31:EB:F2`;
- Windows outgoing SPP port: `COM18`, service `ESP32SPP`.

### Confirmed results

- Bluetooth Classic SPP starts successfully;
- Windows discovers and pairs with `TICOM-EBF2`;
- the SPP server reports the connected client MAC;
- bidirectional data transfer works through the same outgoing COM port;
- data sent from the PC reaches the ESP32 and is echoed back;
- command `BT SEND <TEXT>` sends data from the ESP32 to the Windows terminal;
- connection and disconnection events are reported;
- BLE initializes successfully;
- five-second BLE scan completes successfully and lists detected devices with address and RSSI;
- Classic SPP can be stopped before BLE scanning and the BLE stack can then be stopped cleanly.

### Observed memory

```text
Free heap before Classic SPP: 217716 bytes
Free heap with Classic SPP started: 122560 bytes
```

Bluetooth Classic therefore consumes approximately 95 KB of additional internal heap in this test configuration. The Bluetooth test remains a separate sketch until simultaneous operation with Ethernet, Wi-Fi and microSD is deliberately integrated and tested.

### Representative output

```text
BT CLASSIC START: PASS
Pair with: TICOM-EBF2
SPP event: stack initialized
SPP event: server listening
SPP event: client connected from B0:DC:EF:64:FD:29
BT SEND: PASS

BLE SCAN: 1 device(s) found
BLE stopped.
BLE SCAN: PASS
```

### Result

Bluetooth Classic SPP and BLE scanning are hardware-validated on the real T-Internet-COM board.
