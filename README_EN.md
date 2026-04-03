# UVK5 ESP32-S3 Firmware Project

> ESP32-S3 based Quansheng UV-K5 radio firmware

[中文文档](Readme.md) | English

## 📋 Project Overview

This project uses ESP32-S3 as the main controller for the Quansheng UV-K5 radio, replacing the original DP32G030 MCU. Leveraging the powerful processing capabilities and rich peripherals of ESP32-S3, it brings the following enhanced features to UV-K5:

### ✨ Key Features

- **🌐 NRL Network Audio Bridge**: WiFi-based NRL UDP voice bridging using G.711 A-law
- **🕐 RTC Real-Time Clock**: Precise time management and scheduling functions
- **📶 WiFi OTA Updates**: Wireless firmware upgrades without disassembly
- **🎛️ Complete Radio Functions**: Retains all core features of the original UV-K5
- **🔧 Extensibility**: Based on Arduino framework, easy for secondary development

## 🌐 NRL Network Audio Bridge

The firmware now includes an NRL network audio bridge for bidirectional voice forwarding between RF and the network.

### What It Does

- Connects to an NRL server over WiFi using either an IPv4 address or a hostname
- Uses `UDP` as the transport
- Uses `G.711 A-law` for network voice encoding
- Uses `8kHz / 16-bit / Mono` as the local audio format
- Sends fixed `160-byte` G.711 A-law voice payloads
- Accepts adaptive incoming voice payloads from `160` to `500` bytes
- Plays incoming network audio on the speaker while keying RF transmit
- Plays received RF audio locally while forwarding it to NRL
- Shows the remote `CALLSIGN-SSID` on the main screen during network voice activity

### NRL Packet Mapping

- Header magic: `NRL2`
- Header length: `48` bytes
- Voice packet type: `1`
- Heartbeat packet type: `2`
- Server voice packet type accepted on RX: `9`
- Default heartbeat interval: `2000ms`
- `CPUID` is currently fixed to `0`
- The packet counter increments for each outgoing packet

### Configuration

NRL-related configuration lives in [src/lib/nrl_audio_config.h](/home/caocheng/ham/UVE5/src/lib/nrl_audio_config.h):

- `NRL_AUDIO_WIFI_SSID`: WiFi SSID
- `NRL_AUDIO_WIFI_PASSWORD`: WiFi password
- `NRL_AUDIO_SERVER_HOST`: NRL server address, accepts IP or hostname
- `NRL_AUDIO_SERVER_PORT`: remote server port
- `NRL_AUDIO_LOCAL_PORT`: local UDP bind port, use `0` for an ephemeral port
- `NRL_AUDIO_CALLSIGN`: local callsign
- `NRL_AUDIO_CALLSIGN_SSID`: local SSID
- `NRL_AUDIO_DEVICE_MODE`: NRL device mode field
- `NRL_AUDIO_HEARTBEAT_INTERVAL_MS`: heartbeat period
- `NRL_AUDIO_RX_PACKET_TIMEOUT_MS`: incoming voice timeout

### Notes

- NRL parameters are currently compile-time macros and are not yet exposed in the menu or EEPROM
- The final behavior of “speaker playback while transmitting RF” still depends on the board-level audio routing
- The network audio feature mentioned in this repository currently refers to this NRL UDP audio bridge implementation

## 🛠️ Hardware Platform

- **MCU**: ESP32-S3 (Dual-core Xtensa LX7, 240MHz)
- **Radio Chip**: BK4819 (UHF/VHF Transceiver)
- **FM Chip**: BK1080 (FM Radio)
- **Display**: ST7565 128x64 LCD
- **Storage**: I2C EEPROM (Configuration storage)

## 🚀 Quick Start

### Requirements

- **Operating System**: Windows / macOS / Linux
- **Development Tool**: Visual Studio Code
- **Python**: 3.6+ (Required by PlatformIO)

### Installing VS Code and PlatformIO

#### 1. Install Visual Studio Code

