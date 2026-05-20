# ESP32 Wifi educational project


> An educational, open-source Proof of Concept tool for ESP32 that demonstrates Wi-Fi network vulnerabilities and analyzes wireless protocols. Built for authorized penetration testing and security research.

![License](https://img.shields.io/badge/license-MIT-green) ![Platform](https://img.shields.io/badge/platform-ESP32-blue) ![Framework](https://img.shields.io/badge/framework-Arduino%20%2F%20PlatformIO-orange) ![Type](https://img.shields.io/badge/type-Educational%20PoC-lightgrey)

---

## Legal Disclaimer

**For authorized use only.** This project was created strictly for educational purposes, security research, and white-hat penetration testing. Demonstrating network protocol vulnerabilities through open-source code is a standard and legal practice in cybersecurity education.

Testing network attacks on infrastructure you do not own or lack **written authorization** for is illegal. The author accepts no liability for any misuse or damage caused by third parties using this code.

---

## Features

| Feature | Description |
|---|---|
| **Network Scan** | Detects 2.4 GHz access points with RSSI signal strength and channel analysis |
| **Station Monitor** | Scans and lists client devices connected to a selected network |
| **Deauth Testing** | Demonstrates client disconnection vulnerabilities in older Wi-Fi standards |
| **Flood Simulation** | Generates Beacon and Probe packets to test receiver stability under load |
| **Evil Portal Demo** | Demonstrates captive portal phishing mechanics in open public networks for awareness |

---

## Hardware Requirements

| Component | Specification |
|---|---|
| Microcontroller | ESP32 (e.g. NodeMCU, WROOM) |
| Display | OLED SSD1306 128×64 px via I²C |
| Button 1 | Tact-switch on `GPIO 12` — Up / Stop |
| Button 2 | Tact-switch on `GPIO 14` — Select |

---

## Installation

**1. Set up your development environment**

Install [Arduino IDE](https://www.arduino.cc/en/software) or the [PlatformIO](https://platformio.org/) extension for VS Code.

**2. Add ESP32 board support**

In Arduino IDE, go to *Boards Manager* and install the official ESP32 support package.

**3. Install required libraries**

Open the *Library Manager* and install:
- `Adafruit GFX Library`
- `Adafruit SSD1306`

**4. Upload the firmware**

Open the source file, select your ESP32 board from the board menu, and click **Upload**.

---

## License

This project is released under the [MIT License](LICENSE). You are free to modify and redistribute it, provided that attribution to the original author and the disclaimer are preserved in all distributed copies.
