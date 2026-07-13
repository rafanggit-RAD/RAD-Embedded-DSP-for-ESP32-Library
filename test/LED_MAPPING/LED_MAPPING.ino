// ============================================================================
// LED BRIGHTNESS CURVE MAPPING TOOL v3
// 3 parameter: dB level, kurva N, dB Floor
// Encoder klik = cycle mode: dB -> N -> FLOOR -> dB ...
// Encoder putar = ubah parameter aktif
// Kedua LED (GPIO 32 & 33) menyala bersamaan
// ============================================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// --- OLED SSD1306 (I2C: SDA=14, SCL=12) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Pin Definitions ---
#define LED_AUDIO_PIN 32
#define LED_BT_PIN    33
#define ENC_CLK       25
#define ENC_DT        26
#define ENC_SW        27

// --- Encoder State ---
volatile int encoderPos = 0;
volatile bool encoderChanged = false;

void IRAM_ATTR encoderISR() {
    static uint8_t old_AB = 0;
    old_AB <<= 2;
    old_AB |= (digitalRead(ENC_CLK) << 1) | digitalRead(ENC_DT);
    old_AB &= 0x0F;
    static const int8_t enc_states[] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
    int8_t change = enc_states[old_AB];
    if (change != 0) {
        encoderPos += change;
        encoderChanged = true;
    }
}

// --- Global Parameters ---
float curveN  = 2.5f;     // Eksponen gamma (0.5 .. 10.0)
float simDb   = -15.0f;   // Simulasi level dB (floor..0)
float dbFloor = -50.0f;   // dB Floor: LED mati di bawah ini (-60..-10)

// Edit mode: 0=dB, 1=N, 2=Floor
int editMode = 0;
const char* modeNames[] = {"dB", "N", "FLOOR"};
int lastPos = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("\n[LED CURVE MAPPING v3]");

    Wire.begin(14, 12);
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("[OLED] GAGAL!");
        while (1);
    }
    display.clearDisplay();
    display.display();

    pinMode(ENC_CLK, INPUT_PULLUP);
    pinMode(ENC_DT,  INPUT_PULLUP);
    pinMode(ENC_SW,  INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENC_CLK), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_DT),  encoderISR, CHANGE);

    pinMode(LED_AUDIO_PIN, OUTPUT);
    pinMode(LED_BT_PIN,    OUTPUT);
    analogWrite(LED_AUDIO_PIN, 0);
    analogWrite(LED_BT_PIN,    0);

    lastPos = encoderPos;
    Serial.println("[READY] Klik = cycle mode (dB/N/FLOOR), Putar = ubah nilai");
}

void loop() {
    // --- 1. Baca Encoder ---
    if (encoderChanged) {
        encoderChanged = false;
        int currentPos = encoderPos;
        int steps = currentPos - lastPos;
        lastPos = currentPos;

        if (editMode == 0) {
            // Edit dB: step 0.5
            simDb += steps * 0.5f;
            if (simDb < -60.0f) simDb = -60.0f;
            if (simDb > 0.0f) simDb = 0.0f;
        } else if (editMode == 1) {
            // Edit N: step 0.1
            curveN += steps * 0.1f;
            if (curveN < 0.5f) curveN = 0.5f;
            if (curveN > 10.0f) curveN = 10.0f;
        } else {
            // Edit Floor: step 1 dB
            dbFloor += steps * 1.0f;
            if (dbFloor < -60.0f) dbFloor = -60.0f;
            if (dbFloor > -5.0f)  dbFloor = -5.0f;
        }
    }

    // --- 2. Tombol SW: cycle mode ---
    static bool lastBtnState = HIGH;
    static unsigned long lastDebounce = 0;
    bool btnState = digitalRead(ENC_SW);
    if (btnState == LOW && lastBtnState == HIGH && (millis() - lastDebounce > 200)) {
        editMode = (editMode + 1) % 3;
        lastDebounce = millis();
    }
    lastBtnState = btnState;

    // --- 3. Hitung mapping ---
    // Normalized: 0.0 saat dB <= floor, 1.0 saat dB = 0
    float range = 0.0f - dbFloor;  // misal: 0 - (-30) = 30
    float linear = (simDb - dbFloor) / range;
    if (linear < 0.0f) linear = 0.0f;
    if (linear > 1.0f) linear = 1.0f;

    // Gamma curve
    float curved = powf(linear, curveN);

    int pwmLinear = (int)(linear * 255.0f);
    int pwmCurved = (int)(curved * 255.0f);
    if (pwmLinear > 255) pwmLinear = 255;
    if (pwmCurved > 255) pwmCurved = 255;

    float voltCurved = (float)pwmCurved / 255.0f * 3.3f;

    // --- 4. Tulis ke KEDUA LED ---
    analogWrite(LED_BT_PIN,    pwmCurved);
    analogWrite(LED_AUDIO_PIN, pwmCurved);

    // --- 5. Render OLED ---
    display.clearDisplay();

    // Header bar
    display.fillRoundRect(0, 0, 128, 13, 3, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK);
    char hdrBuf[28];
    snprintf(hdrBuf, sizeof(hdrBuf), "[%s] N=%.1f F=%ddB",
             modeNames[editMode], curveN, (int)dbFloor);
    int hw = strlen(hdrBuf) * 6;
    display.setCursor(64 - hw / 2, 3);
    display.print(hdrBuf);

    // dB value - besar di tengah
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    char dbBuf[12];
    snprintf(dbBuf, sizeof(dbBuf), "%.1fdB", simDb);
    int dw = strlen(dbBuf) * 12;
    display.setCursor(64 - dw / 2, 16);
    display.print(dbBuf);

    // Info baris: Linear vs Curved PWM
    display.setTextSize(1);
    char linBuf[20];
    snprintf(linBuf, sizeof(linBuf), "LIN:%d", pwmLinear);
    display.setCursor(2, 35);
    display.print(linBuf);

    char crvBuf[20];
    snprintf(crvBuf, sizeof(crvBuf), "CRV:%d", pwmCurved);
    int cw = strlen(crvBuf) * 6;
    display.setCursor(126 - cw, 35);
    display.print(crvBuf);

    // Volt & Persen
    char voltBuf[16];
    snprintf(voltBuf, sizeof(voltBuf), "%.3fV", voltCurved);
    display.setCursor(2, 46);
    display.print(voltBuf);

    char pctBuf[12];
    snprintf(pctBuf, sizeof(pctBuf), "%.1f%%", curved * 100.0f);
    int pw = strlen(pctBuf) * 6;
    display.setCursor(126 - pw, 46);
    display.print(pctBuf);

    // Progress bar: filled = curved output
    int barY = 56;
    int barH = 7;
    int barW = 124;
    int fillC = (int)(curved * barW);
    display.drawRoundRect(2, barY, barW, barH, 2, SSD1306_WHITE);
    if (fillC > 0) {
        display.fillRoundRect(2, barY, fillC, barH, 2, SSD1306_WHITE);
    }

    // Linear marker
    int linX = 2 + (int)(linear * (barW - 1));
    display.drawLine(linX, barY - 2, linX, barY - 1, SSD1306_WHITE);

    display.display();

    // Serial log
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 300) {
        Serial.printf("N=%.1f  Floor=%d  dB=%.1f  LIN=%d  CRV=%d  V=%.3f\n",
            curveN, (int)dbFloor, simDb, pwmLinear, pwmCurved, voltCurved);
        lastPrint = millis();
    }

    delay(16);
}
