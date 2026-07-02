#include <RadDSP.h>

RadDSP::I2S i2s0;
RadDSP::Bluetooth bt;

void audioLoop() {
  if (i2s0.readBlock()) {
    int len = i2s0.getBufferLength();
    float *i2sL = i2s0.getLeftBuffer();
    float *i2sR = i2s0.getRightBuffer();

    // Siapkan buffer penampung Bluetooth
    float btL[128], btR[128];

    // Baca data ASRC Bluetooth (otomatis dikonversi ke target rate sistem 48000 Hz)
    if (bt.readAudio(btL, btR, len, 48000)) {
      // Salin hasil audio Bluetooth langsung ke buffer I2S DAC
      memcpy(i2sL, btL, len * sizeof(float));
      memcpy(i2sR, btR, len * sizeof(float));
    } else {
      // Jika underflow / tidak ada musik, kosongkan suara
      memset(i2sL, 0, len * sizeof(float));
      memset(i2sR, 0, len * sizeof(float));
    }

    i2s0.writeBlock();
  }
}

void setup() {
  Serial.begin(115200);

  // Memulai inisialisasi Bluetooth A2DP dengan nama 'RAD-DSP-MUSIC'
  bt.begin("RAD-DSP-MUSIC");

  // Mulai I2S pada 48 kHz, 32-bit, Master mode dengan MCLK di GPIO0
  i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

  // Jalankan Audio Task di Core 1
  RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
  // Kosong
}
