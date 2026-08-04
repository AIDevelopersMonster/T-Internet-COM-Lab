#pragma once

#include <Arduino.h>
#include <ETH.h>

namespace board {

// microSD, SPI
constexpr int SD_MISO = 2;
constexpr int SD_MOSI = 15;
constexpr int SD_SCLK = 14;
constexpr int SD_CS = 13;
constexpr const char *SD_TEST_PATH = "/ticom_test.txt";

// LAN8720 Ethernet PHY
constexpr int ETH_PHY_ADDR = 0;
constexpr int ETH_POWER_PIN = 4;
constexpr int ETH_MDC_PIN = 23;
constexpr int ETH_MDIO_PIN = 18;
constexpr int ETH_RESET_PIN = 5;
constexpr eth_phy_type_t ETH_PHY_TYPE = ETH_PHY_LAN8720;
constexpr eth_clock_mode_t ETH_CLK_MODE = ETH_CLOCK_GPIO0_OUT;

// On-board addressable RGB LED
constexpr int RGB_LED_PIN = 12;
constexpr int RGB_LED_COUNT = 1;

// Bluetooth diagnostics
constexpr const char *BLUETOOTH_NAME_PREFIX = "TICOM";
constexpr uint32_t BLE_SCAN_SECONDS = 5;

// Reserved for the future modem stage. No module is installed yet.
constexpr int MODEM_TX = 33;
constexpr int MODEM_RX = 35;
constexpr int MODEM_PWRKEY = 32;
constexpr bool MODEM_ENABLED = false;

constexpr uint32_t SERIAL_BAUD = 115200;
// Do not use the name RGB_BRIGHTNESS here: Arduino-ESP32 3.x defines it as a macro.
constexpr uint8_t RGB_LED_BRIGHTNESS = 32;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr size_t SERIAL_COMMAND_MAX_LENGTH = 160;

}  // namespace board
