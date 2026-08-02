#pragma once

#include <Arduino.h>
#include <ETH.h>

namespace board {

// microSD, SPI
constexpr int SD_MISO = 2;
constexpr int SD_MOSI = 15;
constexpr int SD_SCLK = 14;
constexpr int SD_CS   = 13;

// LAN8720 Ethernet PHY
constexpr int ETH_PHY_ADDR  = 0;
constexpr int ETH_POWER_PIN = 4;
constexpr int ETH_MDC_PIN   = 23;
constexpr int ETH_MDIO_PIN  = 18;
constexpr int ETH_RESET_PIN = 5;
constexpr eth_phy_type_t ETH_PHY_TYPE = ETH_PHY_LAN8720;
constexpr eth_clock_mode_t ETH_CLK_MODE = ETH_CLOCK_GPIO0_OUT;

// On-board addressable RGB LED
constexpr int RGB_LED_PIN = 12;
constexpr int RGB_LED_COUNT = 1;

// Reserved for the future modem stage. No module is installed yet.
constexpr int MODEM_TX = 33;
constexpr int MODEM_RX = 35;
constexpr int MODEM_PWRKEY = 32;
constexpr bool MODEM_ENABLED = false;

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint8_t RGB_BRIGHTNESS = 32;

}  // namespace board
