# Pinout

GPIO assignments for **ESP32-WROVER** (`esp32:esp32:esp32wrover`). Constants live in `Display.cpp`, `Button.cpp`, `GPS.cpp`, and `Transmitter.cpp`.

**Do not use GPIO 6–11**: internal flash on all ESP32 modules. On WROVER modules **GPIO 16/17** are also wired to the PSRAM.

## Summary

| GPIO | Function | Module | Notes |
|------|----------|--------|--------|
| 2 | G21 | Radio | Stack net (strapping pin) |
| 4 | G19 | Radio | Stack net |
| 13 | JJY 40 kHz out (G22) | Radio | LEDC, when JJY enabled |
| 14 | TFT CS | Display | |
| 15 | G25 | Radio | Stack net (strapping pin) |
| 16 | — | | **Avoid**: PSRAM pin on WROVER |
| 17 | G33 | Radio | Stack net. **Avoid**: PSRAM pin on WROVER |
| 18 | TFT RST | Display | |
| 19 | — | | Free |
| 21 | Button right | Button | |
| 22 | — | | Free (was GPS PPS) |
| 23 | GPS UART TX | GPS | Assigned in firmware, **not wired** |
| 25 | TFT SCL | Display | SPI clock |
| 26 | TFT SDA | Display | SPI MOSI |
| 27 | TFT DC | Display | |
| 32 | Button enter | Button | |
| 33 | Button left | Button | |
| 34 | GPS RX wire (unused) | GPS | Input-only, so nothing is sent to the GPS |
| 35 | GPS UART RX (GPS TX → MCU) | GPS | Input-only |
| 39 (VN) | GPS PPS | GPS | Input-only |

Board header pins labelled **RX / TX** are GPIO 3 / 1 (UART0, USB-to-UART bridge). Keep them free for flashing and the serial monitor.

## Buttons

| Button | GPIO | Wiring |
|--------|------|--------|
| Left | 33 | → GND |
| Enter (middle) | 32 | → GND |
| Right | 21 | → GND |

## Display (GC9A01A, SPI)

| Signal | GPIO |
|--------|------|
| RST | 18 |
| CS | 14 |
| DC | 27 |
| SDA (MOSI) | 26 |
| SCL (SCK) | 25 |

MISO not connected. **Backlight (BL)** on the round module must be tied to **3.3 V** (or a GPIO driven high); otherwise the panel stays dark even when SPI works.

SPI uses **HSPI** at 12 MHz (`Display.cpp`).

## GPS (NEO-7M)

| Signal | GPIO | Wiring |
|--------|------|--------|
| UART RX (MCU) | 35 | GPS **TX** → MCU RX |
| — | 34 | GPS **RX** wire lands here; input-only, so nothing is sent to the GPS |
| PPS | 39 (VN) | GPS PPS |
| UART TX (MCU) | 23 | Firmware only, not wired |

The three GPS wires share one connector on the top header (GPS TX → 35, GPS RX → 34).

9600 8N1, `HardwareSerial(1)`.

## JJY / BF-018A (stack nets)

| Stack net | GPIO | Firmware |
|-----------|------|----------|
| **G22** | **13** | **40 kHz LEDC** (when JJY enabled) |
| G21 | 2 | — |
| G25 | 15 | — |
| G33 | 17 | — |
| G23 | — | Stack UART if routed separately |
| G19 | 4 | — |

5 V + GND to BF-018A per module docs. JJY output is currently **commented out** in `Transmitter.cpp`.

## Notes

- **GPIO 13**: JJY signal (BF-018A G22 net). Moved from GPIO 16 to avoid sharing it with GPS RX.
- **GPIO 35 / 34 / 39**: GPS connector. Moved off RX/TX (USB) and 16/17 (WROVER PSRAM). GPIO 36/39 can see brief false pulses while the ADC is active (ESP32 errata); this only affects PPS sub-second timing.
- **GPIO 1 / 3** (header **TX / RX**): UART0 to the USB bridge. Don't wire peripherals here.
- **GPIO 2 / 15**: strapping pins. Make sure the BF-018A doesn't pull them high/low during boot.
