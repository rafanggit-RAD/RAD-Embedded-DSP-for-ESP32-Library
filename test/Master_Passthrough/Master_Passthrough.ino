#include <RadDSP.h>

RadDSP::I2S i2s0;
RadDSP::I2S i2s1;
RadDSP::Bluetooth bt;
RadDSP::Controller dspControl;
RadDSP::DualCoreWorker dualCore;

// --- 1. I2S CHAIN ---
RadDSP::Biquad EqI2S_0_L, EqI2S_0_R;
RadDSP::Biquad EqI2S_1_L, EqI2S_1_R;
RadDSP::Biquad EqI2S_2_L, EqI2S_2_R;
RadDSP::Biquad EqI2S_3_L, EqI2S_3_R;
RadDSP::Dynamics CompI2S_L, CompI2S_R;

// --- 2. BLUETOOTH CHAIN ---
RadDSP::Biquad EqBT_0_L, EqBT_0_R;
RadDSP::Biquad EqBT_1_L, EqBT_1_R;
RadDSP::Biquad EqBT_2_L, EqBT_2_R;
RadDSP::Biquad EqBT_3_L, EqBT_3_R;

// --- 3. MIXERS ---
RadDSP::Mixer<2> mixerL;
RadDSP::Mixer<2> mixerR;

// --- 4. MASTER BUS ---
RadDSP::Biquad EqMaster_0_L, EqMaster_0_R;
RadDSP::Biquad EqMaster_1_L, EqMaster_1_R;
RadDSP::Biquad EqMaster_2_L, EqMaster_2_R;
RadDSP::Biquad EqMaster_3_L, EqMaster_3_R;
RadDSP::Biquad EqMaster_4_L, EqMaster_4_R;
RadDSP::Biquad EqMaster_5_L, EqMaster_5_R;
RadDSP::FIR firL, firR;
RadDSP::Dynamics LimiterMaster_L, LimiterMaster_R;
RadDSP::Meter meterL, meterR, meterAnalogL, meterAnalogR, meterBtL, meterBtR;

// --- 5. OUTPUT ROUTER ---
// ============================================================================
// DSP ROUTING SCHEMA (PARSED BY RADSCANNER)
// ============================================================================
// @Route: I2S_In -> meterAnalogL -> EqI2S_0_L -> EqI2S_1_L -> EqI2S_2_L -> EqI2S_3_L -> CompI2S_L -> mixerL
// @Route: I2S_In -> meterAnalogR -> EqI2S_0_R -> EqI2S_1_R -> EqI2S_2_R -> EqI2S_3_R -> CompI2S_R -> mixerR
// @Route: BT_In -> meterBtL -> EqBT_0_L -> EqBT_1_L -> EqBT_2_L -> EqBT_3_L -> mixerL
// @Route: BT_In -> meterBtR -> EqBT_0_R -> EqBT_1_R -> EqBT_2_R -> EqBT_3_R -> mixerR
// @Route: mixerL -> EqMaster_0_L -> EqMaster_1_L -> EqMaster_2_L -> EqMaster_3_L -> EqMaster_4_L -> EqMaster_5_L -> firL -> LimiterMaster_L -> meterL -> routerL -> I2S0_Out
// @Route: mixerR -> EqMaster_0_R -> EqMaster_1_R -> EqMaster_2_R -> EqMaster_3_R -> EqMaster_4_R -> EqMaster_5_R -> firR -> LimiterMaster_R -> meterR -> routerR -> I2S0_Out
// @Route: routerL -> I2S1_Out
// @Route: routerR -> I2S1_Out

#include "dsp_schema.h"

RadDSP::MatrixRouter<3, 2> routerL, routerR;



// --- BUFFER PIPELINE PING-PONG ---
float pipeL[128];
float pipeR[128];
float nextPipeL[128];
float nextPipeR[128];
float outPipeL[128];
float outPipeR[128];

