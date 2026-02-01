📚 ESP8266 WiFi Word Learner (Infinite Vocabulary)

A smart, WiFi-connected vocabulary learning assistant powered by an ESP8266, a 1.8" TFT Screen, and an SD Card reader.

Unlike standard Arduino projects that crash with large text files, this project uses a memory-optimized streaming engine. It can read vocabulary lists with thousands of lines (even 5000+) directly from the SD card without using up the ESP8266's limited RAM.

✨ Features

♾️ Infinite List Support: Reads directly from the SD card line-by-line. Supports files with thousands of words.

📱 Web Interface: Control the device from your phone or PC (Next, Prev, Auto Mode, Add Words).

🔄 Auto-Play Mode: Automatically cycles through words at a speed you define (great for passive learning).

📂 File Manager: Create new vocabulary lists and switch between them via the web UI.

💾 Persistent Storage: New words added via WiFi are instantly saved to the SD card.

⚡ Memory Safe: Optimized to prevent heap fragmentation and crashes during long operation.

🛠️ Hardware Requirements

Component

Description

Microcontroller

ESP8266 (NodeMCU v1.0 / WeMos D1 Mini)

Display

1.44" or 1.8" TFT ST7735 SPI Display

Storage

Micro SD Card Module (SPI)

Storage Media

Micro SD Card (formatted FAT32)

Power

USB Cable or 3.7V LiPo Battery

🔌 Wiring Guide (Shared SPI)

This project uses Hardware SPI for both the Screen and the SD Card. This provides the fastest possible performance.

ESP8266 Pin

GPIO

Connect To

D5

GPIO 14

SCK (Screen) + SCK (SD Card)

D7

GPIO 13

SDA (Screen) + MOSI (SD Card)

D6

GPIO 12

MISO (SD Card)

D2

GPIO 4

CS (SD Card Chip Select)

D8

GPIO 15

CS (Screen Chip Select)

D1

GPIO 5

AO / DC (Screen Data/Command)

D0

GPIO 16

RESET (Screen)

3V3

-

VCC (Screen & SD)

GND

-

GND (Screen & SD)

Note: The SCK and MOSI lines are shared. You must twist/solder the wires from the Screen and SD card together for these two pins.

💾 SD Card Setup

Format your Micro SD card to FAT32.

(Optional) Create a file named words.txt in the root directory.

Format your text file like this (English,German):

Hello,Hallo
Water,Wasser
Computer,Computer
Sun,Sonne


(Optional) Add a logo.bmp (128x128px, 24-bit BMP) for a custom boot screen.

💻 Installation

Install Arduino IDE if you haven't already.

Install Libraries: Go to Sketch -> Include Library -> Manage Libraries and install:

Adafruit GFX Library

Adafruit ST7735 and ST7789 Library

Select Board: Set board to NodeMCU 1.0 (ESP-12E Module).

Edit Code: Open the .ino file and update your WiFi credentials:

const char* ssid     = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";


Upload: Connect your ESP8266 and click Upload.

📱 How to Use

Power On: The screen will show "Connecting..." followed by the IP Address (e.g., 192.168.1.105).

Connect: Open a web browser on your phone/PC and type that IP address.

Control:

Next/Prev: Manually move through the list.

Auto Mode: Set an interval (e.g., 3 seconds) and let it play.

Add Word: Type a new word pair and click Save. It saves immediately to the SD card.

Select File: If you have multiple lists (e.g., verbs.txt, nouns.txt), switch between them instantly.

🐛 Troubleshooting

Screen White/Blank: Check wiring, especially RESET and DC pins.

"SD Fail" Error: Ensure card is FAT32. Try formatting it again. Check the wiring on D6 (MISO).

Stops after 2-3 hours: Ensure you are using the latest version of the code which includes the Memory Fragmentation Fix (streaming bytes instead of Strings).

📜 License


This project is open-source. Feel free to modify and improve!
