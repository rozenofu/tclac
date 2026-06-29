# tclac — ESPHome External Component for TCL AC Units

ESPHome external component for controlling TCL air conditioners and compatible units via UART.

This is a fork of [I-am-nightingale/tclac](https://github.com/I-am-nightingale/tclac), rewritten to follow native ESPHome component conventions.

## Changes from upstream

- Removed packages-based configuration in favor of `external_components:` with a self-contained device YAML
- Removed runtime Home Assistant helper entities (Beeper switch, Display switch, Force config switch, swing selects) — all settings are compile-time
- Removed LED wiring and screen management logic unrelated to the AC protocol
- Removed `force_mode` and `module_display` options
- Fixed temperature precision (integer division bug in sensor formula)
- Fixed uninitialized enum members in constructor (undefined behavior)
- Added `initial_sync_done_` pattern: pushes compile-time `beeper` and `show_display` settings to the AC on the first UART response
- Converted `climate.py` to async `to_code()`, applied all airflow/swing compile-time options
- Fixed `VerticalSwingDirection` enum name mismatch between Python and C++ (`UPDOWN` -> `UP_DOWN`)

## Hardware

Tested on **M5Stack ATOM S3 Lite** (ESP32-S3).

UART wiring:
- RX: GPIO7
- TX: GPIO8

The AC unit must expose a UART interface. Depending on the model, this may require soldering or using the native WiFi module connector. Even units with the same model number may differ in PCB revision and connector availability.

## Protocol

- Baud rate: 9600
- Data bits: 8
- Parity: EVEN
- Stop bits: 1
- RX packet: 68 bytes, XOR checksum over bytes [0..66] = byte[67]
- Poll packet sent every 5 seconds: `BB 00 01 04 02 01 00 BD`
- TX command packet: 38 bytes, same XOR checksum scheme

## Supported hardware

Verified compatible units (from upstream):

- Axioma ASX09H1 / ASB09H1
- Ballu Discovery DC BSVI-07HN8, BSVI-09HN8, BSVI-12HN8
- Daichi AIR20AVQ1 / AIR20FV1
- Daichi AIR25AVQS1R-1 / AIR25FVS1R-1
- Daichi AIR35AVQS1R-1 / AIR35FVS1R-1
- Daichi DA35EVQ1-1 / DF35EV1-1
- Dantex RK-12SATI / RK-12SATIE
- Ecostar Radium KVS-RAD09CH
- Royal Clima Gloria Inverter
- TCL ELI ONF 12
- TCL Liferise ONF 09
- TCL TAC-CT09INV/R
- TCL One Inverter TACM-09HRID/E1
- TCL TAC-07CHSA/TPG-W
- TCL TAC-09CHSA/TPG
- TCL TAC-09CHSA/DSEI-W
- TCL TAC-09HRID/E1
- TCL TAC-12CHSA/TPG
- TCL TAC-12CHSA/TPGI
- TCL TAC-XAL24I
- TCL TPG31IHB

## Configuration

```yaml
esphome:
  name: tcl-ac
  friendly_name: TCL AC
  min_version: 2026.4.0

esp32:
  board: m5stack-atoms3
  variant: esp32s3
  framework:
    type: arduino

logger:
  level: DEBUG
  baud_rate: 0

api:
  encryption:
    key: !secret api_key

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

external_components:
  - source:
      url: https://github.com/rozenofu/tclac.git
      type: git
      ref: master
    components: [ tclac ]
    refresh: 30s

uart:
  id: ac_uart
  rx_pin: GPIO7
  tx_pin: GPIO8
  baud_rate: 9600
  data_bits: 8
  parity: EVEN
  stop_bits: 1

climate:
  - platform: tclac
    name: "TCL AC"
    uart_id: ac_uart
    beeper: false
    show_display: true
    supported_modes:
      - "OFF"
      - AUTO
      - COOL
      - HEAT
      - DRY
      - FAN_ONLY
    supported_fan_modes:
      - AUTO
      - QUIET
      - LOW
      - MIDDLE
      - MEDIUM
      - HIGH
      - FOCUS
      - DIFFUSE
    supported_swing_modes:
      - "OFF"
      - VERTICAL
      - HORIZONTAL
      - BOTH
```

### Climate options

| Option | Type | Default | Description |
|---|---|---|---|
| `beeper` | bool | `false` | Enable AC beeper on command |
| `show_display` | bool | `true` | Enable AC display panel |
| `supported_modes` | list | all | Modes exposed in HA |
| `supported_fan_modes` | list | all | Fan speeds exposed in HA |
| `supported_swing_modes` | list | all | Swing modes exposed in HA |
| `supported_presets` | list | all | Presets exposed in HA (NONE, ECO, SLEEP, COMFORT) |
| `vertical_airflow` | enum | `CENTER` | Fixed vertical louver position (LAST, MAX_UP, UP, CENTER, DOWN, MAX_DOWN) |
| `horizontal_airflow` | enum | `CENTER` | Fixed horizontal louver position (LAST, MAX_LEFT, LEFT, CENTER, RIGHT, MAX_RIGHT) |
| `vertical_swing_mode` | enum | `UP_DOWN` | Vertical swing range (UP_DOWN, UPSIDE, DOWNSIDE) |
| `horizontal_swing_mode` | enum | `LEFT_RIGHT` | Horizontal swing range (LEFT_RIGHT, LEFTSIDE, CENTER, RIGHTSIDE) |

Note: `"OFF"` in `supported_modes` and `supported_swing_modes` must be quoted. YAML parses bare `OFF` as boolean false.

### Automation actions

```yaml
# Beeper
- climate.tclac.beeper_on:
- climate.tclac.beeper_off:

# Display
- climate.tclac.display_on:
- climate.tclac.display_off:

# Louver position at runtime
- climate.tclac.set_vertical_airflow:
    vertical_airflow: DOWN
- climate.tclac.set_horizontal_airflow:
    horizontal_airflow: CENTER
- climate.tclac.set_vertical_swing_direction:
    vertical_swing_mode: UP_DOWN
- climate.tclac.set_horizontal_swing_direction:
    horizontal_swing_mode: LEFT_RIGHT
```

---

## Credits

Protocol reverse engineering and original implementation: [@I-am-nightingale](https://github.com/I-am-nightingale/tclac), [@xaxexa](https://github.com/xaxexa), [@junkfix](https://github.com/junkfix).

The protocol analysis, byte-level decoding of AC state packets, and command frame structure are based entirely on the upstream work.
