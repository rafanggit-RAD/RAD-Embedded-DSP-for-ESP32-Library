#ifndef RAD_DSP_H
#define RAD_DSP_H

#include <functional>
#include "RadI2S.h"

#include "RadFIR.h"
#include "RadGain.h"
#include "RadMixer.h"
#include "RadFFT.h"
#include "RadBiquad.h"
#include "RadDynamics.h"
#include "RadControl.h"
#include "RadBluetooth.h"
#include "RadMatrix.h"
#include "RadMeter.h"

namespace RadDSP {
    // Global sample rate used by all DSP algorithms
    extern float systemSampleRate;
    
    // Helper function to get the current global sample rate
    inline float getSampleRate() { return systemSampleRate; }
    
    // Helper function to set the global sample rate
    inline void setSampleRate(float sr) { systemSampleRate = sr; }

    // Tipe untuk callback fungsi audio
    typedef void (*AudioTaskCallback)();

    /**
     * @brief Menjalankan fungsi secara terus-menerus di Core spesifik menggunakan FreeRTOS.
     * Fungsi ini membungkus callback ke dalam while(1) internal dengan prioritas Real-Time tertinggi.
     * 
     * @param callback Fungsi yang ingin dijalankan terus-menerus
     * @param coreID Core ESP32 (0 atau 1)
     * @param killArduinoLoop Jika true, akan langsung membunuh task Arduino loop() agar tidak membuang resource CPU.
     *                        PERINGATAN: Fungsi ini tidak akan pernah return jika killArduinoLoop = true. 
     *                        Letakkan fungsi ini di baris PALING AKHIR pada void setup() Anda!
     */
    void startAudioTask(AudioTaskCallback callback, int coreID = 1, bool killArduinoLoop = true);

    // Tipe untuk callback pemrosesan audio spesifik core (Mendukung Lambda / Anonymous Function)
    typedef std::function<void()> WorkerTask;

    /**
     * @brief Kelas sinkronisasi antar Core (Fork-Join Architecture).
     * Memungkinkan pemrosesan paralel (misal: Core 1 memproses Kiri, Core 0 memproses Kanan)
     * tanpa menambah latensi/delay.
     */
    class DualCoreWorker {
    public:
        DualCoreWorker();
        ~DualCoreWorker();

        /**
         * @brief Inisialisasi thread Helper di Core 0. 
         * Panggil ini sekali di setup().
         */
        void begin();

        /**
         * @brief Memproses dua fungsi secara paralel. 
         * Fungsi ini baru akan kembali (return) setelah KEDUA tugas selesai (Join).
         * @param core1Task Fungsi yang akan dijalankan oleh Core 1 (Utama)
         * @param core0Task Fungsi yang akan dijalankan oleh Core 0 (Helper)
         */
        void process(WorkerTask core1Task, WorkerTask core0Task);

    private:
        WorkerTask _core0Task;
        TaskHandle_t _helperTaskHandle;
        TaskHandle_t _mainTaskHandle;
        static void _helperTaskWrapper(void* pvParameters);
    };

    /**
     * @brief Kelas sinkronisasi antar Core dengan Arsitektur Ping-Pong (Sequential Offload).
     * Memungkinkan pemrogram melempar sinyal audio antar Core secara eksplisit tanpa menguras RAM (Zero-Copy).
     * Sangat cocok untuk DSP Mono berat (seperti efek Gitar) yang sensitif terhadap latency.
     */
    class Router {
    public:
        Router();
        ~Router();

        /**
         * @brief Inisialisasi Router dan menjalankan task loop di Core 1.
         * @param core1TaskFnc Fungsi loop tanpa henti yang akan dieksekusi oleh Core 1.
         */
        void begin(void (*core1TaskFnc)(void*));

        // --- UNTUK CORE 0 ---
        /**
         * @brief Melempar pointer audio ke Core 1
         */
        void sendToCore1(float* left, float* right, int len);
        
        /**
         * @brief Menunggu Core 1 selesai memproses audio (Mencegah Watchdog Panic)
         */
        void receiveFromCore1();

        // --- UNTUK CORE 1 ---
        /**
         * @brief Menunggu lemparan audio dari Core 0
         */
        void receiveFromCore0(float** left, float** right, int* len);
        
        /**
         * @brief Mengembalikan kontrol eksekusi ke Core 0
         */
        void sendToCore0();

    private:
        float* _shared_left;
        float* _shared_right;
        int _shared_len;
        
        TaskHandle_t _core0TaskHandle;
        TaskHandle_t _core1TaskHandle;
    };
}

#endif // RAD_DSP_H
