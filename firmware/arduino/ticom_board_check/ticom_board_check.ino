#include <Arduino.h>
#include <esp_arduino_version.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <Network.h>
#include <WiFi.h>
#include <ETH.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Adafruit_NeoPixel.h>

#include "board_config.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled in this Arduino-ESP32 build.
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Bluetooth Classic SPP is not available for the selected ESP32 target.
#endif

namespace {

Adafruit_NeoPixel rgb(
    board::RGB_LED_COUNT,
    board::RGB_LED_PIN,
    NEO_GRB + NEO_KHZ800);

BluetoothSerial SerialBT;

bool ethernetStarted = false;
bool ethernetLinkUp = false;
bool ethernetHasIp = false;
bool sdMounted = false;

bool classicStarted = false;
bool classicClientConnected = false;
bool bleInitialized = false;
uint32_t classicConnectCount = 0;
uint32_t classicDisconnectCount = 0;

String inputLine;

void setRgb(uint8_t red, uint8_t green, uint8_t blue) {
  rgb.setPixelColor(0, rgb.Color(red, green, blue));
  rgb.show();
}

void printDivider() {
  Serial.println(F("----------------------------------------"));
}

String formatMac(const uint8_t mac[6]) {
  char value[18];
  snprintf(value, sizeof(value), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(value);
}

String bluetoothDeviceName() {
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_BT);

  char name[24];
  snprintf(name, sizeof(name), "%s-%02X%02X",
           board::BLUETOOTH_NAME_PREFIX, mac[4], mac[5]);
  return String(name);
}

const char *wifiModeName(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_MODE_NULL: return "off";
    case WIFI_MODE_STA: return "station";
    case WIFI_MODE_AP: return "access point";
    case WIFI_MODE_APSTA: return "station + access point";
    default: return "unknown";
  }
}

const char *wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS: return "idle";
    case WL_NO_SSID_AVAIL: return "SSID unavailable";
    case WL_SCAN_COMPLETED: return "scan completed";
    case WL_CONNECTED: return "connected";
    case WL_CONNECT_FAILED: return "connection failed";
    case WL_CONNECTION_LOST: return "connection lost";
    case WL_DISCONNECTED: return "disconnected";
    default: return "unknown";
  }
}

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN: return "unknown";
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_EXT: return "external reset";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "panic/crash";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "other watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep wakeup";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "SDIO reset";
    default: return "unrecognized";
  }
}

const char *flashModeName(FlashMode_t mode) {
  switch (mode) {
    case FM_QIO: return "QIO";
    case FM_QOUT: return "QOUT";
    case FM_DIO: return "DIO";
    case FM_DOUT: return "DOUT";
    case FM_FAST_READ: return "FAST_READ";
    case FM_SLOW_READ: return "SLOW_READ";
    default: return "unknown";
  }
}

String deviceId() {
  const uint64_t efuseMac = ESP.getEfuseMac();
  char value[24];
  snprintf(value, sizeof(value), "ESP32-%04X%08X",
           static_cast<uint16_t>(efuseMac >> 32),
           static_cast<uint32_t>(efuseMac));
  return String(value);
}

void printUptime() {
  const uint64_t seconds = millis() / 1000ULL;
  const uint32_t days = seconds / 86400ULL;
  const uint8_t hours = (seconds / 3600ULL) % 24;
  const uint8_t minutes = (seconds / 60ULL) % 60;
  const uint8_t secs = seconds % 60;
  Serial.printf("Uptime: %lu day(s) %02u:%02u:%02u\n",
                static_cast<unsigned long>(days), hours, minutes, secs);
}

