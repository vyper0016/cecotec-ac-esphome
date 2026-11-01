#pragma once
#include <string>
#include <vector>
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

// Last valid frame seen on one CN1 line, used to skip repeats in the log.
struct Cn1Last {
  uint8_t data[16];
  size_t len;
  bool fresh;  // true if the last call saw a frame different from the one before
};

// 7-segment code (bit0 = a ... bit6 = g, bit7 = dot) to text, '?' if unknown.
static std::string cn1_seg_text(uint8_t seg) {
  static const struct {
    uint8_t code;
    char ch;
  } table[] = {
      {0x00, ' '}, {0x3F, '0'}, {0x06, '1'}, {0x5B, '2'}, {0x4F, '3'}, {0x66, '4'}, {0x6D, '5'},
      {0x7D, '6'}, {0x07, '7'}, {0x7F, '8'}, {0x6F, '9'}, {0x77, 'A'}, {0x7C, 'b'}, {0x39, 'C'},
      {0x58, 'c'}, {0x5E, 'd'}, {0x79, 'E'}, {0x71, 'F'}, {0x76, 'H'}, {0x74, 'h'}, {0x38, 'L'},
      {0x54, 'n'}, {0x5C, 'o'}, {0x73, 'P'}, {0x50, 'r'}, {0x3E, 'U'}, {0x1C, 'u'}, {0x40, '-'},
  };
  std::string s = "?";
  for (const auto &e : table) {
    if (e.code == (seg & 0x7F)) {
      s[0] = e.ch;
      break;
    }
  }
  if (seg & 0x80)
    s += '.';
  return s;
}

// Decode one CN1 frame and log it as hex bytes under the given tag.
// Protocol: header ~4480us low / ~2240us high, then per bit a ~250us low mark
// followed by a space (~250us = 0, ~750us = 1), MSB first.
// Last byte = (sum of all other bytes + 0x5A) & 0xFF.
// Timings come from remote_receiver with inverted pin: positive = line low.
// Returns the frame length if valid (frame is then in last.data), else 0.
static size_t cn1_log_frame(const char *tag, const std::vector<int32_t> &x, Cn1Last &last) {
  last.fresh = false;
  if (x.size() < 4 || x[0] < 4000 || x[0] > 5000) {
    // Unknown shape: show the first timings so nothing is lost
    char buf[96] = {0};
    size_t pos = 0;
    for (size_t i = 0; i < x.size() && i < 10 && pos < sizeof(buf) - 12; i++)
      pos += snprintf(buf + pos, sizeof(buf) - pos, "%d ", (int) x[i]);
    ESP_LOGD(tag, "no header (%u timings): %s", (unsigned) x.size(), buf);
    return 0;
  }

  uint8_t bytes[16] = {0};
  size_t nbits = 0;
  // x[2] is the first bit mark, x[3] its space. A bit counts only if the mark
  // after its space is valid too, so the trailing glitch (e.g. -927, 71) is dropped.
  for (size_t i = 3; i + 1 < x.size() && nbits < sizeof(bytes) * 8; i += 2) {
    int32_t space = -x[i];
    int32_t next_mark = x[i + 1];
    if (next_mark < 100 || next_mark > 450)
      break;
    if (space > 150 && space < 500) {
      // 0 bit
    } else if (space >= 500 && space < 880) {  // real 1 ~750us; trailing glitch ~927us
      bytes[nbits / 8] |= 0x80 >> (nbits % 8);
    } else {
      break;
    }
    nbits++;
  }

  // Whole bytes only: drops the trailing stop/glitch bit
  size_t nbytes = nbits / 8;
  bool ok = false;
  if (nbytes >= 2) {
    uint8_t sum = 0x5A;
    for (size_t i = 0; i + 1 < nbytes; i++)
      sum += bytes[i];
    ok = sum == bytes[nbytes - 1];
  }

  char hex[sizeof(bytes) * 3 + 1] = {0};
  for (size_t i = 0; i < nbytes; i++)
    snprintf(hex + i * 3, 4, "%02X ", bytes[i]);

  if (!ok) {
    ESP_LOGD(tag, "bad frame (%u bits): %s", (unsigned) nbits, hex);
    return 0;
  }
  if (nbytes == last.len && memcmp(bytes, last.data, nbytes) == 0)
    return nbytes;
  memcpy(last.data, bytes, nbytes);
  last.len = nbytes;
  last.fresh = true;
  ESP_LOGI(tag, "%s", hex);
  return nbytes;
}

