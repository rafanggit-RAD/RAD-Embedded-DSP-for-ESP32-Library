#include <RadDSP.h>

RadDSP::I2S i2s0;
RadDSP::Bluetooth bt;
RadDSP::Controller dspControl;

// Mixer 2 channel: Input 1 (I2S ADC), Input 2 (Bluetooth Audio)
RadDSP::Mixer<2> mixerL;
RadDSP::Mixer<2> mixerR;

// @Route: I2S_In -> mixerL -> I2S0_Out
// @Route: BT_In -> mixerL
// @Route: I2S_In -> mixerR -> I2S0_Out
// @Route: BT_In -> mixerR

#include "dsp_schema.h"

void audioLoop() {
    // Poll UART untuk memproses perintah kontrol real-time dari RadStudio GUI
    dspControl.poll();

    if (i2s0.readBlock()) {
        int len = i2s0.getBufferLength();
        float* i2sL = i2s0.getLeftBuffer();
        float* i2sR = i2s0.getRightBuffer();

        // Ambil audio Bluetooth (ASRC Hermite adaptif otomatis menyesuaikan ke 48 kHz)
        float btL[128], btR[128];
        if (!bt.readAudio(btL, btR, len, 48000)) {
            memset(btL, 0, len * sizeof(float));
            memset(btR, 0, len * sizeof(float));
        }

        // Campur I2S Input (Line In) dan Bluetooth Audio menggunakan template Mixer variadik
        float* mixL = mixerL.process(len, i2sL, btL);
        float* mixR = mixerR.process(len, i2sR, btR);

        // Tulis kembali hasil pencampuran ke I2S DAC
        memcpy(i2sL, mixL, len * sizeof(float));
        memcpy(i2sR, mixR, len * sizeof(float));
        i2s0.writeBlock();
    }
}

void setup() {
    Serial.begin(115200);

    // Inisialisasi LUT untuk kalkulasi gain db2lin
    RadDSP::LUT::init();

    // Daftarkan Mixer Kiri dan Kanan ke Controller secara mandiri
    dspControl.attach(1, &mixerL);
    dspControl.attach(2, &mixerR);
    dspControl.setSchema(dspSchema);
    dspControl.beginSerial(115200);

    // Mulai Bluetooth A2DP Sink dengan nama RAD-DSP-MIXER
    bt.begin("RAD-DSP-MIXER");

    // Atur gain default secara terpisah untuk Kiri dan Kanan
    mixerL.setParameter(0, 0.0f);
    mixerL.setParameter(1, 0.0f);
    mixerR.setParameter(0, 0.0f);
    mixerR.setParameter(1, 0.0f);

    // Mulai I2S0 (Master, 48 kHz, MCLK GPIO0)
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

    // Jalankan Audio Task di Core 1
    RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
    // Kosong (audio loop berjalan secara asinkron di Core 1)
}
