#pragma once
#include "esphome.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/remote_base/rc_switch_protocol.h"
#include <vector>
#include <stdint.h>
#include <cmath>

namespace esphome {

enum class FanCmd : uint16_t {
    LIGHT_TOGGLE       = 0x048, // Reverted back to 0x048
    ALL_OFF            = 0x046, // Reverted back to 0x046
    FAN_FORWARD        = 0x044,
    FAN_REVERSE        = 0x051,
    FAN_SPEED_1        = 0x050,
    FAN_SPEED_2        = 0x052,
    FAN_SPEED_3        = 0x05D,
    FAN_SPEED_4        = 0x04A,
    FAN_SPEED_5        = 0x04E,
    FAN_SPEED_6        = 0x04C,
    FAN_OFF            = 0x056,
    LIGHT_INTENSITY_UP = 0x043,
    LIGHT_INTENSITY_DN = 0x045,
    LIGHT_TEMP_UP      = 0x047,
    LIGHT_TEMP_DN      = 0x04B,
    BREEZE_MODE        = 0x055,
    TIMER_2H           = 0x049,
    TIMER_4H           = 0x058
};

class CeilingFanHub : public Component {
 private:
  uint16_t device_id_;
  remote_transmitter::RemoteTransmitterComponent *transmitter_;
  uint8_t counter_{0};
  
  std::vector<FanCmd> queue_;
  uint32_t last_transmission_{0};
  
  fan::Fan *fan_{nullptr};

  // Light assumed state tracking
  bool assumed_light_state_{false};
  float assumed_brightness_{1.0f};
  float assumed_color_temp_{0.5f}; // 0.0 (Warm) - 1.0 (Cool) normalized

  // Fan assumed state tracking
  bool assumed_fan_state_{false};
  int assumed_fan_speed_{1};
  fan::FanDirection assumed_fan_direction_{fan::FanDirection::FORWARD};

 public:
  CeilingFanHub(uint16_t device_id, remote_transmitter::RemoteTransmitterComponent *transmitter)
      : device_id_(device_id), transmitter_(transmitter) {}

  void set_fan(fan::Fan *fan) { this->fan_ = fan; }

  void setup() override {
    ESP_LOGI("custom_hub", "Boot setup: queueing absolute ALL_OFF.");
    this->queue_command(FanCmd::ALL_OFF);
  }

  void loop() override {
    if (this->queue_.empty()) return;
    uint32_t now = millis();
    if (now - this->last_transmission_ >= 200) {
      FanCmd cmd = this->queue_.front();
      this->queue_.erase(this->queue_.begin());
      this->transmit_cmd(cmd);
      this->last_transmission_ = now;
    }
  }

  void queue_command(FanCmd cmd) {
    this->queue_.push_back(cmd);
    ESP_LOGD("custom_hub", "Queued command: 0x%03X. Queue size: %d", static_cast<uint16_t>(cmd), this->queue_.size());
  }

  void restore_fan_direction() {
    if (this->fan_ != nullptr && this->fan_->state) {
      if (this->fan_->direction == fan::FanDirection::FORWARD) {
        ESP_LOGD("custom_hub", "Restoring fan state by queueing FAN_FORWARD.");
        this->queue_command(FanCmd::FAN_FORWARD);
      } else {
        ESP_LOGD("custom_hub", "Restoring fan state by queueing FAN_REVERSE.");
        this->queue_command(FanCmd::FAN_REVERSE);
      }
    }
  }

  // Fan controls logic in C++
  void control_fan_state(bool target_state) {
    ESP_LOGI("custom_fan", "HA Fan State Event - Target: %s, Current assumed: %s", 
             target_state ? "ON" : "OFF", this->assumed_fan_state_ ? "ON" : "OFF");
             
    if (target_state) {
      ESP_LOGI("custom_fan", "Action: Turning Fan ON. Queueing direction + speed.");
      if (this->fan_ != nullptr) {
        if (this->fan_->direction == fan::FanDirection::FORWARD) {
          this->queue_command(FanCmd::FAN_FORWARD);
        } else {
          this->queue_command(FanCmd::FAN_REVERSE);
        }
        
        int speed = this->fan_->speed;
        if (speed >= 1 && speed <= 6) {
          FanCmd speed_cmds[] = {
            FanCmd::FAN_SPEED_1,
            FanCmd::FAN_SPEED_2,
            FanCmd::FAN_SPEED_3,
            FanCmd::FAN_SPEED_4,
            FanCmd::FAN_SPEED_5,
            FanCmd::FAN_SPEED_6
          };
          this->queue_command(speed_cmds[speed - 1]);
        }
      }
      this->assumed_fan_state_ = true;
    } else {
      ESP_LOGI("custom_fan", "Action: Turning Fan OFF. Queueing FAN_OFF.");
      this->queue_command(FanCmd::FAN_OFF);
      this->assumed_fan_state_ = false;
    }
  }