// ---------------------------------------------------------------------------
// Unknown-value watch: flags field values never seen while reverse engineering
// (see FINDINGS.md) and keeps them in flash, e.g. to catch the water-full frame.

enum : uint8_t { CN1_DIR_MAIN = 0, CN1_DIR_DISP = 1, CN1_DIR_IR = 2 };
enum : uint8_t { CN1_F_LEN, CN1_F_HEAD, CN1_F_DIGIT, CN1_F_COPY, CN1_F_MODE, CN1_F_FAN, CN1_F_FLAG, CN1_F_KEY, CN1_F_NEC };
static const char *const CN1_DIR_NAME[] = {"main>disp", "disp>main", "IR"};
static const char *const CN1_FIELD_NAME[] = {"len", "header", "digit", "digit copy", "mode", "fan", "flag", "key", "nec"};

static const size_t CN1_UNKNOWN_MAX = 8;

struct Cn1Unknown {
  uint8_t dir;       // CN1_DIR_*
  uint8_t field;     // CN1_F_*
  uint8_t value;     // the new value
  uint8_t count;     // times seen (on frame changes), saturates at 255
  uint8_t frame[9];  // first frame that showed it
  uint8_t len;
};

// Saved to flash as raw bytes, so keep it plain data.
struct Cn1UnknownStore {
  uint8_t n;
  Cn1Unknown e[CN1_UNKNOWN_MAX];
};

static Cn1UnknownStore cn1_unknown_store{};
static esphome::ESPPreferenceObject cn1_unknown_pref;

// Call once at boot (needs esp8266 restore_from_flash: true to survive power loss).
static void cn1_unknown_load() {
  cn1_unknown_pref = esphome::global_preferences->make_preference<Cn1UnknownStore>(0xCEC0A001UL);
  if (!cn1_unknown_pref.load(&cn1_unknown_store) || cn1_unknown_store.n > CN1_UNKNOWN_MAX)
    cn1_unknown_store = Cn1UnknownStore{};
  ESP_LOGI("unknown", "%u stored unknown values", cn1_unknown_store.n);
}

static void cn1_unknown_clear() {
  cn1_unknown_store = Cn1UnknownStore{};
  cn1_unknown_pref.save(&cn1_unknown_store);
}

static std::string cn1_unknown_text(const Cn1Unknown &e) {
  char buf[96];
  int pos = snprintf(buf, sizeof(buf), "%s %s=%02X x%u:", CN1_DIR_NAME[e.dir % 3], CN1_FIELD_NAME[e.field % 9],
                     e.value, e.count);
  for (uint8_t i = 0; i < e.len && i < sizeof(e.frame) && pos < (int) sizeof(buf) - 4; i++)
    pos += snprintf(buf + pos, sizeof(buf) - pos, " %02X", e.frame[i]);
  return buf;
}

// Short list of all stored values for a text sensor (max 255 chars in HA).
static std::string cn1_unknown_summary(const Cn1UnknownStore &st) {
  if (st.n == 0 || st.n > CN1_UNKNOWN_MAX)
    return "none";
  std::string s;
  char buf[40];
  for (uint8_t i = 0; i < st.n; i++) {
    const auto &e = st.e[i];
    snprintf(buf, sizeof(buf), "%s%s %s=%02X x%u", i ? ", " : "", e.dir == CN1_DIR_MAIN ? "M" : e.dir == CN1_DIR_DISP ? "D" : "IR",
             CN1_FIELD_NAME[e.field % 9], e.value, e.count);
    if (s.size() + strlen(buf) > 250)
      break;
    s += buf;
  }
  return s;
}

