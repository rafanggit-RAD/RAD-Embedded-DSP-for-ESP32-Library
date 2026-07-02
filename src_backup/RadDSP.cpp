#include "RadDSP.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace RadDSP {

    static AudioTaskCallback _userAudioTask = nullptr;

    static void _internalAudioTaskWrapper(void* pvParameters) {
        if (_userAudioTask) {
            // Membungkus kode pengguna dengan infinite loop FreeRTOS
            // sehingga pengguna tidak perlu pusing memikirkan while(1)
            while (true) {
                _userAudioTask();
            }
        }
        vTaskDelete(NULL);
    }

    void startAudioTask(AudioTaskCallback callback, int coreID, bool killArduinoLoop) {
        _userAudioTask = callback;

        // Membuat task Audio RTOS pada Core pilihan dengan prioritas tertinggi (Real-Time)
        xTaskCreatePinnedToCore(
            _internalAudioTaskWrapper,
            "RadAudioTask",
            8192,                   // Stack size 8KB (sangat cukup untuk DSP arrays)
            NULL,
            configMAX_PRIORITIES - 1, // Prioritas paling tinggi di ESP32 FreeRTOS
            NULL,
            coreID
        );

        if (killArduinoLoop) {
            // Membunuh task loopTask Arduino! (Kode di bawah baris ini tidak akan pernah dieksekusi)
            vTaskDelete(NULL);
        }
    }

    // ============================================================================
    // DualCoreWorker Implementation (Fork-Join)
    // ============================================================================

    DualCoreWorker::DualCoreWorker() : _core0Task(nullptr), _helperTaskHandle(NULL), _mainTaskHandle(NULL) {
    }

    DualCoreWorker::~DualCoreWorker() {
        if (_helperTaskHandle) {
            vTaskDelete(_helperTaskHandle);
        }
    }

    void DualCoreWorker::_helperTaskWrapper(void* pvParameters) {
        DualCoreWorker* worker = static_cast<DualCoreWorker*>(pvParameters);
        while (true) {
            // Tunggu sinyal (Notifikasi) dari Core 1
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            
            // Eksekusi fungsi Core 0
            if (worker->_core0Task) {
                worker->_core0Task();
            }
            
            // Beri sinyal balik ke Core 1 bahwa kita sudah selesai
            xTaskNotifyGive(worker->_mainTaskHandle);
        }
    }

    void DualCoreWorker::begin() {
        // Buat task helper di Core 0
        xTaskCreatePinnedToCore(
            _helperTaskWrapper,
            "RadHelperTask",
            8192,
            this, // passing pointer to this object
            configMAX_PRIORITIES - 1, // Prioritas tertinggi
            &_helperTaskHandle,
            0 // Pin ke Core 0
        );
    }

    void DualCoreWorker::process(WorkerTask core1Task, WorkerTask core0Task) {
        if (!_helperTaskHandle) return; // Belum begin()

        // Ambil handle dari task yang mengeksekusi process() ini (yaitu RadAudioTask)
        _mainTaskHandle = xTaskGetCurrentTaskHandle();

        // 1. Simpan pointer fungsi Core 0
        _core0Task = core0Task;

        // 2. FORK: Bangunkan Helper Task di Core 0
        xTaskNotifyGive(_helperTaskHandle);

        // 3. PARALLEL: Eksekusi fungsi Core 1 di sini
        if (core1Task) {
            core1Task();
        }

        // 4. JOIN: Tunggu Core 0 selesai
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    // ============================================================================
    // Router Implementation (Sequential Offload / Ping-Pong)
    // ============================================================================

    Router::Router() : _shared_left(nullptr), _shared_right(nullptr), _shared_len(0),
                       _core0TaskHandle(NULL), _core1TaskHandle(NULL) {
    }

    Router::~Router() {
        if (_core1TaskHandle) {
            vTaskDelete(_core1TaskHandle);
        }
    }

    void Router::begin(void (*core1TaskFnc)(void*)) {
        // Buat task helper di Core 1
        xTaskCreatePinnedToCore(
            core1TaskFnc,
            "RadRouterCore1",
            8192,
            this,
            configMAX_PRIORITIES - 1,
            &_core1TaskHandle,
            1 // Pin ke Core 1
        );
    }

    void Router::sendToCore1(float* left, float* right, int len) {
        if (!_core0TaskHandle) {
            _core0TaskHandle = xTaskGetCurrentTaskHandle();
        }
        
        _shared_left = left;
        _shared_right = right;
        _shared_len = len;

        // Bangunkan Core 1
        xTaskNotifyGive(_core1TaskHandle);
    }

    void Router::receiveFromCore1() {
        // Tunggu Core 1 selesai memproses
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    void Router::receiveFromCore0(float** left, float** right, int* len) {
        // Tunggu kiriman dari Core 0
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        *left = _shared_left;
        *right = _shared_right;
        *len = _shared_len;
    }

    void Router::sendToCore0() {
        // Beri tahu Core 0 bahwa proses sudah selesai
        xTaskNotifyGive(_core0TaskHandle);
    }

}
