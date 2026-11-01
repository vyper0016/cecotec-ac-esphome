# Cecotec AC – reverse engineering findings

Monoblock AC with cool, dry and fan modes only (no heat/auto). Setpoint 16–31 °C, two fan speeds.

## Hardware / wiring

| Signal | Board pin | ESP pin | Interface |
| --- | --- | --- | --- |
| CN1 TX (display → main) | CN1 | GPIO4 (D2) | 330Ω/680Ω divider (not GPIO3, see below) |
| CN1 RX (main → display) | CN1 | GPIO14 (D5) | 330Ω/680Ω divider |
| CN3 IR (IR receiver out) | CN3 | GPIO12 (D6) | 1N4148, cathode to IR line, internal pull-up |
| IR inject | CN3 | GPIO13 (D7) | 2N2222, 1k base, 10k base-GND, collector to IR line |
| Room temperature / humidity | "DHT11" module | GPIO5 (D1) | 3V3. Sold as DHT11, sends DHT22/AM2302 format (`model: DHT22`, 0.1 steps) |
| Beeper | BUZ1 (moved off the display board) | GPIO15 (D8) | 2N2222 low-side, piezo to 5V, 1k across the piezo |

Full wiring and diagrams: [wiring.md](wiring.md).

## CN1 bus protocol

- **Not UART.** At 9600 baud it only reads repeating `E6` bytes (the pulse train misread as UART).
- Pulse-distance protocol, like NEC at about half its timing:
  - header: 4480 µs low, then 2240 µs high
  - each bit: 250 µs low mark, then a space of 250 µs (0) or 750 µs (1)
  - MSB first
  - frames end with a trailing stop bit or glitch; the decoder keeps whole bytes only and checks the checksum
  - checksum (last byte) = `(sum of all other bytes + 0x5A) & 0xFF`, both directions
- Decoder: [cecotec/cn1_decode.h](cecotec/cn1_decode.h). It logs a frame only when it differs from the previous one on that line; bad frames only at debug level.
- Rate: idle, the main board only answers the display's keepalive, so **one frame about every 10 s**. After a key or state change, frames come within 50–150 ms. Hence the 25 s link-lost timeout.
- Occasional broken frames (18/38 bits, "no header"), probably relay/motor noise. The checksum filters them.

## main → display frame (GPIO14)

`90 D1 D2 D1 D2 MM PP BB CS`

| Byte | Meaning | Values seen |
| --- | --- | --- |
| 0 | header | `90` |
| 1, 2 | 7-seg digits: left, right | see below |
| 3, 4 | copy of bytes 1, 2 | always identical so far |
| 5 | mode + flags | low nibble: `0` off, `1` cool, `2` dry, `4` fan. `0x10` = sleep, `0x80` = timer |
| 6 | fan speed | `0x20` + speed: `0` off, `1` low, `2` high |
| 7 | event flag | `11` in the first frame after any state change, else `00`. Probably "beep" |
| 8 | checksum | |

7-seg encoding: bit0 = a … bit6 = g.

| Code | Char |
| --- | --- |
| `00` | blank |
| `3F` | 0 |
| `06` | 1 |
| `5B` | 2 |
| `4F` | 3 |
| `66` | 4 |
| `6D` | 5 |
| `71` | F |
| `77` | A |
| `5E` | d |
| `74` | h |

Display content per state:

- **Off:** room temperature (flickers between neighbouring values)
- **Fan:** "FA"
- **Cool:** setpoint
- **Dry:** "dh"

So room temperature is only visible while off; the DHT sensor covers the rest.

### Captured sequence: display buttons

| Action | Frame |
| --- | --- |
| off, idle | `90 5B 5B 5B 5B 00 20 00 76` |
| power → fan | `90 71 77 71 77 04 21 11 F0`, then `… 04 21 00 DF` |
| mode → cool | `90 5B 5B 5B 5B 01 21 11 89`, then `… 01 21 00 78` |
| mode → dry | `90 5E 74 5E 74 02 21 11 C2`, then `… 02 21 00 B1` |
| power → off | `90 5B 5B 5B 5B 00 20 11 87`, then idle |

## display → main frame (GPIO4)

`30 KK CS`: 24 bits plus one trailing stop bit. Same header, timing and checksum as main → display.

GPIO3 does not work: on a D1 mini, RX is driven by the USB-serial chip through 470 Ω, and the divider (~222 Ω source) only pulls it to ~1.1 V, above the ESP's LOW threshold (~0.8 V).

| Byte | Meaning |
| --- | --- |
| 0 | header `30` |
| 1 | key code, `00` = none |
| 2 | checksum |

- `30 00 8A` is a keepalive, about every 9.5 s.
- A key press sends **one** frame with the key code, then `30 00 8A` ~50 ms later.
- The main board answers within 50–150 ms with a new display frame (byte 7 = `11`).

