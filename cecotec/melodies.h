#pragma once
#include <string>
#include <pgmspace.h>

// RTTTL melodies in flash (PROGMEM) to save ESP8266 RAM; melody() copies one out while it plays.
// MELODIES order = options of the "Test melody" select in the YAML (alphabetical); keep both in sync.
// MIDI-derived ones: tools/midi2rtttl.py (recipes in its docstring)

static const char MEL_BEEP[] PROGMEM = "beep:d=8,o=7,b=120:e";
static const char MEL_DOUBLE[] PROGMEM = "double:d=16,o=7,b=160:e,p,e";
static const char MEL_STARTUP[] PROGMEM = "startup:d=16,o=6,b=180:c,e,g,8c7";
static const char MEL_ELISE[] PROGMEM =
    "elise:d=16,o=5,b=75:e6,d#6,e6,d#6,e6,b,d6,c6,8a,p,c,e,a,8b,p,e,g#,b,8c6";
// New unknown value: warble + descending "uh-oh"
static const char MEL_UNKNOWN[] PROGMEM = "unknown:d=32,o=7,b=180:c,g6,c,g6,c,g6,8p,d#,c#,c,b6,a#6,4a6";
static const char MEL_NOKIA[] PROGMEM = "nokia:d=8,o=5,b=225:e6,d6,4f#,4g#,c#6,b,4d,4e,b,a,4c#,4e,2a";
static const char MEL_SMB[] PROGMEM = "smb:d=16,o=6,b=105:e,e,p,e,p,c,e,p,g,8p.,g5";
// *_LONG = "- extended" versions in the select. Lengths like 12/24 are triplets (whole note / 12 or 24).
static const char MEL_SMB_LONG[] PROGMEM =
    "smblong:d=16,o=6,b=105:e,e,p,e,p,c,e,p,g,8p.,g5,8p.,c,8p,g5,8p,e5,8p,a5,p,b5,p,a#5,a5,p,12g5,12e,"
    "12g,a,p,f,g,p,e,p,c,d,b5,8p,c,8p,g5,8p,e5,8p,a5,p,b5,p,a#5,a5,p,12g5,12e,12g,a,p,f,g,p,e,p,c,d,b5,"
    "4p,g,f#,f,d#,p,e,p,g#5,a5,c,p,a5,c,d,8p,g,f#,f,d#,p,e,p,c7,p,c7,c7,4p,p,g,f#,f,d#,p,e,p,g#5,a5,c,p,"
    "a5,c,d,8p,d#,8p,d,8p,c,2p,p,g,f#,f,d#,p,e,p,g#5,a5,c,p,a5,c,d,8p,g,f#,f,d#,p,e,p,c7,p,c7,c7,4p,p,g,"
    "f#,f,d#,p,e,p,g#5,a5,c,p,a5,c,d,8p,d#,8p,d,8p,c";
static const char MEL_COIN[] PROGMEM = "coin:d=16,o=6,b=200:b,4e7.";
static const char MEL_ONEUP[] PROGMEM = "oneup:d=16,o=7,b=220:e6,p,g6,p,e,p,c,p,d,p,2g";
// OoT NA_SE_SY_CORRECT_CHIME (zeldaret/oot seq_0, CHAN_5833): 2 square voices alternating every
// 12 ticks at 120 bpm / 48 ticks per beat = 16ths, last note 48 ticks = quarter; +2 octaves.
static const char MEL_SECRET[] PROGMEM = "secret:d=16,o=6,b=120:g,f#,d#,a5,g#5,e,g#,4c7";
static const char MEL_TETRIS[] PROGMEM =
    "tetris:d=8,o=6,b=150:4e,b5,c,d,16e,16d,c,b5,4a5,a5,c,4e,d,c,4b5.,c,4d,4e,4c,4a5,2a5";
static const char MEL_MEGALOVANIA[] PROGMEM =
    "megalovania:d=16,o=6,b=120:d,d,d7,p,a,8p,g#,p,g,p,8f,d,f,g,c,c,d7,p,a,8p,g#,p,g,p,8f,d,f,g";
