/*
 * Rangmang Controller - Arduino port (revised)
 * Original: Saeid Moghadam (Bascom AVR, ATmega328P)
 * Hardware: ATmega328P, WS2812 on D5, serial 9600
 *
 * Protocol:  A(adr) D(mode) R G B S E M      e.g. A1D0R255G0B0S0E4M0
 *            CH<old>=<new>                   address change -> "New Address=<new>"
 */

#include <FastLED.h>
#include <EEPROM.h>

#define LED_PIN      5
#define NUM_LEDS     14        // number of pixels on your strip
#define COLOR_ORDER  GRB       // WS2812B = GRB, some WS2811 = RBG
#define DEBUG        0         // 1 = print parsed values (do NOT use on a shared bus)

// Rb_shiftleft/right in Bascom: vacated pixel black (0) or current color (1)?
// Verify on hardware against the Bascom build (see notes).
#define SHIFT_FILL_COLOR 0

CRGB leds[NUM_LEDS];

byte  Ch0 = 1, Dan = 0, Danc = 0, Dans = 0;
byte  Rr = 255, Gg = 255, Bb = 255;
byte  Rx = 255, Gx = 255, Bx = 255;
word  Led = NUM_LEDS - 1;
word  Tim1 = 30, Tim2 = 30;
word  Ns = 0, Nf = NUM_LEDS - 1;
byte  Mx = 0, Nnb = 0, Chc = 0;
word  Ado = 0, T = 0;
bool  Rainbo = false;
volatile bool Ina = false;

char  Text[41];
byte  TextLen = 0;

#define ABORT  if (Ina) return

// ---------------- prototypes ----------------
void Chekm(); void Time1(); void Time2(); void Modh(); void Red();
void Snak_run_slow(); void Snak_return_slow(); void Snak_run_fast(); void Snak_return_fast();
void Snak_run_slow_random(); void Snak_return_slow_random();
void Snak_run_fast_random(); void Snak_return_fast_random();
void Snak_runreturn_slow_random(); void Half_snak_out_mid_slow();
void Half_half_right_slow(); void Half_half_left_slow();
void Star_runreturn_slow(); void Star_run_slow(); void Star_run_fast(); void Star_return_fast();
void Snak_growup_slow(); void Star_playful(); void Snak_star_playful(); void Star_fillers();
void Movable_light(); void Snak_runreturn_colored();
void Star_diamonds_blink_slow(); void Star_diamonds_blink_fast(); void Star_diamonds_blink_faster();
void Disco(); void Fillstripe_color(); void Breathing();
void Percolate_run(); void Percolate_return();
void Snak_1_2_run(); void Snak_1_2_return(); void Snak_1_2_run_rev(); void Snak_1_2_return_rev();
void Snak_relax_run(); void Snak_relax_return();
void Snak_rainbow(); void Snak_rainbow_lp(); void Snak_rainbow_bl();

// ---------------- helpers ----------------
// Bascom Rnd(n) returns 0..n-1
static word rnd(word n) { return n ? (word)random(n) : 0; }
static CRGB rndCol()    { return CRGB(rnd(Rx), rnd(Gx), rnd(Bx)); }
static void setPx(int i, CRGB c) { if (i >= 0 && i < NUM_LEDS) leds[i] = c; }
static CRGB fillCol() { return SHIFT_FILL_COLOR ? CRGB(Rx, Gx, Bx) : CRGB(0, 0, 0); }

static void shR() {                      // shift right over 0..Led
  for (int i = Led; i > 0; i--) leds[i] = leds[i - 1];
  leds[0] = fillCol();
}
static void shL() {                      // shift left over 0..Led
  for (int i = 0; i < (int)Led; i++) leds[i] = leds[i + 1];
  leds[Led] = fillCol();
}
static void rotR() {
  CRGB last = leds[Led];
  for (int i = Led; i > 0; i--) leds[i] = leds[i - 1];
  leds[0] = last;
}
static void rotL() {
  CRGB first = leds[0];
  for (int i = 0; i < (int)Led; i++) leds[i] = leds[i + 1];
  leds[Led] = first;
}

