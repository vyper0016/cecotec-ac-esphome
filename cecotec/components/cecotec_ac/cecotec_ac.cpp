#include "cecotec_ac.h"
#include "esphome/components/remote_base/nec_protocol.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::cecotec_ac {

static const char *const TAG = "cecotec_ac";

static const uint16_t NEC_ADDRESS = 0x7F80;
// Indexed by Key: power, mode, up, down, speed, sleep, timer
static const uint16_t NEC_COMMANDS[] = {0x639C, 0x629D, 0x6B94, 0x6A95, 0x6996, 0x649B, 0x609F};
static const char *const KEY_NAMES[] = {"power", "mode", "up", "down", "speed", "sleep", "timer"};

// 7-segment code (bit0 = a ... bit6 = g, bit7 = dot) -> digit, -1 if not a digit
static int seg_digit(uint8_t seg) {
  switch (seg & 0x7F) {
    case 0x3F:
      return 0;
    case 0x06:
      return 1;
    case 0x5B:
      return 2;
    case 0x4F:
      return 3;
    case 0x66:
      return 4;
    case 0x6D:
      return 5;
    case 0x7D:
      return 6;
    case 0x07:
    case 0x27:
      return 7;
    case 0x7F:
      return 8;
    case 0x6F:
      return 9;
    default:
      return -1;
  }
}

climate::ClimateTraits CecotecClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION | climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY |
                           climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY);
  if (this->sensor_ != nullptr)
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_DRY,
                              climate::CLIMATE_MODE_FAN_ONLY});
  traits.set_supported_fan_modes({climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_HIGH});
  traits.set_supported_presets({climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP});
  traits.set_visual_min_temperature(MIN_TEMP);
  traits.set_visual_max_temperature(MAX_TEMP);
  traits.set_visual_target_temperature_step(1.0f);
  traits.set_visual_current_temperature_step(0.1f);
  traits.set_visual_min_humidity(30.0f);
  traits.set_visual_max_humidity(80.0f);
  return traits;
}

void CecotecClimate::setup() {
  if (this->sensor_ != nullptr) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  }
  this->humidity_sensor_->add_on_state_callback([this](float state) {
    this->current_humidity = state;
    this->publish_state();
  });
  this->current_humidity = this->humidity_sensor_->state;

  this->target_temperature = 24;
  this->target_humidity = 55;
  this->fan_mode = climate::CLIMATE_FAN_LOW;
  this->preset = climate::CLIMATE_PRESET_NONE;
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    float temp = restore->target_temperature;
    float hum = restore->target_humidity;
    if (!std::isnan(temp))
      this->target_temperature = std::clamp(temp, (float) MIN_TEMP, (float) MAX_TEMP);
    if (!std::isnan(hum))
      this->target_humidity = std::clamp(hum, 30.0f, 80.0f);
    // The unit state itself comes from CN1; only remember whether humidity control was on.
    this->restore_dry_ = restore->mode == climate::CLIMATE_MODE_DRY;
  }
}

void CecotecClimate::dump_config() {
  LOG_CLIMATE("", "Cecotec AC", this);
  ESP_LOGCONFIG(TAG, "  Humidity hysteresis: %.1f%%", this->hysteresis_);
}

