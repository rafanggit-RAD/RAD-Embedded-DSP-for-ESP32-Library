# Rencana Implementasi: Bluetooth A2DP di Arduino IDE

Karena Anda memutuskan untuk tetap berada di **Arduino IDE**, kita tidak bisa memodifikasi `sdkconfig` (konfigurasi inti ESP-IDF) untuk memangkas Bluetooth dari akarnya. Oleh karena itu, kita akan menempuh jalur **Opsi 1**: Menggunakan library pihak ketiga yang sangat dioptimalkan, yaitu `ESP32-A2DP`, dengan modifikasi khusus agar mengonsumsi RAM seminimal mungkin dan terintegrasi dengan mulus ke dalam *pipeline* `RadDSP` Anda.

## Pertanyaan Terbuka / Open Questions

> [!WARNING]
> 1. **Sumber Audio (Audio Source)**: Apakah Bluetooth A2DP ini akan menggantikan input fisik (Line-In / Mic) yang saat ini menggunakan pin I2S (I2S_NUM_0), atau Anda ingin menggabungkan (mixing) audio dari Bluetooth DAN pin fisik secara bersamaan? (Penggabungan akan memakan CPU dan memori lebih besar).
> 2. **Instalasi Library**: Anda wajib menginstal library `ESP32-A2DP` (oleh Phil Schatzmann) melalui Library Manager Arduino. Apakah Anda sudah menginstalnya?

## Rencana Perubahan (Proposed Changes)

### 1. Integrasi ESP32-A2DP ke `Master_Passthrough.ino`
Kita akan menambahkan fungsionalitas Bluetooth ke dalam sketch utama Anda.
- Menginisialisasi objek `BluetoothA2DPSink`.
- Mengarahkan aliran data (stream) Bluetooth menjauhi output I2S bawaan, dan menangkapnya melalui fungsi *callback* kustom (`audio_data_callback`).
- Mengurangi *buffer size* di dalam konfigurasi A2DP agar tidak menghabiskan sisa RAM yang Anda miliki.

### 2. Jembatan RingBuffer (A2DP -> DSP Core)
*Callback* A2DP dari Bluetooth berjalan secara asinkron di **Core 0** (Core sistem WiFi/BT), sedangkan *Audio Loop* utama kita berjalan di **Core 1**. 
- Jika kita melakukan pemrosesan DSP berat di dalam *callback* Bluetooth, suara akan terputus-putus.
- Solusi: Kita akan membuat **FreeRTOS RingBuffer** (atau Queue) yang sangat efisien. Callback Bluetooth akan melempar data PCM ke RingBuffer ini.
- `audioLoop()` di Core 1 akan membaca dari RingBuffer ini (bukan dari `i2s.readBlock()`), memproses DSP (EQ, FIR, Dynamics), lalu mengirimnya ke DAC (`i2s.writeBlock()`).

### 3. Panduan Optimasi Manual (Wajib Anda Lakukan)
Karena kita di Arduino IDE, saya akan memberikan instruksi tertulis di dokumen *Walkthrough* nantinya tentang cara mengatur menu `Tools` di Arduino IDE (seperti mengatur *Core Debug Level* ke `None` dan memilih *Partition Scheme* "Huge APP") agar ESP32 Anda tidak kehabisan memori.

## Rencana Verifikasi
- Saya akan mengimplementasikan jembatan data dan memperbarui file `.ino`.
- Anda akan mengkompilasinya. Jika terjadi error *Out of Memory* (karena buffer FIR 512-tap ditambah A2DP ternyata melebihi kapasitas SRAM ESP32 yang tersisa), kita harus mengurangi jumlah tap FIR ke 256. Kita akan mengujinya terlebih dahulu.