void printSystemInfo() {
  const float dieTemperature = temperatureRead();
  const esp_reset_reason_t resetReason = esp_reset_reason();

  printDivider();
  Serial.println(F("ESP32 system diagnostics"));
  Serial.printf("Device ID: %s\n", deviceId().c_str());
  Serial.printf("eFuse MAC: %04X%08X\n",
                static_cast<uint16_t>(ESP.getEfuseMac() >> 32),
                static_cast<uint32_t>(ESP.getEfuseMac()));
  Serial.printf("Chip model: %s\n", ESP.getChipModel());
  Serial.printf("Chip revision: %u\n", ESP.getChipRevision());
  Serial.printf("CPU cores: %u\n", ESP.getChipCores());
  Serial.printf("CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Die temperature: %.1f C (approximate)\n", dieTemperature);
  Serial.printf("Last reset: %s (%d)\n",
                resetReasonName(resetReason), static_cast<int>(resetReason));
  printUptime();

  Serial.println(F("Memory:"));
  Serial.printf("  Heap total: %u bytes\n", ESP.getHeapSize());
  Serial.printf("  Heap free: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("  Heap minimum free: %u bytes\n", ESP.getMinFreeHeap());
  Serial.printf("  Heap largest block: %u bytes\n", ESP.getMaxAllocHeap());
  Serial.printf("  PSRAM total: %u bytes\n", ESP.getPsramSize());
  Serial.printf("  PSRAM free: %u bytes\n", ESP.getFreePsram());
  Serial.printf("  PSRAM minimum free: %u bytes\n", ESP.getMinFreePsram());
  Serial.printf("  PSRAM largest block: %u bytes\n", ESP.getMaxAllocPsram());

  Serial.println(F("Flash and firmware:"));
  Serial.printf("  Flash size: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("  Flash speed: %u Hz\n", ESP.getFlashChipSpeed());
  Serial.printf("  Flash mode: %s\n", flashModeName(ESP.getFlashChipMode()));
  Serial.printf("  Sketch size: %u bytes\n", ESP.getSketchSize());
  Serial.printf("  Free sketch space: %u bytes\n", ESP.getFreeSketchSpace());
  Serial.printf("  Sketch MD5: %s\n", ESP.getSketchMD5().c_str());

  Serial.println(F("Software:"));
  Serial.printf("  Arduino core: %s\n", ESP.getCoreVersion());
  Serial.printf("  ESP-IDF: %s\n", ESP.getSdkVersion());
  Serial.printf("  Build: %s %s\n", __DATE__, __TIME__);
  printDivider();
}

void printEthernetStatus() {
  printDivider();
  Serial.println(F("Ethernet status"));
  Serial.printf("Driver started: %s\n", ethernetStarted ? "yes" : "no");
  Serial.printf("Link: %s\n", ethernetLinkUp ? "up" : "down");
  Serial.printf("IP received: %s\n", ethernetHasIp ? "yes" : "no");
  if (ethernetStarted) {
    Serial.printf("MAC: %s\n", ETH.macAddress().c_str());
    Serial.printf("IPv4: %s\n", ETH.localIP().toString().c_str());
    Serial.printf("Gateway: %s\n", ETH.gatewayIP().toString().c_str());
    Serial.printf("DNS: %s\n", ETH.dnsIP().toString().c_str());
    Serial.printf("Link speed: %u Mbps\n", ETH.linkSpeed());
    Serial.printf("Duplex: %s\n", ETH.fullDuplex() ? "full" : "half");
  }
  printDivider();
}

void printWifiStatus() {
  const wifi_mode_t mode = WiFi.getMode();
  const wl_status_t status = WiFi.status();

  printDivider();
  Serial.println(F("Wi-Fi status"));
  Serial.printf("Mode: %s\n", wifiModeName(mode));
  Serial.printf("State: %s\n", wifiStatusName(status));
  if (mode != WIFI_MODE_NULL) {
    Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
  }
  if (status == WL_CONNECTED) {
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("IPv4: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
  }
  printDivider();
}

void scanWifiNetworks() {
  if (WiFi.getMode() == WIFI_MODE_NULL) {
    WiFi.mode(WIFI_MODE_STA);
    delay(100);
  }

  Serial.println(F("Scanning Wi-Fi networks..."));
  const int16_t count = WiFi.scanNetworks();
  if (count < 0) {
    Serial.printf("WIFI SCAN: FAIL (%d)\n", count);
    return;
  }
  if (count == 0) {
    Serial.println(F("WIFI SCAN: no networks found"));
    WiFi.scanDelete();
    return;
  }

  Serial.printf("WIFI SCAN: %d network(s) found\n", count);
  for (int16_t index = 0; index < count; ++index) {
    const bool openNetwork = WiFi.encryptionType(index) == WIFI_AUTH_OPEN;
    Serial.printf("%2d. %s | RSSI %d dBm | CH %d | %s\n",
                  index + 1,
                  WiFi.SSID(index).c_str(),
                  WiFi.RSSI(index),
                  WiFi.channel(index),
                  openNetwork ? "open" : "secured");
  }
  WiFi.scanDelete();
}

void connectWifi(const String &arguments) {
  const int separator = arguments.indexOf('|');
  String ssid = separator >= 0 ? arguments.substring(0, separator) : arguments;
  const String password =
      separator >= 0 ? arguments.substring(separator + 1) : String();
  ssid.trim();

  if (ssid.isEmpty()) {
    Serial.println(F("Usage: WIFI CONNECT <SSID>|<PASSWORD>"));
    Serial.println(F("For an open network: WIFI CONNECT <SSID>|"));
    return;
  }

  WiFi.mode(WIFI_MODE_STA);
  WiFi.persistent(false);
  WiFi.setMinSecurity(password.isEmpty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK);
  WiFi.disconnect(false, false);
  delay(100);

  Serial.printf("Connecting to Wi-Fi SSID: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < board::WIFI_CONNECT_TIMEOUT_MS) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WIFI CONNECT: PASS, IPv4 %s\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("WIFI CONNECT: FAIL, state %s\n",
                  wifiStatusName(WiFi.status()));
  }
}

void disableWifi() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_MODE_NULL);
  Serial.println(F("Wi-Fi disabled."));
}

