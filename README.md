<div align="center">

```text
 ██████╗  ██████╗  ██████╗██╗  ██╗███████╗████████╗     ██████╗ ███████╗
 ██╔══██╗██╔═══██╗██╔════╝██║ ██╔╝██╔════╝╚══██╔══╝    ██╔═══██╗██╔════╝
 ██████╔╝██║   ██║██║     █████╔╝ █████╗     ██║       ██║   ██║███████╗
 ██╔═══╝ ██║   ██║██║     ██╔═██╗ ██╔══╝     ██║       ██║   ██║╚════██║
 ██║     ╚██████╔╝╚██████╗██║  ██╗███████╗   ██║       ╚██████╔╝███████║
 ╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝╚══════╝   ╚═╝        ╚═════╝ ╚══════╝
```

**A dark-mint operating system for a pocket radio tool.**

![version](https://img.shields.io/badge/version-v0.1-5fbc8a?style=flat-square&labelColor=0b0f0d)
![board](https://img.shields.io/badge/board-XIAO%20ESP32--S3-5fbc8a?style=flat-square&labelColor=0b0f0d)
![display](https://img.shields.io/badge/display-ST7789%20240×135-5fbc8a?style=flat-square&labelColor=0b0f0d)
![radios](https://img.shields.io/badge/radios-Wi--Fi%20·%20BLE-ffa500?style=flat-square&labelColor=0b0f0d)
![framework](https://img.shields.io/badge/framework-Arduino-5fbc8a?style=flat-square&labelColor=0b0f0d)

[Hardware](#-hardware) · [Controls](#-controls) · [Tools](#-tools) · [Build](#-build) · [Layout](#-layout)

</div>

---

```text
┌────────────────────────────────────────────┐
│ NEOSTICK / HOME                    B  ((·)) │
├────────────────────────────────────────────┤
│ ▌FLASHER         ╭──────────────────────╮  │
│  WIFI            │ WIFI                 │  │
│  BLE             │ ──────────────────── │  │
│  TIMER           │ 2.4 GHZ LINK         │  │
│  SETTINGS        │ LINK ON              │  │
│  SYSTEM          │                      │  │
│  ABOUT           │ ▬ ▬ ▬ ▬ ▬ ▬ ▬        │  │
│                  ╰──────────────────────╯  │
├────────────────────────────────────────────┤
│ BTN NEXT            2/7            SEL OPEN │
└────────────────────────────────────────────┘
```

Pocket OS is the firmware for a small stick you can carry. The home screen is a list of tools and a focus card. Wi-Fi and BLE stay visible in the header. After a short idle the lock screen shows the clock.

> [!NOTE]
> The radio advertises as **NEOSTICK**. That is the name you pair, and the name of the setup network.

## ◈ Hardware

| Part | Detail |
| --- | --- |
| Board | Seeed Studio XIAO ESP32-S3 |
| Display | ST7789, 240×135, landscape |
| Buttons | Next, Select, Power. All active low, internal pull-ups |
| Radios | 2.4 GHz Wi-Fi and Bluetooth LE |

| Signal | GPIO |
| --- | --- |
| TFT SCLK | 7 |
| TFT MOSI | 9 |
| TFT CS | 6 |
| TFT DC | 5 |
| TFT RST | 4 |
| Next | 1 |
| Select | 2 |
| Power | 3 |

The backlight is not on a GPIO in this build (`BL_PIN` is `-1` in `config.h`). Brightness control starts working when that pin is set to a free GPIO. Avoid strapping pins 0, 3, 45, and 46.

## ◈ Controls

| Press | Does |
| --- | --- |
| <kbd>NEXT</kbd> | Move the highlight, or the “up” action on a tool |
| <kbd>SEL</kbd> | Open, confirm, or the “down” action |
| <kbd>SEL</kbd> held | Deep sleep. Press <kbd>NEXT</kbd> to wake |
| <kbd>PWR</kbd> | Lock screen. Any key unlocks |

On the BLE remote, **Power** leaves the remote instead of locking. Hold **Select** still sleeps.

The lock screen appears on its own after the saver time (30 seconds by default). Sleep itself stays off until you turn it on in Settings, so a USB session is not cut short.

## ◈ Tools

### ▸ Flasher

Full-white torch. Select turns it off. The saver does not cover the torch.

### ▸ Wi-Fi

| Item | What you get |
| --- | --- |
| Connect | Joins the strongest saved network |
| Link info | IP, gateway, DNS, live RSSI, channel, MAC |
| Show Wi-Fi | Nearby networks, channel, security, signal |
| Channels | Access points per channel, and the quietest of 1 / 6 / 11 |
| Setup | Open access point `NEOSTICK` at `http://192.168.4.1` |
| Packets | Frames per second. Hop all channels, or lock one |

On a network row, Select opens **Details**, **Walk**, and **Join** when the network is saved or open. Walk is a single bar that fills as you get closer. Up to four networks are stored on the device. Passwords are typed on the setup page, not kept in the sketch.

> [!TIP]
> The packet view counts frames only. It does not keep addresses or contents. The link drops while it runs.

### ▸ BLE

| Item | What you get |
| --- | --- |
| Scan | Nearby devices. Select a row to walk toward it |
| Beacon | Advertise the name `NEOSTICK` |
| Remote | Bluetooth keyboard |

The remote has two modes. **Slides** sends Left and Right. **Volume** sends volume down and volume up, and repeats while held. Pair it like any keyboard. The beacon pauses while the remote is open.

### ▸ Timer

Stopwatch, or a countdown of 1, 5, 10, 15, 30, or 60 minutes.

### ▸ Settings

Saved on the device.

| Item | Choices |
| --- | --- |
| Brightness | 10, 25, 50, 75, 100% once a backlight pin is set |
| Saver | Off, 15s, 30s, 1m |
| Sleep | Off, 30s, 1m, 2m, 5m |
| Boot | Home, or a tool to open after the splash |
| Rotate | Landscape, or 180° |

### ▸ System

Heap, uptime, CPU frequency, die temperature, flash size, last reset reason, and the Wi-Fi MAC. The temperature is the chip sensor, not the room.

### ▸ About

Build name, radio, and version.

## ◈ Clock

The lock screen clock uses NTP after a successful join. The zone is `IST-5:30` in `config.h`. Change `TZ_INFO` for another zone. Before the first sync the clock shows `--:--`.

## ◈ Build

1. Install the ESP32 board package in the Arduino IDE.
2. Install [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) and [Adafruit ST7735 and ST7789](https://github.com/adafruit/Adafruit-ST7735-Library).
3. Open `m5stick.ino`.
4. Select **XIAO_ESP32S3**.
5. Upload.

```text
Board package   esp32:esp32:XIAO_ESP32S3
```

From the command line, with the Arduino CLI already set up for that board:

```bash
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3 .
arduino-cli upload  --fqbn esp32:esp32:XIAO_ESP32S3 -p PORT .
```

## ◈ Layout

| File | Role |
| --- | --- |
| `m5stick.ino` | Pins, home screen, buttons |
| `config.h` | Name, version, colors, clock, backlight pin |
| `boot_animation.ino` | Splash |
| `lock_screen.ino` | Lock screen and NTP |
| `wifi_config.ino` | Saved networks, scan, link page, channel chart |
| `portal.ino` | Setup access point |
| `packets.ino` | Frame-rate monitor |
| `ble_config.ino` | Scan and beacon |
| `remote.ino` | BLE keyboard |
| `walker.ino` | Signal bar |
| `timer.ino` | Stopwatch and countdown |
| `settings.ino` | Saved preferences |
| `system.ino` | Diagnostics |
| `power_config.ino` | Backlight, idle, deep sleep |
| `vendors.h` | A short table of common radio manufacturers |

## ◈ Theme

| Swatch | Role | RGB565 |
| --- | --- | --- |
| ![bg](https://img.shields.io/badge/-%20%20%20%20-000000?style=flat-square) | Background | `0x0000` |
| ![panel](https://img.shields.io/badge/-%20%20%20%20-08080a?style=flat-square) | Panel | `0x0841` |
| ![mint](https://img.shields.io/badge/-%20%20%20%20-5ff7e0?style=flat-square) | Live state | `0x5FBC` |
| ![dim](https://img.shields.io/badge/-%20%20%20%20-28ceb0?style=flat-square) | Labels | `0x2E76` |
| ![amber](https://img.shields.io/badge/-%20%20%20%20-ffa500?style=flat-square) | Waiting, open networks | `0xFD20` |

Text is the Adafruit 5×7 font, so labels stay short enough for the 240-pixel width.

---

<div align="center">
<sub>Pocket OS · built by Gavindu Achintha</sub>
</div>