static int num(const char* s, byte maxDigits) {
  int v = 0;
  while (maxDigits-- && *s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
  return v;
}

// ---------------- EEPROM (same header addresses as Bascom) ----------------
// 1:Ch0  2:Dan  3,4,5:R,G,B  6-7:Led  9:Memoclr  10-11:Tim1  12-13:Tim2
// slot I (1..98): R@100+I  G@200+I  B@300+I  start@396+4I  end@398+4I   (I = 99 - M)
static word eeGetW(int a) { return (word)EEPROM.read(a) | ((word)EEPROM.read(a + 1) << 8); }
static void eePutW(int a, word v) { EEPROM.update(a, v & 0xFF); EEPROM.update(a + 1, v >> 8); }

static void Memclr() {
  Serial.println(F("   "));
  Serial.println(F(" Rangmang Wait "));
  for (int i = 0; i <= 800; i++) EEPROM.update(i, 0);
  Serial.println(F(" Clear :) "));
  FastLED.clear(); FastLED.show();
}

static void saveSlot(word m) {
  int I = 99 - m;
  EEPROM.update(100 + I, Rr); EEPROM.update(200 + I, Gg); EEPROM.update(300 + I, Bb);
  eePutW(396 + 4 * I, Ns);    eePutW(398 + 4 * I, Nf);
  eePutW(6, Led);
}

static void restoreFromEeprom() {
  Ch0 = EEPROM.read(1); Dan = EEPROM.read(2);
  Rr = EEPROM.read(3);  Gg = EEPROM.read(4); Bb = EEPROM.read(5);
  Led = eeGetW(6);
  byte memoclr = EEPROM.read(9);
  Tim1 = eeGetW(10); Tim2 = eeGetW(12);
  Chekm();
  if (Ch0 > 98 || Ch0 < 1) Ch0 = 1;
  if (Dan > 99) Dan = 0;
  if (Led > 990) Led = 990;
  if (memoclr == 0 || memoclr > 1) { Memclr(); EEPROM.update(9, 1); }
  Danc = Dan;

  if (Dan == 0) {
    if (Led > 0 && Led < 99) {
      for (int I = 1; I <= 98; I++) {
        CRGB c(EEPROM.read(100 + I), EEPROM.read(200 + I), EEPROM.read(300 + I));
        word a = eeGetW(396 + 4 * I), b = eeGetW(398 + 4 * I);
        for (word s = a; s <= b && s < NUM_LEDS; s++) leds[s] = c;
      }
      FastLED.show();
    } else if (Led == 999) {
      fill_solid(leds, NUM_LEDS, CRGB(Rr, Gg, Bb));
      FastLED.show();
    }
  }
}

// ============================================================
void setup() {
  Serial.begin(9600);
  FastLED.addLeds<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
  FastLED.clear(); FastLED.show();
  restoreFromEeprom();
}

// Non-blocking line reader; also called from inside the delays so a new
// command interrupts a running effect immediately (like Bascom Bytematch=13).
static void pollSerial() {
  while (!Ina && Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (TextLen > 0) { Text[TextLen] = 0; Ina = true; }
    } else if (TextLen < sizeof(Text) - 1) {
      Text[TextLen++] = c;
    }
  }
}

void loop() {
  pollSerial();
  if (Ina) {
    Red();
    Ina = false; TextLen = 0; Text[0] = 0;
    return;
  }
  if (Danc > 0) Modh();
}

