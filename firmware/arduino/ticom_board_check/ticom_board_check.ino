#include <Arduino.h>
#include <esp_arduino_version.h>
#include <Network.h>
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

void printStatus() {
  Serial.println();
  Serial.println(F("T-Internet-COM board report"));
  Serial.printf("Modem stage: %s\n",
                board::MODEM_ENABLED ? "enabled" : "disabled, module not installed");
  printSystemInfo();
  printEthernetStatus();
  printSdStatus();
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  HELP       - show this list"));
  Serial.println(F("  STATUS     - complete board report"));
  Serial.println(F("  INFO       - ESP32 information"));
  Serial.println(F("  ETH        - Ethernet state"));
  Serial.println(F("  SD         - microSD state"));
  Serial.println(F("  SD LIST    - list files in SD root"));
  Serial.println(F("  LED RED    - RGB red"));
  Serial.println(F("  LED GREEN  - RGB green"));
  Serial.println(F("  LED BLUE   - RGB blue"));
  Serial.println(F("  LED WHITE  - RGB white"));
  Serial.println(F("  LED OFF    - turn RGB off"));
  Serial.println(F("  REBOOT     - restart ESP32"));
}

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command.isEmpty()) {
    return;
  }

  if (command == "HELP" || command == "?") {
    printHelp();
  } else if (command == "STATUS") {
    printStatus();
  } else if (command == "INFO") {
    printSystemInfo();
  } else if (command == "ETH") {
    printEthernetStatus();
  } else if (command == "SD") {
    printSdStatus();
  } else if (command == "SD LIST") {
    listRootDirectory();
  } else if (command == "LED RED") {
    setRgb(255, 0, 0);
    Serial.println(F("RGB: red"));
  } else if (command == "LED GREEN") {
    setRgb(0, 255, 0);
    Serial.println(F("RGB: green"));
  } else if (command == "LED BLUE") {
    setRgb(0, 0, 255);
    Serial.println(F("RGB: blue"));
  } else if (command == "LED WHITE") {
    setRgb(255, 255, 255);
    Serial.println(F("RGB: white"));
  } else if (command == "LED OFF") {
    setRgb(0, 0, 0);
    Serial.println(F("RGB: off"));
  } else if (command == "REBOOT") {
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

    if (inputLine.length() < 96) {
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

#if ESP_ARDUINO_VERSION_MAJOR >= 2
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
#else
void onNetworkEvent(system_event_id_t event) {
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      ethernetStarted = true;
      ETH.setHostname("ticom-board");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      ethernetLinkUp = true;
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      ethernetHasIp = true;
      setRgb(0, 32, 0);
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      ethernetLinkUp = false;
      ethernetHasIp = false;
      setRgb(32, 16, 0);
      break;
    case SYSTEM_EVENT_ETH_STOP:
      ethernetStarted = false;
      ethernetLinkUp = false;
      ethernetHasIp = false;
      setRgb(32, 0, 0);
      break;
    default:
      break;
  }
}
#endif

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
  Serial.println(F("Stage 1: Ethernet, microSD and RGB. Modem disabled."));

  initSd();
  initEthernet();

  printHelp();
  Serial.print(F("> "));
}

void loop() {
  readSerialCommands();
  delay(2);
}