static const char MEL_MEGALOVANIA_LONG[] PROGMEM =
    "megalong:d=16,o=6,b=120:d,d,d7,p,a,8p,g#,p,g,p,8f,d,f,g,c,c,d7,p,a,8p,g#,p,g,p,8f,d,f,g,b5,b5,d7,p,"
    "a,8p,g#,p,g,p,8f,d,f,g,a#5,a#5,d7,p,a,8p,g#,p,g,p,8f,d,f,g,d,d,d7,p,a,8p,g#,p,g,p,8f,d,f,g,c,c,d7,p,"
    "a,8p,g#,p,g,p,8f,d,f,g,b5,b5,d7,p,a,8p,g#,p,g,p,8f,d,f,g,a#5,a#5,d7,p,a,8p,g#,p,g,p,f,d#,d,f,g,8p,f,"
    "f,p,f,p,8f,d,p,4d,p,8f,f,f,p,g,p,8g#,g,f,d,f,g,8p,8f,f,f,p,g,p,g#,p,a,p,c7,p,8a.,d7,p,d7,p,d7,a,d7,"
    "2c7,p,8a,a,a,p,a,p,8a,g,p,4g,p,8a,a,a,p,a,p,f,p,a,p,d7,p,a,8g,8d7,8a,8g,8f,8c7,8a,8f,8e,8d,e,f,p,a,"
    "p,2c7";
static const char MEL_ALLSTAR[] PROGMEM = "allstar:d=8,o=6,b=108:4f#,c#7,a#,4a#,g#,f#,f#,4b,a#,a#,g#,g#,f#";
static const char MEL_IMPERIAL[] PROGMEM =
    "imperial:d=8,o=6,b=102:g,p,g,p,g,p,d#,16p,16a#,4g,d#,16p,16a#,4g.,p,4d7,4d7,4d7,d#7,16p,16a#,4f#,d#,16p,"
    "16a#,4g";
static const char MEL_FFVICTORY[] PROGMEM = "ffvictory:d=8,o=7,b=225:d#,d#,d#,4d#.,4b6.,4c#.,4d#,c#,1d#";
// Fanfare, then the victory loop once
static const char MEL_FFVICTORY_LONG[] PROGMEM =
    "fflong:d=4,o=6,b=150:12d#7,12d#7,12d#7,d#7,b,c#7,6d#7,12c#7,2d#7.,a#,g#,a#,8g#,c#7,8c#7,c7,8c#7,c7,"
    "8c7,a#,g#,g,8g#,1f,8p,a#,g#,a#,8g#,c#7,8c#7,c7,8c#7,c7,8c7,a#,g#,a#,8c#7,1d#7,8p,a#,g#,a#,8g#,c#7,"
    "8c#7,c7,8c#7,c7,8c7,a#,g#,g,8g#,1f,8p,a#,g#,a#,8g#,c#7,8c#7,c7,8c#7,c7,8c7,a#,g#,a#,8c#7,1d#7";
// Half tempo of the MIDI; mordents as 64ths (allowed per note, only d= is limited to 1..32).
static const char MEL_TOCCATA[] PROGMEM =
    "toccata:d=32,o=5,b=60:64a7,64g7,8a7.,16p.,g7,f7,e7,d7,16c#7,8d7,4p.,64a6,64g6,8a6.,16p.,16e6,16f6,16c#6,8d6,"
    "4p.,64a,64g,8a.,16p.,g,f,e,d,16c#,8d,4p.,2e6.,2d6";
// MEL_TOCCATA (Adagio, half tempo), then the Prestissimo runs at the MIDI's tempo (24 = 16th triplets)
static const char MEL_TOCCATA_LONG[] PROGMEM =
    "toclong:d=32,o=5,b=60:64a7,64g7,8a7.,16p.,g7,f7,e7,d7,16c#7,8d7,4p.,64a6,64g6,8a6.,16p.,16e6,16f6,"
    "16c#6,8d6,4p.,64a,64g,8a.,16p.,g,f,e,d,16c#,8d,4p.,2e6.,2d6,4p,16c#6,24d6,24e6,24c#6,24d6,24e6,"
    "24c#6,24d6,24e6,24c#6,16d6,16e6,24f6,24g6,24e6,24f6,24g6,24e6,24f6,24g6,24e6,16f6,16g6,24a6,24a#6,"
    "24g6,24a6,24a#6,24g6,24a6,24a#6,24g6,16a6,2p,16c#7,24d7,24e7,24c#7,24d7,24e7,24c#7,24d7,24e7,24c#7,"
    "16d7,16e7,24f7,24g7,24e7,24f7,24g7,24e7,24f7,24g7,24e7,16f7,16g7,24a7,24a#7,24g7,24a7,24a#7,24g7,"
    "24a7,24a#7,24g7,16a7,2p,16a7,24g7,24a#7,24e7,24g7,24a#7,24e7,24f7,24a7,24d7,24f7,24a7,24d7,24e7,"
    "24g7,24c7,24e7,24g7,24c7,24d7,24f7,24a#6,24d7,24f7,24a#6,24c7,24e7,24a6,24c7,24e7,24a6,24a#6,24d7,"
    "24g6,24a#6,24d7,24g6,24a6,24c7,24f6,24a6,24c7,24f6,24g6,24a#6,24e6,24g6,24a#6,24e6,24f6,24a6,24d6,"
    "24f6,24a6,24d6,24e6,24g6,24c#6,24e6,24g6,24c#6,4p,2a#6,p,a6,g6,f6,e6,d6,c#6,b,16c#6,16a,16c#6,e6,g6,"
    "8f6.,16e6,4f6";
