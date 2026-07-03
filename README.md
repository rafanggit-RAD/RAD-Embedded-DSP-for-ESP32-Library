# RAD_DSP_LIB

**RAD_DSP_LIB** adalah library pemrosesan sinyal digital (DSP) audio stereo real-time berkinerja tinggi yang dirancang khusus untuk mikrokontroler **ESP32** menggunakan Arduino Core 3.0+ (ESP-IDF 5.x).

Pustaka ini dirancang untuk memproses audio multi-channel dengan latensi ultra-rendah melalui arsitektur dual-core paralel, serta dilengkapi dengan antarmuka Bluetooth A2DP Sink yang disinkronkan secara asinkron.

---

## Fitur Utama

1. **ASRC Kubik Hermite (Hi-Fi Resampler):**
   * Mengonversi audio Bluetooth (44.1 kHz) ke laju jam DAC sistem (**48.0 kHz**) secara asinkron (ASRC).
   * Menggunakan interpolasi kubik Hermite 4-titik (*Horner's Method*) untuk menjaga frekuensi tinggi tetap jernih ($< -0.5\text{ dB}$ redaman pada $20\text{ kHz}$).
   * Dilengkapi algoritma *Frequency Locked Loop* (FLL) otomatis berbasis deteksi level RingBuffer FreeRTOS untuk menghilangkan efek wow, flutter, dan audio stuttering (jitter).
2. **Pipeline Audio Paralel Dual-Core:**
   * Memisahkan beban pemrosesan secara paralel menggunakan *Ping-Pong Buffer Pipeline*:
     * **Core 1:** Menghitung blok input (Biquad EQ per channel, Dynamics Compressor, Mixer).
     * **Core 0:** Menghitung blok output secara simultan (FIR convolution filter, Master Limiter, Matrix Router).
3. **Hardware I2S Master & Slave:**
   * Mendukung konfigurasi fleksibel: **I2S0** berjalan sebagai Master dengan MCLK (GPIO0), sedangkan **I2S1** berjalan sebagai Slave murni.
4. **Matriks Router Adaptif:**
   * Memungkinkan rute dinamis dari input apa pun ke output fisik mana pun dengan gain linier/dB yang independen.
5. **Real-time UART Control (RadStudio GUI):**
   * Mengintegrasikan protokol parser teks UART berbasis modul ID.
   * Terintegrasi penuh dengan GUI tuning desktop **RadStudio** untuk mengubah parameter filter, biquad, compressor, dan routing matrix secara instan dan visual.

---

## Arsitektur Pemrosesan Paralel Dual-Core (Fork-Join Pipeline)

RAD_DSP_LIB dirancang untuk memaksimalkan performa komputasi mikrokontroler dual-core **ESP32** dengan membagi beban kerja secara paralel. Aliran data audio dibagi menjadi dua tahap pemrosesan menggunakan arsitektur **Fork-Join** berbasis *Ping-Pong Buffer Pipeline*:

```mermaid
graph TD
    subgraph Core1 ["Core 1 (Utama)"]
        A[ADC / I2S Input] --> B[Pemrosesan Tahap 1 / Blok Input]
        B --> C[Salin ke nextPipe]
    end
    
    subgraph Core0 ["Core 0 (Helper)"]
        D[Baca dari pipe] --> E[Pemrosesan Tahap 2 / Blok Output]
        E --> F[Salin ke outPipe]
    end
    
    C -. DualCore Sync .-> D
    F --> G[DAC / I2S Output]
```

### 1. Core 1 (Utama / Main Processor)
Core 1 bertanggung jawab atas penanganan I/O real-time (baca input hardware), manajemen kontrol eksternal, dan kalkulasi pemrosesan tahap awal (Blok Input):
* **Alur Tugas:**
  1. Membaca blok sampel audio terbaru dari perangkat input fisik (ADC/I2S).
  2. Melakukan kalkulasi pemrosesan audio tahap awal (Task A).
  3. Menyalin hasil kalkulasi ke buffer perantara (`nextPipe`) untuk dikirim ke Core 0.

### 2. Core 0 (Co-Processor / Helper)
Core 0 dialokasikan khusus untuk menangani kalkulasi matematika intensif secara asinkron dan terpisah dari aktivitas I/O Core 1:
* **Alur Tugas:**
  1. Membaca data buffer hasil kalkulasi Core 1 pada siklus sebelumnya (`pipe`).
  2. Melakukan kalkulasi pemrosesan audio tahap akhir (Task B).
  3. Menyalin hasil kalkulasi akhir ke buffer keluaran (`outPipe`) untuk dituliskan ke DAC/I2S.

### 3. Sinkronisasi Ping-Pong Buffer
* Untuk menghindari *race condition* (di mana satu core menulis ke buffer yang sedang dibaca core lain), pustaka menggunakan metode **Ping-Pong Pipeline**.
* Pada siklus waktu $t$, Core 1 memproses sampel blok audio baru ($N$), sedangkan Core 0 secara simultan memproses hasil kalkulasi Core 1 dari blok sampel sebelumnya ($N-1$).
* Koordinasi fork-join antar core diatur menggunakan *FreeRTOS Binary Semaphore* berprioritas tinggi melalui pemanggilan kelas `dualCore.process()`. Setelah kedua core menyelesaikan tugasnya pada siklus berjalan, buffer ditukar secara atomik.

---

## Struktur Folder Proyek

```text
RAD_DSP_LIB/
├── src/                      # Kode sumber C++ Pustaka DSP
│   ├── RadDSP.h              # Header Utama
│   ├── RadI2S.cpp/.h         # Driver I2S Master/Slave
│   ├── RadBluetooth.cpp/.h   # Kontroler Bluetooth Classic & ASRC
│   ├── RadMatrix.h           # Template MatrixRouter
│   ├── RadMixer.h            # Blok Mixer2 dengan Sibling Sync
│   ├── RadBiquad.cpp/.h      # Filter Biquad IIR (LowPass, HighPass, EQ)
│   ├── RadDynamics.cpp/.h    # Compressor & Limiter
│   └── RadControl.cpp/.h     # Telemetry & Parser UART Serial
├── examples/                 # Contoh sketch mandiri untuk masing-masing blok DSP
│   ├── Biquad_Filter/        # Contoh filter biquad EQ tunggal
│   ├── Bluetooth_Audio/      # Contoh penerimaan Bluetooth audio asinkron
│   ├── Dynamics_Processor/   # Contoh kompresor dinamika audio
│   ├── FIR_Filter/           # Contoh filter konvolusi FIR
│   ├── I2S_Audio/            # Contoh passthrough audio I2S dasar
│   ├── Matrix_Routing/       # Contoh perutean matriks audio adaptif
│   └── Mixer_Stereo/         # Contoh pencampur dua input audio stereo
├── test/                     # Sketsa pengujian integrasi & diagnosa sistem
│   ├── Master_Passthrough/   # Sketsa utama pipeline dual-core & telemetry
│   └── BT_Diagnostic_Test/   # Alat diagnosa koneksi Bluetooth & NVS
├── tools/
│   ├── RadStudio.py          # Aplikasi GUI Desktop Real-time Tuning
│   ├── RadScanner.py         # Scanner Route & Injektor Schema
│   └── read_serial.py        # Logger Serial UART
└── references/               # Pustaka luring dan dokumentasi referensi SDK
```

---

## Cara Memulai Cepat (Quick Start)

### 1. Instalasi Library
Salin seluruh folder `RAD_DSP_LIB` ini ke direktori pustaka Arduino global komputer Anda:
* **Windows:** `C:\Users\<Nama_User>\Documents\Arduino\libraries\RAD_DSP_LIB`

### 2. Flashing Firmware ke ESP32
1. Buka Arduino IDE atau gunakan terminal `arduino-cli`.
2. Buka salah satu berkas di folder `examples/` atau `test/Master_Passthrough/Master_Passthrough.ino`.
3. Pilih board berbasis ESP32, misalnya **LOLIN32 Lite** (atau **ESP32 Dev Module** sesuai modul hardware ESP32 yang Anda gunakan).
4. Kompilasi dan unggah (flash) firmware ke port serial mikrokontroler Anda (misalnya `COM12`).

### 3. Mengontrol Menggunakan RadStudio GUI
1. Pastikan python telah terinstal beserta library `pyserial` dan `tkinter`.
2. Jalankan aplikasi GUI dari terminal.

Dokumentasi ini berisi daftar kelas, fungsi, contoh inisialisasi, cara penggunaan, serta pemetaan parameter untuk setiap modul DSP yang didukung oleh pustaka **RAD_DSP_LIB** dalam mode pemrosesan mono murni.

### 1. Daftar Kelas & Fungsi DSP Library

#### A. Kelas `RadDSP::I2S`
Digunakan untuk komunikasi audio dengan chip DAC/ADC hardware eksternal.
* **Inisialisasi (`setup()`):**
  ```cpp
  // Mulai I2S0 sebagai Master pada 48 kHz, 32-bit (WS/LRCK di GPIO25, BCK di GPIO26, MCLK di GPIO0)
  i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  if (i2s0.readBlock()) { // Membaca satu blok data audio (mengisi buffer internal)
      int len = i2s0.getBufferLength();     // Dapatkan ukuran buffer (misal 128 sampel)
      float* i2sL = i2s0.getLeftBuffer();   // Dapatkan pointer data audio Kiri
      float* i2sR = i2s0.getRightBuffer();  // Dapatkan pointer data audio Kanan
      
      // ... Lakukan pemrosesan audio di sini ...
      
      i2s0.writeBlock(); // Kirim kembali buffer audio internal ke DAC fisik
  }
  ```

---

#### B. Kelas `RadDSP::Bluetooth`
Mengaktifkan penerimaan Bluetooth A2DP Sink dengan resampling asinkron Hi-Fi bawaan (Hermite Cubic ASRC).
* **Inisialisasi (`setup()`):**
  ```cpp
  bt.begin("RAD-DSP-MIXER"); // Nama perangkat Bluetooth saat ditemukan HP
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  float btL[128], btR[128];
  // Membaca audio BT dan mengoversinya secara otomatis ke target sample rate (misal 48000 Hz)
  if (!bt.readAudio(btL, btR, len, 48000)) {
      // Jika HP belum terkoneksi atau ringbuffer kosong, isi dengan hening (mute) secara aman
      memset(btL, 0, len * sizeof(float));
      memset(btR, 0, len * sizeof(float));
  }
  ```

---

#### C. Kelas `RadDSP::Biquad`
Filter Biquad IIR Mono tunggal untuk Equalizer, Crossover (Highpass/Lowpass), Shelf, dan Notch.
* **Inisialisasi & Parameter (`setup()`):**
  ```cpp
  // Contoh inisialisasi Bell/Peaking EQ, 1000 Hz, Boost +6.0 dB, Q=1.0
  eq.setParameter(0, 4.0f);    // 0: Filter Type (1=LPF, 2=HPF, 3=BPF, 4=Peaking, 5=LowShelf, 6=HighShelf)
  eq.setParameter(1, 1000.0f); // 1: Cutoff/Center Frequency (Hz)
  eq.setParameter(2, 6.0f);    // 2: Gain (dB)
  eq.setParameter(3, 1.0f);    // 3: Q-Factor
  eq.setParameter(100, 0.0f);  // 100: Bypass (0.0f = Aktif, 1.0f = Bypass)
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  // Metode 1: Out-of-Place (Input dan Output di buffer terpisah)
  eq.process(inputBuffer, outputBuffer, len);

  // Metode 2: In-Place (Hasil ditulis langsung menimpa buffer input)
  eq.process(audioBuffer, len);
  ```

---

#### D. Kelas `RadDSP::Dynamics`
Modul dinamika amplitudo Mono tunggal untuk Compressor, Limiter, Expander, dan Noise Gate.
* **Inisialisasi & Parameter (`setup()`):**
  ```cpp
  // Contoh inisialisasi Compressor (Threshold -20dB, Ratio 4:1, Attack 10ms, Release 100ms)
  comp.setParameter(0, 0.0f);   // 0: Type (0=Compressor, 1=Limiter, 2=Expander, 3=Noise Gate)
  comp.setParameter(1, -20.0f); // 1: Threshold (dB)
  comp.setParameter(2, 4.0f);   // 2: Ratio (diabaikan pada Limiter & Noise Gate)
  comp.setParameter(3, 10.0f);  // 3: Attack Time (ms)
  comp.setParameter(4, 0.0f);   // 4: Hold Time (ms)
  comp.setParameter(5, 100.0f); // 5: Release Time (ms)
  comp.setParameter(6, 0.0f);   // 6: Makeup Gain (dB)
  comp.setParameter(7, 0.0f);   // 7: Sidechain SC Filter Type (0=None, 1=HPF, 2=LPF, 3=BPF)
  comp.setParameter(8, 1000.0f);// 8: SC Filter Freq (Hz)
  comp.setParameter(100, 0.0f); // 100: Bypass (0.0f = Aktif, 1.0f = Bypass)
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  // Metode 1: Standar Compressor/Limiter (in-place)
  comp.process(audioBuffer, len);

  // Metode 2: Compressor dengan input Sidechain terpisah
  comp.processSidechain(audioBuffer, sidechainSensorBuffer, len);
  ```

---

#### E. Kelas `RadDSP::Mixer<N>`
Pencampur $N$ input audio mono menjadi 1 output mono dengan gain dan mute independen.
* **Inisialisasi & Parameter (`setup()`):**
  ```cpp
  RadDSP::Mixer<2> mixer; // Mixer 2 input mono
  mixer.setParameter(0, 0.0f);   // 0 s.d N-1: Gain Input 1 (dB)
  mixer.setParameter(1, -6.0f);  // Gain Input 2 (dB)
  mixer.setParameter(100, 0.0f); // 100 s.d 100+N-1: Mute Input 1 (0.0f = Aktif, 1.0f = Mute)
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  // Mencampur buffer 'in1' dan 'in2' secara variadik
  float* mixed = mixer.process(len, in1, in2); // Mengembalikan pointer buffer hasil pencampuran
  ```

---


#### G. Kelas `RadDSP::FIR`
Filter Konvolusi Respon Impuls Terbatas (FIR) Mono tunggal untuk pemrosesan kabinet gitar (IR), koreksi fase, atau room correction.
* **Inisialisasi & Parameter (`setup()`):**
  ```cpp
  float lp_coeffs[16] = {1.0f, 0.0f /* ... sisanya 0 ... */};
  fir.setCoeffs(lp_coeffs, 16);  // Muat array koefisien filter FIR
  fir.setParameter(4, 0.0f);     // 4: Output Gain (dB)
  fir.setParameter(100, 0.0f);   // 100: Bypass (0.0f = Aktif, 1.0f = Bypass)
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  fir.process(inputBuffer, outputBuffer, len); // Jalankan konvolusi FIR
  ```

---

#### H. Kelas `RadDSP::MatrixRouter<IN, OUT>`
Matriks perutean audio mono adaptif untuk mencampur dan merutekan input apa saja ke output mana saja secara dinamis.
* **Inisialisasi & Parameter (`setup()`):**
  ```cpp
  RadDSP::MatrixRouter<3, 2> router; // 3 input mono ke 2 output mono
  router.setRouteLinear(0, 0, 1.0f);   // Rute Input 0 ke Output 0 dengan gain 0 dB (1.0f)
  router.setRouteLinear(0, 1, 0.707f); // Rute Input 0 ke Output 1 dengan gain -3 dB (0.707f)
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  float* inputs[3] = {in1, in2, in3};
  float* outputs[2] = {out1, out2};
  router.process(inputs, outputs, len); // Jalankan matrix mixing
  ```

---

#### I. Kelas `RadDSP::Gain`
Blok penguat (gain) mono tunggal dengan opsi mute dan inversi fase (invert).
* **Inisialisasi & Parameter (`setup()`):**
  ```cpp
  // Contoh inisialisasi volume 0 dB, mute tidak aktif, invert fase tidak aktif
  gain.setParameter(0, 0.0f);   // 0: Gain Volume (dB)
  gain.setParameter(1, 0.0f);   // 1: Mute (0.0f = Aktif, 1.0f = Mute)
  gain.setParameter(2, 0.0f);   // 2: Phase Invert (0.0f = Normal, 1.0f = Invert)
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  // Metode 1: Out-of-Place (Input dan Output di buffer terpisah)
  gain.process(inputBuffer, outputBuffer, len);

  // Metode 2: In-Place (Hasil ditulis langsung menimpa buffer input)
  gain.process(audioBuffer, len);
  ```

---

#### J. Kelas `RadDSP::Controller`
Pengatur protokol telemetri serial UART GUI RadStudio.
* **Inisialisasi & Parameter (`setup()`):**
  ```cpp
  dspControl.attach(1, &eqL);        // Daftarkan modul ke ID unik
  dspControl.setSchema(dspSchema);   // Atur JSON skema perutean grafis
  dspControl.beginSerial(115200);    // Aktifkan UART
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  dspControl.poll(); // Wajib dipanggil di awal audio loop untuk memproses perintah eksternal
  ```

---

#### K. Kelas `RadDSP::Meter`
Modul pengukur tingkat sinyal audio (Peak VU Meter) secara real-time yang menyajikan data desibel aktual (dBFS) ke sistem telemetri serial.
* **Inisialisasi & Parameter (`setup()`):**
  ```cpp
  meter.setParameter(1, 0.95f); // 1: Decay Factor (0.5 s.d 0.999f) untuk kelancaran pergerakan VU LED
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  meter.process(audioBuffer, len); // Melacak nilai puncak desibel (dBFS) blok audio saat ini
  ```
* **Telemetry Query (Tuning GUI):**
  * `getParameter(0)`: Mengembalikan nilai desibel aktual saat ini dalam rentang `-80.0 dBFS` s.d `+6.0 dBFS`.

---

#### L. Kelas `RadDSP::FFT`
Penganalisis spektrum frekuensi audio real-time bawaan (Fast Fourier Transform) untuk penganalisis visual (Spectrogram / RTA).
* **Inisialisasi (`setup()`):**
  ```cpp
  RadDSP::FFT fft(512); // Membuat instance FFT dengan ukuran 512 (harus pangkat 2: 256, 512, 1024, 2048)
  fft.begin();          // Alokasikan memori tabel twiddle & buffer complex
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  // 1. Ambil data sampel (misal 512) ke array float lokal
  float fftInput[512]; 
  memcpy(fftInput, audioBuffer, 512 * sizeof(float));

  // 2. Terapkan Hann Window untuk meredam kebocoran spektrum (spectral leakage)
  fft.applyWindow(fftInput);

  // 3. Eksekusi FFT
  float magnitudes[256]; // Output magnitudo berukuran setengah dari FFT size (N/2)
  fft.process(fftInput, magnitudes); 

  // 4. Cari nilai bin puncak frekuensi desibel
  float peakFreq = fft.getFrequency(binIndex, 48000.0f); // Dapatkan frekuensi aktual bin ke-N
  ```

---

#### M. Fungsi Global `RadDSP::startAudioTask()`
Pembangkit RTOS Task berprioritas real-time tertinggi (`configMAX_PRIORITIES - 1`) untuk mengunci eksekusi audio loop di Core tertentu tanpa gangguan watchdog timer Arduino.
* **Sintaks:**
  ```cpp
  RadDSP::startAudioTask(AudioTaskCallback callback, int coreID = 1, bool killArduinoLoop = true);
  ```
* **Parameter:**
  * `callback`: Fungsi audio loop kustom tanpa parameter (misal: `audioLoop`).
  * `coreID`: Core ESP32 target penugasan loop audio (`0` atau `1`).
  * `killArduinoLoop`: Jika `true` (sangat direkomendasikan), loop `loop()` bawaan Arduino akan dihentikan paksa agar core kembali 100% dialokasikan ke loop audio Anda.

---

#### N. Kelas `RadDSP::DualCoreWorker`
Pengendali pemrosesan paralel (Fork-Join Architecture) yang mengoordinasikan dua Core untuk membagi beban kerja secara simultan tanpa menimbulkan jeda/latensi.
* **Inisialisasi (`setup()`):**
  ```cpp
  dualCore.begin(); // Panggil sekali di setup() untuk menyalakan thread penolong di Core 0
  ```
* **Cara Penggunaan (`audioLoop()`):**
  ```cpp
  // Menjalankan tugas Core 1 dan Core 0 secara simultan. 
  // Eksekusi akan memblokir (wait) di baris ini sampai KEDUA Core selesai menghitung.
  dualCore.process(
      [&]() {
          // --- KODE UNTUK CORE 1 ---
          EqI2S_0_L.process(i2sL, len);
      },
      [&]() {
          // --- KODE UNTUK CORE 0 ---
          firL.process(pipeL, fOutL, len);
      }
  );
  ```

---

### Panduan Menghemat RAM: Rilis Memori Bluetooth
Bluetooth A2DP Sink memakan memori Heap (RAM) ESP32 yang sangat besar (~120 KB dinamis + ~20 KB statis). Jika proyek DSP Anda **tidak memerlukan Bluetooth Audio** (misalnya pengolah gitar / mic analog murni), gunakan langkah berikut untuk menghemat RAM secara total:

1. **Jangan Panggil `bt.begin()`**:
   * Bluedroid stack dan RingBuffer Bluetooth tidak akan dialokasikan ke Heap dinamis, menghemat **120 KB RAM**.
2. **Panggil `bt.releaseMemory()` di awal `setup()`**:
   * Metode ini membebaskan RAM controller hardware statis ESP32, mengembalikan **~20 KB s.d 40 KB RAM** statis ke sistem heap umum.

**Contoh Sketsa Hemat RAM:**
```cpp
void setup() {
    Serial.begin(115200);

    // 1. Rilis paksa memori Bluetooth controller hardware
    bt.releaseMemory();

    // 2. Lanjutkan inisialisasi I2S dan DSP yang membutuhkan RAM besar
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);
    ...
}
```

---

### Otomatisasi Pemetaan Skema Topologi (RadScanner & dsp_schema.h)
Untuk merender diagram routing DSP di RadStudio GUI, ESP32 memerlukan blueprint JSON. Anda tidak perlu menyusun JSON ini secara manual. Gunakan **RadScanner.py**:

1. **Letakkan `RadScanner.py`** di dalam folder sketsa `.ino` Anda (misalnya folder `test/Master_Passthrough/`).
2. **Tulis Komentar `@Route`** di file `.ino` Anda untuk mendefinisikan jalur aliran audio. Jalur ini akan dipindai oleh scanner untuk menyusun diagram visual pada GUI.
   
   **Aturan Penulisan Sintaks `@Route`:**
   * Ditulis sebagai komentar C++ biasa dengan awalan `// @Route:` atau `//@Route:`.
   * Node input/output fisik bawaan sistem:
     * Input fisik: `I2S_In` dan `BT_In`.
     * Output fisik: `I2S0_Out` dan `I2S1_Out`.
   * Aliran audio dipisahkan dengan tanda panah `->` (tanpa spasi ketat, spasi opsional dibersihkan otomatis).
   * Nama node di tengah harus mencerminkan nama variabel instance modul C++ DSP Anda secara persis (case-sensitive).
   
   **Contoh Komentar `@Route` pada sketsa `.ino`:**
   ```cpp
   // @Route: I2S_In -> meterAnalogL -> EqI2S_0_L -> CompI2S_L -> mixerL
   // @Route: BT_In -> meterBtL -> EqBT_0_L -> mixerL
   // @Route: mixerL -> EqMaster_0_L -> firL -> LimiterMaster_L -> meterL -> routerL -> I2S0_Out
   // @Route: routerL -> I2S1_Out
   ```

3. Jalankan command terminal di dalam folder sketsa tersebut:
   ```bash
   python RadScanner.py
   ```
4. **Hasil Output**:
   * Scanner memproses file `.ino` lokal Anda, lalu membuat berkas **`dsp_schema.h`** di folder yang sama yang membungkus objek JSON `dspSchema` terkompresi.
5. **Cara Penggunaan**:
   Hapus deklarasi manual string `dspSchema` lama di berkas `.ino` Anda, lalu sertakan berkas header yang digenerate di atas:
   ```cpp
   #include "dsp_schema.h" // Berkas auto-generated oleh RadScanner
   ```

---

### Tabel Referensi Konfigurasi Parameter (`configParam`)

Tabel di bawah ini mendokumentasikan rentang nilai (`min`, `max`), resolusi langkah (`step`), satuan (`unit`), dan skala (`scale`) untuk setiap parameter di semua blok DSP. Konfigurasi ini digunakan oleh **RadStudio GUI** untuk merender knob, slider, dan combobox secara presisi.

#### Biquad (EQ / Filter IIR)

| Param ID | Nama | Min | Max | Step | Unit | Tipe Widget |
|----------|------|-----|-----|------|------|-------------|
| `0` | Filter Type | — | — | — | — | Combobox: `0:LowPass, 1:HighPass, 2:BandPass, 3:Peaking, 4:LowShelf, 5:HighShelf` |
| `1` | Frequency | 20.0 | 20000.0 | 1.0 | Hz | Knob (Log) |
| `2` | Gain | -24.0 | 24.0 | 0.1 | dB | Knob (Lin) |
| `3` | Q-Factor | 0.05 | 20.0 | 0.05 | Q | Knob (Lin) |
| `100` | Bypass | 0 | 1 | 1 | — | Checkbox |

#### Dynamics (Compressor / Limiter / Expander / Gate)

| Param ID | Nama | Min | Max | Step | Unit | Tipe Widget |
|----------|------|-----|-----|------|------|-------------|
| `0` | Dynamics Type | — | — | — | — | Combobox: `0:Compressor, 1:Limiter, 2:Expander, 3:Gate` |
| `1` | Threshold | -80.0 | 0.0 | 0.1 | dB | Knob (Lin) |
| `2` | Ratio | 1.0 | 20.0 | 0.1 | Ratio | Knob (Lin) |
| `3` | Attack | 0.1 | 1000.0 | 0.1 | ms | Knob (Lin) |
| `4` | Hold | 0.0 | 1000.0 | 1.0 | ms | Knob (Lin) |
| `5` | Release | 1.0 | 5000.0 | 1.0 | ms | Knob (Lin) |
| `6` | Makeup Gain | -24.0 | 24.0 | 0.1 | dB | Knob (Lin) |
| `7` | SC Filter Type | — | — | — | — | Combobox: `0:Bypass, 1:HighPass, 2:LowPass, 3:BandPass` |
| `8` | SC Frequency | 20.0 | 20000.0 | 1.0 | Hz | Knob (Log) |
| `100` | Bypass | 0 | 1 | 1 | — | Checkbox |

#### Mixer\<N\> (Pencampur N-Input)

| Param ID | Nama | Min | Max | Step | Unit | Tipe Widget |
|----------|------|-----|-----|------|------|-------------|
| `0` s.d `N-1` | Gain Input (per ch) | -80.0 | 12.0 | 0.1 | dB | Knob (Lin) |
| `100` s.d `100+N-1` | Mute Input (per ch) | 0 | 1 | 1 | — | Checkbox |

#### Gain (Volume / Mute / Invert)

| Param ID | Nama | Min | Max | Step | Unit | Tipe Widget |
|----------|------|-----|-----|------|------|-------------|
| `0` | Gain Volume | -80.0 | 12.0 | 0.1 | dB | Knob (Lin) |
| `1` | Mute | 0 | 1 | 1 | — | Checkbox |
| `2` | Phase Invert | 0 | 1 | 1 | — | Checkbox |

#### FIR (Filter Konvolusi)

| Param ID | Nama | Min | Max | Step | Unit | Tipe Widget |
|----------|------|-----|-----|------|------|-------------|
| `0` | Target Tap Index | 0.0 | 511.0 | 1.0 | Index | (Serial-only) target tap index |
| `1` | Staging Coeff Value | -1.5 | 1.5 | 0.0001 | Coeff | (Serial-only) value of staging tap |
| `2` | Commit Trigger | 0 | 1 | 1 | — | (Serial-only) 1.0 = Swap Buffers |
| `3` | Taps Number | 16.0 | 512.0 | 16.0 | Taps | Knob (Lin) |
| `4` | Output Gain | -24.0 | 24.0 | 0.1 | dB | Knob (Lin) |
| `100` | Bypass | 0 | 1 | 1 | — | Checkbox |

#### MatrixRouter\<IN, OUT\> (Matriks Routing)

| Param ID | Nama | Min | Max | Step | Unit | Tipe Widget |
|----------|------|-----|-----|------|------|-------------|
| `i*OUT+j` | InN→OutM | 0.0 | 1.5 | 0.01 | Lin | Entry (Grid) |

> **Catatan:** Nilai Lin `1.0` setara dengan 0 dB (passthrough), `0.0` = mute, `0.707` ≈ -3 dB. Gain linier di atas `1.0` berarti boost.

#### Meter (VU Meter)

| Param ID | Nama | Min | Max | Step | Unit | Tipe Widget |
|----------|------|-----|-----|------|------|-------------|
| `0` | Level | -80.0 | 6.0 | — | dBFS | Canvas Bar LED (Real-time 120ms) |
| `1` | Decay Factor | 0.5 | 0.999 | 0.001 | Decay | Knob (Lin) |

---

### 2. Cara Mengontrol DSP via Serial UART (External MCU / PC)

Kendali jarak jauh antar mikrokontroler atau dari PC menggunakan protokol **teks JSON berbasis baris** (`\n`).

#### Format Perintah Set Parameter:
```json
{"id":<moduleID>,"p":<paramID>,"v":<value>}
```

#### Format Perintah Get Parameter:
```json
{"id":<moduleID>,"req":<paramID>}
```

#### Contoh Perintah UART:
1. **Mengubah Gain Peaking EQ (Module ID 1, Parameter 2) ke +4.5 dB:**
   ```json
   {"id":1,"p":2,"v":4.5}
   ```
2. **Mengubah Frekuensi Cutoff LPF (Module ID 2, Parameter 1) ke 1200 Hz:**
   ```json
   {"id":2,"p":1,"v":1200.0}
   ```
3. **Mengaktifkan Bypass pada Compressor (Module ID 5, Parameter 100):**
   ```json
   {"id":5,"p":100,"v":1.0}
   ```
4. **Membaca Parameter 1 (Frequency) dari Biquad (Module ID 1):**
   ```json
   {"id":1,"req":1}
   ```
   *ESP32 akan membalas dengan JSON string:* `{"ack":1,"id":1,"p":1,"v":1000.0}`
5. **Membaca Telemetry Sistem DSP (Beban Core & RAM):**
   ```json
   {"id":255,"req":0}
   ```
   *ESP32 akan membalas dengan JSON:* `{"sys":1,"c0":12.5,"c1":32.1,"ramF":145230,"ramT":320000}`
6. **Meminta Skema Topologi (JSON Schema) untuk RadStudio:**
   ```json
   {"id":254,"req":0}
   ```
   *ESP32 akan membalas dengan skema routing JSON lengkap.*

---

### 3. Basic Template (Passthrough Audio & Inisialisasi Dasar)

Di bawah ini adalah sketsa dasar yang benar-benar bersih untuk memulai proyek. Sketsa ini hanya melakukan inisialisasi modul utama (I2S, Bluetooth, Controller) dan mengalirkan sinyal (*passthrough* & *mix*) secara langsung dari input (ADC & Bluetooth) ke output (DAC) dengan penjelasan komentar di setiap baris kodenya:

```cpp
#include <RadDSP.h>

// Instansiasi objek driver I2S fisik untuk komunikasi ADC/DAC eksternal
RadDSP::I2S i2s0;

// Instansiasi objek driver Bluetooth untuk menerima sinyal nirkabel (sink A2DP)
RadDSP::Bluetooth bt;

// Instansiasi objek Controller untuk komunikasi telemetry & kontrol RadStudio GUI via UART
RadDSP::Controller dspControl;

// @Route: I2S_In -> I2S0_Out
// @Route: BT_In -> I2S0_Out

#include "dsp_schema.h"

// Fungsi callback audio loop yang akan dieksekusi terus-menerus di Core 1
void audioLoop() {
    // Membaca input UART serial secara berkala untuk menerima kontrol dari GUI RadStudio
    dspControl.poll();

    // Membaca satu blok sampel data audio secara real-time dari ADC (I2S DMA)
    if (i2s0.readBlock()) {
        // Mendapatkan panjang buffer (banyaknya sampel per blok pemrosesan, default: 128)
        int len = i2s0.getBufferLength();
        
        // Mendapatkan pointer memori float dari buffer audio saluran Kiri (Left)
        float* i2sL = i2s0.getLeftBuffer();
        
        // Mendapatkan pointer memori float dari buffer audio saluran Kanan (Right)
        float* i2sR = i2s0.getRightBuffer();

        // Membaca data audio dari Bluetooth (jika musik sedang diputar dari HP)
        // Data Bluetooth (44.1 kHz) secara otomatis di-resample ke 48 kHz agar sinkron dengan laju jam DAC
        float btL[128], btR[128];
        if (!bt.readAudio(btL, btR, len, 48000)) {
            // Jika Bluetooth tidak aktif atau HP belum terhubung, bersihkan buffer BT (di-mute)
            memset(btL, 0, len * sizeof(float));
            memset(btR, 0, len * sizeof(float));
        }

        // --- PEMROSESAN PASSTHROUGH / MIXING SEDERHANA ---
        // Sinyal input fisik dicampur secara langsung dengan sinyal Bluetooth tanpa efek tambahan
        for (int i = 0; i < len; i++) {
            i2sL[i] = i2sL[i] + btL[i]; // Passthrough & Mix saluran Kiri
            i2sR[i] = i2sR[i] + btR[i]; // Passthrough & Mix saluran Kanan
        }

        // Menuliskan kembali hasil pemrosesan buffer internal ke DAC fisik (I2S DMA Output)
        i2s0.writeBlock();
    }
}

void setup() {
    // Mulai port Serial untuk debugging (baud rate 115200)
    Serial.begin(115200);

    // Inisialisasi Look-Up Tables untuk kalkulasi desibel (dB) yang cepat
    RadDSP::LUT::init();

    // Mengatur skema diagram routing dan mengaktifkan komunikasi GUI RadStudio pada baud rate 115200
    dspControl.setSchema(dspSchema);
    dspControl.beginSerial(115200);

    // Memulai stack Bluetooth Classic A2DP Sink dengan nama perangkat "RAD-DSP-PASSTHROUGH"
    bt.begin("RAD-DSP-PASSTHROUGH");

    // Inisialisasi port hardware I2S0 sebagai Master pada 48 kHz, 32-bit kedalaman bit.
    // Pin BCK (GPIO26), Pin WS/LRCK (GPIO25), Pin Data Out/DAC (GPIO33), Pin Data In/ADC (GPIO23), Pin MCLK (GPIO0)
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

    // Menjalankan callback audioLoop() secara berulang di Core 1 dengan prioritas tertinggi
    // Parameter ketiga set ke 'true' untuk mematikan task loop() Arduino bawaan agar menghemat CPU
    RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
    // Loop kosong karena audioLoop() telah dialihkan secara eksklusif ke FreeRTOS task di Core 1
}
```

---

### 4. Contoh Sketsa Integrasi Efek Lengkap (Full DSP Chain Example)

Di bawah ini adalah template lengkap yang mendemonstrasikan inisialisasi semua blok DSP yang didukung oleh pustaka menggunakan pemrosesan murni mono untuk Kiri (L) dan Kanan (R) secara independen:

```cpp
#include <RadDSP.h>

// 1. Instansiasi Hardware & Kontroler
RadDSP::I2S i2s0;
RadDSP::Bluetooth bt;
RadDSP::Controller dspControl;
RadDSP::DualCoreWorker dualCore;

// 2. Instansiasi Blok-Blok DSP (Mono Terpisah untuk Kiri & Kanan)
RadDSP::Biquad eqInputL, eqInputR;             // ID 1 & 2
RadDSP::Dynamics compressorL, compressorR;     // ID 3 & 4
RadDSP::Mixer<2> mixerL, mixerR;               // ID 5 & 6
RadDSP::Biquad eqMasterL, eqMasterR;           // ID 7 & 8
RadDSP::Dynamics limiterL, limiterR;           // ID 9 & 10
RadDSP::FIR firL, firR;                        // ID 11 & 12
RadDSP::MatrixRouter<3, 2> routerL, routerR;   // ID 13 & 14

// @Route: I2S_In -> eqInputL -> compressorL -> mixerL -> eqMasterL -> firL -> limiterL -> routerL -> I2S0_Out
// @Route: I2S_In -> eqInputR -> compressorR -> mixerR -> eqMasterR -> firR -> limiterR -> routerR -> I2S0_Out
// @Route: BT_In -> mixerL
// @Route: BT_In -> mixerR
// @Route: routerL -> I2S1_Out
// @Route: routerR -> I2S1_Out

#include "dsp_schema.h"

// Buffer Ping-Pong Pipeline untuk transfer antar core
float pipeL[128], pipeR[128];
float nextPipeL[128], nextPipeR[128];
float outPipeL[128], outPipeR[128];

// Audio Loop Utama (Dijalankan di Core 1)
void audioLoop() {
    // 1. Baca Perintah Serial masuk untuk RadStudio GUI
    dspControl.poll();
    
    // 2. Ambil Audio dari I2S DMA
    if (i2s0.readBlock()) {
        int len = i2s0.getBufferLength();
        float* i2sL = i2s0.getLeftBuffer();
        float* i2sR = i2s0.getRightBuffer();
        
        // 3. Ambil Audio Bluetooth (ASRC Kubik Hermite otomatis menyesuaikan sample rate)
        float btL[128], btR[128];
        if (!bt.readAudio(btL, btR, len, 48000)) {
            memset(btL, 0, len * sizeof(float));
            memset(btR, 0, len * sizeof(float));
        }

        // 4. Proses Dual Core secara Paralel (Ping-Pong Pipeline)
        dualCore.process(
            [&]() {
                // --- CORE 1 (BLOK SAAT INI): EQ -> Compressor -> Mixer -> Master EQ ---
                dspControl.markProcessStart(1);
                
                // EQ & Compressor Mono Terpisah
                float* inL = compressorL.process(eqInputL.process(i2sL, len), len);
                float* inR = compressorR.process(eqInputR.process(i2sR, len), len);
                
                // Mixer pencampur I2S + Bluetooth Mono Terpisah
                float* mixL = mixerL.process(len, inL, btL);
                float* mixR = mixerR.process(len, inR, btR);
                
                // EQ Master Bus Mono Terpisah
                float* mastL = eqMasterL.process(mixL, len);
                float* mastR = eqMasterR.process(mixR, len);
                
                memcpy(nextPipeL, mastL, len * sizeof(float));
                memcpy(nextPipeR, mastR, len * sizeof(float));
                
                dspControl.markProcessEnd(1, len, 48000);
            },
            [&]() {
                // --- CORE 0 (BLOK SEBELUMNYA): FIR -> Limiter -> Output Matrix Router ---
                dspControl.markProcessStart(0);
                
                // FIR & Limiter Mono Terpisah
                float fOutL[128], fOutR[128];
                firL.process(pipeL, fOutL, len);
                firR.process(pipeR, fOutR, len);
                
                float* finalL = limiterL.process(fOutL, len);
                float* finalR = limiterR.process(fOutR, len);
                
                // Matrix Router Output Mono Terpisah
                float silent[128] = {0};
                float* inMatrixL[3] = { finalL, silent, silent };
                float* inMatrixR[3] = { finalR, silent, silent };
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
    
    // Inisialisasi Look-Up Tables
    RadDSP::LUT::init();
    
    // Sambungkan modul-modul DSP ke ID Controller
    dspControl.attach(1, &eqInputL);
    dspControl.attach(2, &eqInputR);
    dspControl.attach(3, &compressorL);
    dspControl.attach(4, &compressorR);
    dspControl.attach(5, &mixerL); 
    dspControl.attach(6, &mixerR); 
    dspControl.attach(7, &eqMasterL);
    dspControl.attach(8, &eqMasterR);
    dspControl.attach(9, &limiterL);
    dspControl.attach(10, &limiterR);
    dspControl.attach(11, &firL);
    dspControl.attach(12, &firR);
    dspControl.attach(13, &routerL);
    dspControl.attach(14, &routerR);
    
    // Masukkan Konfigurasi Telemetry & Mulai Komunikasi UART
    dspControl.setSchema(dspSchema);
    dspControl.beginSerial(115200);

    // Inisialisasi Bluetooth
    bt.begin("RAD-DSP-MIXER");

    // Inisialisasi Hardware I2S0 (Master dengan MCLK di GPIO0)
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

    // Inisialisasi Parameter Default Compressor (Threshold -20dB, Ratio 4:1)
    compressorL.setParameter(0, 0.0f);   // Type: Compressor
    compressorL.setParameter(1, -20.0f); // Threshold: -20 dB
    compressorL.setParameter(2, 4.0f);   // Ratio: 4:1
    compressorL.setParameter(3, 10.0f);  // Attack: 10 ms
    compressorL.setParameter(5, 100.0f); // Release: 100 ms
    compressorR.setParameter(0, 0.0f);
    compressorR.setParameter(1, -20.0f);
    compressorR.setParameter(2, 4.0f);
    compressorR.setParameter(3, 10.0f);
    compressorR.setParameter(5, 100.0f);

    // Inisialisasi Parameter Default Limiter (Threshold -1dB, Ratio 20:1)
    limiterL.setParameter(0, 1.0f);   // Type: Limiter
    limiterL.setParameter(1, -1.0f);  // Threshold: -1 dB
    limiterL.setParameter(2, 20.0f);  // Ratio: 20:1 (Brickwall)
    limiterL.setParameter(3, 0.1f);   // Attack: 0.1 ms
    limiterL.setParameter(5, 50.0f);  // Release: 50 ms
    limiterR.setParameter(0, 1.0f);
    limiterR.setParameter(1, -1.0f);
    limiterR.setParameter(2, 20.0f);
    limiterR.setParameter(3, 0.1f);
    limiterR.setParameter(5, 50.0f);

    // Rute Matrix Default (Masing-masing Output 0 diarahkan ke 0dB)
    routerL.setRouteLinear(0, 0, 1.0f);
    routerR.setRouteLinear(0, 0, 1.0f);
    
    // Default Koefisien FIR (Passthrough)
    float default_fir[16] = {1.0f, 0};
    firL.setCoeffs(default_fir, 16);
    firR.setCoeffs(default_fir, 16);

    // Jalankan Dual Core Sync & Audio Task pada Core 1
    dualCore.begin();
    RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
    // Kosong (audio loop berjalan secara asinkron di Core 1)
}
```

---

# RAD_DSP_LIB (English Version)

**RAD_DSP_LIB** is a high-performance, real-time stereo digital signal processing (DSP) audio library specifically designed for the **ESP32** microcontroller using Arduino Core 3.0+ (ESP-IDF 5.x).

This library is engineered to process multi-channel audio with ultra-low latency via a parallel dual-core architecture and features an asynchronously synchronized Bluetooth A2DP Sink interface.

---

## Key Features

1. **Hermite Cubic ASRC (Hi-Fi Resampler):**
   * Asynchronously converts Bluetooth audio (44.1 kHz) to the system's DAC clock rate (**48.0 kHz**).
   * Employs 4-point Hermite cubic interpolation (*Horner's Method*) to maintain crystal-clear high frequencies ($< -0.5\text{ dB}$ attenuation at $20\text{ kHz}$).
   * Equipped with an automatic *Frequency Locked Loop* (FLL) algorithm based on FreeRTOS RingBuffer level detection to completely eliminate wow, flutter, and audio stuttering (jitter).
2. **Dual-Core Parallel Audio Pipeline:**
   * Distributes the processing load in parallel using a *Ping-Pong Buffer Pipeline*:
     * **Core 1:** Computes the input block (Biquad EQ per channel, Dynamics Compressor, Mixer).
     * **Core 0:** Computes the output block simultaneously (FIR convolution filter, Master Limiter, Matrix Router).
3. **Hardware I2S Master & Slave:**
   * Supports flexible configurations: **I2S0** runs as Master with MCLK (GPIO0), while **I2S1** runs as a pure Slave.
4. **Adaptive Matrix Router:**
   * Allows dynamic routing of any input to any physical output with independent linear/dB gain controls.
5. **Real-time UART Control (RadStudio GUI):**
   * Integrates a module ID-based UART text parser protocol.
   * Fully integrated with the **RadStudio** desktop tuning GUI to modify filter, biquad, compressor, and routing matrix parameters instantly and visually.

---

## Dual-Core Parallel Processing Architecture (Fork-Join Pipeline)

RAD_DSP_LIB is designed to maximize the computational performance of the dual-core **ESP32** microcontroller by dividing the workload in parallel. The audio data stream is split into two stages of processing using a **Fork-Join** architecture based on a *Ping-Pong Buffer Pipeline*:

```mermaid
graph TD
    subgraph Core1 ["Core 1 (Main)"]
        A[ADC / I2S Input] --> B[Stage 1 Processing / Input Block]
        B --> C[Copy to nextPipe]
    end
    
    subgraph Core0 ["Core 0 (Helper)"]
        D[Read from pipe] --> E[Stage 2 Processing / Output Block]
        E --> F[Copy to outPipe]
    end
    
    C -. DualCore Sync .-> D
    F --> G[DAC / I2S Output]
```

### 1. Core 1 (Main Processor)
Core 1 is responsible for real-time I/O handling (reading input hardware), external control management, and initial stage calculations (Input Block):
* **Task Flow:**
  1. Reads the latest audio sample block from the physical input device (ADC/I2S).
  2. Performs initial-stage audio processing calculations (Task A).
  3. Copies the calculation results to an intermediate buffer (`nextPipe`) to be sent to Core 0.

### 2. Core 0 (Co-Processor / Helper)
Core 0 is dedicated to handling mathematically intensive calculations asynchronously, isolated from the I/O activity of Core 1:
* **Task Flow:**
  1. Reads the buffer data calculated by Core 1 in the previous cycle (`pipe`).
  2. Performs final-stage audio processing calculations (Task B).
  3. Copies the final calculation results to an output buffer (`outPipe`) to be written to the DAC/I2S.

### 3. Ping-Pong Buffer Synchronization
* To avoid *race conditions* (where one core writes to a buffer that is currently being read by the other core), the library utilizes a **Ping-Pong Pipeline** method.
* At time cycle $t$, Core 1 processes a new audio block sample ($N$), while Core 0 simultaneously processes the calculations from Core 1's previous block sample ($N-1$).
* The fork-join coordination between cores is managed using a high-priority *FreeRTOS Binary Semaphore* via the `dualCore.process()` class call. Once both cores complete their tasks for the current cycle, the buffers are swapped atomically.

---

## Project Directory Structure

```text
RAD_DSP_LIB/
├── src/                      # C++ DSP Library Source Code
│   ├── RadDSP.h              # Main Header
│   ├── RadI2S.cpp/.h         # I2S Master/Slave Drivers
│   ├── RadBluetooth.cpp/.h   # Bluetooth Classic & ASRC Controllers
│   ├── RadMatrix.h           # MatrixRouter Template
│   ├── RadMixer.h            # Mixer Block Template
│   ├── RadBiquad.cpp/.h      # Biquad IIR Filter (LowPass, HighPass, EQ)
│   ├── RadDynamics.cpp/.h    # Compressor & Limiter
│   └── RadControl.cpp/.h     # Telemetry & UART Serial Parser
├── examples/                 # Sketches demonstrating individual DSP blocks
│   ├── Biquad_Filter/        # Single Biquad EQ filter example
│   ├── Bluetooth_Audio/      # Asynchronous Bluetooth audio reception example
│   ├── DualCore_Pipeline/    # Generic fork-join dual-core pipeline example
│   ├── Dynamics_Processor/   # Audio dynamics compressor example
│   ├── FIR_Filter/           # FIR convolution filter example
│   ├── I2S_Audio/            # Basic I2S audio passthrough example
│   ├── Matrix_Routing/       # Adaptive matrix audio routing example
│   └── Mixer_Stereo/         # Two-input stereo audio mixer example
├── test/                     # Integration testing & system diagnostic sketches
│   ├── Master_Passthrough/   # Main dual-core pipeline & telemetry sketch
│   └── BT_Diagnostic_Test/   # Bluetooth connection & NVS diagnostics tool
├── tools/
│   ├── RadStudio.py          # Real-time GUI desktop tuning application
│   ├── RadScanner.py         # Route scanner & schema injector
│   └── read_serial.py        # UART Serial Logger
└── references/               # Offline libraries and SDK documentation references
```

---

## Quick Start

### 1. Library Installation
Copy the entire `RAD_DSP_LIB` folder to your global Arduino library directory:
* **Windows:** `C:\Users\<Username>\Documents\Arduino\libraries\RAD_DSP_LIB`

### 2. Flashing Firmware to ESP32
1. Open the Arduino IDE or use the `arduino-cli` terminal.
2. Open any sketch in the `examples/` directory or `test/Master_Passthrough/Master_Passthrough.ino`.
3. Select an ESP32-based board, e.g., **LOLIN32 Lite** (or **ESP32 Dev Module** depending on your board).
4. Compile and upload (flash) the firmware to your microcontroller's serial port (e.g., `COM12`).

### 3. Controlling Using RadStudio GUI
1. Ensure Python is installed along with the `pyserial` and `tkinter` libraries.
2. Launch the GUI application from the terminal:
   ```bash
   python tools/RadStudio.py
   ```

---

## API & Syntax Reference

This documentation lists the classes, functions, initialization examples, usage patterns, and parameter mappings for each DSP module supported by **RAD_DSP_LIB** in pure mono processing mode.

### 1. DSP Library Classes & Functions

#### A. Class `RadDSP::I2S`
Manages audio communication with external hardware DAC/ADC chips.
* **Initialization (`setup()`):**
  ```cpp
  // Start I2S0 as Master at 48 kHz, 32-bit (WS/LRCK at GPIO25, BCK at GPIO26, MCLK at GPIO0)
  i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  if (i2s0.readBlock()) { // Read one block of audio data (populates internal buffers)
      int len = i2s0.getBufferLength();     // Get buffer size (e.g., 128 samples)
      float* i2sL = i2s0.getLeftBuffer();   // Get pointer to Left audio channel
      float* i2sR = i2s0.getRightBuffer();  // Get pointer to Right audio channel
      
      // ... Perform audio processing here ...
      
      i2s0.writeBlock(); // Write internal audio buffers back to physical DAC
  }
  ```

---

#### B. Class `RadDSP::Bluetooth`
Enables Bluetooth A2DP Sink audio reception with built-in Hi-Fi cubic Hermite ASRC resampling.
* **Initialization (`setup()`):**
  ```cpp
  bt.begin("RAD-DSP-MUSIC"); // Bluetooth device name discoverable by phone
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  float btL[128], btR[128];
  // Read BT audio and resample automatically to target sample rate (e.g., 48000 Hz)
  if (!bt.readAudio(btL, btR, len, 48000)) {
      // If phone is not connected or buffer is empty, output silence safely
      memset(btL, 0, len * sizeof(float));
      memset(btR, 0, len * sizeof(float));
  }
  ```

---

#### C. Class `RadDSP::Biquad`
Single Mono Biquad IIR filter for Equalizers, Crossovers (Highpass/Lowpass), Shelves, and Notch filters.
* **Initialization & Parameters (`setup()`):**
  ```cpp
  // Peaking/Bell EQ example, 1000 Hz, Boost +6.0 dB, Q=1.0
  eq.setParameter(0, 4.0f);    // 0: Filter Type (0=LPF, 1=HPF, 2=BPF, 3=Peaking, 4=LowShelf, 5=HighShelf)
  eq.setParameter(1, 1000.0f); // 1: Cutoff/Center Frequency (Hz)
  eq.setParameter(2, 6.0f);    // 2: Gain (dB)
  eq.setParameter(3, 1.0f);    // 3: Q-Factor
  eq.setParameter(100, 0.0f);  // 100: Bypass (0.0f = Active, 1.0f = Bypass)
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  // Method 1: Out-of-Place (Input and Output in separate buffers)
  eq.process(inputBuffer, outputBuffer, len);

  // Method 2: In-Place (Results written directly overwriting the input buffer)
  eq.process(audioBuffer, len);
  ```

---

#### D. Class `RadDSP::Dynamics`
Single Mono dynamics amplitude module for Compressor, Limiter, Expander, and Noise Gate processing.
* **Initialization & Parameters (`setup()`):**
  ```cpp
  // Compressor example (Threshold -20dB, Ratio 4:1, Attack 10ms, Release 100ms)
  comp.setParameter(0, 0.0f);   // 0: Type (0=Compressor, 1=Limiter, 2=Expander, 3=Noise Gate)
  comp.setParameter(1, -20.0f); // 1: Threshold (dB)
  comp.setParameter(2, 4.0f);   // 2: Ratio (ignored on Limiter & Noise Gate)
  comp.setParameter(3, 10.0f);  // 3: Attack Time (ms)
  comp.setParameter(4, 0.0f);   // 4: Hold Time (ms)
  comp.setParameter(5, 100.0f); // 5: Release Time (ms)
  comp.setParameter(6, 0.0f);   // 6: Makeup Gain (dB)
  comp.setParameter(7, 0.0f);   // 7: Sidechain SC Filter Type (0=None, 1=HPF, 2=LPF, 3=BPF)
  comp.setParameter(8, 1000.0f);// 8: SC Filter Freq (Hz)
  comp.setParameter(100, 0.0f); // 100: Bypass (0.0f = Active, 1.0f = Bypass)
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  // Method 1: Standard Compressor/Limiter (in-place)
  comp.process(audioBuffer, len);

  // Method 2: Compressor with separate Sidechain sensor input buffer
  comp.processSidechain(audioBuffer, sidechainSensorBuffer, len);
  ```

---

#### E. Class `RadDSP::Mixer<N>`
Mixes $N$ mono audio inputs into 1 mono output with independent gain and mute controls.
* **Initialization & Parameters (`setup()`):**
  ```cpp
  RadDSP::Mixer<2> mixer; // 2-input mono mixer
  mixer.setParameter(0, 0.0f);   // 0 to N-1: Input 1 Gain (dB)
  mixer.setParameter(1, -6.0f);  // Input 2 Gain (dB)
  mixer.setParameter(100, 0.0f); // 100 to 100+N-1: Input 1 Mute (0.0f = Active, 1.0f = Mute)
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  // Mix 'in1' and 'in2' buffers variadically
  float* mixed = mixer.process(len, in1, in2); // Returns pointer to mixed output buffer
  ```

---

#### F. Class `RadDSP::FIR`
Single Mono Finite Impulse Response (FIR) Convolution filter for guitar cabinet impulse responses (IR), phase correction, or room correction.
* **Initialization & Parameters (`setup()`):**
  ```cpp
  float lp_coeffs[16] = {1.0f, 0.0f /* ... rest 0 ... */};
  fir.setCoeffs(lp_coeffs, 16);  // Load the FIR coefficients array
  fir.setParameter(4, 0.0f);     // 4: Output Gain (dB)
  fir.setParameter(100, 0.0f);   // 100: Bypass (0.0f = Active, 1.0f = Bypass)
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  fir.process(inputBuffer, outputBuffer, len); // Perform FIR convolution
  ```

---

#### G. Class `RadDSP::MatrixRouter<IN, OUT>`
Adaptive mono audio routing matrix to mix and route any inputs to any outputs dynamically.
* **Initialization & Parameters (`setup()`):**
  ```cpp
  RadDSP::MatrixRouter<3, 2> router; // 3 mono inputs to 2 mono outputs
  router.setRouteLinear(0, 0, 1.0f);   // Route Input 0 to Output 0 at 0 dB gain (1.0f)
  router.setRouteLinear(0, 1, 0.707f); // Route Input 0 to Output 1 at -3 dB gain (0.707f)
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  float* inputs[3] = {in1, in2, in3};
  float* outputs[2] = {out1, out2};
  router.process(inputs, outputs, len); // Execute matrix mixing
  ```

---

#### H. Class `RadDSP::Gain`
Single Mono gain block with mute and phase inversion options.
* **Initialization & Parameters (`setup()`):**
  ```cpp
  gain.setParameter(0, 0.0f);   // 0: Gain Volume (dB)
  gain.setParameter(1, 0.0f);   // 1: Mute (0.0f = Active, 1.0f = Mute)
  gain.setParameter(2, 0.0f);   // 2: Phase Invert (0.0f = Normal, 1.0f = Inverted)
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  // Method 1: Out-of-Place
  gain.process(inputBuffer, outputBuffer, len);

  // Method 2: In-Place
  gain.process(audioBuffer, len);
  ```

---

#### I. Class `RadDSP::Controller`
Manages the UART serial telemetry protocol for the RadStudio GUI.
* **Initialization & Parameters (`setup()`):**
  ```cpp
  dspControl.attach(1, &eqL);        // Register module to a unique ID
  dspControl.setSchema(dspSchema);   // Define the graphical routing schema JSON
  dspControl.beginSerial(115200);    // Enable UART communication
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  dspControl.poll(); // Must be called periodically to process incoming external control commands
  ```

---

#### J. Class `RadDSP::Meter`
Real-time audio level meter (Peak VU Meter) feeding actual decibel (dBFS) data to the UART telemetry system.
* **Initialization & Parameters (`setup()`):**
  ```cpp
  meter.setParameter(1, 0.95f); // 1: Decay Factor (0.5 to 0.999f) for smooth VU transitions
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  meter.process(audioBuffer, len); // Tracks the peak decibel (dBFS) level of the current block
  ```
* **Telemetry Query:**
  * `getParameter(0)`: Returns the current decibel level in the range of `-80.0 dBFS` to `+6.0 dBFS`.

---

#### K. Class `RadDSP::FFT`
Built-in real-time Fast Fourier Transform analyzer for visual spectrum analysis (Spectrogram / RTA).
* **Initialization (`setup()`):**
  ```cpp
  RadDSP::FFT fft(512); // Create FFT instance of size 512 (must be power of 2: 256, 512, 1024, 2048)
  fft.begin();          // Allocates twiddle tables & complex buffer memory
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  // 1. Copy sample data to local float array
  float fftInput[512]; 
  memcpy(fftInput, audioBuffer, 512 * sizeof(float));

  // 2. Apply Hann Window to reduce spectral leakage
  fft.applyWindow(fftInput);

  // 3. Execute FFT
  float magnitudes[256]; // Output magnitudes array of size N/2
  fft.process(fftInput, magnitudes); 

  // 4. Retrieve peak bin frequency
  float peakFreq = fft.getFrequency(binIndex, 48000.0f); // Get actual frequency of bin N
  ```

---

#### L. Global Function `RadDSP::startAudioTask()`
Spawns a real-time RTOS Task with the highest priority (`configMAX_PRIORITIES - 1`) to lock the audio loop execution to a specific core without watchdog interruptions.
* **Syntax:**
  ```cpp
  RadDSP::startAudioTask(AudioTaskCallback callback, int coreID = 1, bool killArduinoLoop = true);
  ```
* **Parameters:**
  * `callback`: Custom audio loop function (e.g., `audioLoop`).
  * `coreID`: Core index (`0` or `1`).
  * `killArduinoLoop`: If `true` (highly recommended), terminates the default Arduino `loop()` task to reclaim Core 1 CPU cycles entirely for your audio loop.

---

#### M. Class `RadDSP::DualCoreWorker`
Parallel execution controller coordinating two cores in a Fork-Join scheme to distribute audio tasks simultaneously without overhead.
* **Initialization (`setup()`):**
  ```cpp
  dualCore.begin(); // Call in setup() to start the helper thread on Core 0
  ```
* **Usage (`audioLoop()`):**
  ```cpp
  // Executes tasks on Core 1 and Core 0 simultaneously.
  // Blocks at this line until BOTH cores complete their work.
  dualCore.process(
      [&]() {
          // --- CODE FOR CORE 1 ---
          compL.process(eqInputL.process(i2sL, len), len);
      },
      [&]() {
          // --- CODE FOR CORE 0 ---
          firL.process(pipeL, fOutL, len);
      }
  );
  ```

---

### RAM Optimization Guide: Release Bluetooth Memory
Bluetooth A2DP Sink consumes a significant portion of the ESP32 Heap RAM (~120 KB dynamic + ~20 KB static). If your DSP project does not require wireless Bluetooth audio (e.g., pure analog input processing), use the following steps to fully free up RAM:

1. **Do Not Call `bt.begin()`**:
   * Bluedroid stack and RingBuffer will not be allocated, saving **120 KB RAM**.
2. **Call `bt.releaseMemory()` at the start of `setup()`**:
   * Releases static Bluetooth hardware controller RAM, returning **~20 KB to 40 KB** back to the general heap.

**Example RAM-Optimized Sketch:**
```cpp
void setup() {
    Serial.begin(115200);

    // 1. Force release Bluetooth hardware memory
    bt.releaseMemory();

    // 2. Initialize memory-heavy I2S and DSP modules
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);
    ...
}
```

---

### Topology Schema Mapping Automation (RadScanner & dsp_schema.h)
To render the DSP routing diagram in the RadStudio GUI, the ESP32 needs a routing schema JSON. You do not need to compile this manually. Use **RadScanner.py**:

1. **Place `RadScanner.py`** in your project sketch folder (same directory as your `.ino` file).
2. **Add `@Route` comments** in your `.ino` file to declare your audio path. These are parsed by the scanner to build the visual routing graph.
   
   **Sintax Rules for `@Route`:**
   * Written as normal C++ comments starting with `// @Route:` or `//@Route:`.
   * Built-in physical inputs: `I2S_In` and `BT_In`.
   * Built-in physical outputs: `I2S0_Out` and `I2S1_Out`.
   * Sinyal flows are separated by `->` (spaces are ignored and stripped automatically).
   * Node names must match your C++ DSP instance variable names exactly (case-sensitive).
   
   **Example `@Route` comments:**
   ```cpp
   // @Route: I2S_In -> meterAnalogL -> EqI2S_0_L -> CompI2S_L -> mixerL
   // @Route: BT_In -> meterBtL -> EqBT_0_L -> mixerL
   // @Route: mixerL -> EqMaster_0_L -> firL -> LimiterMaster_L -> meterL -> routerL -> I2S0_Out
   // @Route: routerL -> I2S1_Out
   ```

3. Run the script inside the sketch folder using the terminal:
   ```bash
   python RadScanner.py
   ```
4. **Output**:
   * The scanner generates a **`dsp_schema.h`** file in the same directory, containing the minimized `dspSchema` JSON string constant.
5. **Usage**:
   Include the generated header file at the top of your `.ino` sketch:
   ```cpp
   #include "dsp_schema.h" // Auto-generated by RadScanner
   ```

---

### Parameter Configuration Reference Table (`configParam`)

This table documents the values range (`min`, `max`), step resolution (`step`), unit, and scale for every parameter in all DSP modules, used by **RadStudio GUI** to render knobs, sliders, and checkboxes.

#### Biquad (EQ / Filter IIR)

| Param ID | Name | Min | Max | Step | Unit | Widget Type |
|----------|------|-----|-----|------|------|-------------|
| `0` | Filter Type | — | — | — | — | Combobox: `0:LowPass, 1:HighPass, 2:BandPass, 3:Peaking, 4:LowShelf, 5:HighShelf` |
| `1` | Frequency | 20.0 | 20000.0 | 1.0 | Hz | Knob (Log) |
| `2` | Gain | -24.0 | 24.0 | 0.1 | dB | Knob (Lin) |
| `3` | Q-Factor | 0.05 | 20.0 | 0.05 | Q | Knob (Lin) |
| `100` | Bypass | 0 | 1 | 1 | — | Checkbox |

#### Dynamics (Compressor / Limiter / Expander / Gate)

| Param ID | Name | Min | Max | Step | Unit | Widget Type |
|----------|------|-----|-----|------|------|-------------|
| `0` | Dynamics Type | — | — | — | — | Combobox: `0:Compressor, 1:Limiter, 2:Expander, 3:Gate` |
| `1` | Threshold | -80.0 | 0.0 | 0.1 | dB | Knob (Lin) |
| `2` | Ratio | 1.0 | 20.0 | 0.1 | Ratio | Knob (Lin) |
| `3` | Attack | 0.1 | 1000.0 | 0.1 | ms | Knob (Lin) |
| `4` | Hold | 0.0 | 1000.0 | 1.0 | ms | Knob (Lin) |
| `5` | Release | 1.0 | 5000.0 | 1.0 | ms | Knob (Lin) |
| `6` | Makeup Gain | -24.0 | 24.0 | 0.1 | dB | Knob (Lin) |
| `7` | SC Filter Type | — | — | — | — | Combobox: `0:Bypass, 1:HighPass, 2:LowPass, 3:BandPass` |
| `8` | SC Frequency | 20.0 | 20000.0 | 1.0 | Hz | Knob (Log) |
| `100` | Bypass | 0 | 1 | 1 | — | Checkbox |

#### Mixer\<N\> (N-Input Mixer)

| Param ID | Name | Min | Max | Step | Unit | Widget Type |
|----------|------|-----|-----|------|------|-------------|
| `0` to `N-1` | Input Gain (per ch) | -80.0 | 12.0 | 0.1 | dB | Knob (Lin) |
| `100` to `100+N-1` | Input Mute (per ch) | 0 | 1 | 1 | — | Checkbox |

#### Gain (Volume / Mute / Invert)

| Param ID | Name | Min | Max | Step | Unit | Widget Type |
|----------|------|-----|-----|------|------|-------------|
| `0` | Gain Volume | -80.0 | 12.0 | 0.1 | dB | Knob (Lin) |
| `1` | Mute | 0 | 1 | 1 | — | Checkbox |
| `2` | Phase Invert | 0 | 1 | 1 | — | Checkbox |

#### FIR (Convolution Filter)

| Param ID | Name | Min | Max | Step | Unit | Widget Type |
|----------|------|-----|-----|------|------|-------------|
| `0` | Target Tap Index | 0.0 | 511.0 | 1.0 | Index | (Serial-only) target tap index |
| `1` | Staging Coeff Value | -1.5 | 1.5 | 0.0001 | Coeff | (Serial-only) value of staging tap |
| `2` | Commit Trigger | 0 | 1 | 1 | — | (Serial-only) 1.0 = Swap Buffers |
| `3` | Taps Number | 16.0 | 512.0 | 16.0 | Taps | Knob (Lin) |
| `4` | Output Gain | -24.0 | 24.0 | 0.1 | dB | Knob (Lin) |
| `100` | Bypass | 0 | 1 | 1 | — | Checkbox |

#### MatrixRouter\<IN, OUT\> (Routing Matrix)

| Param ID | Name | Min | Max | Step | Unit | Widget Type |
|----------|------|-----|-----|------|------|-------------|
| `i*OUT+j` | InN→OutM | 0.0 | 1.5 | 0.01 | Lin | Entry (Grid) |

> **Note:** A linear value of `1.0` equals 0 dB (passthrough), `0.0` = mute, `0.707` ≈ -3 dB. Linear gains above `1.0` boost the signal.

#### Meter (VU Meter)

| Param ID | Name | Min | Max | Step | Unit | Widget Type |
|----------|------|-----|-----|------|------|-------------|
| `0` | Level | -80.0 | 6.0 | — | dBFS | Canvas Bar LED (Real-time 120ms) |
| `1` | Decay Factor | 0.5 | 0.999 | 0.001 | Decay | Knob (Lin) |

---

### 2. Controlling DSP via Serial UART (External MCU / PC)

Remote control from external microcontrollers or PC is done using **line-based JSON protocol** (`\n`).

#### Set Parameter Command Format:
```json
{"id":<moduleID>,"p":<paramID>,"v":<value>}
```

#### Get Parameter Command Format:
```json
{"id":<moduleID>,"req":<paramID>}
```

#### Example UART Commands:
1. **Change Peaking EQ Gain (Module ID 1, Parameter 2) to +4.5 dB:**
   ```json
   {"id":1,"p":2,"v":4.5}
   ```
2. **Change LPF Cutoff Frequency (Module ID 2, Parameter 1) to 1200 Hz:**
   ```json
   {"id":2,"p":1,"v":1200.0}
   ```
3. **Enable Bypass on Compressor (Module ID 5, Parameter 100):**
   ```json
   {"id":5,"p":100,"v":1.0}
   ```
4. **Read Parameter 1 (Frequency) from Biquad (Module ID 1):**
   ```json
   {"id":1,"req":1}
   ```
   *ESP32 will reply with JSON:* `{"ack":1,"id":1,"p":1,"v":1000.0}`
5. **Read DSP System Telemetry (Core Loads & Heap RAM):**
   ```json
   {"id":255,"req":0}
   ```
   *ESP32 will reply with JSON:* `{"sys":1,"c0":12.5,"c1":32.1,"ramF":145230,"ramT":320000}`
6. **Request Routing Topology (JSON Schema) for RadStudio:**
   ```json
   {"id":254,"req":0}
   ```
   *ESP32 will reply with the complete routing JSON schema.*

---

### 3. Basic Template (Passthrough Audio & Basic Init)

Below is a clean starting template sketch. It initializes the main modules (I2S, Bluetooth, Controller) and routes the mixed signals directly from input (ADC & Bluetooth) to output (DAC):

```cpp
#include <RadDSP.h>

// Instantiate physical I2S driver for ADC/DAC communication
RadDSP::I2S i2s0;

// Instantiate Bluetooth driver to receive A2DP wireless audio
RadDSP::Bluetooth bt;

// Instantiate Controller for RadStudio GUI telemetry & control via UART
RadDSP::Controller dspControl;

// @Route: I2S_In -> I2S0_Out
// @Route: BT_In -> I2S0_Out

#include "dsp_schema.h"

// Audio loop callback executed continuously on Core 1
void audioLoop() {
    // Poll serial input for RadStudio GUI controls
    dspControl.poll();

    // Read a block of audio samples from the ADC (I2S DMA)
    if (i2s0.readBlock()) {
        int len = i2s0.getBufferLength(); // Default: 128 samples
        
        // Get float pointers for Left and Right channels
        float* i2sL = i2s0.getLeftBuffer();
        float* i2sR = i2s0.getRightBuffer();

        // Read Bluetooth audio (automatically resampled to 48 kHz system clock)
        float btL[128], btR[128];
        if (!bt.readAudio(btL, btR, len, 48000)) {
            // Mute BT buffers if not connected
            memset(btL, 0, len * sizeof(float));
            memset(btR, 0, len * sizeof(float));
        }

        // --- SIMPLE MIXING & PASSTHROUGH ---
        for (int i = 0; i < len; i++) {
            i2sL[i] = i2sL[i] + btL[i]; // Mix Left channel
            i2sR[i] = i2sR[i] + btR[i]; // Mix Right channel
        }

        // Write processed buffers to DAC (I2S DMA Output)
        i2s0.writeBlock();
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize desibel look-up tables
    RadDSP::LUT::init();

    // Set routing schema and begin UART at 115200 bps
    dspControl.setSchema(dspSchema);
    dspControl.beginSerial(115200);

    // Start Bluetooth A2DP Sink with device name "RAD-DSP-PASSTHROUGH"
    bt.begin("RAD-DSP-PASSTHROUGH");

    // Start I2S0 as Master at 48 kHz, 32-bit (BCK=GPIO26, WS=GPIO25, DOUT=GPIO33, DIN=GPIO23, MCLK=GPIO0)
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

    // Run audioLoop callback task on Core 1 with real-time priority
    RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
    // Empty
}
```

---

### 4. Full DSP Chain Example

Below is a complete template demonstrating the initialization and routing of all DSP blocks supported by the library using separate mono processing for Left (L) and Right (R) channels:

```cpp
#include <RadDSP.h>

// 1. Hardware & Controller Instantiation
RadDSP::I2S i2s0;
RadDSP::Bluetooth bt;
RadDSP::Controller dspControl;
RadDSP::DualCoreWorker dualCore;

// 2. DSP Blocks Instantiation (Separate Mono for Left & Right)
RadDSP::Biquad eqInputL, eqInputR;             // ID 1 & 2
RadDSP::Dynamics compressorL, compressorR;     // ID 3 & 4
RadDSP::Mixer<2> mixerL, mixerR;               // ID 5 & 6
RadDSP::Biquad eqMasterL, eqMasterR;           // ID 7 & 8
RadDSP::Dynamics limiterL, limiterR;           // ID 9 & 10
RadDSP::FIR firL, firR;                        // ID 11 & 12
RadDSP::MatrixRouter<3, 2> routerL, routerR;   // ID 13 & 14

// @Route: I2S_In -> eqInputL -> compressorL -> mixerL -> eqMasterL -> firL -> limiterL -> routerL -> I2S0_Out
// @Route: I2S_In -> eqInputR -> compressorR -> mixerR -> eqMasterR -> firR -> limiterR -> routerR -> I2S0_Out
// @Route: BT_In -> mixerL
// @Route: BT_In -> mixerR
// @Route: routerL -> I2S1_Out
// @Route: routerR -> I2S1_Out

#include "dsp_schema.h"

// Ping-Pong Pipeline buffers
float pipeL[128], pipeR[128];
float nextPipeL[128], nextPipeR[128];
float outPipeL[128], outPipeR[128];

// Main Audio Loop (Core 1)
void audioLoop() {
    // 1. Poll incoming UART commands
    dspControl.poll();
    
    // 2. Read from I2S DMA
    if (i2s0.readBlock()) {
        int len = i2s0.getBufferLength();
        float* i2sL = i2s0.getLeftBuffer();
        float* i2sR = i2s0.getRightBuffer();
        
        // 3. Read Bluetooth audio
        float btL[128], btR[128];
        if (!bt.readAudio(btL, btR, len, 48000)) {
            memset(btL, 0, len * sizeof(float));
            memset(btR, 0, len * sizeof(float));
        }

        // 4. Parallel Dual Core Processing (Ping-Pong Pipeline)
        dualCore.process(
            [&]() {
                // --- CORE 1 (CURRENT BLOCK): EQ -> Compressor -> Mixer -> Master EQ ---
                dspControl.markProcessStart(1);
                
                // Mono EQ & Compressor
                float* inL = compressorL.process(eqInputL.process(i2sL, len), len);
                float* inR = compressorR.process(eqInputR.process(i2sR, len), len);
                
                // Mono Mixer (I2S + Bluetooth)
                float* mixL = mixerL.process(len, inL, btL);
                float* mixR = mixerR.process(len, inR, btR);
                
                // Mono Master EQ
                float* mastL = eqMasterL.process(mixL, len);
                float* mastR = eqMasterR.process(mixR, len);
                
                memcpy(nextPipeL, mastL, len * sizeof(float));
                memcpy(nextPipeR, mastR, len * sizeof(float));
                
                dspControl.markProcessEnd(1, len, 48000);
            },
            [&]() {
                // --- CORE 0 (PREVIOUS BLOCK): FIR -> Limiter -> Output Matrix Router ---
                dspControl.markProcessStart(0);
                
                // Mono FIR & Limiter
                float fOutL[128], fOutR[128];
                firL.process(pipeL, fOutL, len);
                firR.process(pipeR, fOutR, len);
                
                float* finalL = limiterL.process(fOutL, len);
                float* finalR = limiterR.process(fOutR, len);
                
                // Mono Matrix Router (3 inputs, 2 outputs)
                float silent[128] = {0};
                float* rInL[3] = {finalL, silent, silent};
                float* rInR[3] = {finalR, silent, silent};
                
                float* rOutL[2] = {outPipeL, silent};
                float* rOutR[2] = {outPipeR, silent};
                
                routerL.process(rInL, rOutL, len);
                routerR.process(rInR, rOutR, len);
                
                dspControl.markProcessEnd(0, len, 48000);
            }
        );

        // 5. Copy Core 0 output results to I2S buffer
        memcpy(i2sL, outPipeL, len * sizeof(float));
        memcpy(i2sR, outPipeR, len * sizeof(float));
        i2s0.writeBlock();

        // 6. Shift Pipe buffers
        memcpy(pipeL, nextPipeL, len * sizeof(float));
        memcpy(pipeR, nextPipeR, len * sizeof(float));
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize dB Look-Up Tables
    RadDSP::LUT::init();
    
    // Register DSP modules to Controller
    dspControl.attach(1, &eqInputL);
    dspControl.attach(2, &eqInputR);
    dspControl.attach(3, &compressorL);
    dspControl.attach(4, &compressorR);
    dspControl.attach(5, &mixerL); 
    dspControl.attach(6, &mixerR); 
    dspControl.attach(7, &eqMasterL);
    dspControl.attach(8, &eqMasterR);
    dspControl.attach(9, &limiterL);
    dspControl.attach(10, &limiterR);
    dspControl.attach(11, &firL);
    dspControl.attach(12, &firR);
    dspControl.attach(13, &routerL);
    dspControl.attach(14, &routerR);
    
    // Set schema and start Serial
    dspControl.setSchema(dspSchema);
    dspControl.beginSerial(115200);

    // Start Bluetooth
    bt.begin("RAD-DSP-MIXER");

    // Start I2S0
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

    // Initialize default Compressor params (Threshold -20dB, Ratio 4:1)
    compressorL.setParameter(0, 0.0f);
    compressorL.setParameter(1, -20.0f);
    compressorL.setParameter(2, 4.0f);
    compressorL.setParameter(3, 10.0f);
    compressorL.setParameter(5, 100.0f);
    compressorR.setParameter(0, 0.0f);
    compressorR.setParameter(1, -20.0f);
    compressorR.setParameter(2, 4.0f);
    compressorR.setParameter(3, 10.0f);
    compressorR.setParameter(5, 100.0f);

    // Initialize default Limiter params (Threshold -1dB, Ratio 20:1 brickwall)
    limiterL.setParameter(0, 1.0f);
    limiterL.setParameter(1, -1.0f);
    limiterL.setParameter(2, 20.0f);
    limiterL.setParameter(3, 0.1f);
    limiterL.setParameter(5, 50.0f);
    limiterR.setParameter(0, 1.0f);
    limiterR.setParameter(1, -1.0f);
    limiterR.setParameter(2, 20.0f);
    limiterR.setParameter(3, 0.1f);
    limiterR.setParameter(5, 50.0f);

    // Default Matrix Routes
    routerL.setRouteLinear(0, 0, 1.0f);
    routerR.setRouteLinear(0, 0, 1.0f);
    
    // Default FIR coefficients (Passthrough)
    float default_fir[16] = {1.0f, 0};
    firL.setCoeffs(default_fir, 16);
    firR.setCoeffs(default_fir, 16);

    // Start Dual Core Worker & Audio Task
    dualCore.begin();
    RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
    // Empty
}
```

---

### 5. Dual-Core Parallel Processing Example (Fork-Join Pipeline)

Below is a generic example demonstrating the setup of parallel tasks using `DualCoreWorker` and `I2S` passthrough:

```cpp
#include <RadDSP.h>

RadDSP::I2S i2s0;
RadDSP::Controller dspControl;
RadDSP::DualCoreWorker dualCore;

// @Route: I2S_In -> I2S0_Out

#include "dsp_schema.h"

// Ping-Pong buffers for cross-core audio block transfers
float pipeL[128], pipeR[128];
float nextPipeL[128], nextPipeR[128];
float outPipeL[128], outPipeR[128];

void audioLoop() {
    // 1. Poll UART commands
    dspControl.poll();

    // 2. Read from I2S DMA
    if (i2s0.readBlock()) {
        int len = i2s0.getBufferLength();
        float* i2sL = i2s0.getLeftBuffer();
        float* i2sR = i2s0.getRightBuffer();

        // 3. Parallel Fork-Join Dual-Core execution
        dualCore.process(
            [&]() {
                // --- CORE 1: Stage 1 Processing (Input Block) ---
                dspControl.markProcessStart(1);

                // Math Task A: Attenuate Left/Right signals by 50% (-6 dB)
                for (int i = 0; i < len; i++) {
                    nextPipeL[i] = i2sL[i] * 0.5f;
                    nextPipeR[i] = i2sR[i] * 0.5f;
                }

                dspControl.markProcessEnd(1, len, 48000);
            },
            [&]() {
                // --- CORE 0: Stage 2 Processing (Output Block) ---
                dspControl.markProcessStart(0);

                // Math Task B: Further attenuate by another 50%
                for (int i = 0; i < len; i++) {
                    outPipeL[i] = pipeL[i] * 0.5f;
                    outPipeR[i] = pipeR[i] * 0.5f;
                }

                dspControl.markProcessEnd(0, len, 48000);
            }
        );

        // 4. Write Core 0 output block back to I2S DAC
        memcpy(i2sL, outPipeL, len * sizeof(float));
        memcpy(i2sR, outPipeR, len * sizeof(float));
        i2s0.writeBlock();

        // 5. Shift Ping-Pong buffers locally
        memcpy(pipeL, nextPipeL, len * sizeof(float));
        memcpy(pipeR, nextPipeR, len * sizeof(float));
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize Core 0 worker thread
    dualCore.begin();

    // Initialize telemetry schema
    dspControl.setSchema(dspSchema);
    dspControl.beginSerial(115200);

    // Start I2S0 Master at 48 kHz
    i2s0.begin(I2S_NUM_0, 48000, 32, true, 26, 25, 33, 23, 0);

    // Spawn audioLoop callback on Core 1
    RadDSP::startAudioTask(audioLoop, 1, true);
}

void loop() {
    // Empty
}
```