// Store one flagged value. Returns its description if it is new, else "".
static std::string cn1_unknown_add(Cn1UnknownStore &st, uint8_t dir, uint8_t field, uint8_t value, const uint8_t *d,
                                   size_t len) {
  for (uint8_t i = 0; i < st.n; i++) {
    auto &e = st.e[i];
    if (e.dir == dir && e.field == field && e.value == value) {
      if (e.count < 255) {
        e.count++;
        cn1_unknown_pref.save(&st);
      }
      return "";
    }
  }
  Cn1Unknown e{};
  e.dir = dir;
  e.field = field;
  e.value = value;
  e.count = 1;
  e.len = len < sizeof(e.frame) ? len : sizeof(e.frame);
  memcpy(e.frame, d, e.len);
  std::string text = cn1_unknown_text(e);
  if (st.n >= CN1_UNKNOWN_MAX) {
    ESP_LOGW("unknown", "store full, not saved: %s", text.c_str());
    return "";
  }
  st.e[st.n++] = e;
  cn1_unknown_pref.save(&st);
  ESP_LOGW("unknown", "NEW: %s", text.c_str());
  return text;
}

static bool cn1_seg_known(uint8_t seg) {
  // blank, 0-9, and the letters seen so far: F A d h
  static const uint8_t known[] = {0x00, 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
                                  0x7F, 0x6F, 0x71, 0x77, 0x5E, 0x74};
  for (uint8_t k : known)
    if (k == seg)
      return true;
  return false;
}

// Check a valid CN1 frame. Returns the description of the first new value, or "".
static std::string cn1_check_unknown(Cn1UnknownStore &st, uint8_t dir, const uint8_t *d, size_t len) {
  std::string first;
  auto flag = [&](uint8_t field, uint8_t value) {
    std::string t = cn1_unknown_add(st, dir, field, value, d, len);
    if (first.empty())
      first = t;
  };

  if (dir == CN1_DIR_MAIN) {
    if (len != 9) {
      flag(CN1_F_LEN, len);
      return first;
    }
    if (d[0] != 0x90)
      flag(CN1_F_HEAD, d[0]);
    for (int i = 1; i <= 4; i++)
      if (!cn1_seg_known(d[i]))
        flag(CN1_F_DIGIT, d[i]);
    if (d[1] != d[3] || d[2] != d[4])
      flag(CN1_F_COPY, d[3]);
    // byte 5: low nibble = mode (0 off, 1 cool, 2 dry, 4 fan), 0x10 sleep, 0x80 timer
    uint8_t mode = d[5] & 0x0F;
    if ((d[5] & 0x60) || !(mode == 0 || mode == 1 || mode == 2 || mode == 4))
      flag(CN1_F_MODE, d[5]);
    if (d[6] < 0x20 || d[6] > 0x22)
      flag(CN1_F_FAN, d[6]);
    if (d[7] != 0x00 && d[7] != 0x11)
      flag(CN1_F_FLAG, d[7]);
  } else if (dir == CN1_DIR_DISP) {
    if (len != 3) {
      flag(CN1_F_LEN, len);
      return first;
    }
    if (d[0] != 0x30)
      flag(CN1_F_HEAD, d[0]);
    // panel keys 00-05, 07, 08; remote keys forwarded as 0A-10
    uint8_t k = d[1];
    if (!(k <= 0x05 || k == 0x07 || k == 0x08 || (k >= 0x0A && k <= 0x10)))
      flag(CN1_F_KEY, k);
  }
  return first;
}

// Check an NEC command from the remote (address 0x7F80). Returns description if new, else "".
static std::string cn1_check_nec(Cn1UnknownStore &st, uint16_t command) {
  static const uint8_t known[] = {0x9C, 0x9D, 0x94, 0x95, 0x96, 0x9B, 0x9F};
  uint8_t low = command & 0xFF;
  for (uint8_t k : known)
    if (k == low)
      return "";
  uint8_t d[2] = {(uint8_t) (command >> 8), low};
  return cn1_unknown_add(st, CN1_DIR_IR, CN1_F_NEC, low, d, 2);
}