| Key | Code | Frame | Effect |
| --- | --- | --- | --- |
| (none) | `00` | `30 00 8A` | keepalive |
| mode | `01` | `30 01 8B` | cycles cool → dry → fan → cool |
| down | `02` | `30 02 8C` | setpoint −1 |
| power | `03` | `30 03 8D` | on/off, resumes last mode |
| timer | `04` | `30 04 8E` | timer on / cancel while flashing (see below) |
| fan speed | `05` | `30 05 8F` | toggles low (`21`) ↔ high (`22`) |
| swing? | `06` | not seen | probably swing; the K7 "SWING" button is not fitted on this model |
| sleep | `07` | `30 07 91` | toggles byte 5 `0x10` |
| up | `08` | `30 08 92` | setpoint +1 |

### Timer

- Timer key sets byte 5 bit7 (`01` → `81`); the display flashes "01" (`3F 06` alternating with `00 00`, ~0.5 s).
- Up/down while flashing set the hours ("01" → "02" → "03").
- After ~4 s without input the display returns to the setpoint; the timer stays on.
- Timer key while flashing cancels (`81` → `01`).
- Frames carry no countdown. Hours are only visible while setting.

### Captured sequence: keys, unit on

| Key | Display frame after | Display |
| --- | --- | --- |
| `03` power | `90 71 77 71 77 04 21 …` | "FA", on in fan (last mode) |
| `01` mode | `90 5B 5B 5B 5B 01 21 …` | cool, "22" |
| `08` up | `90 5B 4F …` | "23" |
| `08` up | `90 5B 66 …` | "24" |
| `02` down | `90 5B 4F …` | "23" |
| `05` ×4 | byte 6 `21` → `22` → `21` → `22` → `21` | two speeds only |
| `07` ×2 | byte 5 `01` → `11` → `01` | sleep on, off |

## IR remote

Standard **NEC**, address `0x7F80` (ESPHome notation), 7 buttons. One fixed command per button, so the remote sends **single keys, not a full state**. Power toggles (no separate on/off). Each press = one NEC frame plus ~2 repeat codes.

The display board decodes IR itself and forwards each press as a CN1 key code, **different from the panel codes**:

| Button | NEC command | CN1 key (remote) | CN1 key (panel) |
| --- | --- | --- | --- |
| power | `0x639C` | `10` | `03` |
| mode | `0x629D` | `0C` | `01` |
| up | `0x6B94` | `0B` | `08` |
| down | `0x6A95` | `0E` | `02` |
| speed | `0x6996` | `0A` | `05` |
| sleep | `0x649B` | `0D` | `07` |
| timer | `0x609F` | `0F` | `04` |

### Control approach

Control has to be **stateful**: read the state from main → display frames, then press keys until the target is reached (setpoint: up/down × difference; mode: mode until byte 5 matches; fan/power/sleep: one press if different).

Keys are injected as NEC (address `0x7F80`) by `remote_transmitter` on GPIO13 through the NPN. The receiver output is already demodulated, so `carrier_duty_percent: 100%`, pin not inverted (GPIO high pulls the line low = IR mark).

## Climate component (firmware)

ESPHome 2026 dropped `custom:` components, so the climate entity is a local external component: [cecotec/components/cecotec_ac/](cecotec/components/cecotec_ac/).

- **State:** decoded from every valid main → display frame (`on_main_frame`).
  - Setpoint read from the digits in cool mode only.
  - Digits ignored while the timer display may be in edit (see Timer).
- **Sequencer:** one key per step, order power → mode → setpoint → fan → sleep → timer.
  - Waits for a changed frame after each key (1.5 s timeout).
  - 3 tries without reaction drop that target; 30 s overall timeout.
  - No keys while the timer display may be in edit, or when no frame arrived for 25 s.
- **Manual override:** a panel or remote key during an HA command cancels it. Own injected keys echo as remote codes within 1.5 s and are ignored.
- **HA "Dry" = humidity control** (DHT humidity):
  - Runs in dry until humidity < target − hysteresis, then turns off; back on (dry) when humidity > target + hysteresis. "Dry hysteresis" number, default 3%.
  - HA shows mode dry, action drying or idle.
  - No ESP-side run/idle timers: the AC pauses its compressor by itself after a while in dry.
  - If the unit changes outside the controller (panel, remote, AC timer, failed command), the controller steps back and HA follows the unit.
  - Survives a reboot when the unit is in dry or off.
- **Timer (HA):** "Timer" binary sensor (byte 5 bit7), "Timer hours" number (0 = off, setting it presses keys), buttons "Timer +1h", "Timer -1h", "Timer off".
  - Keys: timer off → timer key turns it on ("01"). Flashing → up/down change hours, timer key cancels. On, not flashing → timer key enters edit, current hours flash.
  - Hours are read only from the flashing display: a frame within 600 ms after a timer/up/down key, or the same digits on both sides of a blank frame. The setpoint shown when edit ends differs from the hours, so it is never mistaken for them.
  - "May be in edit" = 4.5 s after the last blank frame or edit key: no climate keys, no setpoint read.
  - Up/down for hours only when edit is certain (blank frame or edit key within 1.5 s).
  - HA shows the hours last set or seen.