void CecotecClimate::on_main_frame(const uint8_t *d, size_t len) {
  if (len != 9 || d[0] != 0x90)
    return;
  uint32_t now = millis();
  this->last_frame_ms_ = now;

  State s = this->actual_;
  switch (d[5] & 0x0F) {
    case 0x0:
      s.mode = climate::CLIMATE_MODE_OFF;
      break;
    case 0x1:
      s.mode = climate::CLIMATE_MODE_COOL;
      break;
    case 0x2:
      s.mode = climate::CLIMATE_MODE_DRY;
      break;
    case 0x4:
      s.mode = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    default:
      return;  // unknown mode, flagged by the unknown-value watch
  }
  s.sleep = d[5] & 0x10;
  s.timer = d[5] & 0x80;
  if (d[6] == 0x21) {
    s.fan = climate::CLIMATE_FAN_LOW;
  } else if (d[6] == 0x22) {
    s.fan = climate::CLIMATE_FAN_HIGH;
  }  // 0x20 (off) keeps the last speed

  // Digits: setpoint in cool mode, timer hours while the timer display flashes ("01" / blank).
  int hi = seg_digit(d[1]);
  int lo = seg_digit(d[2]);
  int value = hi >= 0 && lo >= 0 ? hi * 10 + lo : -1;
  if (d[1] == 0x00 && d[2] == 0x00) {
    this->blank_ms_ = now;
    this->edit_ms_ = now;
    this->digits_before_blank_ = this->last_digits_;
  } else {
    if (this->editing_(now)) {
      // Right after an edit key, or the same digits on both sides of a blank: that's the hours.
      // (When the edit ends the setpoint appears after a blank, but differs from the hours.)
      bool after_key = now - this->edit_key_ms_ < EDIT_KEY_MS;
      bool flashing = now - this->blank_ms_ < EDIT_FRESH_MS && value == this->digits_before_blank_;
      if (s.timer && value >= 1 && value <= MAX_TIMER && (after_key || flashing))
        s.timer_hours = value;
    } else if (s.mode == climate::CLIMATE_MODE_COOL && value >= MIN_TEMP && value <= MAX_TEMP) {
      s.setpoint = value;
    }
    this->last_digits_ = value;
  }
  if (!s.timer)
    s.timer_hours = 0;
  s.valid = true;
  if (s == this->actual_)
    return;

  ESP_LOGD(TAG, "State: %s, setpoint %d, fan %s, sleep %s, timer %s (%dh)",
           LOG_STR_ARG(climate::climate_mode_to_string(s.mode)), s.setpoint,
           LOG_STR_ARG(climate::climate_fan_mode_to_string(s.fan)), YESNO(s.sleep), YESNO(s.timer), s.timer_hours);
  bool first = !this->actual_.valid;
  this->actual_ = s;

  if (first && this->restore_dry_) {
    this->restore_dry_ = false;
    if (s.mode == climate::CLIMATE_MODE_DRY || s.mode == climate::CLIMATE_MODE_OFF) {
      this->humid_ctl_ = true;
      this->drying_ = s.mode == climate::CLIMATE_MODE_DRY;
      ESP_LOGI(TAG, "Resuming humidity control after reboot");
    }
  }
  if (this->waiting_) {
    this->waiting_ = false;
    this->fails_ = 0;
    this->next_key_ms_ = now + STEP_GAP_MS;
  }
  this->publish_if_changed_();
}

void CecotecClimate::on_disp_key(uint8_t key) {
  // Track the timer display for every key, ours included (our IR keys echo as remote codes).
  uint32_t now = millis();
  bool timer_key = key == 0x04 || key == 0x0F;
  bool step_key = key == 0x02 || key == 0x08 || key == 0x0B || key == 0x0E;
  if (timer_key || (step_key && this->editing_(now))) {
    this->edit_ms_ = now;
    this->edit_key_ms_ = now;
  }

  if (key == 0x00 || this->own_change() || !this->has_targets_())
    return;
  // Someone used the panel or remote while an HA command was running: they win.
  ESP_LOGI(TAG, "Manual key %02X, cancelling HA command", key);
  this->clear_targets_();
  this->publish_if_changed_();
}

void CecotecClimate::loop() {
  uint32_t now = millis();
  if (this->has_targets_() && now - this->target_ms_ > TARGET_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Target not reached within %us, giving up", (unsigned) (TARGET_TIMEOUT_MS / 1000));
    this->clear_targets_();
    this->publish_if_changed_();
    this->failed_callback_.call();
  }
  if (!this->actual_.valid || now - this->last_frame_ms_ > STALE_MS)
    return;
  this->run_humidity_control_();
  if (!this->has_targets_())
    return;

  if (this->waiting_) {
    if (now - this->sent_ms_ < STEP_TIMEOUT_MS)
      return;
    this->waiting_ = false;
    if (++this->fails_ >= MAX_FAILS) {
      ESP_LOGW(TAG, "No reaction after %u tries, dropping that target", MAX_FAILS);
      this->drop_field_(this->wait_field_);
      this->fails_ = 0;
      this->publish_if_changed_();
      this->failed_callback_.call();
    }
  }
  if ((int32_t) (now - this->next_key_ms_) < 0)
    return;

  Key key;
  Field field;
  if (!this->next_step_(key, field)) {
    this->publish_if_changed_();
    return;
  }
  this->wait_field_ = field;
  this->send_key_(key);
}

