#include <Arduino.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "bridge_config.h"

// ==============================================================================
// State Variables & Telemetry Counters
// ==============================================================================
static bool _bleConnected = false;
static bool _prevBleConnected = false;

static uint32_t _bytes_vesc_to_ble = 0;
static uint32_t _bytes_ble_to_vesc = 0;
static uint32_t _bytes_vesc_to_wifi = 0;
static uint32_t _bytes_wifi_to_vesc = 0;
static uint32_t _bytes_vesc_to_usb = 0;
static uint32_t _bytes_usb_to_vesc = 0;

static uint32_t _last_status_print_ms = 0;

// ==============================================================================
// Wi-Fi TCP Server
// ==============================================================================
WiFiServer tcpServer(WIFI_TCP_PORT);
WiFiClient tcpClient;

// ==============================================================================
// Bluetooth Low Energy (BLE) Nordic UART Service
// ==============================================================================
BLEServer *pServer = nullptr;
BLECharacteristic *pTxCharacteristic = nullptr;
BLECharacteristic *pRxCharacteristic = nullptr;

class VescBleServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServerInstance) override {
        _bleConnected = true;
    }

    void onDisconnect(BLEServer* pServerInstance) override {
        _bleConnected = false;
        // Automatically resume advertising immediately so mobile apps can reconnect
        BLEDevice::startAdvertising();
    }
};

class VescBleRxCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        std::string rxValue = pCharacteristic->getValue();
        size_t len = rxValue.length();
        if (len > 0) {
            VESC_UART_PORT.write((const uint8_t*)rxValue.data(), len);
            _bytes_ble_to_vesc += len;
        }
    }
};

static void sendBleData(const uint8_t* data, size_t len) {
    if (!_bleConnected || pTxCharacteristic == nullptr || len == 0) return;

    // Send in chunks to ensure reliability across all phone BLE MTUs
    const size_t maxChunk = 128;
    size_t sent = 0;
    while (sent < len) {
        size_t toSend = (len - sent > maxChunk) ? maxChunk : (len - sent);
        pTxCharacteristic->setValue((uint8_t*)(data + sent), toSend);
        pTxCharacteristic->notify();
        sent += toSend;
        _bytes_vesc_to_ble += toSend;
        if (sent < len) {
            delayMicroseconds(600); // Brief pacing to prevent BLE controller queue drop
        }
    }
}

// ==============================================================================
// Setup Function
// ==============================================================================
void setup() {
    // 1. Immediately disable broken LCD backlight pin to eliminate ~400mA draw
    pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_LCD_BACKLIGHT, LOW);

    // 2. Initialize USB CDC serial for PC VESC Tool / debugging
    Serial.begin(115200);

    // 3. Initialize Hardware UART for VESC communication with 4KB queues
    VESC_UART_PORT.setRxBufferSize(UART_BUFFER_SIZE);
    VESC_UART_PORT.setTxBufferSize(UART_BUFFER_SIZE);
    VESC_UART_PORT.begin(VESC_UART_BAUDRATE, SERIAL_8N1, VESC_UART_RX_PIN, VESC_UART_TX_PIN);

    // 4. Start Wi-Fi SoftAP
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(WIFI_AP_LOCAL_IP, WIFI_AP_GATEWAY, WIFI_AP_SUBNET);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    tcpServer.begin();
    tcpServer.setNoDelay(true);

    // 5. Initialize BLE Nordic UART Service (NUS)
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEDevice::setMTU(BLE_PREFERRED_MTU);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new VescBleServerCallbacks());

    BLEService *pService = pServer->createService(BLE_SERVICE_UUID);

    // TX Characteristic: Device notifies phone with VESC telemetry
    pTxCharacteristic = pService->createCharacteristic(
        BLE_CHAR_TX_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // RX Characteristic: Phone writes commands to VESC
    pRxCharacteristic = pService->createCharacteristic(
        BLE_CHAR_RX_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new VescBleRxCallbacks());

    pService->start();

    // Start BLE Advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // Functions well with iOS & Android connection intervals
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    _last_status_print_ms = millis();
}