void listRootDirectory() {
  if (!sdMounted) {
    Serial.println(F("SD is not mounted."));
    return;
  }

  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println(F("Cannot open SD root directory."));
    return;
  }

  Serial.println(F("SD root directory:"));
  File entry = root.openNextFile();
  while (entry) {
    Serial.printf("%s\t%s\t%lu bytes\n",
                  entry.isDirectory() ? "DIR " : "FILE",
                  entry.name(),
                  static_cast<unsigned long>(entry.size()));
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
}

void printSdStatus() {
  printDivider();
  Serial.println(F("microSD status"));
  Serial.printf("Mounted: %s\n", sdMounted ? "yes" : "no");
  if (sdMounted) {
    Serial.printf("Card size: %llu MB\n",
                  SD.cardSize() / (1024ULL * 1024ULL));
    Serial.printf("Used: %llu MB\n",
                  SD.usedBytes() / (1024ULL * 1024ULL));
    Serial.printf("Total: %llu MB\n",
                  SD.totalBytes() / (1024ULL * 1024ULL));
  }
  printDivider();
}

void testSdReadWrite() {
  if (!sdMounted) {
    Serial.println(F("SD TEST: FAIL, card is not mounted."));
    return;
  }

  String payload = F("T-Internet-COM SD test\n");
  payload += F("Uptime_ms=");
  payload += String(millis());
  payload += '\n';

  SD.remove(board::SD_TEST_PATH);
  File output = SD.open(board::SD_TEST_PATH, FILE_WRITE);
  if (!output) {
    Serial.println(F("SD TEST: FAIL, cannot create test file."));
    return;
  }

  const size_t written = output.print(payload);
  output.flush();
  output.close();

  if (written != payload.length()) {
    Serial.printf("SD TEST: FAIL, wrote %u of %u bytes.\n",
                  static_cast<unsigned>(written),
                  static_cast<unsigned>(payload.length()));
    SD.remove(board::SD_TEST_PATH);
    return;
  }
  Serial.printf("SD write: PASS (%u bytes)\n",
                static_cast<unsigned>(written));

  File input = SD.open(board::SD_TEST_PATH, FILE_READ);
  if (!input) {
    Serial.println(F("SD TEST: FAIL, cannot reopen test file."));
    SD.remove(board::SD_TEST_PATH);
    return;
  }

  const String readBack = input.readString();
  input.close();
  if (readBack != payload) {
    Serial.println(F("SD read/verify: FAIL, content mismatch."));
    SD.remove(board::SD_TEST_PATH);
    return;
  }
  Serial.println(F("SD read/verify: PASS"));

  if (!SD.remove(board::SD_TEST_PATH)) {
    Serial.println(F("SD delete: FAIL"));
    return;
  }

  Serial.println(F("SD delete: PASS"));
  Serial.println(F("SD TEST: PASS"));
}