// ============================================================
void Red() {
  // ---- address change:  CH<old>=<new>
  if (Text[0] == 'C' && Text[1] == 'H') {
    char* eq = strchr(Text, '=');
    if (eq && (eq - Text) >= 3 && num(Text + 2, 3) == Ch0) {
      int nw = num(eq + 1, 3);
      if (nw > 0 && nw < 99) {
        Ch0 = nw; EEPROM.update(1, Ch0);
        Serial.print(F("New Address=")); Serial.println(Ch0);
      }
    }
    return;
  }

  byte Ch = 0;
  byte n = strlen(Text); if (n > 32) n = 32;

  for (byte i = 0; i < n; i++) {
    char t = Text[i];
    if (!strchr("ADRGBMSE", t)) continue;
    const char* s = Text + i + 1;

    if (t == 'A') { int v = num(s, 2); if (v > 0 && v < 100) Ch = v; }
    if (Ch != Ch0 && Ch != 99) continue;

    int v = num(s, (t == 'D') ? 2 : 3);
    switch (t) {
      case 'D': if ((v >= 0 && v <= 55) || v == 99) { Danc = v; Dan = v; } break;
      case 'R': if (v < 256) Rr = v; break;
      case 'G': if (v < 256) Gg = v; break;
      case 'B': if (v < 256) Bb = v; break;
      case 'M': Led = v; break;
      case 'S': if (Dan == 0) Ns = v; else Tim1 = v; break;
      case 'E': if (Dan == 0) Nf = v; else Tim2 = v; break;
    }
  }

#if DEBUG
  Serial.print(F("A=")); Serial.print(Ch); Serial.print(F(" D=")); Serial.print(Danc);
  Serial.print(F(" R=")); Serial.print(Rr); Serial.print(F(" G=")); Serial.print(Gg);
  Serial.print(F(" B=")); Serial.print(Bb); Serial.print(F(" S=")); Serial.print(Ns);
  Serial.print(F(" E=")); Serial.print(Nf); Serial.print(F(" M=")); Serial.println(Led);
#endif

  if (Dan < 100) EEPROM.update(2, Dan);

  if (Dan == 0) {
    CRGB c(Rr, Gg, Bb);
    if (Led > 0 && Led < 99) {                 // M1..M98 : paint + save slot
      saveSlot(Led);
      for (word i = Ns; i <= Nf && i < NUM_LEDS; i++) leds[i] = c;
      FastLED.show();
    } else {
      switch (Led) {
        case 0:                                // preview, no save, NO clear
          for (word i = Ns; i <= Nf && i < NUM_LEDS; i++) leds[i] = c;
          FastLED.show();
          break;
        case 996: Memclr(); break;
        case 997: FastLED.clear(); FastLED.show(); delay(100); break;
        case 998: fill_solid(leds, NUM_LEDS, c); FastLED.show(); delay(100); break;
        case 999:
          fill_solid(leds, NUM_LEDS, c); FastLED.show();
          EEPROM.update(3, Rr); EEPROM.update(4, Gg); EEPROM.update(5, Bb);
          eePutW(6, 999);
          break;
      }
    }
  } else {
    if (Led < 1) Led = 1;
    if (Led > 999) Led = 999;
    Chekm();
    if (Danc > 0 && Danc < 100) {
      EEPROM.update(3, Rr); EEPROM.update(4, Gg); EEPROM.update(5, Bb);
      eePutW(6, Led); eePutW(10, Tim1); eePutW(12, Tim2);
    }
  }
}

// ============================================================
void Chekm() {
  if (Tim1 > 999 || Tim1 < 1) Tim1 = 1;
  if (Tim2 > 999 || Tim2 < 1) Tim2 = 1;
}

static void waitMs(word t) {           // t+1 ms, aborts as soon as a full line arrives
  for (word i = 0; i <= t; i++) {
    pollSerial();
    if (Ina) return;
    delay(1);
  }
}
void Time1() { Chekm(); waitMs(Tim1); }
void Time2() { Chekm(); waitMs(Tim2); }

// ============================================================
static byte pickCh(byte v) {
  if (v < 251) return v;
  switch (v) {
    case 251: return rnd(15);
    case 252: return rnd(30);
    case 253: return rnd(60);
    case 254: return rnd(120);
    default:  return rnd(250);
  }
}

