// nyro_bhai_combined.ino
// Arduino Mega + TFT (Adafruit GFX + MCUFRIEND_kbv)
//
// MODEL 1: NYRO — exactly your original face (SKYBLUE + arc smile)
// MODEL 2: BHAI — NEW Handsome Boy (Clean hair, modern look, smooth mouth)
//
// Serial commands from Raspberry Pi:
//   START_SPEECH  → talking animation shuru
//   END_SPEECH    → mouth band, neutral
//   FACE_NYRO     → Nyro face switch
//   FACE_BHAI     → Bhai face switch
//   BUZZER_ON     → gas alert buzzer
//   BUZZER_OFF    → buzzer band

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <math.h>

MCUFRIEND_kbv tft;

// ═══════════════════════════════════════════════════════════
//  COLORS
// ═══════════════════════════════════════════════════════════

// — Nyro original colors —
#define BLACK       0x0000
#define WHITE       0xFFFF
#define SKYBLUE     0x07FF
#define BLUE        0x001F
#define DEEPBLUE    0x0010
#define CYAN        0x07FE
#define PINK        0xF81F

// — NEW Handsome Bhai Face colors —
#define BG_TOP      0x551B   // Soft Sky Blue background
#define BG_BOT      0x2C15   // Deeper Sky Blue
#define SKIN_BASE   0xFEA0   // Warm, clean human skin tone
#define SKIN_SHADOW 0xE54F   // Subtle shading for face depth
#define HAIR_COLOR  0x1000   // Very dark smooth brown/black for clean hair
#define EYE_IRIS    0x1AEB   // Deep handsome blue for eyes
#define MOUTH_DARK  0x4800   // Dark maroon for inside of mouth
#define TONGUE      0xF32E   // Pink tongue color

// ═══════════════════════════════════════════════════════════
//  FACE CENTER + MODE
// ═══════════════════════════════════════════════════════════

int cx = 160, cy = 120;
#define MODE_NYRO 0
#define MODE_BHAI 1
int faceMode = MODE_NYRO;

// ═══════════════════════════════════════════════════════════
//  EYE STATE (shared)
// ═══════════════════════════════════════════════════════════

int  eyeOffset   = 0;
bool eyesClosed  = false;
unsigned long lastBlink   = 0;
unsigned long lastEyeMove = 0;

// ═══════════════════════════════════════════════════════════
//  MOUTH STATE — smooth easing (shared engine)
// ═══════════════════════════════════════════════════════════

bool  mouthTalking  = false;
float mouthOpen     = 0.0f;
float mouthTarget   = 0.0f;
float mouthEase     = 0.35f;

const int SYLLABLE_COUNT = 8;
// Adjusted slightly for smooth natural movement for both faces
int syllableOpen[SYLLABLE_COUNT] = { 4, 10, 12, 8, 5, 2, 9, 11 };
int syllableDur[SYLLABLE_COUNT]  = { 90, 110, 140, 120, 100, 80, 110, 140 };
int syllableIndex  = 0;
unsigned long syllableStart = 0;

// Nyro mouth area (original)
int mouthAreaX = 160 - 80;
int mouthAreaY = 120 + 55 - 50;
int mouthAreaW = 160;
int mouthAreaH = 100;

// ═══════════════════════════════════════════════════════════
//  BHAI EXTRAS
// ═══════════════════════════════════════════════════════════

unsigned long bhaiWinkTimer = 0;

// ═══════════════════════════════════════════════════════════
//  BUZZER
// ═══════════════════════════════════════════════════════════

#define BUZZER_PIN  8
bool          buzzerOn  = false;
unsigned long lastBeep  = 0;

// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  uint16_t ID = tft.readID();
  if (ID == 0x0 || ID == 0xFFFF) ID = 0x9486;
  tft.begin(ID);
  tft.setRotation(1);

  drawNyroBase();   // boot on Nyro face
}

// ═══════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════