  void control_fan_speed(int speed) {
    ESP_LOGI("custom_fan", "HA Fan Speed Event - Target Speed: %d (Assumed: %d)", speed, this->assumed_fan_speed_);
    if (this->fan_ != nullptr && this->fan_->state) {
      if (speed >= 1 && speed <= 6) {
        ESP_LOGI("custom_fan", "Action: Adjusting Speed. Queueing Speed %d command.", speed);
        FanCmd speed_cmds[] = {
          FanCmd::FAN_SPEED_1,
          FanCmd::FAN_SPEED_2,
          FanCmd::FAN_SPEED_3,
          FanCmd::FAN_SPEED_4,
          FanCmd::FAN_SPEED_5,
          FanCmd::FAN_SPEED_6
        };
        this->queue_command(speed_cmds[speed - 1]);
      }
    }
    this->assumed_fan_speed_ = speed;
  }

  void control_fan_direction(fan::FanDirection direction) {
    ESP_LOGI("custom_fan", "HA Fan Direction Event - Target Direction: %s (Assumed: %s)", 
             direction == fan::FanDirection::FORWARD ? "FORWARD" : "REVERSE",
             this->assumed_fan_direction_ == fan::FanDirection::FORWARD ? "FORWARD" : "REVERSE");

    if (this->fan_ != nullptr && this->fan_->state) {
      if (direction != this->assumed_fan_direction_) {
        ESP_LOGI("custom_fan", "Action: Direction changed while Fan is ON. Queueing FAN_OFF + New Direction + Restore Speed.");
        this->queue_command(FanCmd::FAN_OFF);
        if (direction == fan::FanDirection::FORWARD) {
          this->queue_command(FanCmd::FAN_FORWARD);
        } else {
          this->queue_command(FanCmd::FAN_REVERSE);
        }
        int speed = this->fan_->speed;
        if (speed >= 1 && speed <= 6) {
          FanCmd speed_cmds[] = {
            FanCmd::FAN_SPEED_1,
            FanCmd::FAN_SPEED_2,
            FanCmd::FAN_SPEED_3,
            FanCmd::FAN_SPEED_4,
            FanCmd::FAN_SPEED_5,
            FanCmd::FAN_SPEED_6
          };
          this->queue_command(speed_cmds[speed - 1]);
        }
      }
    } else {
      ESP_LOGI("custom_fan", "Action: Direction changed while Fan is OFF. No command sent, updating assumed state.");
    }
    this->assumed_fan_direction_ = direction;
  }

