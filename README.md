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

## Supported RF Codes

This receiver parses 32-bit payloads consisting of a 16-bit Device ID, a 9-bit Command Code, a 3-bit rolling counter (0-7), and a 4-bit XOR checksum. The 8-bit command values we identified are:

| Code (Hex) | Action | Description |
| :--- | :--- | :--- |
| `0x048` | `LIGHT_TOGGLE` | Toggles the light state |
| `0x046` | `ALL_OFF` | Power off both light and fan |
| `0x044` | `FAN_FORWARD` | Fan forward (also powers fan ON) |
| `0x051` | `FAN_REVERSE` | Fan reverse (also powers fan ON) |
| `0x050` | `FAN_SPEED_1` | Set speed 1 |
| `0x052` | `FAN_SPEED_2` | Set speed 2 |
| `0x05D` | `FAN_SPEED_3` | Set speed 3 |
| `0x04A` | `FAN_SPEED_4` | Set speed 4 |
| `0x04E` | `FAN_SPEED_5` | Set speed 5 |
| `0x04C` | `FAN_SPEED_6` | Set speed 6 |
| `0x056` | `FAN_OFF` | Turn fan OFF |
| `0x043` | `LIGHT_INTENSITY_UP` | Increase light brightness |
| `0x045` | `LIGHT_INTENSITY_DN` | Decrease light brightness |
| `0x047` | `LIGHT_TEMP_UP` | Increase color temp (cooler) |
| `0x04B` | `LIGHT_TEMP_DN` | Decrease color temp (warmer) |
| `0x055` | `BREEZE_MODE` | Toggle breeze mode |
| `0x049` | `TIMER_2H` | Turn off after 2 hours |
| `0x058` | `TIMER_4H` | Turn off after 4 hours |

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
