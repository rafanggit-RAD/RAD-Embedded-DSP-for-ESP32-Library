#include <RadDSP.h>

RadDSP::I2S i2s0;
RadDSP::Controller dspControl;

// Dua blok gain mono untuk Kiri dan Kanan
RadDSP::Gain gainL;
RadDSP::Gain gainR;

// @Route: I2S_In -> gainL -> I2S0_Out
// @Route: I2S_In -> gainR -> I2S0_Out

#include "dsp_schema.h"

void audioLoop() {
    // Poll UART untuk memproses perintah dari RadStudio GUI
    dspControl.poll();

    if (i2s0.readBlock()) {
        int len = i2s0.getBufferLength();
        float* i2sL = i2s0.getLeftBuffer();
        float* i2sR = i2s0.getRightBuffer();

        // Terapkan penguatan gain secara mono terpisah (in-place)
        gainL.process(i2sL, len);
        gainR.process(i2sR, len);

        i2s0.writeBlock();
    }
}

void setup() {
    Serial.begin(115200);

    // Daftarkan modul Gain ke Controller
    dspControl.attach(1, &gainL);
    dspControl.attach(2, &gainR);
    dspControl.setSchema(dspSchema);
    dspControl.beginSerial(115200);

    // Set gain default ke 0 dB (passthrough)
    gainL.setParameter(0, 0.0f);
    gainR.setParameter(0, 0.0f);

    // Mulai I2S Master pada 48 kHz
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

    // Jalankan Audio Task di Core 1
    RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
    // Kosong (audio loop berjalan secara asinkron di Core 1)
}
