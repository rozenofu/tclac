# tclac — внешний компонент ESPHome для кондиционеров TCL

Внешний компонент ESPHome для управления кондиционерами TCL и совместимыми моделями через UART.

Является форком [I-am-nightingale/tclac](https://github.com/I-am-nightingale/tclac), переписанным в соответствии с нативными соглашениями ESPHome.

## Отличия от оригинала

- Убрана конфигурация на основе `packages:`, заменена на `external_components:` с самодостаточным YAML-конфигом устройства
- Убраны runtime-переключатели в Home Assistant (Beeper, Display, Force config, выборы направления лопастей) — все настройки задаются на этапе компиляции
- Убрана логика управления светодиодами и дисплеем модуля, не относящаяся к протоколу кондиционера
- Убраны опции `force_mode` и `module_display`
- Исправлена точность температуры (целочисленное деление в формуле датчика)
- Исправлена неинициализированность enum-полей в конструкторе (undefined behavior)
- Добавлен паттерн `initial_sync_done_`: при первом ответе от кондиционера по UART компонент отправляет ему compile-time настройки `beeper` и `show_display`
- `climate.py` переведён на async `to_code()`, все airflow/swing опции применяются
- Исправлено несоответствие имён enum между Python и C++ (`UPDOWN` -> `UP_DOWN`)

## Оборудование

Протестировано на **M5Stack ATOM S3 Lite** (ESP32-S3).

Распиновка UART:

- RX: GPIO7
- TX: GPIO8

Кондиционер должен иметь доступный UART-интерфейс. В зависимости от модели это может потребовать пайки или использования разъёма родного WiFi-модуля. Даже одна и та же модель в разных ревизиях платы управления может отличаться наличием разъёма и распиновкой.

## Протокол

- Скорость: 9600 бод
- Биты данных: 8
- Чётность: EVEN
- Стоп-биты: 1
- RX-пакет: 68 байт, контрольная сумма XOR по байтам [0..66] = байт[67]
- Poll-пакет отправляется каждые 5 секунд: `BB 00 01 04 02 01 00 BD`
- TX-пакет команды: 38 байт, та же схема контрольной суммы XOR

## Поддерживаемое оборудование

Проверенные совместимые модели (из оригинального репозитория):

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

## Конфигурация

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

### Параметры компонента climate

| Параметр | Тип | По умолчанию | Описание |
| --- | --- | --- | --- |
| `beeper` | bool | `false` | Звуковой сигнал при получении команды |
| `show_display` | bool | `true` | Включить дисплей кондиционера |
| `supported_modes` | list | все | Режимы работы, доступные в HA |
| `supported_fan_modes` | list | все | Скорости вентилятора, доступные в HA |
| `supported_swing_modes` | list | все | Режимы качания, доступные в HA |
| `supported_presets` | list | все | Пресеты в HA (NONE, ECO, SLEEP, COMFORT) |
| `vertical_airflow` | enum | `CENTER` | Фиксированное положение вертикальных лопастей (LAST, MAX_UP, UP, CENTER, DOWN, MAX_DOWN) |
| `horizontal_airflow` | enum | `CENTER` | Фиксированное положение горизонтальных лопастей (LAST, MAX_LEFT, LEFT, CENTER, RIGHT, MAX_RIGHT) |
| `vertical_swing_mode` | enum | `UP_DOWN` | Диапазон вертикального качания (UP_DOWN, UPSIDE, DOWNSIDE) |
| `horizontal_swing_mode` | enum | `LEFT_RIGHT` | Диапазон горизонтального качания (LEFT_RIGHT, LEFTSIDE, CENTER, RIGHTSIDE) |

Важно: `"OFF"` в `supported_modes` и `supported_swing_modes` должен быть в кавычках. YAML без кавычек парсит `OFF` как булево `false`.

### Автоматизации

```yaml
# Звуковой сигнал
- climate.tclac.beeper_on:
- climate.tclac.beeper_off:

# Дисплей
- climate.tclac.display_on:
- climate.tclac.display_off:

# Положение лопастей в runtime
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

## Благодарности

Реверс-инжиниринг протокола и оригинальная реализация: [@I-am-nightingale](https://github.com/I-am-nightingale/tclac), [@xaxexa](https://github.com/xaxexa), [@junkfix](https://github.com/junkfix).

Анализ протокола, побайтовое декодирование пакетов состояния и структура командного фрейма полностью основаны на работе авторов оригинального репозитория.