  // Light controls logic
  void control_light(light::LightState *state) {
    bool target_state = state->remote_values.is_on();
    float target_brightness = state->remote_values.get_brightness();
    float target_color_temp = state->remote_values.get_color_temperature();
    
    // Invert mapping: 153 mireds (6500K / Coolest) -> 1.0, 333 mireds (3000K / Warmest) -> 0.0
    float ct_normalized = (333.0f - target_color_temp) / (333.0f - 153.0f);
    if (ct_normalized < 0.0f) ct_normalized = 0.0f;
    if (ct_normalized > 1.0f) ct_normalized = 1.0f;

    ESP_LOGI("custom_light", "HA Light Event - State: %s (Assumed: %s), Brightness: %.2f (Assumed: %.2f), Temp: %.2f Mireds / Norm: %.2f (Assumed: %.2f)",
             target_state ? "ON" : "OFF", this->assumed_light_state_ ? "ON" : "OFF",
             target_brightness, this->assumed_brightness_,
             target_color_temp, ct_normalized, this->assumed_color_temp_);

    bool state_changed = (target_state != this->assumed_light_state_);

    if (state_changed) {
      if (!target_state) {
        ESP_LOGI("custom_light", "Action: Turning Light OFF. Queueing ALL_OFF + restore fan.");
        this->queue_command(FanCmd::ALL_OFF);
        this->restore_fan_direction();
      } else {
        ESP_LOGI("custom_light", "Action: Turning Light ON. Queueing ALL_OFF + LIGHT_TOGGLE + restore fan.");
        this->queue_command(FanCmd::ALL_OFF);
        this->queue_command(FanCmd::LIGHT_TOGGLE);
        this->restore_fan_direction();
      }
      this->assumed_light_state_ = target_state;
    }

    if (this->assumed_light_state_) {
      // Scale brightness to 12 steps
      int target_bright_step = std::round(target_brightness * 12.0f);
      int assumed_bright_step = std::round(this->assumed_brightness_ * 12.0f);
      int bright_diff = target_bright_step - assumed_bright_step;
      
      if (bright_diff != 0) {
        ESP_LOGI("custom_light", "Action: Adjusting Brightness. Target Step: %d, Assumed Step: %d (Diff: %d)", 
                 target_bright_step, assumed_bright_step, bright_diff);
        if (bright_diff > 0) {
          for (int i = 0; i < bright_diff; i++) {
            this->queue_command(FanCmd::LIGHT_INTENSITY_UP);
          }
        } else {
          for (int i = 0; i < -bright_diff; i++) {
            this->queue_command(FanCmd::LIGHT_INTENSITY_DN);
          }
        }
      }
      this->assumed_brightness_ = target_brightness;

      // Scale color temp to 10 steps
      int target_temp_step = std::round(ct_normalized * 10.0f);
      int assumed_temp_step = std::round(this->assumed_color_temp_ * 10.0f);
      int temp_diff = target_temp_step - assumed_temp_step;
      
      if (temp_diff != 0) {
        ESP_LOGI("custom_light", "Action: Adjusting Color Temperature. Target Step: %d, Assumed Step: %d (Diff: %d)", 
                 target_temp_step, assumed_temp_step, temp_diff);
        if (temp_diff > 0) {
          for (int i = 0; i < temp_diff; i++) {
            this->queue_command(FanCmd::LIGHT_TEMP_UP);
          }
        } else {
          for (int i = 0; i < -temp_diff; i++) {
            this->queue_command(FanCmd::LIGHT_TEMP_DN);
          }
        }
      }
      this->assumed_color_temp_ = ct_normalized;
    }
  }

  void transmit_cmd(FanCmd cmd) {
    uint16_t op = static_cast<uint16_t>(cmd);
    
    uint32_t packet_data = ((uint32_t)this->device_id_ << 12) | 
                           ((uint32_t)op << 3)         | 
                           (this->counter_ & 0x07);
    
    uint8_t xor_sum = 0;
    for (int i = 0; i < 7; i++) {
        uint8_t nibble = (packet_data >> (i * 4)) & 0x0F;
        xor_sum ^= nibble;
    }
    
    uint8_t chk = xor_sum ^ 0x0A;
    uint32_t final_payload = (packet_data << 4) | (chk & 0x0F);
    
    ESP_LOGD("custom_fan", "Transmitting Hex: 0x%08X (Counter: %d, Command: 0x%03X)", final_payload, this->counter_, op);
    this->counter_ = (this->counter_ + 1) & 0x07;

    remote_base::RCSwitchBase custom_proto(0, 0, 283, 849, 849, 283, false);

    auto transmission = this->transmitter_->transmit();
    auto *data_buffer = transmission.get_data();
    
    data_buffer->set_carrier_frequency(0);

    for (int repeat = 0; repeat < 3; repeat++) {
        for (int16_t i = 31; i >= 0; i--) {
            if (final_payload & ((uint32_t)1 << i)) {
                custom_proto.one(data_buffer);
            } else {
                custom_proto.zero(data_buffer);
            }
        }
        data_buffer->mark(283);
        data_buffer->space(8773);
    }

    transmission.perform();
  }
};

} // namespace esphome