void CecotecClimate::control(const climate::ClimateCall &call) {
  if (call.get_target_humidity().has_value())
    this->target_humidity = *call.get_target_humidity();
  if (call.get_mode().has_value()) {
    auto mode = *call.get_mode();
    if (mode == climate::CLIMATE_MODE_DRY) {
      this->start_humidity_control_();
    } else {
      this->humid_ctl_ = false;
      this->t_mode_ = mode;
    }
  }
  if (call.get_target_temperature().has_value())
    this->t_temp_ = std::clamp((int) lroundf(*call.get_target_temperature()), MIN_TEMP, MAX_TEMP);
  if (call.get_fan_mode().has_value()) {
    auto fan = *call.get_fan_mode();
    if (fan == climate::CLIMATE_FAN_LOW || fan == climate::CLIMATE_FAN_HIGH)
      this->t_fan_ = fan;
  }
  if (call.get_preset().has_value())
    this->t_sleep_ = *call.get_preset() == climate::CLIMATE_PRESET_SLEEP;
  this->target_ms_ = millis();
  this->fails_ = 0;
  this->publish_if_changed_(true);
}

void CecotecClimate::start_humidity_control_() {
  this->humid_ctl_ = true;
  float h = this->current_humidity;
  this->drying_ = std::isnan(h) || h > this->target_humidity;
  this->t_mode_ = this->drying_ ? climate::CLIMATE_MODE_DRY : climate::CLIMATE_MODE_OFF;
  ESP_LOGI(TAG, "Humidity control on: %.1f%%, target %.0f%% -> %s", h, this->target_humidity,
           this->drying_ ? "drying" : "idle");
}

void CecotecClimate::run_humidity_control_() {
  if (!this->humid_ctl_)
    return;
  auto expected = this->drying_ ? climate::CLIMATE_MODE_DRY : climate::CLIMATE_MODE_OFF;
  if (!this->has_targets_() && !this->own_change() && this->actual_.mode != expected) {
    // Panel, remote, AC timer or a failed command: follow the unit instead of fighting it.
    ESP_LOGI(TAG, "Unit is %s outside humidity control, leaving dry mode",
             LOG_STR_ARG(climate::climate_mode_to_string(this->actual_.mode)));
    this->humid_ctl_ = false;
    this->publish_if_changed_();
    return;
  }

  float h = this->current_humidity;
  if (std::isnan(h))
    return;
  bool want = this->drying_;
  if (this->drying_ && h < this->target_humidity - this->hysteresis_) {
    want = false;
  } else if (!this->drying_ && h > this->target_humidity + this->hysteresis_) {
    want = true;
  }
  if (want == this->drying_)
    return;
  ESP_LOGI(TAG, "Humidity %.1f%% (target %.0f%% ±%.1f): %s", h, this->target_humidity, this->hysteresis_,
           want ? "start drying" : "stop drying");
  this->drying_ = want;
  this->t_mode_ = want ? climate::CLIMATE_MODE_DRY : climate::CLIMATE_MODE_OFF;
  this->target_ms_ = millis();
  this->fails_ = 0;
  this->publish_if_changed_();
}