void Modh() {
  if (Led == 0) Led = 1;
  if (Led >= NUM_LEDS) Led = NUM_LEDS - 1;      // Led = index of last pixel used

  Dans = (Danc == 99) ? random(55) : Danc - 1;
  Rx = pickCh(Rr); Gx = pickCh(Gg); Bx = pickCh(Bb);

  switch (Dans) {
    case 0:  Snak_run_slow(); break;
    case 1:  Snak_return_slow(); break;
    case 2:  Snak_run_slow(); ABORT; Snak_return_slow(); break;
    case 3:  Snak_run_fast(); break;
    case 4:  Snak_return_fast(); break;
    case 5:  Snak_run_fast(); ABORT; Snak_return_fast(); break;
    case 6:  Snak_run_slow_random(); break;
    case 7:  Snak_return_slow_random(); break;
    case 8:  Snak_run_slow_random(); ABORT; Snak_return_slow_random(); break;
    case 9:  Snak_run_fast_random(); break;
    case 10: Snak_return_fast_random(); break;
    case 11: Snak_run_fast_random(); ABORT; Snak_return_fast_random(); break;
    case 12: Snak_runreturn_slow_random(); break;
    case 13: Nnb = 0; Half_snak_out_mid_slow(); break;
    case 14: Nnb = 1; Half_snak_out_mid_slow(); break;
    case 15: Half_snak_out_mid_slow(); Nnb ^= 1; break;   // no recursion: loop() calls us again
    case 16: Half_half_right_slow(); break;
    case 17: Half_half_left_slow(); break;
    case 18: Mx = 0; Star_run_slow(); break;
    case 19: Mx = 1; Star_run_slow(); break;
    case 20: Star_runreturn_slow(); break;
    case 21: Star_run_fast(); break;
    case 22: Star_return_fast(); break;
    case 23: Star_run_fast(); ABORT; Star_return_fast(); break;
    case 24: Snak_growup_slow(); break;
    case 25: Star_playful(); break;
    case 26: Snak_star_playful(); break;
    case 27: Star_fillers(); break;
    case 28: Movable_light(); break;
    case 29: Snak_runreturn_colored(); break;
    case 30: Star_diamonds_blink_slow(); break;
    case 31: Star_diamonds_blink_fast(); break;
    case 32: Star_diamonds_blink_faster(); break;
    case 33: Disco(); break;
    case 34: Fillstripe_color(); break;
    case 35: Breathing(); break;
    case 36: Percolate_run(); break;
    case 37: Percolate_return(); break;
    case 38: Percolate_run(); ABORT; Percolate_return(); break;
    case 39: Snak_1_2_run(); break;
    case 40: Snak_1_2_return(); break;
    case 41: Snak_1_2_run(); ABORT; Snak_1_2_return(); break;
    case 42: Snak_1_2_run_rev(); break;
    case 43: Snak_1_2_return_rev(); break;
    case 44: Snak_1_2_run_rev(); ABORT; Snak_1_2_return_rev(); break;
    case 45: Snak_relax_run(); break;
    case 46: Snak_relax_return(); break;
    case 47: Snak_relax_run(); ABORT; Snak_relax_return(); break;
    case 48: Rainbo = false; Snak_rainbow(); break;
    case 49: Rainbo = true;  Snak_rainbow(); break;
    case 50: Rainbo = false; Snak_rainbow(); ABORT; Rainbo = true; Snak_rainbow(); break;
    case 51: Rainbo = false; Snak_rainbow_lp(); break;
    case 52: Rainbo = true;  Snak_rainbow_lp(); break;
    case 53: Rainbo = false; Snak_rainbow_lp(); ABORT; Rainbo = true; Snak_rainbow_lp(); break;
    case 54: Snak_rainbow_bl(); break;
    default: delay(1); break;               // D56..D98: Bascom does nothing
  }
}

// ============================================================
// Effects (order of Show / Time follows the Bascom source)
// ============================================================
void Snak_run_slow() {
  CRGB c(Rx, Gx, Bx);
  for (word n = 0; n <= Led; n++) { setPx(n, c); Time1(); FastLED.show(); ABORT; }
  for (word n = 0; n <= Led; n++) { setPx(n, CRGB::Black); Time2(); FastLED.show(); ABORT; }
}

void Snak_return_slow() {
  CRGB c(Rx, Gx, Bx);
  for (int n = Led; n >= 0; n--) { setPx(n, c); Time1(); FastLED.show(); ABORT; }
  for (int n = Led; n >= 0; n--) { setPx(n, CRGB::Black); Time2(); FastLED.show(); ABORT; }
}

void Snak_run_fast() {
  CRGB c(Rx, Gx, Bx);
  for (word n = 0; n <= Led; n += 5) {
    for (byte k = 0; k < 5; k++) setPx(n + k, c);
    Time1(); FastLED.show(); ABORT;
  }
  for (word n = 0; n <= Led; n += 5) {
    for (byte k = 0; k < 5; k++) setPx(n + k, CRGB::Black);
    Time2(); FastLED.show(); ABORT;
  }
}

// Bascom sets Nd..Nd+4 here (upwards), unlike Snak_return_fast_random (downwards)
void Snak_return_fast() {
  CRGB c(Rx, Gx, Bx);
  for (int n = Led; n >= 0; n -= 5) {
    for (byte k = 0; k < 5; k++) setPx(n + k, c);
    Time1(); FastLED.show(); ABORT;
  }
  for (int n = Led; n >= 0; n -= 5) {
    for (byte k = 0; k < 5; k++) setPx(n + k, CRGB::Black);
    Time2(); FastLED.show(); ABORT;
  }
}

void Snak_run_slow_random() {
  CRGB c;
  for (word n = 0; n <= Led; n++) {
    if (n % 2 == 0) c = rndCol();
    setPx(n, c); Time1(); FastLED.show(); ABORT;
  }
  for (word k = 1; k < Led; k++) { shR(); Time2(); FastLED.show(); ABORT; }
  FastLED.clear(); FastLED.show();
}

void Snak_return_slow_random() {
  CRGB c;
  for (int n = Led; n >= 0; n--) {
    if (n % 2 == 0) c = rndCol();
    setPx(n, c); Time1(); FastLED.show(); ABORT;
  }
  for (word k = 1; k < Led; k++) { shL(); Time2(); FastLED.show(); ABORT; }
  FastLED.clear(); FastLED.show();
}

