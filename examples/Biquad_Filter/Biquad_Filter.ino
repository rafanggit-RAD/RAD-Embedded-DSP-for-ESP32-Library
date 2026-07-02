#include <RadDSP.h>

RadDSP::I2S i2s0;
RadDSP::Controller dspControl;
RadDSP::Biquad eqL;
RadDSP::Biquad eqR;

// @Route: I2S_In -> eqL -> I2S0_Out
// @Route: I2S_In -> eqR -> I2S0_Out

#include "dsp_schema.h"

void audioLoop() {
  // Poll serial untuk memproses perintah kontrol real-time dari RadStudio GUI
  dspControl.poll();

  if (i2s0.readBlock()) {
    int len = i2s0.getBufferLength();
    float *i2sL = i2s0.getLeftBuffer();
    float *i2sR = i2s0.getRightBuffer();

    // Proses EQ Biquad secara independen untuk L & R (in-place)
    eqL.process(i2sL, len);
    eqR.process(i2sR, len);

    i2s0.writeBlock();
  }
}

void setup() {
  // Mulai serial UART
  Serial.begin(115200);

  // Daftarkan modul ke Controller
  dspControl.attach(1, &eqL);
  dspControl.attach(2, &eqR);
  dspControl.setSchema(dspSchema);
  dspControl.beginSerial(115200);

  // Inisialisasi EQ Kiri & Kanan (Bell/Peaking EQ, 1000 Hz, +6.0 dB, Q=1.0)
  eqL.setParameter(0, 4.0f);    // Filter Type: Peaking EQ
  eqL.setParameter(1, 1000.0f); // Frequency: 1000 Hz
  eqL.setParameter(2, 6.0f);    // Gain: +6.0 dB
  eqL.setParameter(3, 1.0f);    // Q-Factor: 1.0
  eqL.setParameter(100, 0.0f);  // Bypass: 0 (Aktif)

  eqR.setParameter(0, 4.0f);
  eqR.setParameter(1, 1000.0f);
  eqR.setParameter(2, 6.0f);
  eqR.setParameter(3, 1.0f);
  eqR.setParameter(100, 0.0f);

  // Mulai I2S pada 48 kHz, 32-bit, Master mode
  i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

  // Jalankan Audio Task di Core 1
  RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
  // Kosong (audio berjalan secara asinkron di task Core 1)
}
