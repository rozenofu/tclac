/**
 * TCL AC ESPHome component
 * Based on work by Miguel Ángel López, xaxexa, I-am-nightingale
 **/
#include "esphome.h"
#include "esphome/core/defines.h"
#include "tclac.h"

namespace esphome {
namespace tclac {

ClimateTraits tclacClimate::traits() {
    auto traits = climate::ClimateTraits();
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);

    if (this->supported_modes_.empty()) {
        traits.add_supported_mode(climate::CLIMATE_MODE_OFF);
        traits.add_supported_mode(climate::CLIMATE_MODE_AUTO);
    } else {
        for (auto mode : this->supported_modes_)
            traits.add_supported_mode(mode);
    }
    if (this->supported_presets_.empty()) {
        traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_NONE);
    } else {
        for (auto preset : this->supported_presets_)
            traits.add_supported_preset(preset);
    }
    if (this->supported_fan_modes_.empty()) {
        traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
    } else {
        for (auto fan_mode : this->supported_fan_modes_)
            traits.add_supported_fan_mode(fan_mode);
    }
    if (this->supported_swing_modes_.empty()) {
        traits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
    } else {
        for (auto swing_mode : this->supported_swing_modes_)
            traits.add_supported_swing_mode(swing_mode);
    }

    return traits;
}

void tclacClimate::loop() {
    if (esphome::uart::UARTDevice::available() == 0)
        return;

    dataRX[0] = esphome::uart::UARTDevice::read();
    if (dataRX[0] != 0xBB) {
        ESP_LOGD("TCL", "Sync lost: %02X", dataRX[0]);
        return;
    }
    dataRX[1] = esphome::uart::UARTDevice::read();
    dataRX[2] = esphome::uart::UARTDevice::read();
    dataRX[3] = esphome::uart::UARTDevice::read();
    dataRX[4] = esphome::uart::UARTDevice::read();
    esphome::uart::UARTDevice::read_array(dataRX + 5, dataRX[4] + 1);

    uint8_t check = getChecksum(dataRX, sizeof(dataRX));
    if (check != dataRX[67]) {
        ESP_LOGD("TCL", "Bad checksum: calc=%02X got=%02X", check, dataRX[67]);
        return;
    }
    this->readData();
}

void tclacClimate::update() {
    this->esphome::uart::UARTDevice::write_array(poll, sizeof(poll));
}

void tclacClimate::readData() {
    current_temperature = ((float)((dataRX[17] << 8) | dataRX[18]) / 374.0f - 32.0f) / 1.8f;
    target_temperature = (dataRX[FAN_SPEED_POS] & SET_TEMP_MASK) + 16;

    if (dataRX[MODE_POS] & (1 << 4)) {
        uint8_t modeswitch    = MODE_MASK & dataRX[MODE_POS];
        uint8_t fanswitch     = FAN_SPEED_MASK & dataRX[FAN_SPEED_POS];
        uint8_t swingswitch   = SWING_MODE_MASK & dataRX[SWING_POS];

        switch (modeswitch) {
            case MODE_AUTO:     this->mode = climate::CLIMATE_MODE_AUTO;     break;
            case MODE_COOL:     this->mode = climate::CLIMATE_MODE_COOL;     break;
            case MODE_DRY:      this->mode = climate::CLIMATE_MODE_DRY;      break;
            case MODE_FAN_ONLY: this->mode = climate::CLIMATE_MODE_FAN_ONLY; break;
            case MODE_HEAT:     this->mode = climate::CLIMATE_MODE_HEAT;     break;
            default:            this->mode = climate::CLIMATE_MODE_AUTO;
        }

        if (dataRX[FAN_QUIET_POS] & FAN_QUIET) {
            fan_mode = climate::CLIMATE_FAN_QUIET;
        } else if (dataRX[MODE_POS] & FAN_DIFFUSE) {
            fan_mode = climate::CLIMATE_FAN_DIFFUSE;
        } else {
            switch (fanswitch) {
                case FAN_AUTO:   fan_mode = climate::CLIMATE_FAN_AUTO;   break;
                case FAN_LOW:    fan_mode = climate::CLIMATE_FAN_LOW;    break;
                case FAN_MIDDLE: fan_mode = climate::CLIMATE_FAN_MIDDLE; break;
                case FAN_MEDIUM: fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
                case FAN_HIGH:   fan_mode = climate::CLIMATE_FAN_HIGH;   break;
                case FAN_FOCUS:  fan_mode = climate::CLIMATE_FAN_FOCUS;  break;
                default:         fan_mode = climate::CLIMATE_FAN_AUTO;
            }
        }

        switch (swingswitch) {
            case SWING_OFF:        swing_mode = climate::CLIMATE_SWING_OFF;        break;
            case SWING_HORIZONTAL: swing_mode = climate::CLIMATE_SWING_HORIZONTAL; break;
            case SWING_VERTICAL:   swing_mode = climate::CLIMATE_SWING_VERTICAL;   break;
            case SWING_BOTH:       swing_mode = climate::CLIMATE_SWING_BOTH;       break;
        }

        preset = ClimatePreset::CLIMATE_PRESET_NONE;
        if      (dataRX[7]  & (1 << 6)) preset = ClimatePreset::CLIMATE_PRESET_ECO;
        else if (dataRX[9]  & (1 << 2)) preset = ClimatePreset::CLIMATE_PRESET_COMFORT;
        else if (dataRX[19] & (1 << 0)) preset = ClimatePreset::CLIMATE_PRESET_SLEEP;

    } else {
        this->mode       = climate::CLIMATE_MODE_OFF;
        this->swing_mode = climate::CLIMATE_SWING_OFF;
        this->preset     = ClimatePreset::CLIMATE_PRESET_NONE;
    }

    this->publish_state();

    // Push compile-time settings to AC on first contact
    if (!initial_sync_done_) {
        initial_sync_done_ = true;
        this->takeControl();
    } else {
        allow_take_control = true;
    }
}

