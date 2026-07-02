# Catatan Eksperimen DSP & Optimasi ESP32 (Backup Chat)

## 1. Beban CPU: EQ vs FIR
- **EQ (Biquad) & Kompresor (Dynamics)**: Bersifat **O(1)**. Sangat ringan, hanya membutuhkan ~5 operasi perkalian (MAC) per sampel. Menjalankan 10 Biquad Stereo + Dynamics hanya memakan sekitar **1% hingga 3% beban CPU (Core 1)**.
- **FIR Filter (512 Taps)**: Bersifat **O(N)**. Sangat berat! Membutuhkan 512 operasi perkalian per sampel. Pada *sample rate* 48kHz, sebuah FIR Stereo 512 Taps melakukan nyaris **50 Juta MACs per detik**, yang memakan sekitar **80% hingga 95% beban CPU (Core 0/1)**. 

## 2. Optimasi FIR: Circular Double-Buffer
Algoritma dasar FIR sering kali memindahkan data sejarah ke memori baru (`memmove`), yang mengakibatkan transfer data >100 MB/s pada ESP32 dan menyebabkan CPU tersedak hingga 900% (Crash). 
**Solusi**: Mengganti arsitektur ke *Circular Double-Buffer* statis (Array berukuran 2x Lipat). ESP32 tidak lagi menyalin memori, melainkan hanya menggeser index pointer (`_head`), sehingga membebaskan beban drastis.

## 3. Bahaya "Comb Filtering" (Fase Berbenturan)
Jika mem-paralelkan `EqCoba` dan `fir` lalu mencampurnya menggunakan `mixer`, akan tercipta efek seperti suara dari lorong, robotik, flanger, atau "Clock Drift".
**Penyebab**: Sinyal EQ memiliki latensi 0ms, sedangkan sinyal FIR 512 Taps memiliki group delay otomatis ~5ms (titik puncak Impulse Response berada di tengah Window). Mencampur sinyal 0ms dan 5ms bersamaan menciptakan *Comb Filtering*.
**Solusi**: Routing FIR dibuat menjadi sekuensial lurus (Seri), tidak di-mix dengan *dry signal*.

## 4. Bug State Sharing pada Filter Stereo
Menggunakan 1 objek `fir` untuk mengeksekusi dua kanal secara sekuensial (`fir.process(left)` lalu `fir.process(right)`) akan menyebabkan sampel kiri dan kanan saling tertimpa di dalam satu memori *Delay Line* yang sama.
**Solusi**: Memperbarui `RadFIR` agar mendukung parameter `channel`. Menyiapkan array `_delayLine[2][MAX_FIR_TAPS*2]` dan array `_head[2]` agar memori Kiri dan Kanan 100% terisolasi tanpa membebani RAM secara eksponensial.

## 5. Jebakan Sensor Beban CPU (`dspControl`)
- `markProcessStart(0)` dan `markProcessStart(1)` berfungsi seperti stopwatch.
- Jika diletakkan terlalu luar (mengapit keseluruhan blok `audioLoop`), maka alat ukur ini akan merekam total latensi *pipeline* ujung-ke-ujung (termasuk waktu diam/Idle untuk sinkronisasi antar core).
- Hal ini menyebabkan ilusi bahwa kedua Core bekerja 100%, padahal salah satunya sedang menganggur. Sensor harus diapit secara ketat hanya pada beban komputasi nyata.

## 6. Teknik Load Balancing Paralel Terbaik
Alih-alih menaruh FIR (L dan R) murni di Helper Core (Core 0), **arsitektur tercepat** adalah melakukan "Split-Stereo":
- **Core 1 (Utama)** mengerjakan EQ, Kompresor, dan **FIR Kiri**.
- **Core 0 (Helper)** mengerjakan **FIR Kanan**.
Dengan arsitektur ini, beban yang asalnya 90% pada satu Core, bisa terdistribusi menjadi 45% (Core 1) dan 45% (Core 0), menjaga suhu chip ESP32 tetap dingin dan mencegah *underrun*.

## 7. RAM Extraction: Uninstall BLE
Bluetooth Low Energy (BLE) selalu mengambil memori sejak booting walau tak dipakai. Fungsi ajaib `esp_bt_controller_mem_release(ESP_BT_MODE_BLE)` bisa dipanggil sebelum menyalakan A2DP untuk mencabut memori BLE dan mengembalikannya ke sistem (Reclaim RAM), memberikan kita ekstra **~20KB** RAM!

## 8. Mengatasi Noise "Click" (I2S DMA Underrun)
Jika beban DSP sangat tinggi (seperti 86% FIR) pada Core yang juga digunakan oleh sistem (*FreeRTOS/WiFi/Bluetooth*), maka sesekali proses DSP akan terinterupsi selama sepersekian milidetik.
Jika ukuran buffer I2S DMA Anda terlalu kecil (misal: 32 *samples* = Jendela waktu **0.6 ms**), interupsi sekecil apa pun akan membuat DSP telat mengirim data ke DMA (*Underrun*). Saat perangkat DMA kehabisan data, audio akan terputus paksa dan menghasilkan suara **CLICK**.
**Solusi**: Tingkatkan ukuran `dmaBufLen` pada `i2s.begin` dari 32 menjadi **128**. Ini akan melebarkan jeda waktu menjadi **2.6 ms**, memberikan "napas panjang" bagi ESP32 untuk menangani interupsi RTOS tanpa pernah melewatkan *deadline* pengiriman data DMA! Latensi ekstra 2 ms ini 100% tidak terasa oleh manusia.
