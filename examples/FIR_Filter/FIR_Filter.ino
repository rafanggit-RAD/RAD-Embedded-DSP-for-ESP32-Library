#include <RadDSP.h>

RadDSP::I2S i2s0;
RadDSP::Controller dspControl;
RadDSP::FIR firL;
RadDSP::FIR firR;

// @Route: I2S_In -> firL -> I2S0_Out
// @Route: I2S_In -> firR -> I2S0_Out

#include "dsp_schema.h"

// Koefisien filter Low-Pass FIR sederhana (16 Taps) pada 48 kHz
float lp_coeffs[16] = {-0.0016f, -0.0020f, 0.0101f,  0.0384f, 0.0883f, 0.1451f,
                       0.1873f,  0.2031f,  0.1873f,  0.1451f, 0.0883f, 0.0384f,
                       0.0101f,  -0.0020f, -0.0016f, 0.0};

void audioLoop() {
  // Poll serial untuk memproses perintah kontrol real-time dari RadStudio GUI
  dspControl.poll();

  if (i2s0.readBlock()) {
    int len = i2s0.getBufferLength();
    float *i2sL = i2s0.getLeftBuffer();
    float *i2sR = i2s0.getRightBuffer();

    float outL[128], outR[128];

    // Jalankan pemrosesan konvolusi FIR secara independen untuk L & R
    firL.process(i2sL, outL, len);
    firR.process(i2sR, outR, len);

    memcpy(i2sL, outL, len * sizeof(float));
    memcpy(i2sR, outR, len * sizeof(float));
    i2s0.writeBlock();
  }
}

void setup() {
  // Mulai serial UART
  Serial.begin(115200);

  // Daftarkan modul ke Controller
  dspControl.attach(1, &firL);
  dspControl.attach(2, &firR);
  dspControl.setSchema(dspSchema);
  dspControl.beginSerial(115200);

  // Muat koefisien filter FIR ke modul L & R
  firL.setCoeffs(lp_coeffs, 16);
  firL.setParameter(4, 0.0f);   // Output Gain: 0 dB
  firL.setParameter(100, 0.0f); // Bypass: 0 (Aktif)

  firR.setCoeffs(lp_coeffs, 16);
  firR.setParameter(4, 0.0f);
  firR.setParameter(100, 0.0f);

  // Mulai I2S pada 48 kHz, 32-bit, Master mode
  i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

  // Jalankan Audio Task di Core 1
  RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
  // Kosong
}