// JoJo Golden Wind, "Il vento d'oro" (Yugo Kanno): riff, then the theme.
static const char MEL_GOLDENWIND[] PROGMEM =
    "goldenwind:d=8,o=6,b=135:b,b,16b,a,b,d7,b,16f#,a,b,b,16b,a,b,f7,e7,16d7,a";
static const char MEL_GOLDENWIND_LONG[] PROGMEM =
    "gwlong:d=16,o=7,b=135:8b6,8b6,b6,8a6,8b6,8d,8b6,f#6,8a6,8b6,8b6,b6,8a6,8b6,8f,8e,d,8a6,8b6,8b6,b6,"
    "8a6,8b6,8d,8b6,f#6,8a6,8b6,8b6,8b6,a6,b6,4p,b6,c#,d,e,2f#,8p,8b6,8d,8a,2g#,8p,4f#,8e,1f#,2f#.,b6,"
    "c#,d,e,2f#,8p,8b6,8d,8a,4g#.,4a.,4b,8g#.,2f#.,p,2f#.,b6,c#,d,e,2f#,8p,8b6,8d,8a,2g#,8p,4f#,8e,1f#,"
    "2f#.,b6,c#,d,e,2f#,8p,8b6,8d,8a,4b.,4a.,4g#,1f#,2f#.";
// JoJo Stardust Crusaders main theme (Yugo Kanno), sax line. Long version: guitar riff, then the theme twice.
static const char MEL_STARDUST[] PROGMEM =
    "stardust:d=8,o=6,b=140:4g#5.,4c.,4f,1g,4p,4g#,4a#,4b,c7.,b.,g#.,f.,p,c7,b.,a#.,g#.,f.,p,d#7,1d7";
static const char MEL_STARDUST_LONG[] PROGMEM =
    "sclong:d=8,o=6,b=140:f,c,16d#,16e,16p,f.,c,d#,e,f,c,16d#,16e,16p,f.,d#,f,f#,f,c,16d#,16e,16p,f.,c,"
    "d#,e,f,c,16d#,16e,16p,f.,4p.,1f5,4g#5.,4c.,4f,1g.,p,g#,a#,b,c7.,b.,g#.,f.,c,c7,b.,a#.,4g#,4f,d#,1d.,"
    "16p,16f,16e,16d#,16d,16d#,16e,16f,1f5,4g#5.,4c.,4f,1g,4p,4g#,4a#,4b,c7.,b.,g#.,f.,p,c7,b.,a#.,g#.,"
    "f.,p,d#7,1d7";
// Schubert "Die Forelle" D550, voice line in A major at ~100 bpm like Samsung washers play it.
// Short = first two phrases, extended = the whole first verse.
static const char MEL_FORELLE[] PROGMEM =
    "forelle:d=8,o=6,b=100:e,a,a,c#7,c#7,4a,e,e,e.,16e,16b,16a,16g#,16f#,4e,p,e,a,a,c#7,c#7,4a,e,a,g#,16f#,"
    "16g#,a,d#,4e";
static const char MEL_FORELLE_LONG[] PROGMEM =
    "forellelong:d=16,o=6,b=100:8e,8a,8a,8c#7,8c#7,4a,8e,8e,8e.,e,b,a,g#,f#,4e,8p,8e,8a,8a,8c#7,8c#7,4a,8e,"
    "8a,8g#,f#,g#,8a,8d#,4e,8p,8e,8g#,8g#,a,g#,f#,g#,4a,8e,8a,8g#,8g#,g#,d7,b,g#,4a.,8a,8f#,8f#,8f#,8a,4a,"
    "8e,8e,8e.,e,8b,8g#,4a.,8a,g#,f#,8f#,f#,a,g#,b,4a,8e,8e,8e.,e,8b,8g#,4a";
// The "uh-oh" from the end of MEL_UNKNOWN
static const char MEL_ERROR[] PROGMEM = "error:d=32,o=7,b=180:d#,c#,c,b6,a#6,4a6";
// WiFi (re)connected: rising fifth + octave, shorter and higher than MEL_STARTUP
static const char MEL_WIFI[] PROGMEM = "wifi:d=16,o=7,b=180:e6,b6,8e";
// Happy Birthday in C (g6..g7)
static const char MEL_BIRTHDAY[] PROGMEM =
    "birthday:d=4,o=6,b=110:8g.,16g,a,g,c7,2b,8g.,16g,a,g,d7,2c7,8g.,16g,g7,e7,c7,b,2a,8f7.,16f7,e7,c7,d7,"
    "2c7.";