Visit [VS Code Official Website](https://code.visualstudio.com/) to download and install the version for your operating system.

#### 2. Install PlatformIO IDE Extension

1. Open VS Code
2. Click the Extensions icon on the left sidebar (or press `Ctrl+Shift+X` / `Cmd+Shift+X`)
3. Search for "PlatformIO IDE"
4. Click "Install"

![PlatformIO Installation](https://docs.platformio.org/en/latest/_images/platformio-ide-vscode-pkg-installer.png)

#### 3. Wait for PlatformIO Initialization

During the first installation, PlatformIO will automatically download the required toolchains and dependencies, which may take a few minutes.

### Clone the Project

```bash
git clone https://github.com/losehu/UVE5.git
cd UVE5
```

### Open the Project

1. In VS Code, select `File` → `Open Folder`
2. Select the cloned `UVE5` directory
3. PlatformIO will automatically recognize the `platformio.ini` configuration file

### Build Firmware

#### Method 1: Using PlatformIO Buttons

1. Click the "PlatformIO: Build" button in the bottom status bar
2. Or use the shortcut `Ctrl+Alt+B` / `Cmd+Alt+B`

#### Method 2: Using Task Menu

1. Press `Ctrl+Shift+P` / `Cmd+Shift+P` to open the command palette
2. Type "PlatformIO: Build"
3. Select environment `app0_main` or `bootmgr_factory`

#### Method 3: Using Terminal

```bash
# Build main application
platformio run --environment app0_main

# Build Bootloader
platformio run --environment bootmgr_factory

# Clean build output
platformio run --target clean
```

### Upload Firmware

#### Upload via USB Serial

1. Connect the ESP32-S3 board to your computer
2. Click the "PlatformIO: Upload" button in the bottom status bar
3. Or use the command:

```bash
platformio run --target upload --environment app0_main
```

#### Upload via OTA (Over-The-Air)

```bash
platformio run --target upload --upload-port <ESP32_IP_ADDRESS>
```

## 📁 Project Structure

```
UVE5/
├── platformio.ini          # PlatformIO configuration
├── boards/                 # Custom board definitions
│   └── uvk5.json          # UVK5 ESP32-S3 board config
├── src/                    # Source code directory
│   ├── app/               # Main application firmware
│   │   ├── main.cpp       # Arduino main entry point
│   │   ├── board.c/h      # Board initialization
│   │   ├── settings.c/h   # Configuration management
│   │   ├── radio.c/h      # Radio control
│   │   ├── audio.c/h      # Audio processing
│   │   ├── app/           # Application layer
│   │   │   ├── app.c/h           # Main application loop
│   │   │   ├── menu.c/h          # Menu system
│   │   │   ├── scanner.c/h       # Frequency scanner
│   │   │   ├── spectrum.c/h      # Spectrum analyzer
│   │   │   ├── fm.c/h            # FM radio
│   │   │   ├── dtmf.c/h          # DTMF codec
│   │   │   ├── messenger.c/h     # Messaging
│   │   │   └── mdc1200.c/h       # MDC1200 protocol
│   │   ├── driver/        # Hardware driver layer
│   │   │   ├── bk4819.cpp/h      # BK4819 radio chip
│   │   │   ├── bk1080.cpp/h      # BK1080 FM chip
│   │   │   ├── st7565.cpp/h      # ST7565 LCD driver
│   │   │   ├── backlight.cpp/h   # Backlight control
│   │   │   ├── keyboard.cpp/h    # Keyboard input
│   │   │   ├── eeprom.cpp/h      # EEPROM storage
│   │   │   ├── i2c.cpp/h         # I2C communication
│   │   │   ├── adc.cpp/h         # ADC battery monitor
│   │   │   ├── system.cpp/h      # System delays
│   │   │   └── uart.cpp/h        # UART communication
│   │   ├── ui/            # User interface
│   │   │   ├── main.c/h          # Main UI
│   │   │   ├── menu.c/h          # Menu UI
│   │   │   ├── welcome.c/h       # Welcome screen
│   │   │   ├── status.c/h        # Status bar
│   │   │   └── battery.c/h       # Battery indicator
│   │   └── helper/        # Helper functions
│   │       ├── battery.c/h       # Battery management
│   │       └── rds.c/h           # RDS decoder
│   └── bootloader/        # Bootloader firmware
│       └── main.cpp       # Bootloader main program
├── lib/                    # Library files
│   └── shared_flash.h     # Shared flash partition definitions
├── src/lib/                # App-side extra modules
│   ├── nrl_audio_bridge.cpp/h  # NRL UDP audio bridge
│   └── nrl_audio_config.h      # NRL audio configuration
└── scripts/                # Utility scripts
    ├── upload_app0.py     # Upload script
    └── serial_monitor.py  # Serial monitor

```

### Core Module Description

#### 📱 App (Application)

Located in the `src/app/` directory, contains the main radio functions:

- **Main Loop**: `main.cpp` - Arduino-style `setup()` and `loop()` functions
- **Board Support**: `board.c` - Hardware initialization including BK4819, LCD, I2C, etc.
- **Radio Control**: `radio.c` - Frequency setting, modulation modes, power control
- **Audio Processing**: `audio.c` - Audio path control, volume management
- **Settings Management**: `settings.c` - EEPROM read/write, configuration parameters

**Application Layer** (`app/` subdirectory):
- Menu system, frequency scanner, spectrum analyzer
- FM radio, DTMF codec
- MDC1200 protocol support

**Driver Layer** (`driver/` subdirectory):
- Low-level drivers for hardware chips
- GPIO, I2C, SPI, ADC peripheral control

**UI Layer** (`ui/` subdirectory):
- Rendering logic for various screens
- Interaction with ST7565 LCD

#### 🔧 Bootloader

Located in the `src/bootloader/` directory, responsible for:

- **Firmware Boot**: Starting the main application
- **OTA Support**: Managing firmware partition switching
- **Failure Recovery**: Checking if main application is valid
- **Serial Update**: Upgrading firmware via serial port

## 🔨 Build Configuration

### PlatformIO Environments

The project defines two build environments:

#### 1. `app0_main` - Main Application

```ini
[env:app0_main]
platform = espressif32
board = uvk5
framework = arduino
board_build.partitions = partitions.csv
board_build.filesystem = littlefs
```

**Partition Layout** (partitions.csv):
- Bootloader: Boot program partition
- App0: Main application partition (OTA partition 0)
- App1: Backup application partition (OTA partition 1)
- NVS: Non-volatile storage
- LittleFS: File system

#### 2. `bootmgr_factory` - Bootloader

```ini
[env:bootmgr_factory]
platform = espressif32
board = uvk5
framework = arduino
```

### Build Options

Configure in `platformio.ini`:

```ini
build_flags =
    -DENABLE_FMRADIO          # Enable FM radio
    -DENABLE_MESSENGER        # Enable messaging
    -DENABLE_SPECTRUM         # Enable spectrum analyzer
    -DENABLE_DOPPLER          # Enable Doppler feature
```

## 🔌 Pin Definitions

### ESP32-S3 GPIO Mapping

| Function | GPIO | Description |
|----------|------|-------------|
| BK4819 SCN | GPIO 2 | Chip Select |
| BK4819 SCL | GPIO 1 | Serial Clock |
| BK4819 SDA | GPIO 34 | Serial Data |
| LCD RST | GPIO 45 | Reset |
| LCD CS | GPIO 7 | Chip Select |
| LCD MOSI | GPIO 4 | Data |
| LCD CLK | GPIO 6 | Clock |
| LCD A0 | GPIO 5 | Command/Data |
| Backlight | GPIO 8 | Backlight Control |
| I2C SDA | GPIO 13 | EEPROM Data |
| I2C SCL | GPIO 14 | EEPROM Clock |
| ADC Voltage | GPIO 10 | Battery Voltage |
| ADC Current | GPIO 9 | Battery Current |

## 🐛 Troubleshooting

### Build Errors

**Issue**: `undefined reference to xxx`
- **Solution**: Check if header files include `extern "C"` wrapper
- C++ files calling C functions require extern "C" declaration

**Issue**: `fatal error: xxx.h: No such file or directory`
- **Solution**: Check `build_flags` and `lib_deps` in `platformio.ini`

### Upload Failures

**Issue**: Cannot connect to ESP32
- **Solution**: 
  1. Check if USB cable supports data transfer
  2. Hold BOOT button while resetting device
  3. Verify COM port in Device Manager

**Issue**: Device doesn't start after upload
- **Solution**: 
  1. Upload bootloader first
  2. Then upload main application
  3. Check partition table configuration

## 🤝 Contributing

Issues and Pull Requests are welcome!

1. Fork this repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- [Quansheng UV-K5 Original Firmware](https://github.com/DualTachyon/uv-k5-firmware)
- [Quansheng UV-K5 losehu Firmware](https://github.com/losehu/uv-k5-firmware-custom)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- PlatformIO Team

## 📞 Contact

- **Project Homepage**: https://github.com/losehu/UVE5
- **Issues**: https://github.com/losehu/UVE5/issues

---

⭐ If this project helps you, please give it a Star!