void onBluetoothAuthComplete(bool success) {
  Serial.printf("Bluetooth pairing: %s\n", success ? "PASS" : "FAIL");
}

void onSppEvent(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  switch (event) {
    case ESP_SPP_INIT_EVT:
      Serial.println(F("SPP event: stack initialized"));
      break;

    case ESP_SPP_START_EVT:
      Serial.println(F("SPP event: server listening"));
      break;

    case ESP_SPP_SRV_OPEN_EVT:
      classicClientConnected = true;
      ++classicConnectCount;
      if (param != nullptr) {
        Serial.printf("SPP event: client connected from %s\n",
                      formatMac(param->srv_open.rem_bda).c_str());
      } else {
        Serial.println(F("SPP event: client connected"));
      }
      SerialBT.println(F("T-Internet-COM Bluetooth terminal connected."));
      SerialBT.println(F("Type text to receive an echo."));
      break;

    case ESP_SPP_CLOSE_EVT:
      classicClientConnected = false;
      ++classicDisconnectCount;
      Serial.println(F("SPP event: client disconnected"));
      break;

    case ESP_SPP_SRV_STOP_EVT:
      classicClientConnected = false;
      Serial.println(F("SPP event: server stopped"));
      break;

    case ESP_SPP_UNINIT_EVT:
      classicClientConnected = false;
      Serial.println(F("SPP event: stack deinitialized"));
      break;

    default:
      break;
  }
}

void printBluetoothStatus() {
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_BT);

  printDivider();
  Serial.println(F("Bluetooth status"));
  Serial.printf("Device name: %s\n", bluetoothDeviceName().c_str());
  Serial.printf("Bluetooth MAC: %s\n", formatMac(mac).c_str());
  Serial.printf("Classic SPP: %s\n", classicStarted ? "started" : "stopped");
  Serial.printf("Classic client: %s\n",
                classicStarted &&
                        (classicClientConnected || SerialBT.hasClient())
                    ? "connected"
                    : "not connected");
  Serial.printf("SPP connections since start: %lu\n",
                static_cast<unsigned long>(classicConnectCount));
  Serial.printf("SPP disconnections since start: %lu\n",
                static_cast<unsigned long>(classicDisconnectCount));
  Serial.printf("BLE stack: %s\n",
                bleInitialized ? "initialized" : "stopped");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  printDivider();
}

void stopBle() {
  if (!bleInitialized) {
    return;
  }

  BLEDevice::deinit(false);
  bleInitialized = false;
  Serial.println(F("BLE stopped."));
}

void stopClassic() {
  if (!classicStarted) {
    return;
  }

  SerialBT.end();
  classicStarted = false;
  classicClientConnected = false;
  Serial.println(F("Bluetooth Classic SPP stopped."));
}

void startClassic() {
  if (classicStarted) {
    Serial.println(F("Bluetooth Classic SPP is already started."));
    return;
  }

  if (bleInitialized) {
    stopBle();
    delay(250);
  }

  const String name = bluetoothDeviceName();
  const uint32_t heapBefore = ESP.getFreeHeap();
  Serial.println(F("Starting Bluetooth Classic SPP..."));

  SerialBT.enableSSP(false, false);
  SerialBT.onAuthComplete(onBluetoothAuthComplete);
  SerialBT.register_callback(onSppEvent);

  if (!SerialBT.begin(name)) {
    Serial.println(F("BT CLASSIC START: FAIL"));
    return;
  }

  classicStarted = true;
  classicClientConnected = false;
  Serial.println(F("BT CLASSIC START: PASS"));
  Serial.printf("Pair with: %s\n", name.c_str());
  Serial.printf("Free heap: %u -> %u bytes\n",
                heapBefore, ESP.getFreeHeap());
  Serial.println(F("Windows: open the outgoing ESP32SPP COM port."));
  Serial.println(F("Data received over Bluetooth is printed to USB and echoed back."));
}

