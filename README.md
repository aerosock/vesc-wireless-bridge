# VESC Wireless Bridge · ESP32-S3 Headless BLE & Wi-Fi Dongle

Firmware for the **Waveshare ESP32-S3-LCD-3.16** (or any ESP32-S3 module) repurposed as a dedicated headless wireless VESC telemetry bridge. 

Turns your smartphone running the official **VESC Tool Mobile** app into your primary handlebar instrument cluster and ride computer, replacing broken physical displays.

---

## Supported Wireless & Wired Connections

| Interface | Network Name / Mode | Port / Service | Best Used For |
| :--- | :--- | :--- | :--- |
| **Bluetooth Low Energy (BLE)** | `VESC-BLE-BRIDGE` | Nordic UART Service (NUS)<br>`6e400001-b5a3-f393-e0a9-e50e24dcca9e` | **Daily Riding (Recommended):** Real-time telemetry in VESC Tool Mobile without disconnecting your phone from 4G/5G mobile data, GPS maps, or music. |
| | | | |
| **Wi-Fi SoftAP** | `VESC-WIFI-BRIDGE`<br>(Open, no password) | TCP Port `6510`<br>IP: `192.168.4.1` | **Fast Tuning & Motor Wizards:** High-speed TCP connection for full configuration backups and motor detection from phone or laptop. |
| | | | |
| **USB-C CDC Serial** | Direct USB connection | 115200 baud | **Benchtop & PC VESC Tool:** Direct wired serial connection with diagnostic live stats. |

---

## Why BLE Is the Best Option for Smartphone Cockpits

When using a phone as your handlebar dashboard while riding:
1. **Keeps Cellular Internet Active:** Connecting your phone to a Wi-Fi SoftAP that lacks internet often triggers "No Internet Connection" warnings on iOS and Android, or cuts mobile data routing (killing live GPS maps and streaming audio). BLE telemetry runs entirely in the background while leaving your mobile data connection intact.
2. **Instant Reconnection:** When you turn your bike on, VESC Tool Mobile can automatically reconnect to the BLE bridge within seconds.
3. **Ultra-Low Power:** The broken screen backlight and RGB DMA bus are permanently shut down in software. The ESP32-S3 draws only ~80–120mA instead of 500mA.

---

## Wiring & Pinout Reference

Using the onboard PH1.0 4-pin UART connector on the Waveshare board:

| Pin on Waveshare Board | Connects to VESC COMM Port | Description |
| :--- | :--- | :--- |
| **TXD (GPIO 43)** | **VESC COMM RX** | Transmit commands to VESC |
| **RXD (GPIO 44)** | **VESC COMM TX** | Receive live telemetry from VESC |
| **GND** | **VESC COMM GND** | Common signal ground (mandatory) |
| **3V3** | *Leave Disconnected* | Do not connect 3.3V between both boards |

> **Power Supply:**  
> Power the board via USB 5V or an external 12V→5V buck converter. Do not pull power from the VESC logic rail.

---

## How to Connect in VESC Tool Mobile

### Option A · Via Bluetooth LE (Recommended)
1. Turn on the bike / bridge.
2. Open **VESC Tool** on your phone.
3. Go to **Connection** → **Bluetooth LE**.
4. Tap **Scan** → Select **`VESC-BLE-BRIDGE`**.
5. Once connected, swipe over to the **Dashboard** or **RT Data** page to view live speed, battery %, current, and power.

### Option B · Via Wi-Fi TCP
1. On your phone, connect to the Wi-Fi network **`VESC-WIFI-BRIDGE`**.
2. Open **VESC Tool**.
3. Go to **Connection** → **TCP**.
4. Enter Host: `192.168.4.1`, Port: `6510`.
5. Tap **Connect**.

---

## Flashing Instructions

### Requirements
* [PlatformIO](https://platformio.org/) installed (`pio`).
* USB-C data cable connected to the ESP32-S3 board.

### Build and Upload
```bash
# Clone the repository
git clone https://github.com/aerosock/vesc-wireless-bridge.git
cd vesc-wireless-bridge

# Build and flash firmware
pio run -t upload

# Open serial monitor to view live diagnostic stats
pio run -t monitor
```

### Manual Bootloader Mode
If PlatformIO cannot automatically put the ESP32-S3 into download mode:
1. Hold down the **BOOT** button (GPIO 0).
2. Press and release the **RESET** button.
3. Release the **BOOT** button.
4. Run `pio run -t upload` again.