void tclacClimate::control(const climate::ClimateCall &call) {
    if (call.get_mode().has_value())               this->mode               = *call.get_mode();
    if (call.get_target_temperature().has_value()) this->target_temperature = *call.get_target_temperature();
    if (call.get_fan_mode().has_value())           this->fan_mode           = *call.get_fan_mode();
    if (call.get_swing_mode().has_value())         this->swing_mode         = *call.get_swing_mode();
    if (call.get_preset().has_value())             this->preset             = *call.get_preset();

    this->publish_state();
    this->takeControl();
    this->allow_take_control = true;
}

void tclacClimate::takeControl() {
    memset(dataTX, 0, sizeof(dataTX));

    uint8_t target_set = 31 - (int)target_temperature;

    // Header
    dataTX[0] = 0xBB;
    dataTX[2] = 0x01;
    dataTX[3] = 0x03;
    dataTX[4] = 0x20;
    dataTX[5] = 0x03;
    dataTX[6] = 0x01;
    dataTX[13] = 0x01;

    if (beeper_status_)
        dataTX[7] |= 0b00100000;
    if (display_status_ && mode != climate::CLIMATE_MODE_OFF)
        dataTX[7] |= 0b01000000;

    switch (this->mode) {
        case climate::CLIMATE_MODE_OFF:      break;
        case climate::CLIMATE_MODE_AUTO:     dataTX[7] |= 0b00000100; dataTX[8] |= 0b00001000; break;
        case climate::CLIMATE_MODE_COOL:     dataTX[7] |= 0b00000100; dataTX[8] |= 0b00000011; break;
        case climate::CLIMATE_MODE_DRY:      dataTX[7] |= 0b00000100; dataTX[8] |= 0b00000010; break;
        case climate::CLIMATE_MODE_FAN_ONLY: dataTX[7] |= 0b00000100; dataTX[8] |= 0b00000111; break;
        case climate::CLIMATE_MODE_HEAT:     dataTX[7] |= 0b00000100; dataTX[8] |= 0b00000001; break;
    }

    if (this->fan_mode.has_value()) {
        switch (*this->fan_mode) {
            case climate::CLIMATE_FAN_AUTO:    break;
            case climate::CLIMATE_FAN_QUIET:   dataTX[8]  |= 0b10000000; break;
            case climate::CLIMATE_FAN_LOW:     dataTX[10] |= 0b00000001; break;
            case climate::CLIMATE_FAN_MIDDLE:  dataTX[10] |= 0b00000110; break;
            case climate::CLIMATE_FAN_MEDIUM:  dataTX[10] |= 0b00000011; break;
            case climate::CLIMATE_FAN_HIGH:    dataTX[10] |= 0b00000111; break;
            case climate::CLIMATE_FAN_FOCUS:   dataTX[10] |= 0b00000101; break;
            case climate::CLIMATE_FAN_DIFFUSE: dataTX[8]  |= 0b01000000; break;
        }
    }

    switch (this->swing_mode) {
        case climate::CLIMATE_SWING_OFF:        break;
        case climate::CLIMATE_SWING_VERTICAL:   dataTX[10] |= 0b00111000; break;
        case climate::CLIMATE_SWING_HORIZONTAL: dataTX[11] |= 0b00001000; break;
        case climate::CLIMATE_SWING_BOTH:       dataTX[10] |= 0b00111000; dataTX[11] |= 0b00001000; break;
    }

    if (this->preset.has_value()) {
        switch (*this->preset) {
            case ClimatePreset::CLIMATE_PRESET_NONE:    break;
            case ClimatePreset::CLIMATE_PRESET_ECO:     dataTX[7]  |= 0b10000000; break;
            case ClimatePreset::CLIMATE_PRESET_SLEEP:   dataTX[19] |= 0b00000001; break;
            case ClimatePreset::CLIMATE_PRESET_COMFORT: dataTX[8]  |= 0b00010000; break;
        }
    }

    switch (vertical_swing_direction_) {
        case VerticalSwingDirection::UP_DOWN:  dataTX[32] |= 0b00001000; break;
        case VerticalSwingDirection::UPSIDE:   dataTX[32] |= 0b00010000; break;
        case VerticalSwingDirection::DOWNSIDE: dataTX[32] |= 0b00011000; break;
    }
    switch (horizontal_swing_direction_) {
        case HorizontalSwingDirection::LEFT_RIGHT: dataTX[33] |= 0b00001000; break;
        case HorizontalSwingDirection::LEFTSIDE:   dataTX[33] |= 0b00010000; break;
        case HorizontalSwingDirection::CENTER:     dataTX[33] |= 0b00011000; break;
        case HorizontalSwingDirection::RIGHTSIDE:  dataTX[33] |= 0b00100000; break;
    }
    switch (vertical_direction_) {
        case AirflowVerticalDirection::LAST:     break;
        case AirflowVerticalDirection::MAX_UP:   dataTX[32] |= 0b00000001; break;
        case AirflowVerticalDirection::UP:       dataTX[32] |= 0b00000010; break;
        case AirflowVerticalDirection::CENTER:   dataTX[32] |= 0b00000011; break;
        case AirflowVerticalDirection::DOWN:     dataTX[32] |= 0b00000100; break;
        case AirflowVerticalDirection::MAX_DOWN: dataTX[32] |= 0b00000101; break;
    }
    switch (horizontal_direction_) {
        case AirflowHorizontalDirection::LAST:      break;
        case AirflowHorizontalDirection::MAX_LEFT:  dataTX[33] |= 0b00000001; break;
        case AirflowHorizontalDirection::LEFT:      dataTX[33] |= 0b00000010; break;
        case AirflowHorizontalDirection::CENTER:    dataTX[33] |= 0b00000011; break;
        case AirflowHorizontalDirection::RIGHT:     dataTX[33] |= 0b00000100; break;
        case AirflowHorizontalDirection::MAX_RIGHT: dataTX[33] |= 0b00000101; break;
    }

    dataTX[9]  = target_set;
    dataTX[37] = getChecksum(dataTX, sizeof(dataTX));

    this->sendData(dataTX, sizeof(dataTX));
    allow_take_control = false;
}

