#include <Arduino.h>
#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_mac.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled in this Arduino-ESP32 build.
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Bluetooth Classic SPP is not available for the selected ESP32 target.
#endif

namespace {

BluetoothSerial SerialBT;

bool classicStarted = false;
bool classicClientConnected = false;
bool bleInitialized = false;
uint32_t classicConnectCount = 0;
uint32_t classicDisconnectCount = 0;
String inputLine;

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t BLE_SCAN_SECONDS = 5;
constexpr size_t COMMAND_MAX_LENGTH = 160;

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
  snprintf(name, sizeof(name), "TICOM-%02X%02X", mac[4], mac[5]);
  return String(name);
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
                classicStarted && (classicClientConnected || SerialBT.hasClient())
                    ? "connected"
                    : "not connected");
  Serial.printf("SPP connections since start: %lu\n",
                static_cast<unsigned long>(classicConnectCount));
  Serial.printf("SPP disconnections since start: %lu\n",
                static_cast<unsigned long>(classicDisconnectCount));
  Serial.printf("BLE stack: %s\n", bleInitialized ? "initialized" : "stopped");
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
  Serial.println(F("Starting Bluetooth Classic SPP..."));

  // No keyboard or display is required on the ESP32 side. Windows or the
  // phone confirms pairing. This must be configured before begin().
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
  Serial.println(F("Important for Windows:"));
  Serial.println(F("  Pairing alone does not keep an SPP session open."));
  Serial.println(F("  Open the outgoing Bluetooth COM port in PuTTY, MobaXterm or another terminal."));
  Serial.println(F("  The port may disconnect again when that terminal is closed."));
  Serial.println(F("Data received over Bluetooth will be printed here and echoed back."));
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
  Serial.printf("BT SEND: PASS (%u characters)\n", static_cast<unsigned>(text.length()));
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

  Serial.printf("Scanning BLE devices for %u seconds...\n", BLE_SCAN_SECONDS);
  BLEScanResults *results = scanner->start(BLE_SCAN_SECONDS, false);

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
    const String name = device.haveName() ? device.getName() : String("<unnamed>");
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

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  HELP               - show this list"));
  Serial.println(F("  BT STATUS          - Bluetooth state, MAC and connection counters"));
  Serial.println(F("  BT CLASSIC START   - start Bluetooth Classic SPP"));
  Serial.println(F("  BT CLASSIC STOP    - stop Bluetooth Classic SPP"));
  Serial.println(F("  BT SEND <TEXT>     - send a line to the connected SPP client"));
  Serial.println(F("  BLE SCAN           - scan nearby BLE devices"));
  Serial.println(F("  BT OFF             - stop all Bluetooth modes"));
  Serial.println(F("  REBOOT             - restart ESP32"));
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

void readUsbCommands() {
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

    if (inputLine.length() < COMMAND_MAX_LENGTH) {
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

}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  Serial.println();
  Serial.println(F("T-Internet-COM Lab - Bluetooth check"));
  Serial.println(F("Bluetooth starts only by command."));
  printHelp();
  Serial.print(F("> "));
}

void loop() {
  readUsbCommands();
  serviceClassicTerminal();
  delay(2);
}
