#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::cecotec_ac {

// The AC only understands key presses (panel and remote alike), so control is stateful:
// read the state from CN1 main>disp frames, press one key via IR, wait for the next
// frame to confirm, repeat until the target is reached.
//
// HA "Dry" = humidity control: the unit runs in dry until humidity < target - hysteresis,
// then turns off, and turns back on in dry when humidity > target + hysteresis.
//
// Timer: the timer key turns the timer on (display flashes "01") or, while flashing,
// cancels it. Up/down while flashing set the hours. Only the flag is in the frames;
// the hours are only visible while flashing.
class CecotecClimate : public climate::Climate, public Component, public remote_base::RemoteTransmittable {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_humidity_hysteresis(float hysteresis) { this->hysteresis_ = hysteresis; }

  // Valid (checksummed) main>disp frame, repeats included. Called from the YAML on_raw lambda.
  void on_main_frame(const uint8_t *d, size_t len);
  // Key byte of a display>main frame (00 = keepalive).
  void on_disp_key(uint8_t key);
  // True shortly after the ESP sent a key; used to keep HA commands silent.
  bool own_change() const { return millis() - this->sent_ms_ < OWN_WINDOW_MS; }
  // Main board frames are arriving.
  bool link_ok() const { return this->actual_.valid && millis() - this->last_frame_ms_ < STALE_MS; }

  bool timer_on() const { return this->actual_.timer; }
  // Target while a timer command runs, else last seen hours; 0 = off, NAN = on but hours unknown.
  float timer_hours() const;
  // 0 cancels the timer.
  void set_timer(int hours);
  void adjust_timer(int delta);

  // Called when an HA command could not be completed.
  template<typename F> void add_on_failed_callback(F &&callback) { this->failed_callback_.add(std::forward<F>(callback)); }

 protected:
  enum Key : uint8_t { KEY_POWER, KEY_MODE, KEY_UP, KEY_DOWN, KEY_SPEED, KEY_SLEEP, KEY_TIMER };
  enum Field : uint8_t { FIELD_MODE, FIELD_TEMP, FIELD_FAN, FIELD_SLEEP, FIELD_TIMER };

  struct State {
    bool valid{false};
    climate::ClimateMode mode{climate::CLIMATE_MODE_OFF};
    int setpoint{0};  // 0 = not seen yet (only visible in cool mode)
    climate::ClimateFanMode fan{climate::CLIMATE_FAN_LOW};
    bool sleep{false};
    bool timer{false};
    int timer_hours{0};  // 0 = unknown / off
    bool operator==(const State &o) const {
      return valid == o.valid && mode == o.mode && setpoint == o.setpoint && fan == o.fan && sleep == o.sleep &&
             timer == o.timer && timer_hours == o.timer_hours;
    }
    bool operator!=(const State &o) const { return !(*this == o); }
  };

  static const uint32_t OWN_WINDOW_MS = 1500;     // own key -> echo on disp>main and beep frame
  static const uint32_t STEP_TIMEOUT_MS = 1500;   // no state change after a key -> failed try
  static const uint32_t STEP_GAP_MS = 300;        // pause after a confirmed step
  static const uint32_t STALE_MS = 25000;  // idle main board answers the ~10 s keepalive only; no frames -> link lost
  static const uint32_t TARGET_TIMEOUT_MS = 30000;
  static const uint32_t EDIT_HOLD_MS = 4500;   // timer display may still be in edit (no climate keys, no setpoint)
  static const uint32_t EDIT_FRESH_MS = 1500;  // timer display is surely flashing (safe to press up/down)
  static const uint32_t EDIT_KEY_MS = 600;     // frame right after a timer/up/down key shows the hours
  static const uint8_t MAX_FAILS = 3;
  static const int MIN_TEMP = 16;
  static const int MAX_TEMP = 31;
  static const int MAX_TIMER = 24;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  bool editing_(uint32_t now) const { return now - this->edit_ms_ < EDIT_HOLD_MS; }
  bool edit_fresh_(uint32_t now) const { return now - this->edit_ms_ < EDIT_FRESH_MS; }
  void run_humidity_control_();
  void start_humidity_control_();
  bool next_step_(Key &key, Field &field);
  bool timer_step_(Key &key, bool fresh);
  void send_key_(Key key);
  void drop_field_(Field field);
  void clear_targets_();
  bool has_targets_() const;
  void publish_if_changed_(bool force = false);

  sensor::Sensor *sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  State actual_;
  uint32_t last_frame_ms_{0};
  // Timer display tracking
  uint32_t edit_ms_{0};      // last sign of timer edit (blank frame or edit key)
  uint32_t edit_key_ms_{0};  // last timer key, or up/down during edit
  uint32_t blank_ms_{0};
  int last_digits_{-1};
  int digits_before_blank_{-1};

  // Physical targets for the key sequencer.
  optional<climate::ClimateMode> t_mode_;
  optional<int> t_temp_;
  optional<climate::ClimateFanMode> t_fan_;
  optional<bool> t_sleep_;
  optional<int> t_timer_;        // hours, 0 = off
  optional<int> t_timer_delta_;  // +/- hours, resolved once the current hours are read
  uint32_t target_ms_{0};

  bool waiting_{false};
  Field wait_field_{FIELD_MODE};
  uint32_t sent_ms_{0};
  uint32_t next_key_ms_{0};
  uint8_t fails_{0};

  // Humidity control (HA mode "Dry").
  bool humid_ctl_{false};
  bool drying_{false};       // controller wants the unit on in dry
  bool restore_dry_{false};  // HA mode was Dry before reboot; resume once state is known
  float hysteresis_{3.0f};

  CallbackManager<void()> failed_callback_;
};

}  // namespace esphome::cecotec_ac
