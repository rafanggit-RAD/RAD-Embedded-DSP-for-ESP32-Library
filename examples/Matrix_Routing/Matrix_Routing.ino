#include <RadDSP.h>

RadDSP::I2S i2s0;
RadDSP::Controller dspControl;
// Matriks router L & R 3 input ke 2 output
RadDSP::MatrixRouter<3, 2> routerL;
RadDSP::MatrixRouter<3, 2> routerR;

// @Route: I2S_In -> routerL -> I2S0_Out
// @Route: I2S_In -> routerR -> I2S0_Out

#include "dsp_schema.h"

void audioLoop() {
  // Poll serial untuk memproses perintah kontrol real-time dari RadStudio GUI
  dspControl.poll();

  if (i2s0.readBlock()) {
    int len = i2s0.getBufferLength();
    float *i2sL = i2s0.getLeftBuffer();
    float *i2sR = i2s0.getRightBuffer();

    // 3 Input Bus Mono
    float silent[128] = {0};
    float *inL[3] = {i2sL, silent, silent};
    float *inR[3] = {i2sR, silent, silent};

    // 2 Output Bus Mono
    float outL[2][128], outR[2][128];
    float *outPtrL[2] = {outL[0], outL[1]};
    float *outPtrR[2] = {outR[0], outR[1]};

    // Jalankan matriks perkalian perutean audio secara mono terpisah
    routerL.process(inL, outPtrL, len);
    routerR.process(inR, outPtrR, len);

    // Tulis hasil output bus 0 (outL[0]/outR[0]) ke I2S DMA
    memcpy(i2sL, outL[0], len * sizeof(float));
    memcpy(i2sR, outR[0], len * sizeof(float));
    i2s0.writeBlock();
  }
}

void setup() {
  // Mulai serial UART
  Serial.begin(115200);

  // Daftarkan modul ke Controller
  dspControl.attach(1, &routerL);
  dspControl.attach(2, &routerR);
  dspControl.setSchema(dspSchema);
  dspControl.beginSerial(115200);

  // Konfigurasi perutean:
  // Input 0 (I2S) dialirkan ke Output 0 dengan gain 0 dB (1.0f)
  // Input 0 (I2S) dialirkan ke Output 1 dengan gain -3 dB (0.707f)
  routerL.setRouteLinear(0, 0, 1.0f);
  routerL.setRouteLinear(0, 1, 0.707f);

  routerR.setRouteLinear(0, 0, 1.0f);
  routerR.setRouteLinear(0, 1, 0.707f);

  // Mulai I2S pada 48 kHz, 32-bit, Master mode
  i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

  // Jalankan Audio Task di Core 1
  RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
  // Kosong
}