// Sorted by select option name (alphabetical).
static const char *const MELODIES[] PROGMEM = {
    MEL_ALLSTAR,           // 0  All Star
    MEL_BEEP,              // 1  Beep
    MEL_FORELLE,           // 2  Die Forelle
    MEL_FORELLE_LONG,      // 3  Die Forelle - extended
    MEL_DOUBLE,            // 4  Double beep
    MEL_ERROR,             // 5  Error
    MEL_FFVICTORY,         // 6  FF victory
    MEL_FFVICTORY_LONG,    // 7  FF victory - extended
    MEL_ELISE,             // 8  Fur Elise
    MEL_GOLDENWIND,        // 9  Golden Wind
    MEL_GOLDENWIND_LONG,   // 10 Golden Wind - extended
    MEL_BIRTHDAY,          // 11 Happy Birthday
    MEL_IMPERIAL,          // 12 Imperial March
    MEL_ONEUP,             // 13 Mario 1-up
    MEL_COIN,              // 14 Mario coin
    MEL_SMB,               // 15 Mario intro
    MEL_SMB_LONG,          // 16 Mario theme - extended
    MEL_MEGALOVANIA,       // 17 Megalovania
    MEL_MEGALOVANIA_LONG,  // 18 Megalovania - extended
    MEL_NOKIA,             // 19 Nokia ringtone
    MEL_STARDUST,          // 20 Stardust Crusaders
    MEL_STARDUST_LONG,     // 21 Stardust Crusaders - extended
    MEL_STARTUP,           // 22 Startup
    MEL_TETRIS,            // 23 Tetris
    MEL_TOCCATA,           // 24 Toccata and Fugue
    MEL_TOCCATA_LONG,      // 25 Toccata and Fugue - extended
    MEL_UNKNOWN,           // 26 Unknown value
    MEL_WIFI,              // 27 WiFi connected
    MEL_SECRET,            // 28 Zelda secret
};
static const int MELODY_COUNT = sizeof(MELODIES) / sizeof(MELODIES[0]);

// Indices of the melodies the firmware plays by itself.
enum : int {
  MELODY_BEEP = 1,
  MELODY_ERROR = 5,
  MELODY_STARTUP = 22,
  MELODY_UNKNOWN = 26,
  MELODY_WIFI = 27,
};
static_assert(MELODY_COUNT == 29, "MELODIES changed: re-check the MELODY_ indices and the select options");

// Pitch shifter: move every note of an RTTTL string by semis semitones. ESPHome rtttl only
// plays octaves 4-7 (C4..B7), notes pushed outside fold back by whole octaves.
// Shifted notes get an explicit octave; lengths, rests and the header stay as they are.
static std::string transpose(const std::string &in, int semis) {
  size_t hdr = in.find(':');
  size_t body = hdr == std::string::npos ? hdr : in.find(':', hdr + 1);
  if (semis == 0 || body == std::string::npos)
    return in;
  size_t o = in.find("o=", hdr);
  int odef = o < body ? atoi(in.c_str() + o + 2) : 6;  // rtttl's default octave is 6
  static const int8_t BASE[] = {9, 11, 0, 2, 4, 5, 7, 11};  // a..h (h = German b)
  static const char LETTER[] = "ccddeffggaab";
  static const char SHARP[] = " # #  # # # ";
  std::string out(in, 0, body + 1);
  for (size_t i = body + 1; i < in.size();) {
    char c = in[i++];
    if (c < 'a' || c > 'h') {  // lengths, dots, commas, 'p'
      out += c;
      continue;
    }
    int n = BASE[c - 'a'];
    if (i < in.size() && in[i] == '#') {
      n++;
      i++;
    }
    int oct = odef;
    if (i < in.size() && isdigit(in[i]))
      oct = in[i++] - '0';
    int p = oct * 12 + n + semis;
    while (p < 4 * 12)
      p += 12;
    while (p >= 8 * 12)
      p -= 12;
    out += LETTER[p % 12];
    if (SHARP[p % 12] == '#')
      out += '#';
    out += char('0' + p / 12);
  }
  return out;
}

// Copy melody idx from flash into a string for rtttl play(), shifted by semis semitones;
// out of range -> beep.
static std::string melody(int idx, int semis = 0) {
  if (idx < 0 || idx >= MELODY_COUNT)
    idx = MELODY_BEEP;
  const char *p = (const char *) pgm_read_ptr(&MELODIES[idx]);
  std::string s(strlen_P(p), '\0');
  memcpy_P(&s[0], p, s.size());
  return transpose(s, semis);
}
