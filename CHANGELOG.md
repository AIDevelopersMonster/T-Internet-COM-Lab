# Changelog

## 0.2.0 - 2026-08-04

- added `WIFI STATUS`;
- added `WIFI SCAN`;
- added `WIFI CONNECT <SSID>|<PASSWORD>`;
- added `WIFI OFF`;
- added `SD TEST` with file creation, write, reopen, verification and deletion;
- increased the Serial command length for Wi-Fi credentials;
- added `ARDUINO_EVENT_ETH_LOST_IP` handling;
- updated README with confirmed Ethernet and microSD results.

## 0.1.1 - 2026-08-03

- confirmed successful startup on a real T-Internet-COM board;
- added compatibility with Arduino-ESP32 3.3.11;
- replaced `WiFi.onEvent(...)` with `Network.onEvent(...)`;
- updated the `ETH.begin(...)` argument order for ESP32 Core 3.x;
- renamed the RGB brightness constant to avoid a core macro conflict;
- documented the verified Arduino IDE settings;
- added the actual Serial Monitor test log.

## 0.1.0 - 2026-08-03

- created initial repository structure;
- added first-stage Arduino diagnostic firmware;
- added Ethernet, microSD and RGB checks;
- reserved modem pins without enabling the modem;
- documented pin map, roadmap and mechanical mounting problem;
- added issue templates for hardware tests and future modem support.
