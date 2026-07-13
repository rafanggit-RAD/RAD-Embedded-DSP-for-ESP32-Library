#include <RadDSP.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

// --- System Info Configuration ---
const char* SYS_SERIAL_NUMBER = "RAD-AMP-01-0001";
const char* SYS_PRODUCTION_DATE = "2026-07-13";
const char* SYS_FIRMWARE_VERSION = "1.0.5";

// --- Brightness Anti-Log Curve Configuration ---
// 1.0f = Linear, 2.0f = Quadratic (Standard Anti-Log), 3.0f = Cubic (Steeper Curve)
const float BRIGHTNESS_CURVE_EXP = 5.0f;
const int OLED_MIN_CONTRAST = 0;      // Batas minimum kontras OLED (0 s.d 255)
const int LED_MIN_PWM = 65;          // Batas minimum PWM LED (0 s.d 255)

// Instansiasi Driver I2S, Bluetooth, Controller, & DualCoreWorker
RadDSP::I2S i2s0;
RadDSP::Bluetooth bt;
RadDSP::Controller dspControl;
RadDSP::DualCoreWorker worker;

// --- Layar OLED SSD1306 (I2C kustom: SDA=14, SCL=12) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================================
// 1. INSTANSIASI MODUL DSP
// ============================================================================

// --- Parametric EQ 4-Band Stereo untuk ADC (PCM1808) ---
RadDSP::Biquad eqAdc_L1, eqAdc_L2, eqAdc_L3, eqAdc_L4;
RadDSP::Biquad eqAdc_R1, eqAdc_R2, eqAdc_R3, eqAdc_R4;

// --- Parametric EQ 4-Band Stereo untuk Bluetooth (BT) ---
RadDSP::Biquad eqBt_L1, eqBt_L2, eqBt_L3, eqBt_L4;
RadDSP::Biquad eqBt_R1, eqBt_R2, eqBt_R3, eqBt_R4;

// --- Stereo Gain Blocks (BT & ADC Input Gain) ---
RadDSP::StereoGain btGain;
RadDSP::StereoGain adcGain;

// --- Mixer Stereo (2 Inputs: ADC & BT) ---
RadDSP::Mixer<2> mixerL;
RadDSP::Mixer<2> mixerR;

// --- Post-Mixer Graphic EQ 31-Band Stereo ---
RadDSP::GraphicEQ<31> eqOutL;
RadDSP::GraphicEQ<31> eqOutR;

// --- Master Volume Gain Stereo ---
RadDSP::StereoGain masterGain;

// --- Master Limiter Stereo ---
RadDSP::Dynamics limiterL;
RadDSP::Dynamics limiterR;

// --- VU Level Meters Stereo ---
RadDSP::Meter vuMeterL;
RadDSP::Meter vuMeterR;

// --- Input VU Level Meters for LEDs ---
RadDSP::Meter ledBtMeterL;
RadDSP::Meter ledBtMeterR;
RadDSP::Meter ledAdcMeterL;
RadDSP::Meter ledAdcMeterR;

// --- Auto-Save, Presets & Configuration State Management ---
struct DSPState {
    uint32_t magic;
    // Master Volume
    float masterVol;
    // Mixer
    float btVol, btBal;
    uint8_t btMute;
    float adcVol, adcBal;
    uint8_t adcMute;
    // EQ BT (4 bands)
    float eqBtType[4], eqBtFreq[4], eqBtGain[4], eqBtQ[4];
    uint8_t eqBtBypass[4];
    // EQ ADC (4 bands)
    float eqAdcType[4], eqAdcFreq[4], eqAdcGain[4], eqAdcQ[4];
    uint8_t eqAdcBypass[4];
    // Graphic EQ (31 bands)
    float graphEqGain[31];
    // Limiter
    uint8_t limBypass;
    float limThresh, limAttack, limRelease, limRatio, limMakeup;
    // Config
    uint8_t lcdBrightness;
    uint8_t ledBrightness;
};

#define DSP_STATE_MAGIC 0xDE59A1D0
Preferences prefs;
volatile float masterGainDb = 0.0f; // Default volume master: 0 dB (Unity Gain)
volatile bool stateDirty = false;
volatile unsigned long lastStateChangeTime = 0;

// Global settings variables
int configLcdBrightness = 100;
int configLedBrightness = 100;
volatile int ledMaxBrightnessPwm = 255;

void populateState(DSPState& state) {
    state.magic = DSP_STATE_MAGIC;
    // Master Volume
    state.masterVol = masterGainDb;
    // Mixer
    state.btVol = btGain.getParameter(0);
    state.btBal = btGain.getParameter(3);
    state.btMute = (btGain.getParameter(1) > 0.5f) ? 1 : 0;
    
    state.adcVol = adcGain.getParameter(0);
    state.adcBal = adcGain.getParameter(3);
    state.adcMute = (adcGain.getParameter(1) > 0.5f) ? 1 : 0;
    
    // EQ BT
    state.eqBtType[0] = eqBt_L1.getParameter(0); state.eqBtFreq[0] = eqBt_L1.getParameter(1); state.eqBtGain[0] = eqBt_L1.getParameter(2); state.eqBtQ[0] = eqBt_L1.getParameter(3); state.eqBtBypass[0] = (eqBt_L1.getParameter(100) > 0.5f) ? 1 : 0;
    state.eqBtType[1] = eqBt_L2.getParameter(0); state.eqBtFreq[1] = eqBt_L2.getParameter(1); state.eqBtGain[1] = eqBt_L2.getParameter(2); state.eqBtQ[1] = eqBt_L2.getParameter(3); state.eqBtBypass[1] = (eqBt_L2.getParameter(100) > 0.5f) ? 1 : 0;
    state.eqBtType[2] = eqBt_L3.getParameter(0); state.eqBtFreq[2] = eqBt_L3.getParameter(1); state.eqBtGain[2] = eqBt_L3.getParameter(2); state.eqBtQ[2] = eqBt_L3.getParameter(3); state.eqBtBypass[2] = (eqBt_L3.getParameter(100) > 0.5f) ? 1 : 0;
    state.eqBtType[3] = eqBt_L4.getParameter(0); state.eqBtFreq[3] = eqBt_L4.getParameter(1); state.eqBtGain[3] = eqBt_L4.getParameter(2); state.eqBtQ[3] = eqBt_L4.getParameter(3); state.eqBtBypass[3] = (eqBt_L4.getParameter(100) > 0.5f) ? 1 : 0;
    
    // EQ ADC
    state.eqAdcType[0] = eqAdc_L1.getParameter(0); state.eqAdcFreq[0] = eqAdc_L1.getParameter(1); state.eqAdcGain[0] = eqAdc_L1.getParameter(2); state.eqAdcQ[0] = eqAdc_L1.getParameter(3); state.eqAdcBypass[0] = (eqAdc_L1.getParameter(100) > 0.5f) ? 1 : 0;
    state.eqAdcType[1] = eqAdc_L2.getParameter(0); state.eqAdcFreq[1] = eqAdc_L2.getParameter(1); state.eqAdcGain[1] = eqAdc_L2.getParameter(2); state.eqAdcQ[1] = eqAdc_L2.getParameter(3); state.eqAdcBypass[1] = (eqAdc_L2.getParameter(100) > 0.5f) ? 1 : 0;
    state.eqAdcType[2] = eqAdc_L3.getParameter(0); state.eqAdcFreq[2] = eqAdc_L3.getParameter(1); state.eqAdcGain[2] = eqAdc_L3.getParameter(2); state.eqAdcQ[2] = eqAdc_L3.getParameter(3); state.eqAdcBypass[2] = (eqAdc_L3.getParameter(100) > 0.5f) ? 1 : 0;
    state.eqAdcType[3] = eqAdc_L4.getParameter(0); state.eqAdcFreq[3] = eqAdc_L4.getParameter(1); state.eqAdcGain[3] = eqAdc_L4.getParameter(2); state.eqAdcQ[3] = eqAdc_L4.getParameter(3); state.eqAdcBypass[3] = (eqAdc_L4.getParameter(100) > 0.5f) ? 1 : 0;
    
    // Graphic EQ
    for (int b = 0; b < 31; b++) {
        state.graphEqGain[b] = eqOutL.getParameter(b);
    }
    
    // Limiter
    state.limBypass = (limiterL.getParameter(100) > 0.5f) ? 1 : 0;
    state.limThresh = limiterL.getParameter(1);
    state.limAttack = limiterL.getParameter(3);
    state.limRelease = limiterL.getParameter(5);
    state.limRatio = limiterL.getParameter(2);
    state.limMakeup = limiterL.getParameter(6);
    
    // Config
    state.lcdBrightness = configLcdBrightness;
    state.ledBrightness = configLedBrightness;
}

void applyState(const DSPState& state) {
    // Master Volume
    masterGainDb = state.masterVol;
    dspControl.executeCommand(21, 0, masterGainDb);

    // Mixer
    dspControl.executeCommand(31, 0, state.btVol);
    dspControl.executeCommand(31, 3, state.btBal);
    dspControl.executeCommand(31, 1, state.btMute ? 1.0f : 0.0f);
    
    dspControl.executeCommand(30, 0, state.adcVol);
    dspControl.executeCommand(30, 3, state.adcBal);
    dspControl.executeCommand(30, 1, state.adcMute ? 1.0f : 0.0f);
    
    // EQ BT
    for (int b = 0; b < 4; b++) {
        int bandId = 9 + b;
        dspControl.executeCommand(bandId, 0, state.eqBtType[b]);
        dspControl.executeCommand(bandId, 1, state.eqBtFreq[b]);
        dspControl.executeCommand(bandId, 2, state.eqBtGain[b]);
        dspControl.executeCommand(bandId, 3, state.eqBtQ[b]);
        dspControl.executeCommand(bandId, 100, state.eqBtBypass[b] ? 1.0f : 0.0f);
    }
    
    // EQ ADC
    for (int b = 0; b < 4; b++) {
        int bandId = 1 + b;
        dspControl.executeCommand(bandId, 0, state.eqAdcType[b]);
        dspControl.executeCommand(bandId, 1, state.eqAdcFreq[b]);
        dspControl.executeCommand(bandId, 2, state.eqAdcGain[b]);
        dspControl.executeCommand(bandId, 3, state.eqAdcQ[b]);
        dspControl.executeCommand(bandId, 100, state.eqAdcBypass[b] ? 1.0f : 0.0f);
    }
    
    // Graphic EQ
    for (int b = 0; b < 31; b++) {
        dspControl.executeCommand(19, b, state.graphEqGain[b]);
    }
    
    // Limiter
    dspControl.executeCommand(22, 100, state.limBypass ? 1.0f : 0.0f);
    dspControl.executeCommand(22, 1, state.limThresh);
    dspControl.executeCommand(22, 3, state.limAttack);
    dspControl.executeCommand(22, 5, state.limRelease);
    dspControl.executeCommand(22, 2, state.limRatio);
    dspControl.executeCommand(22, 6, state.limMakeup);
    
    // Config Brightness
    configLcdBrightness = state.lcdBrightness;
    configLedBrightness = state.ledBrightness;
    
    // Apply LCD Brightness (Anti-log / Exponential Curve)
    float norm = (configLcdBrightness - 2.0f) / 98.0f;
    int contrast = OLED_MIN_CONTRAST + (int)((255 - OLED_MIN_CONTRAST) * powf(norm, BRIGHTNESS_CURVE_EXP));
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(contrast);
    
    // Apply LED Brightness max PWM mapping (Anti-log / Exponential Curve, min LED_MIN_PWM, with support for OFF)
    if (configLedBrightness == 0) {
        ledMaxBrightnessPwm = 0;
    } else {
        float ledNorm = (configLedBrightness - 1.0f) / 99.0f;
        ledMaxBrightnessPwm = LED_MIN_PWM + (int)((255 - LED_MIN_PWM) * powf(ledNorm, BRIGHTNESS_CURVE_EXP));
    }
}

void loadFactorySettings() {
    DSPState state;
    state.magic = DSP_STATE_MAGIC;
    state.masterVol = 0.0f;
    state.btVol = 0.0f;
    state.btBal = 0.0f;
    state.btMute = 0;
    
    state.adcVol = 0.0f;
    state.adcBal = 0.0f;
    state.adcMute = 0;
    
    for (int b = 0; b < 4; b++) {
        state.eqBtType[b] = 3.0f; // Peaking
        state.eqBtQ[b] = 1.0f;
        state.eqBtGain[b] = 0.0f;
        state.eqBtBypass[b] = 0;
        
        state.eqAdcType[b] = 3.0f;
        state.eqAdcQ[b] = 1.0f;
        state.eqAdcGain[b] = 0.0f;
        state.eqAdcBypass[b] = 0;
    }
    state.eqBtFreq[0] = 80.0f;
    state.eqBtFreq[1] = 400.0f;
    state.eqBtFreq[2] = 2000.0f;
    state.eqBtFreq[3] = 8000.0f;
    
    state.eqAdcFreq[0] = 80.0f;
    state.eqAdcFreq[1] = 400.0f;
    state.eqAdcFreq[2] = 2000.0f;
    state.eqAdcFreq[3] = 8000.0f;
    
    for (int b = 0; b < 31; b++) {
        state.graphEqGain[b] = 0.0f;
    }
    
    state.limBypass = 1; // Bypassed by default
    state.limThresh = 0.0f;
    state.limAttack = 5.0f;
    state.limRelease = 100.0f;
    state.limRatio = 100.0f;
    state.limMakeup = 0.0f;
    
    state.lcdBrightness = 100;
    state.ledBrightness = 100;
    
    applyState(state);
    Serial.println("[NVS] Factory settings applied");
}

void getSlotKey(int slot, char* keyBuf, size_t bufSize) {
    if (slot == 0) {
        strncpy(keyBuf, "autosave", bufSize);
    } else {
        snprintf(keyBuf, bufSize, "preset_%d", slot);
    }
}