void Snak_run_fast_random() {
  CRGB c;
  for (word n = 0; n <= Led; n += 5) {
    if (n % 2 == 0) c = rndCol();
    for (byte k = 0; k < 5; k++) setPx(n + k, c);
    FastLED.show(); Time1(); ABORT;
  }
  for (word n = 0; n <= Led; n += 5) {
    for (byte k = 0; k < 5; k++) setPx(n + k, CRGB::Black);
    FastLED.show(); Time2(); ABORT;
  }
  FastLED.clear(); FastLED.show();
}

void Snak_return_fast_random() {
  CRGB c;
  for (int n = Led; n >= 4; n -= 5) {
    if (n % 2 == 0) c = rndCol();
    for (byte k = 0; k < 5; k++) setPx(n - k, c);
    Time1(); FastLED.show(); ABORT;
  }
  for (int n = Led; n >= 4; n -= 5) {
    for (byte k = 0; k < 5; k++) setPx(n - k, CRGB::Black);
    Time2(); FastLED.show(); ABORT;
  }
  FastLED.clear(); FastLED.show();
}

void Snak_runreturn_slow_random() {
  CRGB c;
  for (int n = Led; n >= 0; n--) {
    if (n % 2 == 0) c = rndCol();
    setPx(n, c); Time1(); FastLED.show(); ABORT;
  }
  for (word k = 1; k < Led; k++) { shR(); Time2(); FastLED.show(); ABORT; }
  for (word n = 0; n <= Led; n++) {
    if (n % 2 == 0) c = rndCol();
    setPx(n, c); Time1(); FastLED.show(); ABORT;
  }
  for (word k = 1; k < Led; k++) { shL(); Time2(); FastLED.show(); ABORT; }
}

void Half_snak_out_mid_slow() {
  CRGB c(Rx, Gx, Bx);
  word Jk = Led / 2;
  if (Nnb == 0) {
    for (word n = 0; n <= Jk; n++) { setPx(n, c); setPx(Led - n, c); Time1(); FastLED.show(); ABORT; }
  } else {
    for (int n = Jk; n >= 0; n--)  { setPx(n, c); setPx(Led - n, c); Time1(); FastLED.show(); ABORT; }
  }
}

void Half_half_right_slow() {
  FastLED.clear();
  CRGB c(Rx, Gx, Bx);
  word Jk = Led / 2;
  for (word n = 0; n <= Jk; n++) { setPx(n, c); setPx(Jk + n, c); Time1(); FastLED.show(); ABORT; }
}

void Half_half_left_slow() {
  FastLED.clear();
  CRGB c(Rx, Gx, Bx);
  word Jk = Led / 2;
  for (int n = Jk; n >= 0; n--) { setPx(n, c); setPx(Jk + n, c); Time1(); FastLED.show(); ABORT; }
}

void Star_run_slow() {
  if (Chc == 0) { Ado = Led; Chc = 1; }
  if (Ado == 1) Chc = 0;
  CRGB c;
  for (word n = 0; n <= Ado; n += 2) {
    c = (Mx == 0) ? CRGB(Rx, Gx, Bx) : rndCol();
    setPx(n, c); setPx(n + 1, c);
    FastLED.show(); Time1(); FastLED.clear(); ABORT;
  }
  c = (Mx == 0) ? CRGB(222, 222, 222) : CRGB(rnd(222), rnd(222), rnd(222));
  word Dv = Ado / 10;
  if (Ado > 10) { for (word n = Ado; n <= Ado + Dv; n++) setPx(n, c); }
  else          { setPx(Ado, c); }
  FastLED.show(); Time2();
  if (Chc == 1) { if (Ado > 10) Ado -= Dv; else Ado--; }
}

void Star_runreturn_slow() {
  CRGB c(Rx, Gx, Bx);
  for (word n = 0; n <= Led; n += 2) {
    setPx(n, c); setPx(n + 1, c); setPx(n + 2, c);
    FastLED.show(); Time1(); FastLED.clear(); ABORT;
  }
  for (int n = Led; n >= 0; n -= 2) {
    setPx(n, c); setPx(n + 1, c); setPx(n + 2, c);
    FastLED.show(); Time2(); FastLED.clear(); ABORT;
  }
}

void Star_run_fast() {
  CRGB c(Rx, Gx, Bx);
  for (word n = 0; n <= Led; n += 5) {
    for (byte k = 0; k < 5; k++) setPx(n + k, c);
    FastLED.show(); Time1(); FastLED.clear(); ABORT;
  }
}