void loop() {

  // ── Serial commands ──────────────────────────────────────
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if      (cmd == "START_SPEECH") startSpeech();
    else if (cmd == "END_SPEECH")   endSpeech();
    else if (cmd == "FACE_NYRO")    switchFace(MODE_NYRO);
    else if (cmd == "FACE_BHAI")    switchFace(MODE_BHAI);
    else if (cmd == "BUZZER_ON")  { buzzerOn = true;  tone(BUZZER_PIN, 880); }
    else if (cmd == "BUZZER_OFF") { buzzerOn = false; noTone(BUZZER_PIN); }
  }

  unsigned long now = millis();

  // ── Eye movement ──────────────────────────────────────────
  if (now - lastEyeMove > 1200) {
    eyeOffset = random(-4, 5);
    if (faceMode == MODE_NYRO) drawNyroEyes(eyeOffset);
    else                       drawBhaiEyes(eyeOffset, false);
    lastEyeMove = now;
  }

  // ── Blink ─────────────────────────────────────────────────
  if (now - lastBlink > (unsigned long)random(2500, 5500)) {
    blinkEyes();
    lastBlink = now;
  }

  // ── Bhai wink (random) ───────────────────────────────────
  if (faceMode == MODE_BHAI && !mouthTalking &&
      now - bhaiWinkTimer > (unsigned long)random(8000, 15000)) {
    drawBhaiWink();
    bhaiWinkTimer = now;
  }

  // ── Syllable engine ───────────────────────────────────────
  if (mouthTalking) {
    if (now - syllableStart >= (unsigned long)syllableDur[syllableIndex]) {
      syllableIndex = (syllableIndex + 1) % SYLLABLE_COUNT;
      syllableStart  = now;
      mouthTarget    = syllableOpen[syllableIndex];
    }
  }

  // ── Smooth easing ─────────────────────────────────────────
  float delta = mouthTarget - mouthOpen;
  mouthOpen  += delta * mouthEase;

  static int lastDrawnOpen = -999;
  int openInt = (int)round(mouthOpen);
  if (openInt != lastDrawnOpen) {
    if (faceMode == MODE_NYRO) drawSmile(openInt);
    else                       drawBhaiMouth(openInt);
    lastDrawnOpen = openInt;
  }

  // ── Buzzer beep ───────────────────────────────────────────
  if (buzzerOn && now - lastBeep > 600) {
    tone(BUZZER_PIN, 880, 200);
    lastBeep = now;
  }
}

// ═══════════════════════════════════════════════════════════
//  SPEECH CONTROL
// ═══════════════════════════════════════════════════════════

void startSpeech() {
  mouthTalking  = true;
  syllableIndex = 0;
  syllableStart = millis();
  mouthTarget   = syllableOpen[0];
}

void endSpeech() {
  mouthTalking = false;
  mouthTarget  = 0;
  mouthOpen    = 0;
  if (faceMode == MODE_NYRO) drawSmile(0);
  else                       drawBhaiMouth(0);
}

// ═══════════════════════════════════════════════════════════
//  FACE SWITCH
// ═══════════════════════════════════════════════════════════

void switchFace(int newMode) {
  if (faceMode == newMode) return;
  faceMode = newMode;
  endSpeech();
  eyeOffset = 0;
  if (faceMode == MODE_NYRO) drawNyroBase();
  else                       drawBhaiBase();
}

// ═══════════════════════════════════════════════════════════
//  ███████  MODEL 1: NYRO FACE (ORIGINAL)
// ═══════════════════════════════════════════════════════════

void drawNyroBase() {
  for (int y = 0; y < 240; y++) {
    uint16_t shade = (y < 100) ? CYAN : SKYBLUE;
    tft.drawFastHLine(0, y, 320, shade);
  }
  drawNyroEyes(0);
  drawSmile(0);
  tft.fillCircle(cx - 90, cy + 40, 12, PINK);
  tft.fillCircle(cx + 90, cy + 40, 12, PINK);
}

void drawNyroEyes(int offset) {
  int eyeY = cy - 30;
  drawNyroEye(cx - 60, eyeY, offset);
  drawNyroEye(cx + 60, eyeY, offset);
}