void saveStateToNVS(int slot) {
    prefs.begin("dsp_state", false);
    char key[16];
    getSlotKey(slot, key, sizeof(key));
    
    DSPState state;
    populateState(state);
    
    prefs.putBytes(key, &state, sizeof(state));
    prefs.end();
    Serial.printf("[NVS] State saved successfully to slot %d\n", slot);
}

bool loadStateFromNVS(int slot) {
    prefs.begin("dsp_state", true);
    char key[16];
    getSlotKey(slot, key, sizeof(key));
    
    DSPState state;
    size_t bytesRead = prefs.getBytes(key, &state, sizeof(state));
    prefs.end();
    
    if (bytesRead == sizeof(state) && state.magic == DSP_STATE_MAGIC) {
        applyState(state);
        Serial.printf("[NVS] State loaded successfully from slot %d\n", slot);
        return true;
    }
    Serial.printf("[NVS] No valid state found in slot %d\n", slot);
    return false;
}

// ============================================================================
// 2. LED SIGNAL INDICATOR TUNING (ubah nilai di sini untuk tuning manual)
// ============================================================================
#define LED_DB_MIN   -50.0f   // dB di bawah ini = LED mati (PWM 0)
#define LED_DB_MAX     5.0f   // dB di atas ini  = LED terang maksimal
#define LED_PWM_MIN    1      // PWM minimum saat dB = LED_DB_MIN
#define LED_PWM_MAX    255    // PWM maksimum saat dB = LED_DB_MAX
#define LED_LOG_BASE   0.15f  // Basis logaritma (semakin besar = kurva makin curam di bawah)

// ============================================================================
// 3. KONTROL HARDWARE & MONITORING TAMPILAN
// ============================================================================

// Tracking level desibel rata-rata pasca-decay untuk pergerakan jarum VU
volatile float smoothDbL = -60.0f;
volatile float smoothDbR = -60.0f;

// Variabel kontrol Rotary Encoder
volatile int encoderPos = 0;
volatile bool encoderChanged = false;
volatile unsigned long lastEncoderMoveTime = 0;

// Interrupt Service Routine (ISR) Rotary Encoder (GPIO 25 = CLK, GPIO 26 = DT)
// Menggunakan State Machine 2-Bit agar benar-benar bebas dari getaran/bounce mekanis
void IRAM_ATTR encoderISR() {
    static uint8_t old_AB = 0;
    old_AB <<= 2;                   // Simpan state sebelumnya
    old_AB |= (digitalRead(25) << 1) | digitalRead(26);  // Baca pin A dan B
    old_AB &= 0x0F;                 // Ambil 4 bit terakhir (old A, old B, new A, new B)
    
    // Tabel transisi keadaan encoder (Full-Step / Half-Step)
    static const int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    int8_t change = enc_states[old_AB];
    if (change != 0) {
        encoderPos += change;
        encoderChanged = true;
    }
}

// Fungsi menggambar logo Bluetooth kecil (5x9 piksel) pada koordinat top-left (x, y)
void drawBluetoothIcon(int x, int y) {
    // Spine vertikal
    display.drawLine(x + 2, y, x + 2, y + 8, SSD1306_WHITE);
    // Sayap kanan atas
    display.drawLine(x + 2, y, x + 4, y + 2, SSD1306_WHITE);
    display.drawLine(x + 4, y + 2, x + 2, y + 4, SSD1306_WHITE);
    // Sayap kanan bawah
    display.drawLine(x + 2, y + 4, x + 4, y + 6, SSD1306_WHITE);
    display.drawLine(x + 4, y + 6, x + 2, y + 8, SSD1306_WHITE);
    // Antena kiri
    display.drawLine(x, y + 2, x + 2, y + 4, SSD1306_WHITE);
    display.drawLine(x + 2, y + 4, x, y + 6, SSD1306_WHITE);
}

// Fungsi menggambar jarum VU Meter Analog berbentuk Busur beserta Tick Marks
void drawAnalogVUMeter(int xc, int yc, int r, float levelDb) {
    // Normalisasi level dB: -50 dB s.d 0 dB dipetakan ke 0.0 s.d 1.0
    float val = (levelDb + 50.0f) / 50.0f;
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;

    // 1. Gambar busur luar (dari sudut 150 derajat s.d 30 derajat)
    int lastX = -1, lastY = -1;
    for (int deg = 150; deg >= 30; deg -= 10) {
        float rad = deg * 3.14159265f / 180.0f;
        int ax = xc + (int)(r * cos(rad));
        int ay = yc - (int)(r * sin(rad));
        if (lastX != -1) {
            display.drawLine(lastX, lastY, ax, ay, SSD1306_WHITE);
        }
        lastX = ax;
        lastY = ay;
    }

    // 2. Gambar garis penanda (ticks) skala -inf (-50dB) dan 0dB
    // Ticks -inf di sudut 150 derajat
    float radMin = 150.0f * 3.14159265f / 180.0f;
    int t1x_start = xc + (int)((r - 3) * cos(radMin));
    int t1y_start = yc - (int)((r - 3) * sin(radMin));
    int t1x_end = xc + (int)(r * cos(radMin));
    int t1y_end = yc - (int)(r * sin(radMin));
    display.drawLine(t1x_start, t1y_start, t1x_end, t1y_end, SSD1306_WHITE);

    // Ticks 0dB di sudut 30 derajat
    float radMax = 30.0f * 3.14159265f / 180.0f;
    int t2x_start = xc + (int)((r - 3) * cos(radMax));
    int t2y_start = yc - (int)((r - 3) * sin(radMax));
    int t2x_end = xc + (int)(r * cos(radMax));
    int t2y_end = yc - (int)(r * sin(radMax));
    display.drawLine(t2x_start, t2y_start, t2x_end, t2y_end, SSD1306_WHITE);

    // 3. Gambar jarum penunjuk
    float angle = 150.0f - val * 120.0f;
    float rad = angle * 3.14159265f / 180.0f;
    int nx = xc + (int)((r - 2) * cos(rad));
    int ny = yc - (int)((r - 2) * sin(rad));
    display.drawLine(xc, yc, nx, ny, SSD1306_WHITE);

// 4. Gambar poros jarum (pusat lingkaran kecil)
    display.fillCircle(xc, yc, 2, SSD1306_WHITE);
}

// Fungsi menggambar ikon menu di pusat box (xc, yc) dengan skala tertentu
void drawMenuIcon(int itemIdx, int xc, int yc, float scale) {
    if (itemIdx == 0) { // Back
        display.drawLine(xc + (int)(6 * scale), yc, xc - (int)(6 * scale), yc, SSD1306_WHITE);
        display.drawLine(xc - (int)(6 * scale), yc, xc - (int)(2 * scale), yc - (int)(4 * scale), SSD1306_WHITE);
        display.drawLine(xc - (int)(6 * scale), yc, xc - (int)(2 * scale), yc + (int)(4 * scale), SSD1306_WHITE);
    }
    else if (itemIdx == 1) { // BT EQ
        int bx = xc - 4;
        display.drawLine(bx + 1, yc - 4, bx + 1, yc + 4, SSD1306_WHITE);
        display.drawLine(bx + 1, yc - 4, bx + 3, yc - 2, SSD1306_WHITE);
        display.drawLine(bx + 3, yc - 2, bx + 1, yc, SSD1306_WHITE);
        display.drawLine(bx + 1, yc, bx + 3, yc + 2, SSD1306_WHITE);
        display.drawLine(bx + 3, yc + 2, bx + 1, yc + 4, SSD1306_WHITE);
        display.drawLine(bx - 1, yc - 2, bx + 1, yc, SSD1306_WHITE);
        display.drawLine(bx + 1, yc, bx - 1, yc + 2, SSD1306_WHITE);

        int sx = xc + 4;
        display.drawLine(sx, yc - (int)(5 * scale), sx, yc + (int)(5 * scale), SSD1306_WHITE);
        display.fillRect(sx - 2, yc - 2, 5, 3, SSD1306_WHITE);
    }
    else if (itemIdx == 2) { // Anlg EQ
        for (float dx = -7.0f; dx <= 7.0f; dx += 1.0f) {
            float dy = sinf(dx * 0.45f) * 5.0f;
            display.drawPixel(xc + (int)(dx * scale), yc + (int)(dy * scale), SSD1306_WHITE);
        }
    }
    else if (itemIdx == 3) { // Mixer
        int s1 = xc - (int)(5 * scale);
        display.drawLine(s1, yc - (int)(6 * scale), s1, yc + (int)(6 * scale), SSD1306_WHITE);
        display.fillRect(s1 - 2, yc - (int)(3 * scale), 5, 3, SSD1306_WHITE);

        int s2 = xc;
        display.drawLine(s2, yc - (int)(6 * scale), s2, yc + (int)(6 * scale), SSD1306_WHITE);
        display.fillRect(s2 - 2, yc + (int)(2 * scale), 5, 3, SSD1306_WHITE);

        int s3 = xc + (int)(5 * scale);
        display.drawLine(s3, yc - (int)(6 * scale), s3, yc + (int)(6 * scale), SSD1306_WHITE);
        display.fillRect(s3 - 2, yc - (int)(1 * scale), 5, 3, SSD1306_WHITE);
    }
    else if (itemIdx == 4) { // GraphEQ
        int w = (int)(2 * scale);
        if (w < 1) w = 1;
        display.fillRect(xc - (int)(7 * scale), yc + (int)(3 * scale) - (int)(4 * scale), w, (int)(4 * scale), SSD1306_WHITE);
        display.fillRect(xc - (int)(3 * scale), yc + (int)(3 * scale) - (int)(9 * scale), w, (int)(9 * scale), SSD1306_WHITE);
        display.fillRect(xc + (int)(1 * scale), yc + (int)(3 * scale) - (int)(6 * scale), w, (int)(6 * scale), SSD1306_WHITE);
        display.fillRect(xc + (int)(5 * scale), yc + (int)(3 * scale) - (int)(11 * scale), w, (int)(11 * scale), SSD1306_WHITE);
    }
    else if (itemIdx == 5) { // Limiter
        display.drawLine(xc - (int)(7 * scale), yc - (int)(6 * scale), xc - (int)(7 * scale), yc + (int)(6 * scale), SSD1306_WHITE);
        display.drawLine(xc - (int)(7 * scale), yc - (int)(6 * scale), xc - (int)(4 * scale), yc - (int)(6 * scale), SSD1306_WHITE);
        display.drawLine(xc - (int)(7 * scale), yc + (int)(6 * scale), xc - (int)(4 * scale), yc + (int)(6 * scale), SSD1306_WHITE);

        display.drawLine(xc + (int)(7 * scale), yc - (int)(6 * scale), xc + (int)(7 * scale), yc + (int)(6 * scale), SSD1306_WHITE);
        display.drawLine(xc + (int)(7 * scale), yc - (int)(6 * scale), xc + (int)(4 * scale), yc - (int)(6 * scale), SSD1306_WHITE);
        display.drawLine(xc + (int)(7 * scale), yc + (int)(6 * scale), xc + (int)(4 * scale), yc + (int)(6 * scale), SSD1306_WHITE);

        for (float dx = -4.0f; dx <= 4.0f; dx += 1.0f) {
            float dy = sinf(dx * 0.8f) * 4.0f;
            if (dy > 3.0f) dy = 3.0f;
            if (dy < -3.0f) dy = -3.0f;
            display.drawPixel(xc + (int)(dx * scale), yc + (int)(dy * scale), SSD1306_WHITE);
        }
    }
    else if (itemIdx == 6) { // Config
        int r = (int)(3 * scale);
        display.drawCircle(xc, yc, r, SSD1306_WHITE);
        for (int a = 0; a < 360; a += 45) {
            float rad = a * 3.14159f / 180.0f;
            int x1 = xc + r * cosf(rad);
            int y1 = yc + r * sinf(rad);
            int x2 = xc + (int)((r + 3 * scale) * cosf(rad));
            int y2 = yc + (int)((r + 3 * scale) * sinf(rad));
            display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
        }
    }
}

// Helper format frekuensi audio
void formatFrequency(float freq, char* buf, size_t bufSize) {
    if (freq < 1000.0f) {
        if (freq == (int)freq) {
            snprintf(buf, bufSize, "%d Hz", (int)freq);
        } else if (freq * 10.0f == (int)(freq * 10.0f)) {
            snprintf(buf, bufSize, "%.1f Hz", freq);
        } else {
            snprintf(buf, bufSize, "%.2f Hz", freq);
        }
    } else {
        float khz = freq / 1000.0f;
        if (khz == (int)khz) {
            snprintf(buf, bufSize, "%dk", (int)khz);
        } else if (khz * 10.0f == (int)(khz * 10.0f)) {
            snprintf(buf, bufSize, "%.1fk", khz);
        } else {
            snprintf(buf, bufSize, "%.2fk", khz);
        }
    }
}