void Star_return_fast() {
  CRGB c(Rx, Gx, Bx);
  for (int n = Led; n >= 5; n -= 5) {
    for (byte k = 0; k < 5; k++) setPx(n - k, c);
    FastLED.show(); Time2(); FastLED.clear(); ABORT;
  }
}

void Snak_growup_slow() {
  CRGB c(Rx, Gx, Bx);
  for (byte n = 1; n <= 4; n++) setPx(n, c);
  FastLED.show();
  if (T < Led) T += 5;
  if (T >= Led) T = 1;
  for (word k = 1; k <= Led - T; k++) { shR(); Time1(); FastLED.show(); ABORT; }
  for (int n = Led - 1; n >= (int)Led - 3 && n >= 0; n--) { setPx(n, c); FastLED.show(); }
  for (word k = 1; k <= Led - T; k++) { shL(); Time2(); FastLED.show(); ABORT; }
}

void Star_playful() {
  CRGB c(Rx, Gx, Bx);
  for (byte n = 1; n <= 4; n++) setPx(n, c);
  FastLED.show();
  word Yfd = rnd(5);
  if (T < Led) T += Yfd;
  if (T >= Led) T = 1;
  for (word k = 1; k <= Led - T; k++) { shR(); Time1(); FastLED.show(); ABORT; }
  word Xfd = rnd(5);
  for (int n = Led - 1; n >= (int)Led - (int)Xfd; n -= 5) { setPx(n, c); FastLED.show(); }
  word Vfd = rnd(T);
  for (word k = 1; k <= Led - Vfd; k++) { shL(); Time2(); FastLED.show(); ABORT; }
}

// NOTE: forces Led>=22 like Bascom; on a 14-pixel strip part of the motion is off-strip.
void Snak_star_playful() {
  if (Led < 22) Led = 22;
  CRGB c(Rx, Gx, Bx);
  int  L = Led;
  byte Pp = rnd(10); if (!Pp) Pp = 1;
  for (byte n = 1; n <= Pp; n++) setPx(n, c);
  FastLED.show();

  Pp = rnd(10); if (!Pp) Pp = 1;
  if (T < Led) T++;
  if (T >= Led) T = 1;
  T = rnd(T); T += Pp; if (T == 0) T = 1;

  for (int n = 1; n <= L - (int)T; n += Pp) { shR(); FastLED.show(); Time1(); ABORT; }
  for (int n = L - 1; n >= L - (int)T; n -= 3) { if (n > 0) { setPx(n, c); FastLED.show(); } }

  Pp = rnd(10); if (!Pp) Pp = 3;
  for (int n = 1; n <= L - (int)T; n += Pp) { shL(); FastLED.show(); Time2(); ABORT; }

  Pp = rnd(10); if (!Pp) Pp = 1;
  if (T < Led) T++;
  if (T >= Led) T = 1;
  T = rnd(T); T += Pp; if (T == 0) T = 1;
  int Md = rnd(Led); if (Md < (int)T) Md = T + 1;
  for (int n = 1; n <= Md - (int)T; n += Pp) {
    int Jd = L - Pp;
    if (n > 0) setPx(n, c);
    if (Jd > 0) setPx(Jd, c);
    FastLED.show(); Time1(); ABORT;
  }
  // Bascom: "For Nd = Led-t To Led-1 Step -3" (start < end with negative step) - ported as ascending
  for (int n = L - (int)T; n <= L - 1; n += 3) { if (n > 0) setPx(n, c); }
  FastLED.show();

  Pp = rnd(10); if (!Pp) Pp = 3;
  for (int n = L - (int)T; n >= 1; n -= Pp) { shR(); FastLED.show(); Time2(); ABORT; }
  for (byte n = 1; n <= Pp; n++) setPx(n, c);
  FastLED.show();
}

void Star_fillers() {
  if (Ado < Led) Ado++;
  if (Ado == Led) Ado = 1;
  CRGB c(Rx, Gx, Bx);
  for (word n = 1; n <= Ado; n += 4) { setPx(n, c); FastLED.show(); }
  // Bascom: "For N = 1 To Led-ado Step -4" - ported as ascending (see notes)
  for (int k = 1; k <= (int)Led - (int)Ado; k += 4) { shR(); Time1(); FastLED.show(); ABORT; }
  for (int n = Led - 1; n >= (int)Led - (int)Ado; n -= 5) { setPx(n, c); FastLED.show(); }
  for (word k = 1; k <= Led - Ado; k++) { shL(); Time2(); FastLED.show(); ABORT; }
}

