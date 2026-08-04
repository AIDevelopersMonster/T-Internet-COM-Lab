#include <Arduino.h>
#include <esp_arduino_version.h>
#include <Network.h>
#include <WiFi.h>
#include <ETH.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>

#include "board_config.h"

namespace {

Adafruit_NeoPixel rgb(
    board::RGB_LED_COUNT,
    board::RGB_LED_PIN,
    NEO_GRB + NEO_KHZ800);

bool ethernetStarted = false;
bool ethernetLinkUp = false;
bool ethernetHasIp = false;
bool sdMounted = false;
String inputLine;

void setRgb(uint8_t red, uint8_t green, uint8_t blue) {
  rgb.setPixelColor(0, rgb.Color(red, green, blue));
  rgb.show();
}

void printDivider() {
  Serial.println(F("----------------------------------------"));
}

const char *wifiModeName(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_MODE_NULL:
      return "off";
    case WIFI_MODE_STA:
      return "station";
    case WIFI_MODE_AP:
      return "access point";
    case WIFI_MODE_APSTA:
      return "station + access point";
    default:
      return "unknown";
  }
}

const char *wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "idle";
    case WL_NO_SSID_AVAIL:
      return "SSID unavailable";
    case WL_SCAN_COMPLETED:
      return "scan completed";
    case WL_CONNECTED:
      return "connected";
    case WL_CONNECT_FAILED:
      return "connection failed";
    case WL_CONNECTION_LOST:
      return "connection lost";
    case WL_DISCONNECTED:
      return "disconnected";
    default:
      return "unknown";
  }
}

void printSystemInfo() {
  printDivider();
  Serial.println(F("ESP32 system information"));
  Serial.printf("Chip model: %s\n", ESP.getChipModel());
  Serial.printf("Chip revision: %u\n", ESP.getChipRevision());
  Serial.printf("CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash size: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("PSRAM size: %u bytes\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
  Serial.printf("Uptime: %lu ms\n", static_cast<unsigned long>(millis()));
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
    Serial.printf(
        "%2d. %s | RSSI %d dBm | CH %d | %s\n",
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
  const String password = separator >= 0 ? arguments.substring(separator + 1) : String();
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
    Serial.printf("WIFI CONNECT: PASS, IPv4 %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("WIFI CONNECT: FAIL, state %s\n", wifiStatusName(WiFi.status()));
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
    Serial.printf("Card size: %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));
    Serial.printf("Used: %llu MB\n", SD.usedBytes() / (1024ULL * 1024ULL));
    Serial.printf("Total: %llu MB\n", SD.totalBytes() / (1024ULL * 1024ULL));
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
  Serial.printf("SD write: PASS (%u bytes)\n", static_cast<unsigned>(written));

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

void printStatus() {
  Serial.println();
  Serial.println(F("T-Internet-COM board report"));
  Serial.printf("Modem stage: %s\n",
                board::MODEM_ENABLED ? "enabled" : "disabled, module not installed");
  printSystemInfo();
  printEthernetStatus();
  printWifiStatus();
  printSdStatus();
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  HELP                         - show this list"));
  Serial.println(F("  STATUS                       - complete board report"));
  Serial.println(F("  INFO                         - ESP32 information"));
  Serial.println(F("  ETH                          - Ethernet state"));
  Serial.println(F("  WIFI STATUS                  - Wi-Fi state"));
  Serial.println(F("  WIFI SCAN                    - scan nearby Wi-Fi networks"));
  Serial.println(F("  WIFI CONNECT <SSID>|<PASS>   - connect as Wi-Fi station"));
  Serial.println(F("  WIFI OFF                     - disconnect and disable Wi-Fi"));
  Serial.println(F("  SD                           - microSD state"));
  Serial.println(F("  SD LIST                      - list files in SD root"));
  Serial.println(F("  SD TEST                      - write/read/delete test file"));
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
  } else if (normalized == "INFO") {
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
  Serial.println(F("T-Internet-COM Lab - board check"));
  Serial.println(F("Stage 2: Ethernet, microSD, Wi-Fi and RGB. Modem disabled."));

  initSd();
  initEthernet();

  printHelp();
  Serial.print(F("> "));
}

void loop() {
  readSerialCommands();
  delay(2);
}