// Background task FreeRTOS untuk UI (Layar OLED SSD1306) di Core 0
void uiTask(void* pvParameters) {
    display.clearDisplay();
    display.display();
    
    int lastPos = 0;
    int accumulatedTicks = 0;
    
    // Status navigasi menu
    enum UIState { UI_HOME, UI_MENU, UI_MIXER, UI_EQ_BT, UI_EQ_ADC, UI_LIMITER, UI_GRAPHEQ, UI_CONFIG, UI_SYSMON, UI_SYSINFO };
    UIState uiState = UI_HOME;
    
    int menuTargetIdx = 0;
    float menuCurrentPos = 0.0f;
    
    // Mixer Page State
    int mixerTargetIdx = 0;
    float mixerCurrentPos = 0.0f;
    bool mixerAdjustMode = false;

    // Config Page State
    int configTargetIdx = 0;
    float configCurrentPos = 0.0f;
    bool configAdjustMode = false;
    
    // Preset Overlay State
    bool presetOverlayActive = false;
    int presetOverlayBank = 1; // 1 to 16
    int presetOverlayTarget = 0; // 0 = SAVE, 1 = RECALL, 2 = CANCEL
    int presetOverlayMode = 0; // 0 = Select Bank, 1 = Select Action
    bool presetShowStatusMsg = false;
    unsigned long presetStatusMsgTime = 0;
    const char* presetStatusMsgText = "";

    // Initialize Confirmation State
    bool initConfirmActive = false;
    bool initConfirmSelect = false; // false = CANCEL, true = RESET

    // Reset Memory Confirmation State
    bool resetMemConfirmActive = false;
    bool resetMemConfirmSelect = false; // false = CANCEL, true = RESET

    // System Monitor Page Scroll State
    int sysMonScrollIdx = 0;
    float sysMonCurrentScroll = 0.0f;
    
    // Keuntungan & Status input BT dan ADC
    float localBtVol = 0.0f;
    float localBtBal = 0.0f;
    bool localBtMute = false;
    float localAdcVol = 0.0f;
    float localAdcBal = 0.0f;
    bool localAdcMute = false;
    
    // EQ Page State (BT / ADC)
    int eqTargetIdx = 0;
    float eqCurrentPos = 0.0f;
    bool eqAdjustMode = false;
    int selectedBand = 0; // 0 s.d 3 (Band 1-4)
    float localEqType = 3.0f;
    float localEqFreq = 1000.0f;
    float localEqGain = 0.0f;
    float localEqQ = 1.0f;
    bool localEqBandBypass = false;
    bool localEqGlobalBypass = false;
    
    // Limiter Page State
    int limTargetIdx = 0;
    float limCurrentPos = 0.0f;
    bool limAdjustMode = false;
    bool localLimBypass = false;
    float localLimThresh = 0.0f;
    float localLimAttack = 5.0f;
    float localLimRelease = 100.0f;
    float localLimRatio = 100.0f;
    float localLimMakeup = 0.0f;
    
    // GraphEQ Page State
    int graphEqTargetIdx = 0;
    float graphEqCurrentPos = 0.0f;
    bool graphEqAdjustMode = false;
    
    unsigned long lastUserActivityTime = millis();
    unsigned long lastEncoderMoveTime = 0;
    
    static bool lastButtonState = HIGH;
    
    const char* menuNames[] = {"BACK", "BT EQ", "ANLG EQ", "MIXER", "GRAPHEQ", "LIMITER", "CONFIG"};
    
    // Helper lambda untuk mengambil pointer band biquad aktif secara cepat
    auto getActiveBand = [&](int bandIdx) -> RadDSP::Biquad* {
        if (uiState == UI_EQ_BT) {
            if (bandIdx == 0) return &eqBt_L1;
            if (bandIdx == 1) return &eqBt_L2;
            if (bandIdx == 2) return &eqBt_L3;
            return &eqBt_L4;
        } else {
            if (bandIdx == 0) return &eqAdc_L1;
            if (bandIdx == 1) return &eqAdc_L2;
            if (bandIdx == 2) return &eqAdc_L3;
            return &eqAdc_L4;
        }
    };
    
    while (true) {
        unsigned long frameStart = millis();
        
        // 1. Baca input tombol putar encoder (GPIO 27, Active LOW dengan pullup)
        bool currentButtonState = digitalRead(27);
        if (currentButtonState == LOW && lastButtonState == HIGH) {
            unsigned long pressStart = millis();
            bool isLongPress = false;
            
            // Deteksi long press (> 600ms) untuk kembali ke menu utama dari mana saja
            while (digitalRead(27) == LOW) {
                if (millis() - pressStart > 600) {
                    isLongPress = true;
                    break;
                }
                delay(10);
            }
            
            lastUserActivityTime = millis();
            
            if (isLongPress) {
                if (uiState == UI_MIXER || uiState == UI_EQ_BT || uiState == UI_EQ_ADC || uiState == UI_LIMITER || uiState == UI_GRAPHEQ) {
                    // Setel kembali menu asal
                    if (uiState == UI_MIXER) menuTargetIdx = 3;
                    else if (uiState == UI_EQ_BT) menuTargetIdx = 1;
                    else if (uiState == UI_EQ_ADC) menuTargetIdx = 2;
                    else if (uiState == UI_GRAPHEQ) menuTargetIdx = 4;
                    else if (uiState == UI_LIMITER) menuTargetIdx = 5;
                    
                    uiState = UI_MENU;
                    menuCurrentPos = (float)menuTargetIdx;
                    mixerAdjustMode = false;
                    eqAdjustMode = false;
                    limAdjustMode = false;
                    graphEqAdjustMode = false;
                    
                    while (digitalRead(27) == LOW) { delay(10); } // Tunggu tombol lepas
                }
            } else {
                // Logika Klik Biasa (Short Press)
                if (uiState == UI_HOME) {
                    uiState = UI_MENU;
                    menuTargetIdx = 0;
                    menuCurrentPos = 0.0f;
                } 
                else if (uiState == UI_MENU) {
                    if (menuTargetIdx == 0) { // BACK
                        uiState = UI_HOME;
                    } 
                    else if (menuTargetIdx == 1) { // BT EQ
                        uiState = UI_EQ_BT;
                        selectedBand = 0;
                        RadDSP::Biquad* b = getActiveBand(0);
                        localEqType = b->getParameter(0);
                        localEqFreq = b->getParameter(1);
                        localEqGain = b->getParameter(2);
                        localEqQ = b->getParameter(3);
                        localEqBandBypass = (b->getParameter(100) > 0.5f);
                        localEqGlobalBypass = false;
                        
                        eqTargetIdx = 0;
                        eqCurrentPos = 0.0f;
                        eqAdjustMode = false;
                    }
                    else if (menuTargetIdx == 2) { // ANLG EQ
                        uiState = UI_EQ_ADC;
                        selectedBand = 0;
                        RadDSP::Biquad* b = getActiveBand(0);
                        localEqType = b->getParameter(0);
                        localEqFreq = b->getParameter(1);
                        localEqGain = b->getParameter(2);
                        localEqQ = b->getParameter(3);
                        localEqBandBypass = (b->getParameter(100) > 0.5f);
                        localEqGlobalBypass = false;
                        
                        eqTargetIdx = 0;
                        eqCurrentPos = 0.0f;
                        eqAdjustMode = false;
                    }
                    else if (menuTargetIdx == 3) { // MIXER
                        localBtVol = btGain.getParameter(0);
                        localBtMute = (btGain.getParameter(1) > 0.5f);
                        localBtBal = btGain.getParameter(3);
                        localAdcVol = adcGain.getParameter(0);
                        localAdcMute = (adcGain.getParameter(1) > 0.5f);
                        localAdcBal = adcGain.getParameter(3);
                        
                        uiState = UI_MIXER;
                        mixerTargetIdx = 0;
                        mixerCurrentPos = 0.0f;
                        mixerAdjustMode = false;
                    }
                    else if (menuTargetIdx == 4) { // GRAPHEQ
                        uiState = UI_GRAPHEQ;
                        graphEqTargetIdx = 0;
                        graphEqCurrentPos = 0.0f;
                        graphEqAdjustMode = false;
                    }
                    else if (menuTargetIdx == 5) { // LIMITER
                        localLimBypass = (limiterL.getParameter(100) > 0.5f);
                        localLimThresh = limiterL.getParameter(1);
                        localLimAttack = limiterL.getParameter(3);
                        localLimRelease = limiterL.getParameter(5);
                        localLimRatio = limiterL.getParameter(2);
                        localLimMakeup = limiterL.getParameter(6);
                        
                        uiState = UI_LIMITER;
                        limTargetIdx = 0;
                        limCurrentPos = 0.0f;
                        limAdjustMode = false;
                    }
                    else if (menuTargetIdx == 6) { // CONFIG
                        uiState = UI_CONFIG;
                        configTargetIdx = 0;
                        configCurrentPos = 0.0f;
                        configAdjustMode = false;
                        presetOverlayActive = false;
                        initConfirmActive = false;
                    }
                    else {
                        // Tampilkan selected untuk menu kosong/belum siap
                        display.fillRect(0, 0, 128, 64, SSD1306_WHITE);
                        display.setTextColor(SSD1306_BLACK);
                        display.setTextSize(1);
                        const char* text = "SELECTED";
                        display.setCursor((128 - strlen(text) * 6) / 2, 20);
                        display.print(text);
                        display.setCursor((128 - strlen(menuNames[menuTargetIdx]) * 6) / 2, 36);
                        display.print(menuNames[menuTargetIdx]);
                        display.display();
                        delay(500);
                        uiState = UI_HOME;
                    }
                }
                else if (uiState == UI_MIXER) {
                    if (mixerAdjustMode == false) {
                        if (mixerTargetIdx == 6) { // EXIT
                            uiState = UI_MENU;
                            menuTargetIdx = 3;
                            menuCurrentPos = 3.0f;
                        } else {
                            mixerAdjustMode = true;
                        }
                    } else {
                        mixerAdjustMode = false;
                    }
                }
                else if (uiState == UI_EQ_BT || uiState == UI_EQ_ADC) {
                    if (eqAdjustMode == false) {
                        if (eqTargetIdx == 7) { // EXIT
                            uiState = UI_MENU;
                            menuTargetIdx = (uiState == UI_EQ_BT) ? 1 : 2;
                            menuCurrentPos = (float)menuTargetIdx;
                        } else {
                            eqAdjustMode = true;
                        }
                    } else {
                        eqAdjustMode = false;
                    }
                }
                else if (uiState == UI_GRAPHEQ) {
                    if (graphEqAdjustMode == false) {
                        if (graphEqTargetIdx == 31) { // EXIT
                            uiState = UI_MENU;
                            menuTargetIdx = 4;
                            menuCurrentPos = 4.0f;
                        } else {
                            graphEqAdjustMode = true;
                        }
                    } else {
                        graphEqAdjustMode = false;
                    }
                }
                else if (uiState == UI_LIMITER) {
                    if (limAdjustMode == false) {
                        if (limTargetIdx == 6) { // EXIT
                            uiState = UI_MENU;
                            menuTargetIdx = 5;
                            menuCurrentPos = 5.0f;
                        } else {
                            limAdjustMode = true;
                        }
                    } else {
                        limAdjustMode = false;
                    }
                }
                else if (uiState == UI_CONFIG) {
                    if (presetOverlayActive) {
                        if (presetOverlayMode == 0) {
                            // Pilih Bank -> Pindah ke pilih Aksi (SAVE, RECALL, CANCEL)
                            presetOverlayMode = 1;
                            presetOverlayTarget = 0; // Default SAVE
                        } else {
                            // Pilih Aksi -> Eksekusi
                            if (presetOverlayTarget == 0) { // SAVE
                                saveStateToNVS(presetOverlayBank);
                                presetStatusMsgText = "PRESET SAVED";
                                presetShowStatusMsg = true;
                                presetStatusMsgTime = millis();
                                presetOverlayActive = false;
                            } else if (presetOverlayTarget == 1) { // RECALL
                                if (loadStateFromNVS(presetOverlayBank)) {
                                    presetStatusMsgText = "RECALLED";
                                    // Sinc UI variables
                                    localBtVol = btGain.getParameter(0);
                                    localBtMute = (btGain.getParameter(1) > 0.5f);
                                    localBtBal = btGain.getParameter(3);
                                    localAdcVol = adcGain.getParameter(0);
                                    localAdcMute = (adcGain.getParameter(1) > 0.5f);
                                    localAdcBal = adcGain.getParameter(3);
                                    localLimBypass = (limiterL.getParameter(100) > 0.5f);
                                    localLimThresh = limiterL.getParameter(1);
                                    localLimAttack = limiterL.getParameter(3);
                                    localLimRelease = limiterL.getParameter(5);
                                    localLimRatio = limiterL.getParameter(2);
                                    localLimMakeup = limiterL.getParameter(6);
                                } else {
                                    presetStatusMsgText = "BANK EMPTY";
                                }
                                presetShowStatusMsg = true;
                                presetStatusMsgTime = millis();
                                presetOverlayActive = false;
                            } else { // CANCEL
                                presetOverlayActive = false;
                            }
                        }
                    } else if (initConfirmActive) {
                        if (initConfirmSelect) { // RESET
                            loadFactorySettings();
                            saveStateToNVS(0); // auto-save update
                            presetStatusMsgText = "FACTORY RESET";
                            presetShowStatusMsg = true;
                            presetStatusMsgTime = millis();
                            initConfirmActive = false;
                        } else { // CANCEL
                            initConfirmActive = false;
                        }
                    } else if (resetMemConfirmActive) {
                        if (resetMemConfirmSelect) { // RESET ALL BANKS
                            // Clear Slots 1 to 16
                            for (int slot = 1; slot <= 16; slot++) {
                                char key[16];
                                getSlotKey(slot, key, sizeof(key));
                                prefs.begin("dsp_state", false);
                                prefs.remove(key);
                                prefs.end();
                            }
                            presetStatusMsgText = "MEM INITIALIZED";
                            presetShowStatusMsg = true;
                            presetStatusMsgTime = millis();
                            resetMemConfirmActive = false;
                        } else { // CANCEL
                            resetMemConfirmActive = false;
                        }
                    } else {
                        if (configAdjustMode == false) {
                            if (configTargetIdx == 7) { // EXIT
                                uiState = UI_MENU;
                                menuTargetIdx = 6;
                                menuCurrentPos = 6.0f;
                            } else if (configTargetIdx == 2) { // USER PRESET
                                presetOverlayActive = true;
                                presetOverlayBank = 1;
                                presetOverlayMode = 0;
                            } else if (configTargetIdx == 3) { // INITIALIZE
                                initConfirmActive = true;
                                initConfirmSelect = false;
                            } else if (configTargetIdx == 4) { // RESET MEM
                                resetMemConfirmActive = true;
                                resetMemConfirmSelect = false;
                            } else if (configTargetIdx == 5) { // SYS MONITOR
                                uiState = UI_SYSMON;
                            } else if (configTargetIdx == 6) { // SYS INFO
                                uiState = UI_SYSINFO;
                            } else { // LCD or LED Brightness
                                configAdjustMode = true;
                            }
                        } else {
                            configAdjustMode = false;
                            stateDirty = true;
                            lastStateChangeTime = millis();
                        }
                    }
                }
                else if (uiState == UI_SYSMON) {
                    uiState = UI_CONFIG;
                    configTargetIdx = 5;
                }
                else if (uiState == UI_SYSINFO) {
                    uiState = UI_CONFIG;
                    configTargetIdx = 6;
                }
            }
            delay(150);
        }
        lastButtonState = currentButtonState;
        
        // 2. Baca perputaran encoder
        if (encoderChanged) {
            encoderChanged = false;
            lastUserActivityTime = millis();
            
            int currentPos = encoderPos;
            int diff = currentPos - lastPos;
            lastPos = currentPos;
            
            accumulatedTicks += diff;
            int steps = accumulatedTicks / 4;
            accumulatedTicks %= 4;
            
            if (steps != 0) {
                unsigned long now = millis();
                static unsigned long lastTickTime = 0;
                unsigned long timeDelta = now - lastTickTime;
                lastTickTime = now;
                
                float stepSize = 0.5f;
                if (timeDelta < 40) stepSize = 3.0f;
                else if (timeDelta < 100) stepSize = 1.5f;
                
                if (uiState == UI_HOME) {
                    lastEncoderMoveTime = now;
                    masterGainDb += (steps > 0 ? 1 : -1) * stepSize;
                    if (masterGainDb > 10.0f) masterGainDb = 10.0f;
                    if (masterGainDb < -80.0f) masterGainDb = -80.0f;
                    dspControl.executeCommand(21, 0, masterGainDb);
                }
                else if (uiState == UI_MENU) {
                    menuTargetIdx += (steps > 0 ? 1 : -1);
                    if (menuTargetIdx < 0) menuTargetIdx = 0;
                    if (menuTargetIdx > 6) menuTargetIdx = 6;
                }
                else if (uiState == UI_MIXER) {
                    if (mixerAdjustMode == false) {
                        mixerTargetIdx += (steps > 0 ? 1 : -1);
                        if (mixerTargetIdx < 0) mixerTargetIdx = 0;
                        if (mixerTargetIdx > 6) mixerTargetIdx = 6;
                    } else {
                        if (mixerTargetIdx == 0) { // BT VOL
                            localBtVol += (steps > 0 ? 1 : -1) * stepSize;
                            if (localBtVol > 10.0f) localBtVol = 10.0f;
                            if (localBtVol < -80.0f) localBtVol = -80.0f;
                            dspControl.executeCommand(31, 0, localBtVol);
                        } 
                        else if (mixerTargetIdx == 1) { // BT BAL
                            localBtBal += (steps > 0 ? 0.02f : -0.02f);
                            if (localBtBal > 1.0f) localBtBal = 1.0f;
                            if (localBtBal < -1.0f) localBtBal = -1.0f;
                            dspControl.executeCommand(31, 3, localBtBal);
                        }
                        else if (mixerTargetIdx == 2) { // BT MUTE
                            localBtMute = !localBtMute;
                            dspControl.executeCommand(31, 1, localBtMute ? 1.0f : 0.0f);
                        }
                        else if (mixerTargetIdx == 3) { // ADC VOL
                            localAdcVol += (steps > 0 ? 1 : -1) * stepSize;
                            if (localAdcVol > 10.0f) localAdcVol = 10.0f;
                            if (localAdcVol < -80.0f) localAdcVol = -80.0f;
                            dspControl.executeCommand(30, 0, localAdcVol);
                        }
                        else if (mixerTargetIdx == 4) { // ADC BAL
                            localAdcBal += (steps > 0 ? 0.02f : -0.02f);
                            if (localAdcBal > 1.0f) localAdcBal = 1.0f;
                            if (localAdcBal < -1.0f) localAdcBal = -1.0f;
                            dspControl.executeCommand(30, 3, localAdcBal);
                        }
                        else if (mixerTargetIdx == 5) { // ADC MUTE
                            localAdcMute = !localAdcMute;
                            dspControl.executeCommand(30, 1, localAdcMute ? 1.0f : 0.0f);
                        }
                    }
                }
                else if (uiState == UI_EQ_BT || uiState == UI_EQ_ADC) {
                    if (eqAdjustMode == false) {
                        eqTargetIdx += (steps > 0 ? 1 : -1);
                        if (eqTargetIdx < 0) eqTargetIdx = 0;
                        if (eqTargetIdx > 7) eqTargetIdx = 7;
                    } else {
                        int bandId = (uiState == UI_EQ_BT ? 9 : 1) + selectedBand;
                        
                        if (eqTargetIdx == 0) { // EQ BYPASS
                            localEqGlobalBypass = !localEqGlobalBypass;
                            int startId = (uiState == UI_EQ_BT) ? 9 : 1;
                            for (int b = 0; b < 4; b++) {
                                dspControl.executeCommand(startId + b, 100, localEqGlobalBypass ? 1.0f : 0.0f);
                            }
                        }
                        else if (eqTargetIdx == 1) { // BAND SEL
                            selectedBand += (steps > 0 ? 1 : -1);
                            if (selectedBand < 0) selectedBand = 0;
                            if (selectedBand > 3) selectedBand = 3;
                            
                            // Ambil data band baru
                            RadDSP::Biquad* b = getActiveBand(selectedBand);
                            localEqType = b->getParameter(0);
                            localEqFreq = b->getParameter(1);
                            localEqGain = b->getParameter(2);
                            localEqQ = b->getParameter(3);
                            localEqBandBypass = (b->getParameter(100) > 0.5f);
                        }
                        else if (eqTargetIdx == 2) { // FREQ
                            // Steps 1/32 Octave (Logarithmic frequency steps)
                            localEqFreq *= powf(2.0f, (float)steps / 32.0f);
                            if (localEqFreq > 20000.0f) localEqFreq = 20000.0f;
                            if (localEqFreq < 20.0f) localEqFreq = 20.0f;
                            dspControl.executeCommand(bandId, 1, localEqFreq);
                        }
                        else if (eqTargetIdx == 3) { // GAIN
                            localEqGain += steps * 0.5f;
                            if (localEqGain > 15.0f) localEqGain = 15.0f;
                            if (localEqGain < -15.0f) localEqGain = -15.0f;
                            dspControl.executeCommand(bandId, 2, localEqGain);
                        }
                        else if (eqTargetIdx == 4) { // Q-FACTOR
                            localEqQ += steps * 0.05f;
                            if (localEqQ > 10.0f) localEqQ = 10.0f;
                            if (localEqQ < 0.1f) localEqQ = 0.1f;
                            dspControl.executeCommand(bandId, 3, localEqQ);
                        }
                        else if (eqTargetIdx == 5) { // TYPE
                            localEqType += (steps > 0 ? 1.0f : -1.0f);
                            if (localEqType > 5.0f) localEqType = 5.0f;
                            if (localEqType < 0.0f) localEqType = 0.0f;
                            dspControl.executeCommand(bandId, 0, localEqType);
                        }
                        else if (eqTargetIdx == 6) { // BAND BYP
                            localEqBandBypass = !localEqBandBypass;
                            dspControl.executeCommand(bandId, 100, localEqBandBypass ? 1.0f : 0.0f);
                        }
                    }
                }
                else if (uiState == UI_GRAPHEQ) {
                    if (graphEqAdjustMode == false) {
                        graphEqTargetIdx += (steps > 0 ? 1 : -1);
                        if (graphEqTargetIdx < 0) graphEqTargetIdx = 0;
                        if (graphEqTargetIdx > 31) graphEqTargetIdx = 31;
                    } else {
                        // Mengatur Gain untuk Graphic EQ Band (ID 19 / 20)
                        float currentGain = eqOutL.getParameter(graphEqTargetIdx);
                        float stepVal = (timeDelta < 40) ? 1.5f : 0.5f;
                        currentGain += (steps > 0 ? 1.0f : -1.0f) * stepVal;
                        if (currentGain > 12.0f) currentGain = 12.0f;
                        if (currentGain < -12.0f) currentGain = -12.0f;
                        dspControl.executeCommand(19, graphEqTargetIdx, currentGain);
                    }
                }
                else if (uiState == UI_LIMITER) {
                    if (limAdjustMode == false) {
                        limTargetIdx += (steps > 0 ? 1 : -1);
                        if (limTargetIdx < 0) limTargetIdx = 0;
                        if (limTargetIdx > 6) limTargetIdx = 6;
                    } else {
                        if (limTargetIdx == 0) { // BYPASS
                            localLimBypass = !localLimBypass;
                            dspControl.executeCommand(22, 100, localLimBypass ? 1.0f : 0.0f);
                        }
                        else if (limTargetIdx == 1) { // THRESHOLD
                            localLimThresh += steps * 0.5f;
                            if (localLimThresh > 0.0f) localLimThresh = 0.0f;
                            if (localLimThresh < -60.0f) localLimThresh = -60.0f;
                            dspControl.executeCommand(22, 1, localLimThresh);
                        }
                        else if (limTargetIdx == 2) { // ATTACK
                            localLimAttack += steps * 0.5f;
                            if (localLimAttack > 100.0f) localLimAttack = 100.0f;
                            if (localLimAttack < 0.1f) localLimAttack = 0.1f;
                            dspControl.executeCommand(22, 3, localLimAttack);
                        }
                        else if (limTargetIdx == 3) { // RELEASE
                            float rStep = (timeDelta < 40) ? 20.0f : 5.0f;
                            localLimRelease += steps * rStep;
                            if (localLimRelease > 1000.0f) localLimRelease = 1000.0f;
                            if (localLimRelease < 10.0f) localLimRelease = 10.0f;
                            dspControl.executeCommand(22, 5, localLimRelease);
                        }
                        else if (limTargetIdx == 4) { // RATIO
                            localLimRatio += steps * 0.5f;
                            if (localLimRatio > 20.0f) localLimRatio = 20.0f;
                            if (localLimRatio < 1.0f) localLimRatio = 1.0f;
                            dspControl.executeCommand(22, 2, localLimRatio);
                        }
                        else if (limTargetIdx == 5) { // MAKEUP
                            localLimMakeup += steps * 0.5f;
                            if (localLimMakeup > 24.0f) localLimMakeup = 24.0f;
                            if (localLimMakeup < 0.0f) localLimMakeup = 0.0f;
                            dspControl.executeCommand(22, 6, localLimMakeup);
                        }
                    }
                }
                else if (uiState == UI_CONFIG) {
                    if (presetOverlayActive) {
                        if (presetOverlayMode == 0) {
                            presetOverlayBank += (steps > 0 ? 1 : -1);
                            if (presetOverlayBank < 1) presetOverlayBank = 1;
                            if (presetOverlayBank > 16) presetOverlayBank = 16;
                        } else {
                            presetOverlayTarget += (steps > 0 ? 1 : -1);
                            if (presetOverlayTarget < 0) presetOverlayTarget = 0;
                            if (presetOverlayTarget > 2) presetOverlayTarget = 2;
                        }
                    } else if (initConfirmActive) {
                        initConfirmSelect = !initConfirmSelect;
                    } else if (resetMemConfirmActive) {
                        resetMemConfirmSelect = !resetMemConfirmSelect;
                    } else {
                        if (configAdjustMode == false) {
                            configTargetIdx += (steps > 0 ? 1 : -1);
                            if (configTargetIdx < 0) configTargetIdx = 0;
                            if (configTargetIdx > 7) configTargetIdx = 7;
                        } else {
                            if (configTargetIdx == 0) { // LCD BRIGHTNESS
                                configLcdBrightness += (steps > 0 ? 2 : -2);
                                if (configLcdBrightness < 2) configLcdBrightness = 2;
                                if (configLcdBrightness > 100) configLcdBrightness = 100;
                                
                                float norm = (configLcdBrightness - 2.0f) / 98.0f;
                                int contrast = OLED_MIN_CONTRAST + (int)((255 - OLED_MIN_CONTRAST) * powf(norm, BRIGHTNESS_CURVE_EXP));
                                display.ssd1306_command(SSD1306_SETCONTRAST);
                                display.ssd1306_command(contrast);
                            }
                            else if (configTargetIdx == 1) { // LED BRIGHTNESS
                                configLedBrightness += (steps > 0 ? 2 : -2);
                                if (configLedBrightness < 0) configLedBrightness = 0;
                                if (configLedBrightness > 100) configLedBrightness = 100;
                                
                                if (configLedBrightness == 0) {
                                    ledMaxBrightnessPwm = 0;
                                } else {
                                    float ledNorm = (configLedBrightness - 1.0f) / 99.0f;
                                    ledMaxBrightnessPwm = LED_MIN_PWM + (int)((255 - LED_MIN_PWM) * powf(ledNorm, BRIGHTNESS_CURVE_EXP));
                                }
                            }
                        }
                    }
                }
                else if (uiState == UI_SYSMON) {
                    sysMonScrollIdx += (steps > 0 ? 1 : -1);
                    if (sysMonScrollIdx < 0) sysMonScrollIdx = 0;
                    if (sysMonScrollIdx > 9) sysMonScrollIdx = 9; // 13 items total, 4 visible -> 13 - 4 = 9 max top index
                }
                
                // Trigger NVS auto-save untuk semua perubahan parameter
                bool paramChanged = false;
                if (uiState == UI_HOME) paramChanged = true;
                else if (uiState == UI_MIXER && mixerAdjustMode) paramChanged = true;
                else if ((uiState == UI_EQ_BT || uiState == UI_EQ_ADC) && eqAdjustMode) paramChanged = true;
                else if (uiState == UI_GRAPHEQ && graphEqAdjustMode) paramChanged = true;
                else if (uiState == UI_LIMITER && limAdjustMode) paramChanged = true;
                else if (uiState == UI_CONFIG && configAdjustMode) paramChanged = true;

                if (paramChanged) {
                    stateDirty = true;
                    lastStateChangeTime = millis();
                }
            }
        }
        
        // 3. Logika auto-timeout 10 detik ke HOME (kecuali di halaman tertentu)
        bool disableTimeout = (uiState == UI_SYSMON || uiState == UI_SYSINFO || (uiState == UI_CONFIG && presetOverlayActive));
        if (uiState != UI_HOME && !disableTimeout && (millis() - lastUserActivityTime > 10000)) {
            uiState = UI_HOME;
        }
        
        // 4. Interpolasi posisi menu/mixer/EQ/Limiter/GraphEQ (Low-Pass Filter) untuk efek 120 FPS
        if (uiState == UI_MENU) {
            menuCurrentPos += (menuTargetIdx - menuCurrentPos) * 0.13f;
            if (fabsf(menuTargetIdx - menuCurrentPos) < 0.01f) menuCurrentPos = menuTargetIdx;
        }
        else if (uiState == UI_MIXER) {
            mixerCurrentPos += (mixerTargetIdx - mixerCurrentPos) * 0.13f;
            if (fabsf(mixerTargetIdx - mixerCurrentPos) < 0.01f) mixerCurrentPos = mixerTargetIdx;
        }
        else if (uiState == UI_EQ_BT || uiState == UI_EQ_ADC) {
            eqCurrentPos += (eqTargetIdx - eqCurrentPos) * 0.13f;
            if (fabsf(eqTargetIdx - eqCurrentPos) < 0.01f) eqCurrentPos = eqTargetIdx;
        }
        else if (uiState == UI_GRAPHEQ) {
            graphEqCurrentPos += (graphEqTargetIdx - graphEqCurrentPos) * 0.13f;
            if (fabsf(graphEqTargetIdx - graphEqCurrentPos) < 0.01f) graphEqCurrentPos = graphEqTargetIdx;
        }
        else if (uiState == UI_LIMITER) {
            limCurrentPos += (limTargetIdx - limCurrentPos) * 0.13f;
            if (fabsf(limTargetIdx - limCurrentPos) < 0.01f) limCurrentPos = limTargetIdx;
        }
        else if (uiState == UI_CONFIG) {
            configCurrentPos += (configTargetIdx - configCurrentPos) * 0.13f;
            if (fabsf(configTargetIdx - configCurrentPos) < 0.01f) configCurrentPos = configTargetIdx;
        }
        else if (uiState == UI_SYSMON) {
            sysMonCurrentScroll += (sysMonScrollIdx - sysMonCurrentScroll) * 0.13f;
            if (fabsf(sysMonScrollIdx - sysMonCurrentScroll) < 0.01f) sysMonCurrentScroll = sysMonScrollIdx;
        }
        
        // 4.5 Auto-Save Check (debounce 2 seconds after parameter changes)
        if (stateDirty && (millis() - lastStateChangeTime > 2000)) {
            saveStateToNVS(0);
            stateDirty = false;
        }
        
        // 5. Render Antarmuka Layar
        display.clearDisplay();
        
        if (uiState == UI_HOME) {
            // Tampilkan Overlay Box Volume jika encoder diputar kurang dari 1.5 detik yang lalu
            if (millis() - lastEncoderMoveTime < 1500) {
                display.drawRoundRect(10, 10, 108, 44, 4, SSD1306_WHITE);
                display.setTextSize(1);
                display.setTextColor(SSD1306_WHITE);
                display.setCursor(18, 16);
                display.print("MASTER VOLUME");
                
                float normalized = (masterGainDb + 80.0f) / 90.0f;
                if (normalized < 0.0f) normalized = 0.0f;
                if (normalized > 1.0f) normalized = 1.0f;
                int barWidth = (int)(normalized * 92);
                
                display.drawRect(18, 28, 92, 6, SSD1306_WHITE);
                if (barWidth > 0) {
                    display.fillRect(18, 28, barWidth, 6, SSD1306_WHITE);
                }
                
                display.setCursor(18, 38);
                if (masterGainDb <= -79.9f) {
                    display.print("MUTE");
                } else {
                    display.print(masterGainDb, 1);
                    display.print(" dB");
                }
            }
            else {
                // Tampilan Home: Analog VU Meter Jarum Stereo (L & R) dengan Box Ber-Radius
                display.setTextSize(1);
                
                // Badge judul "RAD AMP" yang di-invert di bagian atas tengah
                display.fillRoundRect(36, 0, 56, 10, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK);
                display.setCursor(43, 1);
                display.print("RAD AMP");
                
                // Indikator Bluetooth di pojok kiri atas (Solid jika terkoneksi, Berkedip jika pairing)
                bool btConnected = bt.isConnected();
                if (btConnected || (millis() % 1000 < 500)) {
                    drawBluetoothIcon(4, 0);
                }
                
                // 1. Kotak ber-radius untuk VU meter Kiri (x:2 s.d 62) dan Kanan (x:66 s.d 126)
                display.drawRoundRect(2, 12, 60, 50, 4, SSD1306_WHITE);
                display.drawRoundRect(66, 12, 60, 50, 4, SSD1306_WHITE);
                
                // 2. Render jarum VU Kiri (xc=32, yc=42, r=24) dan Kanan (xc=96, yc=42, r=24)
                drawAnalogVUMeter(32, 42, 24, smoothDbL);
                drawAnalogVUMeter(96, 42, 24, smoothDbR);
                
                // 3. Format teks desibel numerik
                char bufL[12];
                if (smoothDbL <= -49.9f) {
                    strcpy(bufL, "-INFdB L");
                } else {
                    snprintf(bufL, sizeof(bufL), "%.1fdB L", smoothDbL);
                }
                
                char bufR[12];
                if (smoothDbR <= -49.9f) {
                    strcpy(bufR, "R -INFdB");
                } else {
                    snprintf(bufR, sizeof(bufR), "R %.1fdB", smoothDbR);
                }
                
                // 4. Render dengan logika Invert jika level menyentuh 0 dB / kliping (>= -0.1 dB)
                // Left Channel
                if (smoothDbL >= -0.1f) {
                    display.fillRoundRect(4, 50, 56, 10, 2, SSD1306_WHITE);
                    display.setTextColor(SSD1306_BLACK);
                } else {
                    display.setTextColor(SSD1306_WHITE);
                }
                display.setCursor(6, 51);
                display.print(bufL);
                
                // Right Channel
                if (smoothDbR >= -0.1f) {
                    display.fillRoundRect(68, 50, 56, 10, 2, SSD1306_WHITE);
                    display.setTextColor(SSD1306_BLACK);
                } else {
                    display.setTextColor(SSD1306_WHITE);
                }
                display.setCursor(70, 51);
                display.print(bufR);
            }
        }
        else if (uiState == UI_MENU) {
            display.setTextSize(1);
            
            // 1. Header: Invert box dengan radius dan lebar penuh (128px)
            display.fillRoundRect(0, 0, 128, 13, 2, SSD1306_WHITE);
            
            int activeIdx = (int)(menuCurrentPos + 0.5f);
            if (activeIdx < 0) activeIdx = 0;
            if (activeIdx > 6) activeIdx = 6;
            
            int txtLen = strlen(menuNames[activeIdx]) * 6 - 1;
            int txtX = (128 - txtLen) / 2;
            
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(txtX, 3);
            display.print(menuNames[activeIdx]);
            
            // 2. Render horizontal carousel item boxes
            for (int i = 0; i < 7; i++) {
                float dx = i - menuCurrentPos;
                float xi = 64.0f + dx * 46.0f; // Jarak antar item 46px
                
                // Lewati render jika di luar batas layar
                if (xi < -25.0f || xi > 153.0f) continue;
                
                float t = fabsf(dx);
                if (t > 1.0f) t = 1.0f;
                
                // Interpolasi ukuran box 1:1 (dari 32px ke 20px)
                float size = 32.0f - t * 12.0f;
                float boxY = 38.0f - size / 2.0f;
                float boxX = xi - size / 2.0f;
                float radius = 4.0f - t * 1.0f;
                
                display.drawRoundRect((int)boxX, (int)boxY, (int)size, (int)size, (int)radius, SSD1306_WHITE);
                
                // Gambar ikon menu dengan skala dinamis menyesuaikan ukuran box
                drawMenuIcon(i, (int)xi, 38, size / 32.0f);
            }
            
            // 3. Dot indicator minimalis di bagian bawah (y=58)
            for (int i = 0; i < 7; i++) {
                int dotX = 64 + (i - 3) * 8;
                if (i == activeIdx) {
                    display.fillCircle(dotX, 58, 2, SSD1306_WHITE);
                } else {
                    display.drawCircle(dotX, 58, 1, SSD1306_WHITE);
                }
            }
        }
        else if (uiState == UI_MIXER) {
            display.setTextSize(1);
            
            // 1. Header: Invert box dengan radius dan lebar penuh (128px)
            display.fillRoundRect(0, 0, 128, 13, 2, SSD1306_WHITE);
            
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(49, 3);
            display.print("MIXER");
            
            int activeMixerIdx = (int)(mixerCurrentPos + 0.5f);
            if (activeMixerIdx < 0) activeMixerIdx = 0;
            if (activeMixerIdx > 6) activeMixerIdx = 6;
            
            // 2. Render vertical carousel strip boxes (7 items)
            for (int i = 0; i < 7; i++) {
                float dy = i - mixerCurrentPos;
                float yi = 38.0f + dy * 18.0f; // Jarak antar item vertikal 18px
                
                if (yi < -10.0f || yi > 86.0f) continue;
                
                float t = fabsf(dy);
                if (t > 1.0f) t = 1.0f;
                
                // Interpolasi dimensi box berdasarkan kejauhan vertikal
                // Box tengah: lebar 120px, tinggi 20px
                // Box samping: lebar 100px, tinggi 12px
                float w = 120.0f - t * 20.0f;
                float h = 20.0f - t * 8.0f;
                float radius = 4.0f - t * 1.0f;
                
                float boxX = 64.0f - w / 2.0f;
                float boxY = yi - h / 2.0f;
                
                // Format teks deskripsi parameter
                char textBuf[32];
                if (i == 0) { // BT VOL
                    if (i == activeMixerIdx) {
                        if (localBtVol <= -79.9f) strcpy(textBuf, "BT VOL: MUTE");
                        else snprintf(textBuf, sizeof(textBuf), "BT VOL: %.1f dB", localBtVol);
                    } else strcpy(textBuf, "BT VOL");
                }
                else if (i == 1) { // BT BAL
                    if (i == activeMixerIdx) {
                        if (localBtBal == 0.0f) strcpy(textBuf, "BT BAL: C");
                        else if (localBtBal < 0.0f) {
                            int valL = (int)roundf(fabsf(localBtBal) * 50.0f);
                            snprintf(textBuf, sizeof(textBuf), "BT BAL: %dL", valL);
                        } else {
                            int valR = (int)roundf(localBtBal * 50.0f);
                            snprintf(textBuf, sizeof(textBuf), "BT BAL: %dR", valR);
                        }
                    } else strcpy(textBuf, "BT BAL");
                }
                else if (i == 2) { // BT MUTE
                    if (i == activeMixerIdx) {
                        snprintf(textBuf, sizeof(textBuf), "BT: %s", localBtMute ? "MUTED" : "UNMUTD");
                    } else strcpy(textBuf, "BT MUTE");
                }
                else if (i == 3) { // ADC VOL
                    if (i == activeMixerIdx) {
                        if (localAdcVol <= -79.9f) strcpy(textBuf, "ADC VOL: MUTE");
                        else snprintf(textBuf, sizeof(textBuf), "ADC VOL: %.1f dB", localAdcVol);
                    } else strcpy(textBuf, "ADC VOL");
                }
                else if (i == 4) { // ADC BAL
                    if (i == activeMixerIdx) {
                        if (localAdcBal == 0.0f) strcpy(textBuf, "ADC BAL: C");
                        else if (localAdcBal < 0.0f) {
                            int valL = (int)roundf(fabsf(localAdcBal) * 50.0f);
                            snprintf(textBuf, sizeof(textBuf), "ADC BAL: %dL", valL);
                        } else {
                            int valR = (int)roundf(localAdcBal * 50.0f);
                            snprintf(textBuf, sizeof(textBuf), "ADC BAL: %dR", valR);
                        }
                    } else strcpy(textBuf, "ADC BAL");
                }
                else if (i == 5) { // ADC MUTE
                    if (i == activeMixerIdx) {
                        snprintf(textBuf, sizeof(textBuf), "ADC: %s", localAdcMute ? "MUTED" : "UNMUTD");
                    } else strcpy(textBuf, "ADC MUTE");
                }
                else { // EXIT
                    strcpy(textBuf, "EXIT");
                }
                
                // Gambar box
                if (i == activeMixerIdx) {
                    if (mixerAdjustMode) {
                        // Edit Mode: Solid white background dengan teks hitam
                        display.fillRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                        display.setTextColor(SSD1306_BLACK);
                    } else {
                        // Selected Mode: Outline biasa dengan teks putih
                        display.drawRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                        display.setTextColor(SSD1306_WHITE);
                    }
                } else {
                    // Unselected Mode: Box kecil dengan teks putih
                    display.drawRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                    display.setTextColor(SSD1306_WHITE);
                }
                
                // Cetak teks ditengah-tengah box
                int len = strlen(textBuf) * 6 - 1;
                int tx = 64 - len / 2;
                int ty = (int)(yi - 3.5f);
                display.setCursor(tx, ty);
                display.print(textBuf);
            }
        }
        else if (uiState == UI_EQ_BT || uiState == UI_EQ_ADC) {
            display.setTextSize(1);
            
            // 1. Header: Invert box dengan radius dan lebar penuh (128px)
            display.fillRoundRect(0, 0, 128, 13, 2, SSD1306_WHITE);
            
            display.setTextColor(SSD1306_BLACK);
            int activeIdx = (int)(eqCurrentPos + 0.5f);
            if (activeIdx < 0) activeIdx = 0;
            if (activeIdx > 7) activeIdx = 7;
            
            const char* title = (uiState == UI_EQ_BT) ? "BT EQ" : "ANLG EQ";
            int tLen = strlen(title) * 6 - 1;
            display.setCursor(64 - tLen / 2, 3);
            display.print(title);
            
            // 2. Render vertical carousel strip boxes (8 items)
            for (int i = 0; i < 8; i++) {
                float dy = i - eqCurrentPos;
                float yi = 38.0f + dy * 18.0f;
                
                if (yi < -10.0f || yi > 86.0f) continue;
                
                float t = fabsf(dy);
                if (t > 1.0f) t = 1.0f;
                
                float w = 120.0f - t * 20.0f;
                float h = 20.0f - t * 8.0f;
                float radius = 4.0f - t * 1.0f;
                
                float boxX = 64.0f - w / 2.0f;
                float boxY = yi - h / 2.0f;
                
                // Format teks deskripsi parameter
                char textBuf[32];
                if (i == 0) { // EQ BYPASS
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "EQ BYP: %s", localEqGlobalBypass ? "BYPASSED" : "ACTIVE");
                    } else strcpy(textBuf, "EQ BYPASS");
                }
                else if (i == 1) { // BAND SEL
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "BAND: %d", selectedBand + 1);
                    } else strcpy(textBuf, "BAND SEL");
                }
                else if (i == 2) { // FREQ
                    if (i == activeIdx) {
                        char fBuf[16];
                        formatFrequency(localEqFreq, fBuf, sizeof(fBuf));
                        snprintf(textBuf, sizeof(textBuf), "FREQ: %s", fBuf);
                    } else strcpy(textBuf, "FREQ");
                }
                else if (i == 3) { // GAIN
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "GAIN: %.1f dB", localEqGain);
                    } else strcpy(textBuf, "GAIN");
                }
                else if (i == 4) { // Q-FACTOR
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "Q: %.2f", localEqQ);
                    } else strcpy(textBuf, "Q-FACTOR");
                }
                else if (i == 5) { // TYPE
                    if (i == activeIdx) {
                        const char* typeNames[] = {"LOWPASS", "HIGHPASS", "BANDPASS", "PEAKING", "LOWSHELF", "HIGHSHELF"};
                        int tIdx = (int)localEqType;
                        if (tIdx < 0) tIdx = 0;
                        if (tIdx > 5) tIdx = 5;
                        snprintf(textBuf, sizeof(textBuf), "TYPE: %s", typeNames[tIdx]);
                    } else strcpy(textBuf, "TYPE");
                }
                else if (i == 6) { // BAND BYP
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "BAND: %s", localEqBandBypass ? "BYPASSED" : "ACTIVE");
                    } else strcpy(textBuf, "BAND BYP");
                }
                else { // EXIT
                    strcpy(textBuf, "EXIT");
                }
                
                // Gambar box
                if (i == activeIdx) {
                    if (eqAdjustMode) {
                        display.fillRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                        display.setTextColor(SSD1306_BLACK);
                    } else {
                        display.drawRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                        display.setTextColor(SSD1306_WHITE);
                    }
                } else {
                    display.drawRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                    display.setTextColor(SSD1306_WHITE);
                }
                
                int len = strlen(textBuf) * 6 - 1;
                int tx = 64 - len / 2;
                int ty = (int)(yi - 3.5f);
                display.setCursor(tx, ty);
                display.print(textBuf);
            }
        }
        else if (uiState == UI_GRAPHEQ) {
            display.setTextSize(1);
            
            // 1. Header: Invert box dengan radius dan lebar penuh (128px)
            display.fillRoundRect(0, 0, 128, 13, 2, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            
            int activeIdx = (int)(graphEqCurrentPos + 0.5f);
            if (activeIdx < 0) activeIdx = 0;
            if (activeIdx > 31) activeIdx = 31;
            
            // Format teks header kiri: Nomor Band
            char bandBuf[8];
            if (activeIdx == 31) {
                strcpy(bandBuf, "EX");
            } else {
                snprintf(bandBuf, sizeof(bandBuf), "B%d", activeIdx + 1);
            }
            display.setCursor(3, 3);
            display.print(bandBuf);
            
            // Format teks header tengah: Frekuensi (selalu center)
            char freqBuf[16];
            if (activeIdx == 31) {
                strcpy(freqBuf, "EXIT");
            } else {
                float fVal = 20.0f * powf(1000.0f, (float)activeIdx / 30.0f);
                formatFrequency(fVal, freqBuf, sizeof(freqBuf));
            }
            int fLen = strlen(freqBuf) * 6 - 1;
            display.setCursor(64 - fLen / 2, 3);
            display.print(freqBuf);
            
            // Format teks header kanan: Gain (+/-xx.xdB)
            char gainBuf[16];
            if (activeIdx == 31) {
                strcpy(gainBuf, "");
            } else {
                float gVal = eqOutL.getParameter(activeIdx);
                if (gVal >= 0.0f) {
                    snprintf(gainBuf, sizeof(gainBuf), "+%.1fdB", gVal);
                } else {
                    snprintf(gainBuf, sizeof(gainBuf), "%.1fdB", gVal);
                }
            }
            int gLen = strlen(gainBuf) * 6 - 1;
            display.setCursor(125 - gLen, 3);
            display.print(gainBuf);
            
            // 2. Render 5 fader strips horizontal kesamping (box next & before tidak usah dikecilkan)
            for (int j = -2; j <= 2; j++) {
                int bandIdx = (int)roundf(graphEqCurrentPos) + j;
                if (bandIdx < 0 || bandIdx > 31) continue;
                
                float dx = bandIdx - graphEqCurrentPos;
                float xi = 64.0f + dx * 22.0f; // Spacing 22px
                
                if (xi < -15.0f || xi > 143.0f) continue;
                
                float boxX = xi - 9.0f;
                float boxY = 20.0f;
                
                uint16_t boxColor = SSD1306_WHITE;
                uint16_t contentColor = SSD1306_WHITE;
                
                if (bandIdx == activeIdx) {
                    if (graphEqAdjustMode) {
                        display.fillRoundRect((int)boxX, (int)boxY, 18, 36, 3, SSD1306_WHITE);
                        boxColor = SSD1306_WHITE;
                        contentColor = SSD1306_BLACK;
                    } else {
                        display.drawRoundRect((int)boxX, (int)boxY, 18, 36, 3, SSD1306_WHITE);
                        boxColor = SSD1306_WHITE;
                        contentColor = SSD1306_WHITE;
                    }
                } else {
                    display.drawRoundRect((int)boxX, (int)boxY, 18, 36, 3, SSD1306_WHITE);
                    boxColor = SSD1306_WHITE;
                    contentColor = SSD1306_WHITE;
                }
                
                if (bandIdx == 31) {
                    // EXIT Box: Gambar silang 'X' di tengah
                    display.drawLine((int)(xi - 4), 34, (int)(xi + 4), 42, contentColor);
                    display.drawLine((int)(xi + 4), 34, (int)(xi - 4), 42, contentColor);
                } else {
                    // Band Fader Box
                    float gVal = eqOutL.getParameter(bandIdx);
                    float norm = (gVal + 12.0f) / 24.0f;
                    if (norm < 0.0f) norm = 0.0f;
                    if (norm > 1.0f) norm = 1.0f;
                    float yKnob = 52.0f - norm * 24.0f; // Fader line dari y=28 ke y=52
                    
                    display.drawLine((int)xi, 23, (int)xi, 53, contentColor);
                    display.fillRect((int)(xi - 6), (int)(yKnob - 1), 12, 3, contentColor);
                }
            }
        }
        else if (uiState == UI_LIMITER) {
            display.setTextSize(1);
            
            // 1. Header: Invert box dengan radius dan lebar penuh (128px)
            display.fillRoundRect(0, 0, 128, 13, 2, SSD1306_WHITE);
            
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(43, 3);
            display.print("LIMITER");
            
            int activeIdx = (int)(limCurrentPos + 0.5f);
            if (activeIdx < 0) activeIdx = 0;
            if (activeIdx > 6) activeIdx = 6;
            
            // 2. Render vertical carousel strip boxes (7 items)
            for (int i = 0; i < 7; i++) {
                float dy = i - limCurrentPos;
                float yi = 38.0f + dy * 18.0f;
                
                if (yi < -10.0f || yi > 86.0f) continue;
                
                float t = fabsf(dy);
                if (t > 1.0f) t = 1.0f;
                
                float w = 120.0f - t * 20.0f;
                float h = 20.0f - t * 8.0f;
                float radius = 4.0f - t * 1.0f;
                
                float boxX = 64.0f - w / 2.0f;
                float boxY = yi - h / 2.0f;
                
                // Format teks deskripsi parameter
                char textBuf[32];
                if (i == 0) { // BYPASS
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "BYPASS: %s", localLimBypass ? "BYPASSED" : "ACTIVE");
                    } else strcpy(textBuf, "LIMITER BYPASS");
                }
                else if (i == 1) { // THRESHOLD
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "THRESH: %.1f dB", localLimThresh);
                    } else strcpy(textBuf, "THRESHOLD");
                }
                else if (i == 2) { // ATTACK
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "ATTACK: %.1f ms", localLimAttack);
                    } else strcpy(textBuf, "ATTACK");
                }
                else if (i == 3) { // RELEASE
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "RELEASE: %d ms", (int)localLimRelease);
                    } else strcpy(textBuf, "RELEASE");
                }
                else if (i == 4) { // RATIO
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "RATIO: %.1f:1", localLimRatio);
                    } else strcpy(textBuf, "RATIO");
                }
                else if (i == 5) { // MAKEUP
                    if (i == activeIdx) {
                        snprintf(textBuf, sizeof(textBuf), "MAKEUP: %.1f dB", localLimMakeup);
                    } else strcpy(textBuf, "MAKEUP");
                }
                else { // EXIT
                    strcpy(textBuf, "EXIT");
                }
                
                // Gambar box
                if (i == activeIdx) {
                    if (limAdjustMode) {
                        display.fillRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                        display.setTextColor(SSD1306_BLACK);
                    } else {
                        display.drawRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                        display.setTextColor(SSD1306_WHITE);
                    }
                } else {
                    display.drawRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                    display.setTextColor(SSD1306_WHITE);
                }
                
                int len = strlen(textBuf) * 6 - 1;
                int tx = 64 - len / 2;
                int ty = (int)(yi - 3.5f);
                display.setCursor(tx, ty);
                display.print(textBuf);
            }
        }
        else if (uiState == UI_CONFIG) {
            display.setTextSize(1);
            
            // 1. Header: Invert box dengan radius dan lebar penuh (128px)
            display.fillRoundRect(0, 0, 128, 13, 2, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(34, 3);
            display.print("CONFIGURATION");
            
            if (presetOverlayActive) {
                // 3. User Preset Card Overlay (Standalone clean screen under header)
                display.fillRect(0, 14, 128, 50, SSD1306_BLACK);
                display.drawRoundRect(4, 16, 120, 44, 4, SSD1306_WHITE);
                display.setTextColor(SSD1306_WHITE);
                
                if (presetOverlayMode == 0) {
                    display.setCursor(14, 22);
                    display.print("SELECT BANK (1-16)");
                    
                    display.setCursor(14, 36);
                    display.printf("Bank %02d", presetOverlayBank);
                    
                    display.drawRect(72, 36, 44, 8, SSD1306_WHITE);
                    display.fillRect(72, 36, (presetOverlayBank * 44 / 16), 8, SSD1306_WHITE);
                } else {
                    display.setCursor(14, 22);
                    display.printf("BANK %02d ACTION:", presetOverlayBank);
                    
                    const char* actions[] = {"SAVE", "RECL", "CNCL"};
                    for (int a = 0; a < 3; a++) {
                        int bx = 10 + a * 37;
                        int by = 34;
                        int bw = 34;
                        int bh = 18;
                        
                        if (a == presetOverlayTarget) {
                            display.fillRoundRect(bx, by, bw, bh, 2, SSD1306_WHITE);
                            display.setTextColor(SSD1306_BLACK);
                        } else {
                            display.drawRoundRect(bx, by, bw, bh, 2, SSD1306_WHITE);
                            display.setTextColor(SSD1306_WHITE);
                        }
                        int tLen = strlen(actions[a]) * 6 - 1;
                        display.setCursor(bx + (bw - tLen) / 2, by + 5);
                        display.print(actions[a]);
                    }
                }
            }
            else if (initConfirmActive) {
                // 4. Initialize Confirmation Card Overlay
                display.fillRect(0, 14, 128, 50, SSD1306_BLACK);
                display.drawRoundRect(4, 16, 120, 44, 4, SSD1306_WHITE);
                display.setTextColor(SSD1306_WHITE);
                
                display.setCursor(18, 22);
                display.print("RESET TO FACTORY?");
                
                const char* initOptions[] = {"NO", "YES"};
                for (int a = 0; a < 2; a++) {
                    int bx = 28 + a * 40;
                    int by = 34;
                    int bw = 32;
                    int bh = 16;
                    
                    if ((a == 1 && initConfirmSelect) || (a == 0 && !initConfirmSelect)) {
                        display.fillRoundRect(bx, by, bw, bh, 2, SSD1306_WHITE);
                        display.setTextColor(SSD1306_BLACK);
                    } else {
                        display.drawRoundRect(bx, by, bw, bh, 2, SSD1306_WHITE);
                        display.setTextColor(SSD1306_WHITE);
                    }
                    int tLen = strlen(initOptions[a]) * 6 - 1;
                    display.setCursor(bx + (bw - tLen) / 2, by + 4);
                    display.print(initOptions[a]);
                }
            }
            else if (resetMemConfirmActive) {
                // Reset Memory Card Overlay
                display.fillRect(0, 14, 128, 50, SSD1306_BLACK);
                display.drawRoundRect(4, 16, 120, 44, 4, SSD1306_WHITE);
                display.setTextColor(SSD1306_WHITE);
                
                display.setCursor(14, 22);
                display.print("RESET MEMORY BANKS?");
                
                const char* options[] = {"NO", "YES"};
                for (int a = 0; a < 2; a++) {
                    int bx = 28 + a * 40;
                    int by = 34;
                    int bw = 32;
                    int bh = 16;
                    
                    if ((a == 1 && resetMemConfirmSelect) || (a == 0 && !resetMemConfirmSelect)) {
                        display.fillRoundRect(bx, by, bw, bh, 2, SSD1306_WHITE);
                        display.setTextColor(SSD1306_BLACK);
                    } else {
                        display.drawRoundRect(bx, by, bw, bh, 2, SSD1306_WHITE);
                        display.setTextColor(SSD1306_WHITE);
                    }
                    int tLen = strlen(options[a]) * 6 - 1;
                    display.setCursor(bx + (bw - tLen) / 2, by + 4);
                    display.print(options[a]);
                }
            }
            else {
                // 2. Render vertical carousel strip boxes (8 items) - Hanya jika overlay tidak aktif
                int activeIdx = (int)(configCurrentPos + 0.5f);
                if (activeIdx < 0) activeIdx = 0;
                if (activeIdx > 7) activeIdx = 7;
                
                for (int i = 0; i < 8; i++) {
                    float dy = i - configCurrentPos;
                    float yi = 38.0f + dy * 18.0f;
                    
                    if (yi < -10.0f || yi > 86.0f) continue;
                    
                    float t = fabsf(dy);
                    if (t > 1.0f) t = 1.0f;
                    
                    float w = 120.0f - t * 20.0f;
                    float h = 20.0f - t * 8.0f;
                    float radius = 4.0f - t * 1.0f;
                    
                    float boxX = 64.0f - w / 2.0f;
                    float boxY = yi - h / 2.0f;
                    
                    char textBuf[32];
                    if (i == 0) { // LCD BRIGHTNESS
                        if (i == activeIdx) {
                            snprintf(textBuf, sizeof(textBuf), "LCD BRIGHT: %d%%", configLcdBrightness);
                        } else strcpy(textBuf, "LCD BRIGHTNESS");
                    }
                    else if (i == 1) { // LED BRIGHTNESS
                        if (i == activeIdx) {
                            if (configLedBrightness == 0) {
                                strcpy(textBuf, "LED BRIGHT: OFF");
                            } else {
                                snprintf(textBuf, sizeof(textBuf), "LED BRIGHT: %d%%", configLedBrightness);
                            }
                        } else strcpy(textBuf, "LED BRIGHTNESS");
                    }
                    else if (i == 2) { // USER PRESET
                        strcpy(textBuf, "USER PRESET");
                    }
                    else if (i == 3) { // INITIALIZE
                        strcpy(textBuf, "INITIALIZE");
                    }
                    else if (i == 4) { // RESET MEM
                        strcpy(textBuf, "RESET MEMORY");
                    }
                    else if (i == 5) { // SYS MONITOR
                        strcpy(textBuf, "SYS MONITOR");
                    }
                    else if (i == 6) { // SYS INFO
                        strcpy(textBuf, "SYS INFO");
                    }
                    else { // EXIT
                        strcpy(textBuf, "EXIT");
                    }
                    
                    // Gambar box
                    if (i == activeIdx) {
                        if (configAdjustMode) {
                            display.fillRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                            display.setTextColor(SSD1306_BLACK);
                        } else {
                            display.drawRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                            display.setTextColor(SSD1306_WHITE);
                        }
                    } else {
                        display.drawRoundRect((int)boxX, (int)boxY, (int)w, (int)h, (int)radius, SSD1306_WHITE);
                        display.setTextColor(SSD1306_WHITE);
                    }
                    
                    int len = strlen(textBuf) * 6 - 1;
                    int tx = 64 - len / 2;
                    int ty = (int)(yi - 3.5f);
                    display.setCursor(tx, ty);
                    display.print(textBuf);
                }
            }

            // 5. Preset Status message overlay (Draw on top of everything)
            if (presetShowStatusMsg) {
                display.fillRect(14, 22, 100, 24, SSD1306_BLACK);
                display.drawRoundRect(14, 22, 100, 24, 3, SSD1306_WHITE);
                display.setTextSize(1);
                display.setTextColor(SSD1306_WHITE);
                int len = strlen(presetStatusMsgText) * 6 - 1;
                display.setCursor(64 - len / 2, 30);
                display.print(presetStatusMsgText);
                
                if (millis() - presetStatusMsgTime > 1500) {
                    presetShowStatusMsg = false;
                }
            }
        }
        else if (uiState == UI_SYSMON) {
            display.setTextSize(1);
            display.fillRoundRect(0, 0, 128, 13, 2, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            
            const char* title = "SYS MONITOR";
            int tLen = strlen(title) * 6 - 1;
            display.setCursor(64 - tLen / 2, 3);
            display.print(title);
            
            display.setTextColor(SSD1306_WHITE);
            
            // 250ms Refresh Throttle untuk metrik sistem
            static unsigned long lastSysMonUpdate = 0;
            static float cpu0 = 0.0f;
            static float cpu1 = 0.0f;
            static uint32_t freeHeap = 0;
            static uint32_t totalHeap = 0;
            static uint32_t usedHeap = 0;
            static size_t btFill = 0;
            static size_t btCap = 0;
            static float temp = 47.0f;
            
            if (millis() - lastSysMonUpdate > 250 || lastSysMonUpdate == 0) {
                lastSysMonUpdate = millis();
                cpu0 = dspControl.getCpuLoad(0);
                cpu1 = dspControl.getCpuLoad(1);
                if (cpu0 < 0.0f) cpu0 = 0.0f;
                if (cpu1 < 0.0f) cpu1 = 0.0f;
                
                freeHeap = ESP.getFreeHeap();
                totalHeap = ESP.getHeapSize();
                usedHeap = totalHeap - freeHeap;
                
                btFill = RadDSP::Bluetooth::getRingBufferFillBytes();
                btCap = RadDSP::Bluetooth::getRingBufferCapacity();
                
                temp = temperatureRead();
            }
            
            // Render 13 Scrollable Items
            for (int i = 0; i < 13; i++) {
                float dy = i - sysMonCurrentScroll;
                float yi = 20.0f + dy * 12.0f; // line spacing 12px
                
                if (yi < 14.0f || yi > 62.0f) continue;
                
                char itemBuf[32];
                if (i == 0) snprintf(itemBuf, sizeof(itemBuf), "CPU 0: %.1f %%", cpu0);
                else if (i == 1) snprintf(itemBuf, sizeof(itemBuf), "CPU 1: %.1f %%", cpu1);
                else if (i == 2) snprintf(itemBuf, sizeof(itemBuf), "CPU Freq: %d MHz", getCpuFrequencyMhz());
                else if (i == 3) snprintf(itemBuf, sizeof(itemBuf), "RAM Used: %d KB", (int)(usedHeap / 1024));
                else if (i == 4) snprintf(itemBuf, sizeof(itemBuf), "RAM Free: %d KB", (int)(freeHeap / 1024));
                else if (i == 5) snprintf(itemBuf, sizeof(itemBuf), "RAM Total: %d KB", (int)(totalHeap / 1024));
                else if (i == 6) snprintf(itemBuf, sizeof(itemBuf), "BT Buf: %.1f %%", btCap > 0 ? ((float)btFill / btCap * 100.0f) : 0.0f);
                else if (i == 7) snprintf(itemBuf, sizeof(itemBuf), "BT Used: %d KB", (int)(btFill / 1024));
                else if (i == 8) snprintf(itemBuf, sizeof(itemBuf), "BT Cap: %d KB", (int)(btCap / 1024));
                else if (i == 9) snprintf(itemBuf, sizeof(itemBuf), "CPU Temp: %.1f C", temp);
                else if (i == 10) strcpy(itemBuf, "3.3V Rail: NC");
                else if (i == 11) {
                    unsigned long sec = millis() / 1000;
                    unsigned long hr = sec / 3600;
                    sec %= 3600;
                    unsigned long min = sec / 60;
                    sec %= 60;
                    snprintf(itemBuf, sizeof(itemBuf), "Uptime: %02d:%02d:%02d", (int)hr, (int)min, (int)sec);
                }
                else {
                    unsigned long sec = millis() / 1000;
                    unsigned long hr = sec / 3600;
                    unsigned long days = hr / 24;
                    int d = 13 + (int)days;
                    int m = 7;
                    int y = 2026;
                    if (d > 31) { d = d - 31; m++; }
                    if (m > 12) { m = 1; y++; }
                    snprintf(itemBuf, sizeof(itemBuf), "Date: %02d/%02d/%04d", d, m, y);
                }
                
                display.setCursor(6, (int)yi);
                display.print(itemBuf);
            }
            
            // Gambar Scrollbar di Kanan Layar
            int barY = 16;
            int barH = 44;
            int barX = 123;
            display.drawRect(barX, barY, 3, barH, SSD1306_WHITE);
            
            int thumbH = 13; // Proporsional
            int thumbY = barY + (sysMonScrollIdx * (barH - thumbH) / 9);
            display.fillRect(barX, thumbY, 3, thumbH, SSD1306_WHITE);
        }
        else if (uiState == UI_SYSINFO) {
            display.setTextSize(1);
            display.fillRoundRect(0, 0, 128, 13, 2, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            
            const char* title = "SYS INFO";
            int tLen = strlen(title) * 6 - 1;
            display.setCursor(64 - tLen / 2, 3);
            display.print(title);
            
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(6, 22);
            display.print("S/N: ");
            display.print(SYS_SERIAL_NUMBER);
            
            display.setCursor(6, 35);
            display.print("Date: ");
            display.print(SYS_PRODUCTION_DATE);
            
            display.setCursor(6, 48);
            display.print("F/W: ");
            display.print(SYS_FIRMWARE_VERSION);
        }
        
        display.display();
        
        // 6. Pacing frame rate agar stabil di 120 FPS (~8.3ms per frame)
        unsigned long elapsed = millis() - frameStart;
        int delayMs = 8 - (int)elapsed;
        if (delayMs < 1) delayMs = 1;
        vTaskDelay(pdMS_TO_TICKS(delayMs));
    }
}

// ============================================================================
// AUDIO LOOP (DIJALANKAN PADA CORE 1)
// ============================================================================
void audioLoop() {
    // Poll command serial dari RadStudio GUI
    dspControl.poll();

    // Membaca satu blok sampel audio dari ADC (DIN: GPIO 34)
    if (i2s0.readBlock()) {
        int len = i2s0.getBufferLength();
        float* left = i2s0.getLeftBuffer();
        float* right = i2s0.getRightBuffer();

        // Alokasi buffer lokal untuk stream Bluetooth audio (defensif: 512 sampel)
        static float btLeft[512];
        static float btRight[512];
        int safeLen = len > 512 ? 512 : len;

        // Jalankan pemrosesan paralel (Fork-Join)
        // Core 1 memproses EQ ADC, Core 0 memproses Bluetooth audio
        worker.process(
            // --- TASK CORE 1 (ADC EQ) ---
            [&]() {
                // Proses Stereo Gain input ADC sebelum parametric EQ
                adcGain.process(left, right, safeLen);

                eqAdc_L1.process(left, safeLen);
                eqAdc_L2.process(left, safeLen);
                eqAdc_L3.process(left, safeLen);
                eqAdc_L4.process(left, safeLen);

                eqAdc_R1.process(right, safeLen);
                eqAdc_R2.process(right, safeLen);
                eqAdc_R3.process(right, safeLen);
                eqAdc_R4.process(right, safeLen);

                // Ukur level sinyal input ADC (post-gain, post-EQ) untuk LED_AUDIO
                ledAdcMeterL.process(left, safeLen, 1);
                ledAdcMeterR.process(right, safeLen, 1);
            },
            // --- TASK CORE 0 (BT Pull & EQ) ---
            [&]() {
                // Aktifkan telemetri waktu mulai proses di Core 0
                dspControl.markProcessStart(0);

                // Membaca data Bluetooth (ASRC otomatis melakukan sinkronisasi ke 48 kHz)
                bool hasBt = bt.readAudio(btLeft, btRight, safeLen, 48000);
                if (!hasBt) {
                    memset(btLeft, 0, safeLen * sizeof(float));
                    memset(btRight, 0, safeLen * sizeof(float));
                }

                // Proses Stereo Gain input BT sebelum parametric EQ
                btGain.process(btLeft, btRight, safeLen);

                // Proses EQ parametrik khusus Bluetooth
                eqBt_L1.process(btLeft, safeLen);
                eqBt_L2.process(btLeft, safeLen);
                eqBt_L3.process(btLeft, safeLen);
                eqBt_L4.process(btLeft, safeLen);

                eqBt_R1.process(btRight, safeLen);
                eqBt_R2.process(btRight, safeLen);
                eqBt_R3.process(btRight, safeLen);
                eqBt_R4.process(btRight, safeLen);

                // Ukur level sinyal input BT (post-gain, post-EQ) untuk LED_BT
                ledBtMeterL.process(btLeft, safeLen, 1);
                ledBtMeterR.process(btRight, safeLen, 1);

                // Selesaikan telemetri hitung utilisasi Core 0
                dspControl.markProcessEnd(0, safeLen, 48000);
            }
        );

        // --- LED SIGNAL INDICATOR (GPIO 33=BT, GPIO 32=Analog) ---
        // Ambil level dB tertinggi dari kedua channel L dan R per sumber
        float btDbL  = ledBtMeterL.getParameter(0);
        float btDbR  = ledBtMeterR.getParameter(0);
        float adcDbL = ledAdcMeterL.getParameter(0);
        float adcDbR = ledAdcMeterR.getParameter(0);
        float btDbMax  = (btDbL > btDbR) ? btDbL : btDbR;
        float adcDbMax = (adcDbL > adcDbR) ? adcDbL : adcDbR;

        // Logarithmic LED mapping: dB -> PWM
        // Formula: t = (dB - min) / (max - min), clamped 0..1
        //          pwm = PWM_MIN + (PWM_MAX - PWM_MIN) * log(1 + t*(base-1)) / log(base)
        auto ledMap = [](float dB) -> int {
            if (dB <= LED_DB_MIN) return 0;  // Di bawah floor = mati total
            float t = (dB - LED_DB_MIN) / (LED_DB_MAX - LED_DB_MIN);
            if (t > 1.0f) t = 1.0f;
            float curved = log1pf(t * (LED_LOG_BASE - 1.0f)) / logf(LED_LOG_BASE);
            int pwm = LED_PWM_MIN + (int)((ledMaxBrightnessPwm - LED_PWM_MIN) * curved);
            if (pwm > ledMaxBrightnessPwm) pwm = ledMaxBrightnessPwm;
            return pwm;
        };

        analogWrite(33, ledMap(btDbMax));   // LED_BT = Sinyal Bluetooth
        analogWrite(32, ledMap(adcDbMax));  // LED_AUDIO = Sinyal Analog (ADC)

        // --- PROSES TAHAP 2: MIXER STEREO (Berjalan di Core 1) ---
        float* mixedLeft = mixerL.process(safeLen, left, btLeft);
        float* mixedRight = mixerR.process(safeLen, right, btRight);

        // --- PROSES TAHAP 3: OUT GRAPHIC EQ 31-BAND (Berjalan di Core 1) ---
        eqOutL.process(mixedLeft, safeLen);
        eqOutR.process(mixedRight, safeLen);

        // --- PROSES TAHAP 4: MASTER GAIN BLOCK (Berjalan di Core 1) ---
        masterGain.process(mixedLeft, mixedRight, safeLen);

        // --- PROSES TAHAP 5: MASTER LIMITER BLOCK (Berjalan di Core 1) ---
        limiterL.process(mixedLeft, safeLen);
        limiterR.process(mixedRight, safeLen);

        // Copy hasil pemrosesan akhir ke buffer I2S TX (PCM1754)
        memcpy(left, mixedLeft, safeLen * sizeof(float));
        memcpy(right, mixedRight, safeLen * sizeof(float));

        // Hitung level desibel puncak (Peak Tracking) untuk VU meter OLED menggunakan RadDSP::Meter
        smoothDbL = vuMeterL.process(mixedLeft, safeLen, 1);
        smoothDbR = vuMeterR.process(mixedRight, safeLen, 1);

        // Tulis kembali sampel audio hasil pemrosesan ke DAC (DOUT: GPIO 17)
        i2s0.writeBlock();
    }
}

// Helper untuk inisiasi Parametric EQ
void initParametric(RadDSP::Biquad& eq, float freq) {
    eq.setParameter(0, 3.0f);     // 3 = Peaking
    eq.setParameter(1, freq);     // Frekuensi
    eq.setParameter(2, 0.0f);     // Gain: 0 dB
    eq.setParameter(3, 1.0f);     // Q: 1.0
    eq.setParameter(100, 0.0f);   // Bypass: Off
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=======================================================");
    Serial.println("  RAD_DSP_AMP - Dual-Core BT/ADC Parallel DSP Setup ");
    Serial.println("=======================================================");

    // 1. Inisialisasi thread helper DualCoreWorker di Core 0
    worker.begin();
    Serial.println("[Worker] Thread helper di Core 0 berhasil dijalankan!");

    // 2. Inisialisasi Native Bluetooth A2DP Sink
    if (bt.begin("RAD-DSP-AMP")) {
        Serial.println("[BT] Bluetooth A2DP Sink terinisialisasi: 'RAD-DSP-AMP'");
    } else {
        Serial.println("[BT] Inisialisasi Bluetooth GAGAL!");
    }
    
    // 3. Inisialisasi Driver I2S sesuai pin_configuration.txt:
    // (BCLK: 16, WS: 4, DOUT: 17, DIN: 34, MCLK: 0)
    bool init_ok = i2s0.begin(
        I2S_NUM_0, 48000, 32, true, 16, 4, 17, 34, 0, false, 8, 128
    );

    if (init_ok) {
        Serial.println("[I2S] Inisialisasi berhasil!");
    } else {
        Serial.println("[I2S] Inisialisasi GAGAL!");
        while (true) {
            delay(1000);
        }
    }

    // 4. Inisialisasi Layar OLED SSD1306 (I2C SDA=GPIO 14, SCL=GPIO 12)
    Wire.begin(14, 12);
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("[OLED] GAGAL Mengalokasikan SSD1306!"));
    } else {
        Serial.println(F("[OLED] Inisialisasi SSD1306 Berhasil!"));
        display.clearDisplay();
        display.display();
    }

    // 5. Setup Input Pin Rotary Encoder (GPIO 25=CLK, GPIO 26=DT, GPIO 27=SW)
    pinMode(25, INPUT_PULLUP);
    pinMode(26, INPUT_PULLUP);
    pinMode(27, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(25), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(26), encoderISR, CHANGE);
    Serial.println("[Encoder] Rotary Encoder terpasang pada GPIO 25, 26");
    
    // Setup Output Pin LED Indikator (GPIO 32=LED_AUDIO, GPIO 33=LED_BT)
    pinMode(32, OUTPUT);
    pinMode(33, OUTPUT);
    analogWrite(32, 0);
    analogWrite(33, 0);

    // 6. Attach Seluruh Modul ke Controller agar bisa dikendalikan oleh RadStudio
    // ADC (PCM1808) EQ Left (IDs 1-4)
    dspControl.attach(1, &eqAdc_L1, "ADC_L_EQ1");
    dspControl.attach(2, &eqAdc_L2, "ADC_L_EQ2");
    dspControl.attach(3, &eqAdc_L3, "ADC_L_EQ3");
    dspControl.attach(4, &eqAdc_L4, "ADC_L_EQ4");
    // ADC (PCM1808) EQ Right (IDs 5-8)
    dspControl.attach(5, &eqAdc_R1, "ADC_R_EQ1");
    dspControl.attach(6, &eqAdc_R2, "ADC_R_EQ2");
    dspControl.attach(7, &eqAdc_R3, "ADC_R_EQ3");
    dspControl.attach(8, &eqAdc_R4, "ADC_R_EQ4");

    // Bluetooth EQ Left (IDs 9-12)
    dspControl.attach(9, &eqBt_L1, "BT_L_EQ1");
    dspControl.attach(10, &eqBt_L2, "BT_L_EQ2");
    dspControl.attach(11, &eqBt_L3, "BT_L_EQ3");
    dspControl.attach(12, &eqBt_L4, "BT_L_EQ4");
    // Bluetooth EQ Right (IDs 13-16)
    dspControl.attach(13, &eqBt_R1, "BT_R_EQ1");
    dspControl.attach(14, &eqBt_R2, "BT_R_EQ2");
    dspControl.attach(15, &eqBt_R3, "BT_R_EQ3");
    dspControl.attach(16, &eqBt_R4, "BT_R_EQ4");

    // Mixers (IDs 17-18)
    dspControl.attach(17, &mixerL, "Mixer_Left");
    dspControl.attach(18, &mixerR, "Mixer_Right");

    // Output Graphic EQ 31-Band (IDs 19-20)
    dspControl.attach(19, &eqOutL, "EQ_Out_Left");
    dspControl.attach(20, &eqOutR, "EQ_Out_Right");

    // Master Volume Gain (ID 21)
    dspControl.attach(21, &masterGain, "Master_Gain");

    // Master Limiter (IDs 22-23)
    dspControl.attach(22, &limiterL, "Limiter_Left");
    dspControl.attach(23, &limiterR, "Limiter_Right");

    // VU Meters (IDs 24-25)
    dspControl.attach(24, &vuMeterL, "Meter_Left");
    dspControl.attach(25, &vuMeterR, "Meter_Right");
    dspControl.attach(26, &ledBtMeterL, "MeterBT_Left");
    dspControl.attach(27, &ledBtMeterR, "MeterBT_Right");
    dspControl.attach(28, &ledAdcMeterL, "MeterADC_Left");
    dspControl.attach(29, &ledAdcMeterR, "MeterADC_Right");

    // Input Gains (IDs 26-27)
    dspControl.attach(30, &adcGain, "ADC_Gain");
    dspControl.attach(31, &btGain, "BT_Gain");

    // Penautan Parameter Stereo (Link Left & Right)
    dspControl.link(&eqAdc_L1, &eqAdc_R1);
    dspControl.link(&eqAdc_L2, &eqAdc_R2);
    dspControl.link(&eqAdc_L3, &eqAdc_R3);
    dspControl.link(&eqAdc_L4, &eqAdc_R4);
    dspControl.link(&eqBt_L1,  &eqBt_R1);
    dspControl.link(&eqBt_L2,  &eqBt_R2);
    dspControl.link(&eqBt_L3,  &eqBt_R3);
    dspControl.link(&eqBt_L4,  &eqBt_R4);
    dspControl.link(&eqOutL,   &eqOutR);
    dspControl.link(&limiterL,  &limiterR);
    dspControl.link(&ledBtMeterL,  &ledBtMeterR);
    dspControl.link(&ledAdcMeterL,  &ledAdcMeterR);
    dspControl.link(&vuMeterL,  &vuMeterR);
    dspControl.link(&mixerL,    &mixerR);

    // 7. Inisialisasi Parameter Default Modul
    // ADC EQs
    initParametric(eqAdc_L1, 80.0f);   initParametric(eqAdc_R1, 80.0f);
    initParametric(eqAdc_L2, 300.0f);  initParametric(eqAdc_R2, 300.0f);
    initParametric(eqAdc_L3, 1500.0f); initParametric(eqAdc_R3, 1500.0f);
    initParametric(eqAdc_L4, 5000.0f); initParametric(eqAdc_R4, 5000.0f);

    // BT EQs
    initParametric(eqBt_L1, 80.0f);   initParametric(eqBt_R1, 80.0f);
    initParametric(eqBt_L2, 300.0f);  initParametric(eqBt_R2, 300.0f);
    initParametric(eqBt_L3, 1500.0f); initParametric(eqBt_R3, 1500.0f);
    initParametric(eqBt_L4, 5000.0f); initParametric(eqBt_R4, 5000.0f);

    // Mixers (Default: volume unity 0dB, unmuted)
    mixerL.setParameter(0, 0.0f);   mixerR.setParameter(0, 0.0f);
    mixerL.setParameter(1, 0.0f);   mixerR.setParameter(1, 0.0f);
    mixerL.setParameter(100, 0.0f); mixerR.setParameter(100, 0.0f);
    mixerL.setParameter(101, 0.0f); mixerR.setParameter(101, 0.0f);

    // Master Gain (Default: 0dB, unmuted, centered balance)
    masterGain.setParameter(0, 0.0f);
    masterGain.setParameter(1, 0.0f);
    masterGain.setParameter(2, 0.0f);
    masterGain.setParameter(3, 0.0f);

    // Master Limiter (Default: Limiter Mode, Threshold 0dB, Release 100ms)
    limiterL.setParameter(0, 1.0f);   limiterR.setParameter(0, 1.0f);
    limiterL.setParameter(1, 0.0f);   limiterR.setParameter(1, 0.0f);
    limiterL.setParameter(3, 5.0f);   limiterR.setParameter(3, 5.0f);
    limiterL.setParameter(4, 0.0f);   limiterR.setParameter(4, 0.0f);
    limiterL.setParameter(5, 100.0f); limiterR.setParameter(5, 100.0f);
    limiterL.setParameter(6, 0.0f);   limiterR.setParameter(6, 0.0f);
    limiterL.setParameter(7, 0.0f);   limiterR.setParameter(7, 0.0f);
    limiterL.setParameter(100, 0.0f); limiterR.setParameter(100, 0.0f);

    // VU Meters (Default: Decay Factor 0.90f)
    vuMeterL.setParameter(1, 0.99f);
    vuMeterR.setParameter(1, 0.99f);

    // Input LED Meters (Decay Factor: 0.985f)
    ledBtMeterL.setParameter(1, 0.98);
    ledBtMeterR.setParameter(1, 0.98);
    ledAdcMeterL.setParameter(1, 0.98);
    ledAdcMeterR.setParameter(1, 0.98);

    // Input Gains (Default: 0dB, unmuted, centered balance)
    adcGain.setParameter(0, 0.0f);
    adcGain.setParameter(1, 0.0f);
    adcGain.setParameter(2, 0.0f);
    adcGain.setParameter(3, 0.0f);
    btGain.setParameter(0, 0.0f);
    btGain.setParameter(1, 0.0f);
    btGain.setParameter(2, 0.0f);
    btGain.setParameter(3, 0.0f);

    // 7.5 Load NVS settings, if not found use factory default
    if (!loadStateFromNVS(0)) {
        loadFactorySettings();
        saveStateToNVS(0); // Save initial auto-save state
    }

    // 8. Buat background FreeRTOS Task untuk UI OLED di Core 0 (prioritas rendah: 3)
    xTaskCreatePinnedToCore(
        uiTask,
        "RadUITask",
        4096,
        NULL,
        3,
        NULL,
        0
    );
    Serial.println("[UI] Task UI OLED SSD1306 diluncurkan di Core 0!");

    // 9. Aktifkan port Serial Controller untuk komunikasi dengan RadStudio
    dspControl.beginSerial(115200);

    // 10. Jalankan task audio pada Core 1 dengan prioritas tinggi
    RadDSP::startAudioTask(audioLoop, 1, true);
}

// ============================================================================
// LOOP (KOSONG KARENA DI-KILL OLEH startAudioTask)
// ============================================================================
void loop() {
    // Menganggur
}
