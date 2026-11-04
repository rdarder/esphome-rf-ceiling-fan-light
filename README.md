# ESPHome 433MHz RF Ceiling Fan & Light Combo Driver

This repository contains a native, consolidated ESPHome custom C++ driver and blueprint for controlling white-label 433MHz RF-controlled ceiling fan and light combos (such as those marketed under the *Novohome* brand and other generic white-label imports).

It uses a consolidated parent-child coordinator architecture where a single C++ Hub component manages the rolling counter, checksum generation, and non-blocking command serialization, while standard ESPHome `template` fan and `color_temperature` light entities provide native Home Assistant integration.

---

## Key Features

1. **State Reconciliation**: Disables transitions and uses absolute `ALL_OFF` commands during OFF sequences and boot initialization to ensure the physical device state matches Home Assistant without dropping out of sync.
2. **Non-blocking Command Queue**: Commands are spaced out by exactly $200\text{ms}$ in a C++ scheduler, preventing overlapping RF signal collisions (which can freeze the ESP32's loop or corrupt packets).
3. **True Color Temperature Mapping**: Inverts and maps the ESPHome mired values correctly to the bluest ($6500\text{K}$ / $153\text{ mireds}$) and warmest ($3000\text{K}$ / $333\text{ mireds}$) limits across 11 discrete steps.
4. **Subtle Dimming Control**: Normalizes HA's target brightness range across 14 discrete steps.
5. **No Dynamic Hacks**: Avoids hacky dynamic entity injection, allowing normal YAML validation, Home Assistant discovery, and easy registration of multiple ceiling fans.

---

## Hardware Connection Diagram

To transmit the 433MHz signals, you connect a generic 433MHz RF transmitter module (like the FS1000A or similar ASK/OOK transmitter) to your ESP32-C3 dev kit.

### Wiring Diagram

```
      +--------------------+            +--------------------+
      |  ESP32-C3 Dev Kit  |            | 433MHz Transmitter |
      |                    |            |                    |
      |                5V  |----------->|  VCC               |
      |                GND |----------->|  GND               |
      |             GPIO10 |----------->|  DATA              |
      +--------------------+            +--------------------+
                                                  |
                                                  |
                                           [ ANTENNA (17.3cm) ]
```

> [!NOTE]
> Connecting a $17.3\text{cm}$ straight wire to the transmitter antenna pad is highly recommended to achieve reliable range.

---

## Protocol & RF Packet Format

The remote transmits 32-bit payloads consisting of:
- **20-bit Device ID** (bits 31 to 12, values `0x00000` to `0xFFFFF`, unique to each fan/remote pair)
- **5-bit Universal Command Code** (bits 11 to 7, values `0x00` to `0x1F`)
- **3-bit Rolling Counter** (bits 6 to 4, values `0` to `7`)
- **4-bit Checksum** (bits 3 to 0, XOR sum of the 7 preceding 4-bit nibbles XORed with `0x0A`)

```
 31                     12 11     7 6   4 3        0
+-------------------------+--------+-----+----------+
|   Device ID (20 bits)   | Opcode | Ctr | Checksum |
|   (0x00000..0xFFFFF)    |(5 bits)|(3b) | (4 bits) |
+-------------------------+--------+-----+----------+
```

### Supported Command Opcodes (5-bit)

These opcodes are universal across all fans and remotes of this type:

| Code (Hex) | Dec | Action | Description |
| :---: | :---: | :--- | :--- |
| `0x03` | 3 | `LIGHT_INTENSITY_UP` | Increase light brightness |
| `0x04` | 4 | `FAN_FORWARD` | Fan forward / summer (also powers fan ON) |
| `0x05` | 5 | `LIGHT_INTENSITY_DN` | Decrease light brightness |
| `0x06` | 6 | `ALL_OFF` | Power off both light and fan |
| `0x07` | 7 | `LIGHT_TEMP_UP` | Increase color temp (cooler) |
| `0x08` | 8 | `LIGHT_TOGGLE` | Toggles the light state |
| `0x09` | 9 | `TIMER_2H` | Turn off after 2 hours |
| `0x0A` | 10 | `FAN_SPEED_4` | Set speed 4 |
| `0x0B` | 11 | `LIGHT_TEMP_DN` | Decrease color temp (warmer) |
| `0x0C` | 12 | `FAN_SPEED_6` | Set speed 6 (highest) |
| `0x0D` | 13 | `COLOR_TEMP_CYCLE` | Cycle light color temperature presets |
| `0x0E` | 14 | `FAN_SPEED_5` | Set speed 5 |
| `0x10` | 16 | `FAN_SPEED_1` | Set speed 1 (lowest) |
| `0x11` | 17 | `FAN_REVERSE` | Fan reverse / winter (also powers fan ON) |
| `0x12` | 18 | `FAN_SPEED_2` | Set speed 2 |
| `0x14` | 20 | `NIGHT_LIGHT` | Minimum brightness night mode |
| `0x15` | 21 | `BREEZE_MODE` | Toggle natural breeze mode |
| `0x16` | 22 | `FAN_OFF` | Turn fan OFF |
| `0x19` | 25 | `TIMER_4H` | Turn off after 4 hours |
| `0x1D` | 29 | `FAN_SPEED_3` | Set speed 3 |

---

## Project Structure

* **`ceiling_fan_hub.h`**: The C++ implementation containing the command queue loop scheduler, absolute state machine rules, and signal transmission wrappers.
* **`ceiling_fan_blueprint.yaml`**: The blueprint configuration containing the template fan and light state definitions.
* **`living-room-fans.yaml`**: The master configuration mapping multiple rooms/combos.

---

## ESPHome YAML Configuration

### 1. Master Configuration File (`living-room-fans.yaml`)

```yaml
esphome:
  name: fan-controller

esp32:
  board: esp32-c3-devkitm-1
  framework:
    type: arduino

logger:
  level: DEBUG 

api:
ota:
  platform: esphome

wifi:
  ssid: "YOUR_WIFI_SSID"
  password: "YOUR_WIFI_PASSWORD"

remote_transmitter:
  id: rf_transmitter
  pin: GPIO10
  carrier_duty_percent: 100%
  non_blocking: true

packages:
  living_room_fan:
    !include
    file: ceiling_fan_blueprint.yaml
    vars:
      room_name: "living_room"
      room_friendly_name: "Living Room"
      radio_id: "0xC6C9"
  bedroom_fan:
    !include
    file: ceiling_fan_blueprint.yaml
    vars:
      room_name: "bedroom"
      room_friendly_name: "Bedroom"
      radio_id: "0x36A7"
```
