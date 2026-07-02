#include <RadDSP.h>

RadDSP::I2S i2s0;
RadDSP::Controller dspControl;
RadDSP::Dynamics compL;
RadDSP::Dynamics compR;

// @Route: I2S_In -> compL -> I2S0_Out
// @Route: I2S_In -> compR -> I2S0_Out

#include "dsp_schema.h"

void audioLoop() {
  // Poll serial untuk memproses perintah kontrol real-time dari RadStudio GUI
  dspControl.poll();

  if (i2s0.readBlock()) {
    int len = i2s0.getBufferLength();
    float *i2sL = i2s0.getLeftBuffer();
    float *i2sR = i2s0.getRightBuffer();

    // Proses compressor secara independen untuk L & R (in-place)
    compL.process(i2sL, len);
    compR.process(i2sR, len);

    i2s0.writeBlock();
  }
}

void setup() {
  // Mulai serial UART
  Serial.begin(115200);

  // Inisialisasi Tabel Pencarian Dinamika (Wajib)
  RadDSP::LUT::init();

  // Daftarkan modul ke Controller
  dspControl.attach(1, &compL);
  dspControl.attach(2, &compR);
  dspControl.setSchema(dspSchema);
  dspControl.beginSerial(115200);

  // Inisialisasi Compressor Kiri & Kanan (Threshold -20dB, Ratio 4:1, Attack 10ms, Release 100ms)
  compL.setParameter(0, 0.0f);   // Type: Compressor
  compL.setParameter(1, -20.0f); // Threshold: -20 dB
  compL.setParameter(2, 4.0f);   // Ratio: 4:1
  compL.setParameter(3, 10.0f);  // Attack: 10 ms
  compL.setParameter(4, 0.0f);   // Hold: 0 ms
  compL.setParameter(5, 100.0f); // Release: 100 ms
  compL.setParameter(6, 4.0f);   // Makeup Gain: +4 dB
  compL.setParameter(100, 0.0f); // Bypass: 0 (Aktif)

  compR.setParameter(0, 0.0f);
  compR.setParameter(1, -20.0f);
  compR.setParameter(2, 4.0f);
  compR.setParameter(3, 10.0f);
  compR.setParameter(4, 0.0f);
  compR.setParameter(5, 100.0f);
  compR.setParameter(6, 4.0f);
  compR.setParameter(100, 0.0f);

  // Mulai I2S pada 48 kHz, 32-bit, Master mode
  i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

  // Jalankan Audio Task di Core 1
  RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
  // Kosong
}