// Picks the next key towards the targets. Clears targets that are reached.
bool CecotecClimate::next_step_(Key &key, Field &field) {
  const State &a = this->actual_;
  uint32_t now = millis();
  // While the timer display is in edit, up/down set the hours: only timer steps, climate waits.
  if (this->editing_(now)) {
    field = FIELD_TIMER;
    return this->timer_step_(key, this->edit_fresh_(now));
  }
  if (this->t_mode_.has_value()) {
    field = FIELD_MODE;
    bool want_on = *this->t_mode_ != climate::CLIMATE_MODE_OFF;
    bool is_on = a.mode != climate::CLIMATE_MODE_OFF;
    if (want_on != is_on) {
      key = KEY_POWER;  // power on resumes the last mode
      return true;
    }
    if (*this->t_mode_ != a.mode) {
      key = KEY_MODE;  // cycles cool -> dry -> fan
      return true;
    }
    this->t_mode_.reset();
  }
  if (a.mode == climate::CLIMATE_MODE_OFF) {
    this->t_temp_.reset();  // only the timer can change while off
    this->t_fan_.reset();
    this->t_sleep_.reset();
    field = FIELD_TIMER;
    return this->timer_step_(key, false);
  }
  if (this->t_temp_.has_value()) {
    field = FIELD_TEMP;
    if (a.mode != climate::CLIMATE_MODE_COOL) {
      ESP_LOGD(TAG, "Setpoint can only be changed in cool mode");
      this->t_temp_.reset();
    } else if (a.setpoint == 0) {
      return false;  // not shown yet, wait (target timeout covers a stuck display)
    } else if (*this->t_temp_ != a.setpoint) {
      key = *this->t_temp_ > a.setpoint ? KEY_UP : KEY_DOWN;
      return true;
    } else {
      this->t_temp_.reset();
    }
  }
  if (this->t_fan_.has_value()) {
    field = FIELD_FAN;
    if (*this->t_fan_ != a.fan) {
      key = KEY_SPEED;  // toggles low <-> high
      return true;
    }
    this->t_fan_.reset();
  }
  if (this->t_sleep_.has_value()) {
    field = FIELD_SLEEP;
    if (*this->t_sleep_ != a.sleep) {
      key = KEY_SLEEP;
      return true;
    }
    this->t_sleep_.reset();
  }
  field = FIELD_TIMER;
  return this->timer_step_(key, false);
}

// fresh = the timer display is surely flashing right now (up/down set hours, timer key cancels).
bool CecotecClimate::timer_step_(Key &key, bool fresh) {
  State &a = this->actual_;
  if (this->t_timer_delta_.has_value()) {
    int delta = *this->t_timer_delta_;
    if (!a.timer) {
      this->t_timer_delta_.reset();
      if (delta > 0)
        this->t_timer_ = std::min(delta, MAX_TIMER);
    } else if (!fresh) {
      a.timer_hours = 0;  // enter edit to read the hours
      key = KEY_TIMER;
      return true;
    } else if (a.timer_hours == 0) {
      return false;  // wait for the hours to show
    } else {
      this->t_timer_delta_.reset();
      this->t_timer_ = std::clamp(a.timer_hours + delta, 0, MAX_TIMER);
    }
  }
  if (!this->t_timer_.has_value())
    return false;

  int hours = *this->t_timer_;
  if (hours == 0) {
    if (!a.timer) {
      this->t_timer_.reset();
      return false;
    }
    key = KEY_TIMER;  // flashing: cancels; otherwise enters edit, and the next press cancels
    return true;
  }
  if (!a.timer) {
    key = KEY_TIMER;  // timer on, display flashes "01"
    return true;
  }
  if (!fresh) {
    a.timer_hours = 0;  // enter edit; the flashing digits show the current hours
    key = KEY_TIMER;
    return true;
  }
  if (a.timer_hours == 0)
    return false;
  if (a.timer_hours != hours) {
    key = a.timer_hours < hours ? KEY_UP : KEY_DOWN;
    return true;
  }
  this->t_timer_.reset();  // reached; the display leaves edit by itself after ~4 s
  return false;
}

float CecotecClimate::timer_hours() const {
  if (this->t_timer_.has_value())
    return *this->t_timer_;
  if (!this->actual_.timer)
    return 0;
  return this->actual_.timer_hours ? this->actual_.timer_hours : NAN;
}

void CecotecClimate::set_timer(int hours) {
  this->t_timer_ = std::clamp(hours, 0, MAX_TIMER);
  this->t_timer_delta_.reset();
  this->target_ms_ = millis();
  this->fails_ = 0;
}

