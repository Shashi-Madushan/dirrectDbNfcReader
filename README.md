# ESP8266 NFC Attendance System

This project is an ESP8266-based NFC attendance system that uses the MFRC522 NFC reader, MySQL database, and a NeoPixel LED strip for status indication. It allows employees to check in and out using NFC cards.

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Components](#components)
- [Hardware Connections](#hardware-connections)
- [Software Setup](#software-setup)
- [Usage](#usage)
- [Configuration](#configuration)
- [License](#license)

## Introduction

This project is designed to track employee attendance using NFC cards. The system connects to a MySQL database to store attendance records and provides a web interface for configuration.

## Features

- NFC card reading for employee check-in and check-out
- WiFi connectivity for database access and web configuration
- MySQL database integration
- NeoPixel LED strip for visual status indication
- Configurable time ranges for attendance tracking
- Web interface for configuration

## Components

- ESP8266 D1 Mini
- MFRC522 NFC Reader
- NeoPixel LED Strip (4 LEDs)
- Buzzer
- MySQL Server
- 3.3V and 5V power supply

## Hardware Connections

### 1. ESP8266 D1 Mini to MFRC522 NFC Reader

| MFRC522 Pin | ESP8266 D1 Mini Pin |
|-------------|---------------------|
| RST         | D1 (GPIO5)          |
| SDA (SS)    | D2 (GPIO4)          |
| MOSI        | D7 (GPIO13)         |
| MISO        | D6 (GPIO12)         |
| SCK         | D5 (GPIO14)         |
| GND         | GND                 |
| 3.3V        | 3V3                 |

### 2. ESP8266 D1 Mini to Adafruit NeoPixel LED Strip

| NeoPixel Pin | ESP8266 D1 Mini Pin |
|--------------|---------------------|
| Data In      | D4 (GPIO2)          |
| GND          | GND                 |
| 5V           | 5V                  |

### 3. ESP8266 D1 Mini to Buzzer

| Buzzer Pin | ESP8266 D1 Mini Pin |
|------------|---------------------|
| +          | D0 (GPIO16)         |
| -          | GND                 |

## Software Setup

1. Install the following Arduino libraries:
    - `ESP8266WiFi`
    - `ESP8266WebServer`
    - `SPI`
    - `MFRC522`
    - `MySQL_Connection`
    - `MySQL_Cursor`
    - `ArduinoJson`
    - `NTPClient`
    - `WiFiUdp`
    - `TimeLib`
    - `Adafruit_NeoPixel`

2. Upload the code to your ESP8266 D1 Mini using the Arduino IDE.

3. Create a MySQL database with a table named `orders`:
    ```sql
    CREATE TABLE attendence (
        id INT AUTO_INCREMENT PRIMARY KEY,
        employee_id VARCHAR(255) NOT NULL,
        date DATE NOT NULL,
        place_time TIME NOT NULL,
        taken VARCHAR(255) NOT NULL
    );
    ```

## Usage

1. Power up the ESP8266 D1 Mini.
2. The NeoPixel LED strip will indicate the status:
    - Solid blue: WiFi connected
    - Solid green: MySQL connected
    - Red blinking: Issues with connections or NFC reading
3. Use the web interface to configure WiFi and MySQL settings.
4. Tap an NFC card to the MFRC522 reader to record attendance.

## Configuration

1. Connect to the ESP8266 AP named `NFC_v1.0.1_Config` using the default password.
2. Open a browser and navigate to `http://192.168.4.1/`.
3. Fill in the WiFi and MySQL configuration details.
4. Save the configuration and the device will restart with the new settings.

## License

This project is licensed under the MIT License.
