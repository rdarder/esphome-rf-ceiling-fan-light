#pragma once
#include "esphome.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/remote_base/rc_switch_protocol.h"
#include <stdint.h>

namespace esphome {

enum class FanCmd : uint16_t {
    LIGHT_TOGGLE       = 0x048,
    ALL_OFF            = 0x046,
    FAN_FORWARD        = 0x044,
    FAN_REVERSE        = 0x051,
    FAN_SPEED_1        = 0x050,
    FAN_SPEED_2        = 0x052,
    FAN_SPEED_3        = 0x05D,
    FAN_SPEED_4        = 0x04A,
    FAN_SPEED_5        = 0x04E,
    FAN_SPEED_6        = 0x04C,
    FAN_OFF            = 0x056,
    LIGHT_INTENSITY_UP = 0x042,
    LIGHT_INTENSITY_DN = 0x045,
    LIGHT_TEMP_UP      = 0x047,
    LIGHT_TEMP_DN      = 0x04B,
    BREEZE_MODE        = 0x055,
    TIMER_2H           = 0x049,
    TIMER_4H           = 0x058
};

class CeilingFanDevice : public Component {
private:
    uint16_t _device_id;
    uint8_t  _counter;
    remote_transmitter::RemoteTransmitterComponent *_transmitter;

public:
    CeilingFanDevice(uint16_t device_id, remote_transmitter::RemoteTransmitterComponent *transmitter) 
        : _device_id(device_id), _transmitter(transmitter), _counter(0) {}

    void transmit_cmd(FanCmd cmd) {
        uint16_t op = static_cast<uint16_t>(cmd);
        
        uint32_t packet_data = ((uint32_t)_device_id << 12) | 
                               ((uint32_t)op << 3)         | 
                               (_counter & 0x07);
        
        uint8_t xor_sum = 0;
        for (int i = 0; i < 7; i++) {
            uint8_t nibble = (packet_data >> (i * 4)) & 0x0F;
            xor_sum ^= nibble;
        }
        
        uint8_t chk = xor_sum ^ 0x0A;
        uint32_t final_payload = (packet_data << 4) | (chk & 0x0F);
        
        ESP_LOGD("custom_fan", "Transmitting Hex: 0x%08X (Counter: %d)", final_payload, _counter);
        _counter = (_counter + 1) & 0x07;

        // Custom protocol initialization scaled perfectly to a 283 microsecond pulse clock base
        // Arguments mapping: (sync_high, sync_low, zero_high, zero_low, one_high, one_low, inverted)
        remote_base::RCSwitchBase custom_proto(0, 0, 283, 849, 849, 283, false);

        auto transmission = _transmitter->transmit();
        auto *data_buffer = transmission.get_data();
        
        data_buffer->set_carrier_frequency(0);

        // Standard household receivers require multiple continuous repeats to validate an action
        for (int repeat = 0; repeat < 6; repeat++) {
            // 1. Transmit data bits first
            for (int16_t i = 31; i >= 0; i--) {
                if (final_payload & ((uint32_t)1 << i)) {
                    custom_proto.one(data_buffer);
                } else {
                    custom_proto.zero(data_buffer);
                }
            }
            // 2. Trailing sync footer matching exactly the 283us base (1 unit mark, 31 units space)
            data_buffer->mark(283);
            data_buffer->space(8773);
        }

        transmission.perform();
    }
};

} // namespace esphome