void Movable_light() {
  CRGB white(222, 222, 222);
  CRGB col(rnd(Rx), rnd(Gx), rnd(Bx));
  for (word n = 0; n <= Led; n++) {
    setPx(n + 3, white); setPx(n + 2, white); setPx(n + 1, white);
    setPx(n, col);
    Time1(); FastLED.show(); ABORT;
  }
  col = CRGB(rnd(Rx), rnd(Gx), rnd(Bx));
  for (int n = Led; n >= 0; n--) {
    setPx(n, white); setPx(n + 1, white); setPx(n + 2, white);
    setPx(n + 3, col);
    Time2(); FastLED.show(); ABORT;
  }
}

void Snak_runreturn_colored() {
  CRGB c = rndCol();
  for (word n = 0; n <= Led; n++) { setPx(n, c); Time1(); FastLED.show(); ABORT; }
  c = rndCol();
  for (int n = Led; n >= 0; n--) { setPx(n, c); Time2(); FastLED.show(); ABORT; }
}

void Star_diamonds_blink_slow() {
  CRGB c = rndCol();
  setPx(rnd(Led), c);
  Time1(); ABORT;
  setPx(rnd(Led), c);
  Time2();
  FastLED.show();
}

void Star_diamonds_blink_fast() {
  CRGB c(Rx, Gx, Bx);
  Time1(); Time2(); ABORT;
  setPx(rnd(Led), c);
  FastLED.show(); FastLED.clear();
}

void Star_diamonds_blink_faster() {
  CRGB c(Rx, Gx, Bx);
  Time1(); Time2(); ABORT;
  for (byte k = 0; k < 8; k++) setPx(rnd(Led), c);
  FastLED.show(); FastLED.clear();
}

void Disco() {
  CRGB c(Rx, Gx, Bx);
  for (word i = 0; i <= Led; i++) { Time1(); ABORT; setPx(rnd(Led), c); }
  FastLED.show();
  for (word i = 0; i <= Led; i++) { Time2(); ABORT; setPx(rnd(Led), CRGB::Black); }
  FastLED.show();
}

void Fillstripe_color() {                    // Rb_fillstripe = whole strip
  fill_solid(leds, NUM_LEDS, rndCol());
  FastLED.show();
  for (word i = 0; i <= 200; i++) { Time1(); ABORT; Time2(); ABORT; }
}

void Breathing() {
  byte A1 = Rx, A2 = Gx, A3 = Bx;
  for (word l = 0; l <= 255; l++) {
    if (A1 == 1 || A2 == 1 || A3 == 1) break;
    if (A1 > 1) A1--; if (A2 > 1) A2--; if (A3 > 1) A3--;
    fill_solid(leds, NUM_LEDS, CRGB(A1, A2, A3));
    FastLED.show(); Time1(); ABORT;
  }
  for (word l = 0; l <= 255; l++) {
    if (A1 == Rx || A2 == Gx || A3 == Bx) break;
    if (A1 < Rx) A1++; if (A2 < Gx) A2++; if (A3 < Bx) A3++;
    fill_solid(leds, NUM_LEDS, CRGB(A1, A2, A3));
    FastLED.show(); Time2(); ABORT;
  }
}

void Percolate_run() {
  CRGB c(Rx, Gx, Bx);
  word Jk = Led / 2;
  for (word n = 0; n <= Jk; n++) {
    setPx(n, c); setPx(Led - n, c);
    Time1(); FastLED.show(); FastLED.clear(); ABORT;
  }
}

void Percolate_return() {
  CRGB c(Rx, Gx, Bx);
  word Jk = Led / 2;
  for (int n = Jk; n >= 0; n--) {
    setPx(n, c); setPx(Led - n, c);
    delay(1); pollSerial(); FastLED.show(); FastLED.clear(); ABORT;
  }
}

static void clampMin10(CRGB &c) { if (c.r < 10) c.r = 10; if (c.g < 10) c.g = 10; if (c.b < 10) c.b = 10; }

void Snak_1_2_run() {
  FastLED.clear();
  CRGB c(Rx, Gx, Bx);
  for (word n = 0; n <= Led; n += 2) { setPx(n, c); FastLED.show(); Time1(); ABORT; }
  c = rndCol(); clampMin10(c);
  for (word n = 1; n <= Led; n += 2) { setPx(n, c); FastLED.show(); Time2(); ABORT; }
}

