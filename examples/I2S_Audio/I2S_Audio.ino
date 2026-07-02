#include <RadDSP.h>

RadDSP::I2S i2s0;

void audioLoop() {
  // Membaca satu blok data audio stereo dari input (ADC)
  if (i2s0.readBlock()) {
    // Data di dalam getLeftBuffer() & getRightBuffer() siap diproses.
    // Di sini kita langsung menulisnya kembali ke output (DAC) sebagai
    // passthrough murni.
    i2s0.writeBlock();
  }
}

void setup() {
  Serial.begin(115200);

  // Konfigurasi I2S0:
  // Port: I2S_NUM_0
  // Sampling rate: 48000 Hz (48 kHz)
  // Bit depth: 32-bit (untuk ADC/DAC modern)
  // Mode: Master (true)
  // BCK Pin: 26, WS Pin: 25, Data Out Pin: 33, Data In Pin: 23, MCLK Pin: 0
  i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

  // Jalankan task audio secara asinkron pada Core 1
  RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
  // Kosong
}
