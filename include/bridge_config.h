#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// ==============================================================================
// Hardware Pin Mapping (Waveshare ESP32-S3-LCD-3.16 Board)
// ==============================================================================

// VESC Hardware UART (PH1.0 4-PIN Connector)
// Pin 3: TXD (GPIO 43) -> Connected to Flipsky COMM RX
// Pin 4: RXD (GPIO 44) -> Connected to Flipsky COMM TX
#define VESC_UART_PORT        Serial0
#define VESC_UART_RX_PIN      43
#define VESC_UART_TX_PIN      44
#define VESC_UART_BAUDRATE    115200

// LCD Backlight Pin (GPIO 6)
// Kept permanently driven LOW to disable broken screen backlight & save power (~400mA)
#define PIN_LCD_BACKLIGHT     6

// ==============================================================================
// Serial Buffer Sizing
// ==============================================================================
// 4KB buffers prevent dropped packets during large VESC Tool motor config (COMM_SET_MCCONF)
#define UART_BUFFER_SIZE      4096
#define BRIDGE_CHUNK_SIZE     512

// ==============================================================================
// Wi-Fi SoftAP & TCP Server Configuration
// ==============================================================================
#define WIFI_AP_SSID          "VESC-WIFI-BRIDGE"
#define WIFI_AP_PASS          ""  // Open network
#define WIFI_TCP_PORT         6510

// SoftAP Static IP (192.168.4.1)
const IPAddress WIFI_AP_LOCAL_IP(192, 168, 4, 1);
const IPAddress WIFI_AP_GATEWAY(192, 168, 4, 1);
const IPAddress WIFI_AP_SUBNET(255, 255, 255, 0);

// ==============================================================================
// Bluetooth Low Energy (BLE) Configuration - Nordic UART Service (NUS)
// ==============================================================================
// Standard Nordic UART Service recognized natively by VESC Tool Mobile & Desktop
#define BLE_DEVICE_NAME       "VESC-BLE-BRIDGE"
#define BLE_SERVICE_UUID      "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // Write / Write No Response
#define BLE_CHAR_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // Notify
#define BLE_PREFERRED_MTU     517