void sendClassicText(const String &text) {
  if (!classicStarted) {
    Serial.println(F("BT SEND: FAIL, start Classic SPP first."));
    return;
  }

  if (!(classicClientConnected || SerialBT.hasClient())) {
    Serial.println(F("BT SEND: FAIL, no SPP client has opened the Bluetooth COM port."));
    return;
  }

  SerialBT.println(text);
  Serial.printf("BT SEND: PASS (%u characters)\n",
                static_cast<unsigned>(text.length()));
}

void scanBle() {
  if (classicStarted) {
    Serial.println(F("Stopping Bluetooth Classic before BLE scan..."));
    stopClassic();
    delay(250);
  }

  if (!bleInitialized) {
    Serial.println(F("Initializing BLE..."));
    if (!BLEDevice::init("")) {
      Serial.println(F("BLE SCAN: FAIL, initialization error."));
      return;
    }
    bleInitialized = true;
  }

  BLEScan *scanner = BLEDevice::getScan();
  if (scanner == nullptr) {
    Serial.println(F("BLE SCAN: FAIL, scanner unavailable."));
    stopBle();
    return;
  }

  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(99);

  Serial.printf("Scanning BLE devices for %u seconds...\n",
                board::BLE_SCAN_SECONDS);
  BLEScanResults *results =
      scanner->start(board::BLE_SCAN_SECONDS, false);

  if (results == nullptr) {
    Serial.println(F("BLE SCAN: FAIL, no result object."));
    scanner->clearResults();
    stopBle();
    return;
  }

  const int count = results->getCount();
  Serial.printf("BLE SCAN: %d device(s) found\n", count);

  for (int index = 0; index < count; ++index) {
    BLEAdvertisedDevice device = results->getDevice(index);
    const String name =
        device.haveName() ? device.getName() : String("<unnamed>");
    const String address = device.getAddress().toString();
    const int rssi = device.haveRSSI() ? device.getRSSI() : 0;

    Serial.printf("%2d. %s | %s | RSSI %d dBm | %s\n",
                  index + 1,
                  name.c_str(),
                  address.c_str(),
                  rssi,
                  device.isConnectable() ? "connectable" : "not connectable");
  }

  scanner->clearResults();
  stopBle();
  Serial.println(F("BLE SCAN: PASS"));
}

void stopAllBluetooth() {
  stopClassic();
  stopBle();
  Serial.println(F("Bluetooth disabled."));
}

void printModemStatus() {
  printDivider();
  Serial.println(F("Cellular modem status"));
  Serial.printf("Module installed: %s\n",
                board::MODEM_INSTALLED ? "yes" : "no");
  if (board::MODEM_INSTALLED) {
    Serial.printf("Model: %s\n",
                  strlen(board::MODEM_MODEL) > 0
                      ? board::MODEM_MODEL
                      : "unknown, automatic AT detection not enabled yet");
  } else {
    Serial.println(F("Model: unavailable"));
  }
  Serial.printf("UART TX/RX: GPIO%d / GPIO%d\n",
                board::MODEM_TX, board::MODEM_RX);
  Serial.printf("PWRKEY: GPIO%d\n", board::MODEM_PWRKEY);
  Serial.println(F("AT probing remains disabled until a modem is installed."));
  printDivider();
}