- **Error melody** (falling G-E♭-C, max once a minute, muted by the "Beeper" switch, silent for 60 s after boot):
  - HA command gave up (3 tries or 30 s timeout)
  - WiFi not connected (checked every 15 s, once per outage)
  - CN1 link lost: no main → display frames for 25 s (also the "AC link" diagnostic sensor)
  - DHT read failed (NaN)
- **Beep:** byte 7 ≠ `00` → beep on BUZ1. Skipped when "Beeper" is off or within 1.5 s of an ESP key (HA commands stay silent).
  - "Beeper volume" (5–100%) sets PWM duty to asin(v²)/π; 100% = 50% duty, loudest.
  - Boot melody after every start, WiFi chime on every connect.
  - A **new** unknown value plays a warble + "uh-oh" instead of the beep.

## Display board

Board `JRS-AD19D-LED-DM` (label `BA019D-D0…`), see [pics/board front.jpeg](pics/board%20front.jpeg). Shared design, some parts not fitted:

- **K7 "SWING"** button and **LED2 "SWING"**: not fitted → panel key `06` is most likely swing, unused here.
- **LED4 "WATER FULL"** (red, next to CN1): fitted. The display board drives it, so the main board must send a water-full flag over CN1. Bit unknown.
- Other LEDs: TIMER (LED8), fan LOW / MID / HIGH (LED5 / LED1 / LED3), SLEEP, COOL (LED6), DEHUM (LED12), FAN (LED10), POWER (LED7). MID is unused (two speeds).
- CN2: 5-pin programming header (RST, +5V, GND, SDA, SCK).

### Triggering water full (not done yet)

The float switch is wired to the **main board**. Options:

1. Magnet at the float, or lift the float by hand (usually magnet + reed switch).
2. Unplug the float connector on the main board (NC switch reads full) or short its pins (NO). Unplug the unit from mains first.
3. Close the drain plug and run dry in a humid room until the tank fills (slow).

The unknown-value watch should catch it: a new byte 5 bit (`0x08`, `0x20`, `0x40` unused), a new byte 7 value, or new digits (e.g. "FL" = `71 38`). Also note whether the compressor stops and whether the unit recovers when the float drops.

## Buzzer

- BUZ1 (17 mm, open circuit on the ohmmeter → passive piezo) was on the display board. The main board triggers beeps via byte 7 = `11`, so every AC beep, alarms included, shows up on CN1.
- BUZ1 is now disconnected from the display board and driven by the ESP (GPIO15 → 2N2222, piezo between 5V and collector, 1k across the piezo).

## Unknown-value watch (firmware)

Every changed CN1 frame and every remote command (NEC `0x7F80`) is checked against the values documented here. A value never seen before is flagged and saved to flash (up to 8 entries, survives power loss via `restore_from_flash: true`). Goal: catch frames not seen yet, such as water full or error codes.

Known values (`cn1_check_unknown()` / `cn1_check_nec()` in [cecotec/cn1_decode.h](cecotec/cn1_decode.h)):

| Line | Field | Known values |
| --- | --- | --- |
| main → display | length / header | 9 bytes, `90` |
| main → display | digit bytes 1–4 | blank, 0–9, F, A, d, h. Bytes 3–4 must copy bytes 1–2 |
| main → display | byte 5 | low nibble 0/1/2/4, plus `0x10` sleep and `0x80` timer |
| main → display | byte 6 | `20`–`22` |
| main → display | byte 7 | `00`, `11` |
| display → main | length / header | 3 bytes, `30` |
| display → main | key | `00`–`05`, `07`, `08`, `0A`–`10` |
| IR | NEC command low byte | `9C 9D 94 95 96 9B 9F` |

Home Assistant entities (diagnostic):

- **Unknown value (last):** newest entry with its frame, e.g. `main>disp digit=79 x1: 90 79 06 79 06 …`
- **Unknown values:** short list of all entries with counts
- **Unknown values count**
- **Clear unknown values** (button): use after documenting the new values here

Each new value is also logged at WARN under the tag `unknown`.

Optional HA automation for a notification per new value (check the entity id):

```yaml
automation:
  - alias: Cecotec unknown CN1 value
    trigger:
      - platform: state
        entity_id: sensor.cecotec_ac_unknown_value_last
    condition: "{{ trigger.to_state.state not in ['none', 'unknown', 'unavailable', ''] }}"
    action:
      - service: persistent_notification.create
        data:
          title: Cecotec AC – new unknown value
          message: "{{ trigger.to_state.state }}"
```

## Open questions

- [ ] Water-full flag: which bit? See [Triggering water full](#triggering-water-full-not-done-yet)