// ============================================================================
// MAIN TASK (AUDIO LOOP)
// ============================================================================
void audioLoop() {
    // 1. Poll perintah serial dari RadStudio GUI
    dspControl.poll();

    // 2. Baca audio dari I2S DMA
    if (i2s0.readBlock()) {
        int len = i2s0.getBufferLength();
        float *i2sL = i2s0.getLeftBuffer();
        float *i2sR = i2s0.getRightBuffer();

        meterAnalogL.process(i2sL, len);
        meterAnalogR.process(i2sR, len);

        // 3. Baca Bluetooth Audio (ASRC Hermite 44.1kHz -> 48kHz)
        float btL[128], btR[128];
        if (!bt.readAudio(btL, btR, len, 48000)) {
            memset(btL, 0, len * sizeof(float));
            memset(btR, 0, len * sizeof(float));
        }

        meterBtL.process(btL, len);
        meterBtR.process(btR, len);

        // 4. Pipeline Dual Core Paralel (Ping-Pong Buffer)
        dualCore.process(
            [&]() {
                // --- CORE 1: Input EQ -> Compressor -> Mixer -> Master EQ ---
                dspControl.markProcessStart(1);

                // I2S Input EQ Chain (4-band Biquad per channel)
                EqI2S_0_L.process(i2sL, len);
                EqI2S_1_L.process(i2sL, len);
                EqI2S_2_L.process(i2sL, len);
                EqI2S_3_L.process(i2sL, len);
                EqI2S_0_R.process(i2sR, len);
                EqI2S_1_R.process(i2sR, len);
                EqI2S_2_R.process(i2sR, len);
                EqI2S_3_R.process(i2sR, len);

                // Compressor (in-place)
                CompI2S_L.process(i2sL, len);
                CompI2S_R.process(i2sR, len);

                // BT Input EQ Chain (4-band Biquad per channel)
                EqBT_0_L.process(btL, len);
                EqBT_1_L.process(btL, len);
                EqBT_2_L.process(btL, len);
                EqBT_3_L.process(btL, len);
                EqBT_0_R.process(btR, len);
                EqBT_1_R.process(btR, len);
                EqBT_2_R.process(btR, len);
                EqBT_3_R.process(btR, len);

                // Mixer I2S + BT
                float* mixL = mixerL.process(len, i2sL, btL);
                float* mixR = mixerR.process(len, i2sR, btR);

                // Master EQ Chain (6-band Biquad per channel)
                EqMaster_0_L.process(mixL, len);
                EqMaster_1_L.process(mixL, len);
                EqMaster_2_L.process(mixL, len);
                EqMaster_3_L.process(mixL, len);
                EqMaster_4_L.process(mixL, len);
                EqMaster_5_L.process(mixL, len);
                EqMaster_0_R.process(mixR, len);
                EqMaster_1_R.process(mixR, len);
                EqMaster_2_R.process(mixR, len);
                EqMaster_3_R.process(mixR, len);
                EqMaster_4_R.process(mixR, len);
                EqMaster_5_R.process(mixR, len);

                memcpy(nextPipeL, mixL, len * sizeof(float));
                memcpy(nextPipeR, mixR, len * sizeof(float));
                dspControl.markProcessEnd(1, len, 48000);
            },
            [&]() {
                // --- CORE 0: FIR -> Limiter -> Matrix Router ---
                dspControl.markProcessStart(0);

                float fOutL[128], fOutR[128];
                firL.process(pipeL, fOutL, len);
                firR.process(pipeR, fOutR, len);

                LimiterMaster_L.process(fOutL, len);
                LimiterMaster_R.process(fOutR, len);

                // Ukur tingkat sinyal pasca Limiter (Master Output Level)
                meterL.process(fOutL, len);
                meterR.process(fOutR, len);

                float silent[128] = {0};
                float* inMatrixL[3] = { fOutL, silent, silent };
                float* inMatrixR[3] = { fOutR, silent, silent };
                float* outMatrixL[2] = { outPipeL, silent };
                float* outMatrixR[2] = { outPipeR, silent };

                routerL.process(inMatrixL, outMatrixL, len);
                routerR.process(inMatrixR, outMatrixR, len);

                dspControl.markProcessEnd(0, len, 48000);
            }
        );

        // 5. Tulis Hasil Akhir Core 0 ke I2S DAC
        memcpy(i2s0.getLeftBuffer(), outPipeL, len * sizeof(float));
        memcpy(i2s0.getRightBuffer(), outPipeR, len * sizeof(float));
        i2s0.writeBlock();

        // 6. Geser Pipa
        memcpy(pipeL, nextPipeL, len * sizeof(float));
        memcpy(pipeR, nextPipeR, len * sizeof(float));
    }
}