void drawNyroEye(int x, int y, int offset) {
  if (eyesClosed) {
    tft.fillRect(x - 32, y - 32, 64, 64, SKYBLUE);
    tft.drawFastHLine(x - 25, y, 50, DEEPBLUE);
    return;
  }
  tft.fillCircle(x, y, 32, WHITE);
  tft.fillCircle(x + offset, y, 26, SKYBLUE);
  tft.fillCircle(x + offset, y, 14, DEEPBLUE);
  tft.fillCircle(x + offset - 6, y - 6, 4, WHITE);
  tft.fillCircle(x + offset - 4, y - 4, 2, WHITE);
}

void drawSmile(int open) {
  int mx = cx, my = cy + 55;
  int r = 40;
  int thickness = 3;

  tft.fillRect(mouthAreaX, mouthAreaY, mouthAreaW, mouthAreaH, SKYBLUE);

  for (int i = 0; i < thickness; i++) {
    for (int angle = 20; angle <= 160; angle += 2) {
      int x1 = mx + (r+i) * cos(radians(angle));
      int y1 = my + (r+i) * sin(radians(angle)) - open;
      int x2 = mx + (r+i) * cos(radians(angle+2));
      int y2 = my + (r+i) * sin(radians(angle+2)) - open;
      tft.drawLine(x1, y1, x2, y2, DEEPBLUE);
    }
  }

  for (int angle = 30; angle <= 150; angle += 3) {
    int x1 = mx + (r - 4) * cos(radians(angle));
    int y1 = my + (r - 4) * sin(radians(angle)) - open;
    int x2 = mx + (r - 4) * cos(radians(angle + 3));
    int y2 = my + (r - 4) * sin(radians(angle + 3)) - open;
    tft.drawLine(x1, y1, x2, y2, BLUE);
  }
}

void blinkEyes() {
  eyesClosed = true;
  if (faceMode == MODE_NYRO) drawNyroEyes(eyeOffset);
  else                       drawBhaiEyes(eyeOffset, true);
  delay(120);
  eyesClosed = false;
  if (faceMode == MODE_NYRO) drawNyroEyes(eyeOffset);
  else                       drawBhaiEyes(eyeOffset, false);
}

// ═══════════════════════════════════════════════════════════
//  ███████  MODEL 2: NEW HANDSOME BHAI FACE
// ═══════════════════════════════════════════════════════════

void drawBhaiBackground() {
  for(int y = 0; y < 240; y++) {
    uint16_t c = (y < 120) ? BG_TOP : BG_BOT;
    tft.drawFastHLine(0, y, 320, c);
  }
}

void drawBhaiBase() {
  drawBhaiBackground();
  drawBhaiFaceShape();
  drawBhaiHair();
  drawBhaiEyes(0, false);
  drawBhaiMouth(0);
  drawBhaiName();
}

void drawBhaiFaceShape() {
  // Ears
  tft.fillCircle(cx - 75, cy + 5, 16, SKIN_SHADOW);
  tft.fillCircle(cx + 75, cy + 5, 16, SKIN_SHADOW);
  tft.fillCircle(cx - 75, cy + 5, 12, SKIN_BASE);
  tft.fillCircle(cx + 75, cy + 5, 12, SKIN_BASE);

  // Face Oval
  tft.fillEllipse(cx, cy, 80, 95, SKIN_SHADOW); 
  tft.fillEllipse(cx, cy, 76, 91, SKIN_BASE);   
}

void drawBhaiHair() {
  // Top main volume
  tft.fillEllipse(cx, cy - 85, 82, 35, HAIR_COLOR);

  // Left side smooth wave
  tft.fillEllipse(cx - 40, cy - 65, 45, 25, HAIR_COLOR);

  // Right side smooth wave
  tft.fillEllipse(cx + 40, cy - 70, 40, 20, HAIR_COLOR);

  // Stylish front drop
  tft.fillTriangle(cx - 60, cy - 50, cx - 20, cy - 60, cx - 35, cy - 30, HAIR_COLOR);

  // Clean sideburns
  tft.fillRect(cx - 80, cy - 30, 12, 30, HAIR_COLOR);
  tft.fillTriangle(cx - 80, cy, cx - 68, cy, cx - 80, cy + 15, HAIR_COLOR);
  
  tft.fillRect(cx + 68, cy - 30, 12, 30, HAIR_COLOR);
  tft.fillTriangle(cx + 80, cy, cx + 68, cy, cx + 80, cy + 15, HAIR_COLOR);
}