void printStatus() {
  Serial.println();
  Serial.println(F("T-Internet-COM complete board report"));
  printSystemInfo();
  printEthernetStatus();
  printWifiStatus();
  printSdStatus();
  printBluetoothStatus();
  printModemStatus();
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  HELP                         - show this list"));
  Serial.println(F("  STATUS                       - complete board report"));
  Serial.println(F("  INFO                         - ESP32 system diagnostics"));
  Serial.println(F("  SYSTEM                       - alias for INFO"));
  Serial.println(F("  ETH                          - Ethernet state"));
  Serial.println(F("  WIFI STATUS                  - Wi-Fi state"));
  Serial.println(F("  WIFI SCAN                    - scan nearby Wi-Fi networks"));
  Serial.println(F("  WIFI CONNECT <SSID>|<PASS>   - connect as Wi-Fi station"));
  Serial.println(F("  WIFI OFF                     - disconnect and disable Wi-Fi"));
  Serial.println(F("  SD                           - microSD state"));
  Serial.println(F("  SD LIST                      - list files in SD root"));
  Serial.println(F("  SD TEST                      - write/read/delete test file"));
  Serial.println(F("  BT STATUS                    - Bluetooth state and MAC"));
  Serial.println(F("  BT CLASSIC START             - start Bluetooth Classic SPP"));
  Serial.println(F("  BT CLASSIC STOP              - stop Bluetooth Classic SPP"));
  Serial.println(F("  BT SEND <TEXT>               - send text to SPP client"));
  Serial.println(F("  BLE SCAN                     - scan nearby BLE devices"));
  Serial.println(F("  BT OFF                       - stop all Bluetooth modes"));
  Serial.println(F("  MODEM STATUS                 - installed state and model field"));
  Serial.println(F("  LED RED                      - RGB red"));
  Serial.println(F("  LED GREEN                    - RGB green"));
  Serial.println(F("  LED BLUE                     - RGB blue"));
  Serial.println(F("  LED WHITE                    - RGB white"));
  Serial.println(F("  LED OFF                      - turn RGB off"));
  Serial.println(F("  REBOOT                       - restart ESP32"));
}

void handleCommand(String command) {
  command.trim();
  String normalized = command;
  normalized.toUpperCase();

  if (normalized.isEmpty()) {
    return;
  }

  if (normalized == "HELP" || normalized == "?") {
    printHelp();
  } else if (normalized == "STATUS") {
    printStatus();
  } else if (normalized == "INFO" || normalized == "SYSTEM") {
    printSystemInfo();
  } else if (normalized == "ETH") {
    printEthernetStatus();
  } else if (normalized == "WIFI STATUS" || normalized == "WIFI") {
    printWifiStatus();
  } else if (normalized == "WIFI SCAN") {
    scanWifiNetworks();
  } else if (normalized.startsWith("WIFI CONNECT ")) {
    connectWifi(command.substring(13));
  } else if (normalized == "WIFI OFF") {
    disableWifi();
  } else if (normalized == "SD") {
    printSdStatus();
  } else if (normalized == "SD LIST") {
    listRootDirectory();
  } else if (normalized == "SD TEST") {
    testSdReadWrite();
  } else if (normalized == "BT STATUS" || normalized == "BT") {
    printBluetoothStatus();
  } else if (normalized == "BT CLASSIC START") {
    startClassic();
  } else if (normalized == "BT CLASSIC STOP") {
    stopClassic();
  } else if (normalized.startsWith("BT SEND ")) {
    sendClassicText(command.substring(8));
  } else if (normalized == "BLE SCAN") {
    scanBle();
  } else if (normalized == "BT OFF") {
    stopAllBluetooth();
  } else if (normalized == "MODEM STATUS" || normalized == "MODEM") {
    printModemStatus();
  } else if (normalized == "LED RED") {
    setRgb(255, 0, 0);
    Serial.println(F("RGB: red"));
  } else if (normalized == "LED GREEN") {
    setRgb(0, 255, 0);
    Serial.println(F("RGB: green"));
  } else if (normalized == "LED BLUE") {
    setRgb(0, 0, 255);
    Serial.println(F("RGB: blue"));
  } else if (normalized == "LED WHITE") {
    setRgb(255, 255, 255);
    Serial.println(F("RGB: white"));
  } else if (normalized == "LED OFF") {
    setRgb(0, 0, 0);
    Serial.println(F("RGB: off"));
  } else if (normalized == "REBOOT") {
    Serial.println(F("Restarting..."));
    Serial.flush();
    delay(100);
    ESP.restart();
  } else {
    Serial.printf("Unknown command: %s\n", command.c_str());
    Serial.println(F("Enter HELP for a command list."));
  }
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    const char value = static_cast<char>(Serial.read());

    if (value == '\r') {
      continue;
    }

    if (value == '\n') {
      handleCommand(inputLine);
      inputLine = "";
      Serial.print(F("> "));
      continue;
    }

    if (inputLine.length() < board::SERIAL_COMMAND_MAX_LENGTH) {
      inputLine += value;
    } else {
      inputLine = "";
      Serial.println(F("Input line too long."));
      Serial.print(F("> "));
    }
  }
}