void setup() {
    Serial.begin(115200);
    
    // ====================================================================
    // 1. Inisialisasi Perangkat Keras Utama (Paling Pertama untuk Amankan RAM)
    // ====================================================================
    // Initialize Bluetooth BEFORE any dynamic allocations to secure BLE Heap & DMA
    if (!bt.begin("RAD-DSP-MIXER")) {
        Serial.println("!!! BLUETOOTH INIT FAILED !!!");
    }
    
    // Initialize Hardware I2S
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0, false, 8, 128);
    i2s1.begin(I2S_NUM_1, 48000, 32, false, 18, 19, 21, -1, -1, false, 8, 128);

    // Initialize Dynamics Look-Up Tables
    RadDSP::LUT::init();
    
    // ====================================================================
    // 2. Attach SEMUA Modul ke Controller (ID harus cocok dengan dspSchema)
    // ====================================================================
    // I2S Input EQ (ID 1-8)
    dspControl.attach(1, &EqI2S_0_L);
    dspControl.attach(2, &EqI2S_0_R);
    dspControl.attach(3, &EqI2S_1_L);
    dspControl.attach(4, &EqI2S_1_R);
    dspControl.attach(5, &EqI2S_2_L);
    dspControl.attach(6, &EqI2S_2_R);
    dspControl.attach(7, &EqI2S_3_L);
    dspControl.attach(8, &EqI2S_3_R);
    // I2S Compressor (ID 9-10)
    dspControl.attach(9, &CompI2S_L);
    dspControl.attach(10, &CompI2S_R);
    // BT Input EQ (ID 11-18)
    dspControl.attach(11, &EqBT_0_L);
    dspControl.attach(12, &EqBT_0_R);
    dspControl.attach(13, &EqBT_1_L);
    dspControl.attach(14, &EqBT_1_R);
    dspControl.attach(15, &EqBT_2_L);
    dspControl.attach(16, &EqBT_2_R);
    dspControl.attach(17, &EqBT_3_L);
    dspControl.attach(18, &EqBT_3_R);
    // Mixers (ID 19-20)
    dspControl.attach(19, &mixerL); 
    dspControl.attach(20, &mixerR); 
    // Master EQ (ID 21-32)
    dspControl.attach(21, &EqMaster_0_L);
    dspControl.attach(22, &EqMaster_0_R);
    dspControl.attach(23, &EqMaster_1_L);
    dspControl.attach(24, &EqMaster_1_R);
    dspControl.attach(25, &EqMaster_2_L);
    dspControl.attach(26, &EqMaster_2_R);
    dspControl.attach(27, &EqMaster_3_L);
    dspControl.attach(28, &EqMaster_3_R);
    dspControl.attach(29, &EqMaster_4_L);
    dspControl.attach(30, &EqMaster_4_R);
    dspControl.attach(31, &EqMaster_5_L);
    dspControl.attach(32, &EqMaster_5_R);
    // FIR Filters (ID 33-34)
    dspControl.attach(33, &firL);
    dspControl.attach(34, &firR);
    // Master Limiters (ID 35-36)
    dspControl.attach(35, &LimiterMaster_L);
    dspControl.attach(36, &LimiterMaster_R);
    // VU Meters (ID 39-44)
    dspControl.attach(39, &meterL);
    dspControl.attach(40, &meterR);
    dspControl.attach(41, &meterAnalogL);
    dspControl.attach(42, &meterAnalogR);
    dspControl.attach(43, &meterBtL);
    dspControl.attach(44, &meterBtR);
    // Output Routers (ID 37-38)
    dspControl.attach(37, &routerL);
    dspControl.attach(38, &routerR);
    
    dspControl.setSchema(dspSchema);
    dspControl.beginSerial(115200);

    // ====================================================================
    // 3. Inisialisasi Parameter Default Semua Modul (configParam)
    // ====================================================================

    // --- Compressor: Type=Comp, Threshold=-20dB, Ratio=4:1, Attack=10ms, Release=100ms ---
    CompI2S_L.setParameter(0, 0.0f);   // Type: Compressor
    CompI2S_L.setParameter(1, -20.0f); // Threshold: -20 dB
    CompI2S_L.setParameter(2, 4.0f);   // Ratio: 4:1
    CompI2S_L.setParameter(3, 10.0f);  // Attack: 10 ms
    CompI2S_L.setParameter(4, 0.0f);   // Hold: 0 ms
    CompI2S_L.setParameter(5, 100.0f); // Release: 100 ms
    CompI2S_L.setParameter(6, 0.0f);   // Makeup Gain: 0 dB
    CompI2S_L.setParameter(100, 0.0f); // Bypass: Off
    CompI2S_R.setParameter(0, 0.0f);
    CompI2S_R.setParameter(1, -20.0f);
    CompI2S_R.setParameter(2, 4.0f);
    CompI2S_R.setParameter(3, 10.0f);
    CompI2S_R.setParameter(4, 0.0f);
    CompI2S_R.setParameter(5, 100.0f);
    CompI2S_R.setParameter(6, 0.0f);
    CompI2S_R.setParameter(100, 0.0f);
    
    // --- Limiter: Type=Limiter, Threshold=-1dB, Ratio=20:1, Attack=0.1ms, Release=50ms ---
    LimiterMaster_L.setParameter(0, 1.0f);   // Type: Limiter
    LimiterMaster_L.setParameter(1, -1.0f);  // Threshold: -1 dB
    LimiterMaster_L.setParameter(2, 20.0f);  // Ratio: 20:1 (Brickwall)
    LimiterMaster_L.setParameter(3, 0.1f);   // Attack: 0.1 ms
    LimiterMaster_L.setParameter(4, 0.0f);   // Hold: 0 ms
    LimiterMaster_L.setParameter(5, 50.0f);  // Release: 50 ms
    LimiterMaster_L.setParameter(6, 0.0f);   // Makeup Gain: 0 dB
    LimiterMaster_L.setParameter(100, 0.0f); // Bypass: Off
    LimiterMaster_R.setParameter(0, 1.0f);
    LimiterMaster_R.setParameter(1, -1.0f);
    LimiterMaster_R.setParameter(2, 20.0f);
    LimiterMaster_R.setParameter(3, 0.1f);
    LimiterMaster_R.setParameter(4, 0.0f);
    LimiterMaster_R.setParameter(5, 50.0f);
    LimiterMaster_R.setParameter(6, 0.0f);
    LimiterMaster_R.setParameter(100, 0.0f);

    // --- Mixer: Gain=0dB, Mute=Off ---
    mixerL.setParameter(0, 0.0f);   // Gain I2S: 0 dB
    mixerL.setParameter(1, 0.0f);   // Gain BT: 0 dB
    mixerL.setParameter(100, 0.0f); // Mute I2S: Off
    mixerL.setParameter(101, 0.0f); // Mute BT: Off
    mixerR.setParameter(0, 0.0f);
    mixerR.setParameter(1, 0.0f);
    mixerR.setParameter(100, 0.0f);
    mixerR.setParameter(101, 0.0f);

    // --- FIR: Inisialisasi awal 16 Taps dengan impulse respons delta (Flat Pass) ---
    float default_fir[16] = {1.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    firL.setCoeffs(default_fir, 16);
    firL.setParameter(3, 16.0f);  // Taps: 16
    firL.setParameter(4, 0.0f);    // Gain: 0 dB
    firL.setParameter(100, 0.0f);  // Bypass: Off
    
    firR.setCoeffs(default_fir, 16);
    firR.setParameter(3, 16.0f);
    firR.setParameter(4, 0.0f);
    firR.setParameter(100, 0.0f);

    // --- Router: In0->Out0 & Out1 = 0dB (1.0 Lin) ---
    routerL.setRouteLinear(0, 0, 1.0f); // In0->Out0 = 0 dB
    routerL.setRouteLinear(0, 1, 1.0f); // In0->Out1 = 0 dB
    routerR.setRouteLinear(0, 0, 1.0f);
    routerR.setRouteLinear(0, 1, 1.0f);
    
    // --- Meter: Decay Factor = 0.95f ---
    meterL.setParameter(1, 0.95f);
    meterR.setParameter(1, 0.95f);

    meterBtL.setParameter(1, 0.95f);
    meterBtR.setParameter(1, 0.95f);

    meterAnalogL.setParameter(1, 0.95f);
    meterAnalogR.setParameter(1, 0.95f);

    // Run Dual Core Synchronization
    dualCore.begin();
    
    // Start Audio Task on Core 1
    RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
    // Empty
}