void drawBhaiEyes(int offset, bool closed) {
  int leyX = cx - 35, reyX = cx + 35, eyY = cy - 20;

  // Clear eye area within face bounds
  tft.fillRect(leyX - 25, eyY - 25, 50, 50, SKIN_BASE);
  tft.fillRect(reyX - 25, eyY - 25, 50, 50, SKIN_BASE);

  // Eyebrows
  tft.fillRoundRect(leyX - 20, eyY - 30, 35, 6, 2, HAIR_COLOR);
  tft.fillRoundRect(reyX - 15, eyY - 30, 35, 6, 2, HAIR_COLOR);

  if (closed) {
    for (int i = -16; i <= 16; i++) {
      int dy = (i * i) / 20; 
      tft.drawPixel(leyX + i, eyY + dy, HAIR_COLOR);
      tft.drawPixel(leyX + i, eyY + dy + 1, HAIR_COLOR);
      tft.drawPixel(reyX + i, eyY + dy, HAIR_COLOR);
      tft.drawPixel(reyX + i, eyY + dy + 1, HAIR_COLOR);
    }
    return;
  }

  // Sclera
  tft.fillCircle(leyX, eyY, 24, WHITE);
  tft.fillCircle(reyX, eyY, 24, WHITE);
  
  // Iris
  tft.fillCircle(leyX + offset, eyY, 17, EYE_IRIS);
  tft.fillCircle(reyX + offset, eyY, 17, EYE_IRIS);
  
  // Pupil
  tft.fillCircle(leyX + offset, eyY, 10, BLACK);
  tft.fillCircle(reyX + offset, eyY, 10, BLACK);
  
  // Shines
  tft.fillCircle(leyX + offset - 5, eyY - 5, 4, WHITE);
  tft.fillCircle(leyX + offset - 2, eyY - 2, 2, WHITE);
  
  tft.fillCircle(reyX + offset - 5, eyY - 5, 4, WHITE);
  tft.fillCircle(reyX + offset - 2, eyY - 2, 2, WHITE);
}

void drawBhaiMouth(int open) {
  int my = cy + 45;
  
  // Clear mouth area securely
  tft.fillRect(cx - 30, my - 10, 60, 45, SKIN_BASE);

  if (open < 3) {
    // Handsome smile
    for (int i = -20; i <= 20; i++) {
      int dy = (i * i) / 30; 
      tft.drawPixel(cx + i, my + dy, SKIN_SHADOW);
      tft.drawPixel(cx + i, my + dy + 1, MOUTH_DARK);
    }
    // Dimples
    tft.drawLine(cx - 22, my + 10, cx - 20, my + 14, SKIN_SHADOW);
    tft.drawLine(cx + 22, my + 10, cx + 20, my + 14, SKIN_SHADOW);
  } else {
    // Open mouth talking with teeth & tongue
    int h = min(open * 2, 25); 
    tft.fillEllipse(cx, my + (h/2) - 2, 20, h, MOUTH_DARK);
    
    // Teeth
    if (h > 8) {
       tft.fillEllipse(cx, my + 2, 16, 4, WHITE);
       tft.fillRect(cx - 16, my - 1, 32, 4, WHITE);
    }
    
    // Tongue
    if (h > 15) {
       tft.fillEllipse(cx, my + h - 5, 12, 6, TONGUE);
    }
  }
}

void drawBhaiWink() {
  int reyX = cx + 35, eyY = cy - 20;
  
  tft.fillRect(reyX - 25, eyY - 25, 50, 50, SKIN_BASE);
  tft.fillRoundRect(reyX - 15, eyY - 30, 35, 6, 2, HAIR_COLOR);

  for (int i = -16; i <= 16; i++) {
    int dy = -(i * i) / 25; 
    tft.drawPixel(reyX + i, eyY + 5 + dy, HAIR_COLOR);
    tft.drawPixel(reyX + i, eyY + 6 + dy, HAIR_COLOR);
  }
  
  delay(400);
  drawBhaiEyes(eyeOffset, false);
}

void drawBhaiName() {
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(120, 215);
  tft.print("B H A I");
}