void serviceClassicTerminal() {
  if (!classicStarted) {
    return;
  }

  while (SerialBT.available() > 0) {
    const int value = SerialBT.read();
    if (value < 0) {
      break;
    }

    Serial.write(static_cast<uint8_t>(value));
    SerialBT.write(static_cast<uint8_t>(value));
  }
}

void initRgb() {
  rgb.begin();
  rgb.setBrightness(board::RGB_LED_BRIGHTNESS);
  setRgb(0, 0, 16);
}

void initSd() {
  SPI.begin(board::SD_SCLK, board::SD_MISO, board::SD_MOSI, board::SD_CS);
  sdMounted = SD.begin(board::SD_CS, SPI);
  if (sdMounted) {
    Serial.printf("microSD mounted: %llu MB\n",
                  SD.cardSize() / (1024ULL * 1024ULL));
  } else {
    Serial.println(F("microSD not detected. This is acceptable if no card is installed."));
  }
}

void resetEthernetPhy() {
  pinMode(board::ETH_RESET_PIN, OUTPUT);
  digitalWrite(board::ETH_RESET_PIN, LOW);
  delay(100);
  digitalWrite(board::ETH_RESET_PIN, HIGH);
  delay(100);
}

void onNetworkEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ethernetStarted = true;
      ETH.setHostname("ticom-board");
      Serial.println(F("Ethernet driver started."));
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      ethernetLinkUp = true;
      Serial.println(F("Ethernet cable connected."));
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      ethernetHasIp = true;
      setRgb(0, 32, 0);
      Serial.printf("Ethernet IPv4: %s\n", ETH.localIP().toString().c_str());
      break;

    case ARDUINO_EVENT_ETH_LOST_IP:
      ethernetHasIp = false;
      Serial.println(F("Ethernet lost its IP address."));
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      ethernetLinkUp = false;
      ethernetHasIp = false;
      setRgb(32, 16, 0);
      Serial.println(F("Ethernet cable disconnected."));
      break;

    case ARDUINO_EVENT_ETH_STOP:
      ethernetStarted = false;
      ethernetLinkUp = false;
      ethernetHasIp = false;
      setRgb(32, 0, 0);
      Serial.println(F("Ethernet driver stopped."));
      break;

    default:
      break;
  }
}

void initEthernet() {
  Network.onEvent(onNetworkEvent);
  resetEthernetPhy();

  const bool beginResult = ETH.begin(
      board::ETH_PHY_TYPE,
      board::ETH_PHY_ADDR,
      board::ETH_MDC_PIN,
      board::ETH_MDIO_PIN,
      board::ETH_POWER_PIN,
      board::ETH_CLK_MODE);

  if (!beginResult) {
    Serial.println(F("ETH.begin() failed."));
    setRgb(32, 0, 0);
  }
}

}  // namespace

void setup() {
  Serial.begin(board::SERIAL_BAUD);
  delay(500);

  initRgb();

  Serial.println();
  Serial.println(F("T-Internet-COM Lab - unified board check"));
  Serial.println(F("Stage 4: system, Ethernet, Wi-Fi, microSD, Bluetooth and RGB."));
  Serial.println(F("Cellular modem is not installed; only generic status is available."));

  initSd();
  initEthernet();

  printHelp();
  Serial.print(F("> "));
}

void loop() {
  readSerialCommands();
  serviceClassicTerminal();
  delay(2);
}
