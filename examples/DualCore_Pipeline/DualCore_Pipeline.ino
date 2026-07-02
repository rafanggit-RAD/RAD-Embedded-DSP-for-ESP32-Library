#include <RadDSP.h>

RadDSP::I2S i2s0;
RadDSP::Controller dspControl;
RadDSP::DualCoreWorker dualCore;

// @Route: I2S_In -> I2S0_Out

#include "dsp_schema.h"

// Buffer Ping-Pong Pipeline untuk transfer antar core
float pipeL[128], pipeR[128];
float nextPipeL[128], nextPipeR[128];
float outPipeL[128], outPipeR[128];

void audioLoop() {
    // 1. Poll UART untuk kontrol GUI real-time
    dspControl.poll();

    // 2. Baca audio dari I2S DMA
    if (i2s0.readBlock()) {
        int len = i2s0.getBufferLength();
        float* i2sL = i2s0.getLeftBuffer();
        float* i2sR = i2s0.getRightBuffer();

        // 3. Eksekusi pemrosesan paralel dual-core (Fork-Join)
        dualCore.process(
            [&]() {
                // --- CORE 1: Pemrosesan Tahap 1 (Blok Input) ---
                dspControl.markProcessStart(1);

                // Contoh kalkulasi dasar (Task A): Reduksi volume L/R sebesar 50% (-6 dB)
                for (int i = 0; i < len; i++) {
                    nextPipeL[i] = i2sL[i] * 0.5f;
                    nextPipeR[i] = i2sR[i] * 0.5f;
                }

                dspControl.markProcessEnd(1, len, 48000);
            },
            [&]() {
                // --- CORE 0: Pemrosesan Tahap 2 (Blok Output) ---
                dspControl.markProcessStart(0);

                // Contoh kalkulasi lanjutan (Task B): Reduksi volume lanjutan sebesar 50%
                for (int i = 0; i < len; i++) {
                    outPipeL[i] = pipeL[i] * 0.5f;
                    outPipeR[i] = pipeR[i] * 0.5f;
                }

                dspControl.markProcessEnd(0, len, 48000);
            }
        );

        // 4. Salin hasil dari Core 0 (Blok Output) ke buffer I2S dan write ke DAC
        memcpy(i2sL, outPipeL, len * sizeof(float));
        memcpy(i2sR, outPipeR, len * sizeof(float));
        i2s0.writeBlock();

        // 5. Geser buffer Ping-Pong (Lokal)
        memcpy(pipeL, nextPipeL, len * sizeof(float));
        memcpy(pipeR, nextPipeR, len * sizeof(float));
    }
}

void setup() {
    Serial.begin(115200);

    // Inisialisasi thread penolong di Core 0
    dualCore.begin();

    // Inisialisasi kontroler telemetry
    dspControl.setSchema(dspSchema);
    dspControl.beginSerial(115200);

    // Mulai I2S0 Master pada 48 kHz
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

    // Jalankan Audio Task di Core 1
    RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
    // Kosong (audio loop berjalan secara asinkron di Core 1)
}