void CecotecClimate::adjust_timer(int delta) {
  if (this->t_timer_.has_value()) {
    this->t_timer_ = std::clamp(*this->t_timer_ + delta, 0, MAX_TIMER);  // pressed again while running
  } else {
    this->t_timer_delta_ = this->t_timer_delta_.value_or(0) + delta;
  }
  this->target_ms_ = millis();
  this->fails_ = 0;
}

void CecotecClimate::send_key_(Key key) {
  ESP_LOGD(TAG, "Pressing %s", KEY_NAMES[key]);
  remote_base::NECData data{NEC_ADDRESS, NEC_COMMANDS[key], 1};
  this->transmit_<remote_base::NECProtocol>(data);
  this->sent_ms_ = millis();
  this->waiting_ = true;
}

void CecotecClimate::drop_field_(Field field) {
  switch (field) {
    case FIELD_MODE:
      this->t_mode_.reset();
      break;
    case FIELD_TEMP:
      this->t_temp_.reset();
      break;
    case FIELD_FAN:
      this->t_fan_.reset();
      break;
    case FIELD_SLEEP:
      this->t_sleep_.reset();
      break;
    case FIELD_TIMER:
      this->t_timer_.reset();
      this->t_timer_delta_.reset();
      break;
  }
}

void CecotecClimate::clear_targets_() {
  this->t_mode_.reset();
  this->t_temp_.reset();
  this->t_fan_.reset();
  this->t_sleep_.reset();
  this->t_timer_.reset();
  this->t_timer_delta_.reset();
  this->waiting_ = false;
}

bool CecotecClimate::has_targets_() const {
  return this->t_mode_.has_value() || this->t_temp_.has_value() || this->t_fan_.has_value() ||
         this->t_sleep_.has_value() || this->t_timer_.has_value() || this->t_timer_delta_.has_value();
}

// HA shows the target while a command runs, otherwise what the unit reports.
void CecotecClimate::publish_if_changed_(bool force) {
  if (!this->actual_.valid && !force)
    return;
  const State &a = this->actual_;
  bool changed = force;

  climate::ClimateMode mode = a.mode;
  if (this->humid_ctl_) {
    mode = climate::CLIMATE_MODE_DRY;
  } else if (this->t_mode_.has_value()) {
    mode = *this->t_mode_;
  }
  if (mode != this->mode) {
    this->mode = mode;
    changed = true;
  }

  climate::ClimateAction action = climate::CLIMATE_ACTION_OFF;
  switch (a.mode) {
    case climate::CLIMATE_MODE_COOL:
      action = climate::CLIMATE_ACTION_COOLING;
      break;
    case climate::CLIMATE_MODE_DRY:
      action = climate::CLIMATE_ACTION_DRYING;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      action = climate::CLIMATE_ACTION_FAN;
      break;
    default:
      action = this->humid_ctl_ ? climate::CLIMATE_ACTION_IDLE : climate::CLIMATE_ACTION_OFF;
      break;
  }
  if (action != this->action) {
    this->action = action;
    changed = true;
  }

  float temp = this->target_temperature;
  if (this->t_temp_.has_value()) {
    temp = *this->t_temp_;
  } else if (a.setpoint != 0) {
    temp = a.setpoint;
  }
  if (temp != this->target_temperature) {
    this->target_temperature = temp;
    changed = true;
  }

  auto fan = this->t_fan_.has_value() ? *this->t_fan_ : a.fan;
  if (!this->fan_mode.has_value() || *this->fan_mode != fan) {
    this->fan_mode = fan;
    changed = true;
  }

  bool sleep = this->t_sleep_.has_value() ? *this->t_sleep_ : a.sleep;
  auto preset = sleep ? climate::CLIMATE_PRESET_SLEEP : climate::CLIMATE_PRESET_NONE;
  if (!this->preset.has_value() || *this->preset != preset) {
    this->preset = preset;
    changed = true;
  }

  if (changed)
    this->publish_state();
}

}  // namespace esphome::cecotec_ac