void Snak_1_2_return() {
  word Dv = Led + 1;
  FastLED.clear();
  CRGB c(Rx, Gx, Bx);
  for (int n = Led; n >= 0; n -= 2) { setPx(n, c); FastLED.show(); Time1(); ABORT; }
  c = rndCol(); clampMin10(c);
  for (int n = Dv; n >= 0; n -= 2) { setPx(n, c); FastLED.show(); Time2(); ABORT; }
}

void Snak_1_2_run_rev() {
  FastLED.clear();
  CRGB c(Rx, Gx, Bx);
  for (word n = 0; n <= Led; n += 2) { setPx(n, c); FastLED.show(); Time1(); ABORT; }
  c = rndCol(); clampMin10(c);
  for (int n = Led; n >= 0; n -= 2) { setPx(n, c); FastLED.show(); Time2(); ABORT; }
}

void Snak_1_2_return_rev() {
  FastLED.clear();
  CRGB c(Rx, Gx, Bx);
  for (int n = Led; n >= 0; n -= 2) { setPx(n, c); FastLED.show(); Time1(); ABORT; }
  c = rndCol(); clampMin10(c);
  for (word n = 0; n <= Led; n += 2) { setPx(n, c); FastLED.show(); Time2(); ABORT; }
}

void Snak_relax_run() {
  FastLED.clear();
  byte Xf = (Rx > 10) ? Rx / 10 : 0, Yf = (Gx > 10) ? Gx / 10 : 0, Vf = (Bx > 10) ? Bx / 10 : 0;
  byte r = 0, g = 0, b = 0;
  for (int n = Led; n >= (int)Led - 10; n--) { r += Xf; g += Yf; b += Vf; setPx(n, CRGB(r, g, b)); }
  for (int n = (int)Led - 11; n >= (int)Led - 20; n--) { r -= Xf; g -= Yf; b -= Vf; setPx(n, CRGB(r, g, b)); }
  FastLED.show();
  for (word k = 0; k <= Led; k++) { shL(); Time1(); FastLED.show(); ABORT; }
}

void Snak_relax_return() {
  FastLED.clear();
  byte Xf = (Rx > 10) ? Rx / 10 : 0, Yf = (Gx > 10) ? Gx / 10 : 0, Vf = (Bx > 10) ? Bx / 10 : 0;
  byte r = 0, g = 0, b = 0;
  for (word n = 0; n <= 10; n++)  { r += Xf; g += Yf; b += Vf; setPx(n, CRGB(r, g, b)); }
  for (word n = 11; n <= 21; n++) { r -= Xf; g -= Yf; b -= Vf; setPx(n, CRGB(r, g, b)); }
  FastLED.show();
  for (word k = 0; k <= Led; k++) { shR(); Time1(); FastLED.show(); ABORT; }
}

static void rainbowRotate() {                 // Bascom: Rainbo=0 -> Rb_rotateright, 1 -> Rb_rotateleft
  for (word k = 0; k <= Led; k++) {
    if (!Rainbo) rotR(); else rotL();
    Time1(); FastLED.show(); ABORT;
  }
}

void Snak_rainbow() {
  for (byte I = 0; I <= 23; I++) {
    word Bf = 20 * I;
    if (Bf >= NUM_LEDS) break;
    CRGB c = rndCol();
    byte Xf = (c.r > 10) ? c.r / 10 : 0, Yf = (c.g > 10) ? c.g / 10 : 0, Vf = (c.b > 10) ? c.b / 10 : 0;
    byte r = 0, g = 0, b = 0;
    word Jk = 10 + Bf, Lk = 20 + Bf;
    for (word n = Bf; n <= Jk; n++) { r += Xf; g += Yf; b += Vf; setPx(n, CRGB(r, g, b)); }
    for (word n = Jk; n <= Lk; n++) { r -= Xf; g -= Yf; b -= Vf; setPx(n, CRGB(r, g, b)); }
  }
  FastLED.show();
  rainbowRotate();
}

void Snak_rainbow_lp() {
  for (byte I = 0; I <= 23; I++) {
    word Bf = 20 * I;
    if (Bf >= NUM_LEDS) break;
    for (word n = Bf; n <= 20 + Bf; n++) setPx(n, rndCol());
  }
  FastLED.show();
  rainbowRotate();
}

void Snak_rainbow_bl() {
  FastLED.clear();
  for (byte I = 0; I <= 23; I++) {
    word Bf = 20 * I;
    if (Bf >= NUM_LEDS) break;
    for (word n = Bf; n <= 20 + Bf; n++) setPx(n, rndCol());
  }
  FastLED.show();
  Time1(); ABORT;
  rotL();
  FastLED.show();
  FastLED.clear();
  Time2();
}