// ==============================================================================
// Main Loop
// ==============================================================================
void loop() {
    // --------------------------------------------------------------------------
    // A. Handle Incoming Wi-Fi TCP Clients & Data
    // --------------------------------------------------------------------------
    if (tcpServer.hasClient()) {
        if (!tcpClient || !tcpClient.connected()) {
            if (tcpClient) tcpClient.stop();
            tcpClient = tcpServer.accept();
            tcpClient.setNoDelay(true);
        } else {
            // Reject secondary client to protect active VESC session
            WiFiClient excessClient = tcpServer.accept();
            excessClient.stop();
        }
    }

    if (tcpClient && tcpClient.connected()) {
        uint8_t tcpBuf[BRIDGE_CHUNK_SIZE];
        while (tcpClient.available() > 0) {
            size_t bytesRead = tcpClient.read(tcpBuf, sizeof(tcpBuf));
            if (bytesRead > 0) {
                VESC_UART_PORT.write(tcpBuf, bytesRead);
                _bytes_wifi_to_vesc += bytesRead;
            }
        }
    }

    // --------------------------------------------------------------------------
    // B. Handle Incoming USB CDC Data (from PC VESC Tool)
    // --------------------------------------------------------------------------
    if (Serial && Serial.available() > 0) {
        uint8_t usbBuf[BRIDGE_CHUNK_SIZE];
        size_t bytesRead = Serial.read(usbBuf, sizeof(usbBuf));
        if (bytesRead > 0) {
            VESC_UART_PORT.write(usbBuf, bytesRead);
            _bytes_usb_to_vesc += bytesRead;
        }
    }

    // --------------------------------------------------------------------------
    // C. Handle Incoming VESC UART Responses -> Broadcast to Active Interfaces
    // --------------------------------------------------------------------------
    if (VESC_UART_PORT.available() > 0) {
        uint8_t vescBuf[BRIDGE_CHUNK_SIZE];
        size_t bytesRead = VESC_UART_PORT.read(vescBuf, sizeof(vescBuf));
        if (bytesRead > 0) {
            // Forward to BLE if connected
            if (_bleConnected) {
                sendBleData(vescBuf, bytesRead);
            }

            // Forward to Wi-Fi TCP if connected
            if (tcpClient && tcpClient.connected()) {
                tcpClient.write(vescBuf, bytesRead);
                _bytes_vesc_to_wifi += bytesRead;
            }

            // Forward to USB CDC if connected
            if (Serial) {
                Serial.write(vescBuf, bytesRead);
                _bytes_vesc_to_usb += bytesRead;
            }
        }
    }

    // --------------------------------------------------------------------------
    // D. Periodic Diagnostic Output (Visible on USB CDC Serial Monitor)
    // --------------------------------------------------------------------------
    uint32_t now = millis();
    if (now - _last_status_print_ms >= 5000) {
        _last_status_print_ms = now;

        // Print connection telemetry only when Serial monitor is connected
        if (Serial) {
            Serial.printf("[VESC BRIDGE] Uptime: %lu s | BLE: %s | WiFi Clients: %d | TCP Connected: %s\n",
                          now / 1000,
                          _bleConnected ? "CONNECTED" : "ADVERTISING",
                          WiFi.softAPgetStationNum(),
                          (tcpClient && tcpClient.connected()) ? "YES" : "NO");
            Serial.printf("              BLE Bytes (TX:%u, RX:%u) | WiFi Bytes (TX:%u, RX:%u) | USB Bytes (TX:%u, RX:%u)\n",
                          _bytes_vesc_to_ble, _bytes_ble_to_vesc,
                          _bytes_vesc_to_wifi, _bytes_wifi_to_vesc,
                          _bytes_vesc_to_usb, _bytes_usb_to_vesc);
        }
    }

    yield();
}