void tclacClimate::sendData(uint8_t *message, uint8_t size) {
    this->esphome::uart::UARTDevice::write_array(message, size);
    ESP_LOGD("TCL", "Command sent");
}

uint8_t tclacClimate::getChecksum(const uint8_t *message, size_t size) {
    uint8_t crc = 0;
    for (size_t i = 0; i < size - 1; i++)
        crc ^= message[i];
    return crc;
}

void tclacClimate::set_beeper_state(bool state) {
    beeper_status_ = state;
    if (allow_take_control) takeControl();
}

void tclacClimate::set_display_state(bool disp_state) {
    display_status_ = disp_state;
    if (allow_take_control) takeControl();
}

void tclacClimate::set_vertical_airflow(AirflowVerticalDirection v_airflow) {
    vertical_direction_ = v_airflow;
    if (allow_take_control) takeControl();
}

void tclacClimate::set_horizontal_airflow(AirflowHorizontalDirection h_airflow) {
    horizontal_direction_ = h_airflow;
    if (allow_take_control) takeControl();
}

void tclacClimate::set_vertical_swing_direction(VerticalSwingDirection vs_direction) {
    vertical_swing_direction_ = vs_direction;
    if (allow_take_control) takeControl();
}

void tclacClimate::set_horizontal_swing_direction(HorizontalSwingDirection hs_direction) {
    horizontal_swing_direction_ = hs_direction;
    if (allow_take_control) takeControl();
}

void tclacClimate::set_supported_modes(climate::ClimateModeMask modes) {
    supported_modes_ = modes;
}

void tclacClimate::set_supported_fan_modes(climate::ClimateFanModeMask fan_modes) {
    supported_fan_modes_ = fan_modes;
}

void tclacClimate::set_supported_swing_modes(climate::ClimateSwingModeMask swing_modes) {
    supported_swing_modes_ = swing_modes;
}

void tclacClimate::set_supported_presets(climate::ClimatePresetMask presets) {
    supported_presets_ = presets;
}

}  // namespace tclac
}  // namespace esphome
