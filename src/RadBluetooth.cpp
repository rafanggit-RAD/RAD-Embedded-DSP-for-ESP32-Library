#include "RadBluetooth.h"
#include <nvs_flash.h>
#include "esp_bt.h"

// PENTING: Arduino ESP32 membebaskan memori BT secara default jika tidak ada library BT standard yang di-link.
// Kita override fungsi btInUse() agar bernilai true secara eksplisit untuk mencegah pelepasan memori BT!
extern "C" bool btInUse(void) {
    return true;
}

namespace RadDSP {

RingbufHandle_t Bluetooth::_ringBuf = NULL;
volatile uint32_t Bluetooth::_bluetoothSampleRate = 44100;
esp_bd_addr_t Bluetooth::_lastBda = {0};
bool Bluetooth::_hasLastBda = false;

Bluetooth::Bluetooth() : _initialized(false), _inBufHead(0), _inBufTail(0), _phase(0.0), _lastRatio(0.91875f) {
    memset(_inBufL, 0, sizeof(_inBufL));
    memset(_inBufR, 0, sizeof(_inBufR));
}

Bluetooth::~Bluetooth() {}

bool Bluetooth::begin(const char* deviceName) {
    if (_initialized) return true;

    Serial.println("[BT] ===== Bluetooth A2DP Sink Init =====");
    Serial.printf("[BT] Free heap before init: %u bytes\n", ESP.getFreeHeap());

    // Step 0: RingBuffer
    _ringBuf = xRingbufferCreate(8192, RINGBUF_TYPE_BYTEBUF);
    if (_ringBuf == NULL) {
        Serial.println("[BT] FAIL Step 0: RingBuffer!");
        return false;
    }
    Serial.println("[BT] Step 0 OK: RingBuffer created");

    // Step 1: NVS (wajib untuk BT pairing keys)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    Serial.printf("[BT] Step 1: NVS = %s (code %d)\n", ret == ESP_OK ? "OK" : "FAIL", ret);
    if (ret != ESP_OK) return false;

    // Step 2: BT Controller Init & Enable menggunakan API Core 3.0+
    Serial.print("[BT] Step 2: btStartMode(BT_MODE_CLASSIC_BT)... ");
    bool bt_ok = btStartMode(BT_MODE_CLASSIC_BT);
    Serial.println(bt_ok ? "OK" : "FAIL");
    if (!bt_ok) {
        Serial.print("[BT] Step 2a: Try btStartMode(BT_MODE_BTDM) as fallback... ");
        bt_ok = btStartMode(BT_MODE_BTDM);
        Serial.println(bt_ok ? "OK" : "FAIL");
        if (!bt_ok) return false;
    }

    // Step 3: Bluedroid Init
    esp_bluedroid_status_t bd_status = esp_bluedroid_get_status();
    if (bd_status == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        ret = esp_bluedroid_init();
        if (ret != ESP_OK) {
            Serial.printf("[BT] FAIL Step 3: Bluedroid init error %d\n", ret);
            return false;
        }
        Serial.println("[BT] Step 3 OK: Bluedroid initialized");
    }

    // Step 4: Bluedroid Enable
    bd_status = esp_bluedroid_get_status();
    if (bd_status == ESP_BLUEDROID_STATUS_INITIALIZED) {
        ret = esp_bluedroid_enable();
        if (ret != ESP_OK) {
            Serial.printf("[BT] FAIL Step 4: Bluedroid enable error %d\n", ret);
            return false;
        }
        Serial.println("[BT] Step 4 OK: Bluedroid enabled");
    }

    // Step 5: Device Name & GAP
    esp_bt_dev_set_device_name(deviceName);
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    Serial.println("[BT] Step 5 OK: GAP configured");

    // Step 6: A2DP Sink Init
    ret = esp_a2d_sink_init();
    if (ret != ESP_OK) {
        Serial.printf("[BT] FAIL Step 6: A2DP Sink init error %d\n", ret);
        return false;
    }
    esp_a2d_register_callback(a2d_cb);
    esp_a2d_sink_register_data_callback(a2d_data_cb);
    Serial.println("[BT] Step 6 OK: A2DP Sink ready");

    _initialized = true;
    Serial.printf("[BT] ===== SUCCESS! \"%s\" is discoverable! =====\n", deviceName);
    Serial.printf("[BT] Free heap after init: %u bytes\n", ESP.getFreeHeap());
    
    // Panggil Fast Connect jika ada device tersimpan
    loadLastDevice();
    if (_hasLastBda) {
        connectToLastDevice();
    }

    return true;
}

bool Bluetooth::releaseMemory() {
    if (_initialized) {
        Serial.println("[BT] Cannot release memory: Bluetooth is currently running.");
        return false;
    }
    
    // Bebaskan memori BT Controller (mode Classic & BLE) ke heap umum
    esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
    if (err == ESP_OK) {
        Serial.println("[BT] Bluetooth controller memory released successfully!");
        return true;
    } else {
        Serial.printf("[BT] Failed to release controller memory. Error code: %d\n", err);
        return false;
    }
}

void Bluetooth::loadLastDevice() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("rad_bt_store", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        size_t required_size = sizeof(esp_bd_addr_t);
        err = nvs_get_blob(my_handle, "last_bda", _lastBda, &required_size);
        if (err == ESP_OK) {
            _hasLastBda = true;
            Serial.printf("[BT] Loaded last paired BDA: %02x:%02x:%02x:%02x:%02x:%02x\n",
                          _lastBda[0], _lastBda[1], _lastBda[2], _lastBda[3], _lastBda[4], _lastBda[5]);
        }
        nvs_close(my_handle);
    }
}

void Bluetooth::saveLastDevice(esp_bd_addr_t bda) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("rad_bt_store", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(my_handle, "last_bda", bda, sizeof(esp_bd_addr_t));
        if (err == ESP_OK) {
            err = nvs_commit(my_handle);
            if (err == ESP_OK) {
                memcpy(_lastBda, bda, sizeof(esp_bd_addr_t));
                _hasLastBda = true;
                Serial.printf("[BT] Saved new BDA to NVS: %02x:%02x:%02x:%02x:%02x:%02x\n",
                              bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
            }
        }
        nvs_close(my_handle);
    }
}

void Bluetooth::connectToLastDevice() {
    if (!_hasLastBda) return;
    Serial.printf("[BT] Fast Connect: Reconnecting back to last device...\n");
    esp_a2d_sink_connect(_lastBda);
}

void Bluetooth::a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                Serial.println("[BT] A2DP Connected!");
                // Simpan MAC Address perangkat baru ke NVS
                saveLastDevice(param->conn_stat.remote_bda);
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                Serial.println("[BT] A2DP Disconnected");
            }
            break;
        case ESP_A2D_AUDIO_STATE_EVT:
            if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
                Serial.println("[BT] Audio streaming started");
            }
            break;
        case ESP_A2D_AUDIO_CFG_EVT: {
            uint32_t sample_rate = 44100; // default fallback
            // Decode SBC negotiated sample rate (bits 4-7 of SBC Octet 0)
            char oct0 = param->audio_cfg.mcc.cie.sbc[0];
            if (oct0 & (0x01 << 6)) {
                sample_rate = 32000;
            } else if (oct0 & (0x01 << 5)) {
                sample_rate = 44100;
            } else if (oct0 & (0x01 << 4)) {
                sample_rate = 48000;
            }
            Serial.printf("[BT] Codec configuration event. Negotiated rate: %u Hz\n", sample_rate);
            _bluetoothSampleRate = sample_rate;
            break;
        }
        default:
            break;
    }
}

void Bluetooth::a2d_data_cb(const uint8_t *data, uint32_t len) {
    if (_ringBuf) {
        xRingbufferSend(_ringBuf, (void *)data, len, 0);
    }
}

bool Bluetooth::readAudio(float* leftBuffer, float* rightBuffer, int length, uint32_t systemSampleRate) {
    if (!_ringBuf) return false;

    // 1. Tentukan sampling rate asal (dari negosiasi BT) dan target (dari parameter I2S)
    uint32_t srcRate = _bluetoothSampleRate;
    uint32_t dstRate = systemSampleRate;

    // Jika laju asal sama dengan laju target (misal sama-sama 44.1 kHz atau 48 kHz),
    // kita lakukan bypass resampling kubik Hermite untuk menghemat CPU!
    if (srcRate == dstRate) {
        int bytesNeeded = length * 2 * sizeof(int16_t);
        size_t item_size = 0;
        uint8_t* item = (uint8_t*)xRingbufferReceiveUpTo(_ringBuf, &item_size, 0, bytesNeeded);
        if (item != NULL) {
            int samplesPulled = item_size / (2 * sizeof(int16_t));
            int16_t* ptr = (int16_t*)item;
            for (int i = 0; i < samplesPulled; i++) {
                leftBuffer[i] = (float)ptr[i * 2] * 3.0517578125e-5f;
                rightBuffer[i] = (float)ptr[i * 2 + 1] * 3.0517578125e-5f;
            }
            vRingbufferReturnItem(_ringBuf, (void*)item);
            
            // Pad sisanya jika ditarik kurang dari length
            for (int i = samplesPulled; i < length; i++) {
                leftBuffer[i] = 0.0f;
                rightBuffer[i] = 0.0f;
            }
            return (samplesPulled > 0);
        }
        
        // Underflow murni
        for (int i = 0; i < length; i++) {
            leftBuffer[i] = 0.0f;
            rightBuffer[i] = 0.0f;
        }
        return false;
    }

    // 2. Jika laju berbeda, lakukan ASRC Adaptif
    // Monitor level pengisian RingBuffer Bluetooth
    size_t uxFree = 0, uxRead = 0, uxWrite = 0, uxAcquire = 0, uxItems = 0;
    vRingbufferGetInfo(_ringBuf, &uxFree, &uxRead, &uxWrite, &uxAcquire, &uxItems);
    
    float fillLevel = (float)(8192 - uxFree) / 8192.0f;
    if (fillLevel < 0.0f) fillLevel = 0.0f;
    if (fillLevel > 1.0f) fillLevel = 1.0f;
    
    // Rasio konversi nominal dinamis
    float nominalRatio = (float)srcRate / (float)dstRate;
    
    // Batasi deviasi maksimum ke +/- 0.003f dari nominal ratio
    float targetFill = 0.5f;
    float error = fillLevel - targetFill;
    float targetRatio = nominalRatio + error * 0.003f;
    
    // LPF (EMA) untuk meredam fluktuasi pitch cepat (chorus effect)
    float ratio = _lastRatio + 0.002f * (targetRatio - _lastRatio);
    
    // Batasi dalam rentang sangat aman dekat nominal (+/- 0.5%)
    float ratioMin = nominalRatio * 0.995f;
    float ratioMax = nominalRatio * 1.005f;
    if (ratio < ratioMin) ratio = ratioMin;
    if (ratio > ratioMax) ratio = ratioMax;
    _lastRatio = ratio;

    // 3. Hitung sisa ruang kosong di circular buffer internal kita
    int availableSamples = (_inBufHead - _inBufTail + 1024) % 1024;
    int freeSpace = 1023 - availableSamples;
    
    int maxSamplesToPull = freeSpace;
    int bytesToPull = maxSamplesToPull * 2 * sizeof(int16_t);
    
    if (bytesToPull > 0) {
        size_t item_size = 0;
        uint8_t* item = (uint8_t*)xRingbufferReceiveUpTo(_ringBuf, &item_size, 0, bytesToPull);
        if (item != NULL) {
            int samplesPulled = item_size / (2 * sizeof(int16_t));
            int16_t* ptr = (int16_t*)item;
            for (int i = 0; i < samplesPulled; i++) {
                _inBufL[_inBufHead] = (float)ptr[i * 2] * 3.0517578125e-5f;
                _inBufR[_inBufHead] = (float)ptr[i * 2 + 1] * 3.0517578125e-5f;
                _inBufHead = (_inBufHead + 1) % 1024;
            }
            vRingbufferReturnItem(_ringBuf, (void*)item);
            availableSamples += samplesPulled;
        }
    }

    // 4. Tentukan jumlah sampel output yang aman untuk dirender
    int samplesToRender = length;
    int samplesWeWillConsume = (int)(length * ratio) + 3;
    
    if (availableSamples < samplesWeWillConsume) {
        if (availableSamples > 3) {
            samplesToRender = (int)((availableSamples - 3) / ratio);
            if (samplesToRender < 0) samplesToRender = 0;
            if (samplesToRender > length) samplesToRender = length;
        } else {
            samplesToRender = 0;
        }
    }

    // 5. Lakukan interpolasi kubik Hermite ke buffer output
    double p = _phase;
    int tail = _inBufTail;
    
    for (int i = 0; i < samplesToRender; i++) {
        int idx1 = (tail + (int)p) % 1024;
        int idx0 = (idx1 - 1 + 1024) % 1024;
        int idx2 = (idx1 + 1) % 1024;
        int idx3 = (idx1 + 2) % 1024;
        
        float frac = (float)(p - (int)p);
        
        // Channel Kiri
        {
            float y0 = _inBufL[idx0];
            float y1 = _inBufL[idx1];
            float y2 = _inBufL[idx2];
            float y3 = _inBufL[idx3];
            float c0 = y1;
            float c1 = 0.5f * (y2 - y0);
            float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            float c3 = 0.5f * (y1 - y3) + 1.5f * (y2 - y1);
            leftBuffer[i] = ((c3 * frac + c2) * frac + c1) * frac + c0;
        }
        
        // Channel Kanan
        {
            float y0 = _inBufR[idx0];
            float y1 = _inBufR[idx1];
            float y2 = _inBufR[idx2];
            float y3 = _inBufR[idx3];
            float c0 = y1;
            float c1 = 0.5f * (y2 - y0);
            float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            float c3 = 0.5f * (y1 - y3) + 1.5f * (y2 - y1);
            rightBuffer[i] = ((c3 * frac + c2) * frac + c1) * frac + c0;
        }
        
        p += ratio;
    }
    
    // Pad sisa buffer dengan zero jika terjadi underflow parsial
    for (int i = samplesToRender; i < length; i++) {
        leftBuffer[i]  = 0.0f;
        rightBuffer[i] = 0.0f;
    }
    
    // Perbarui penunjuk baca circular buffer dan sisa fase fraksional
    _inBufTail = (tail + (int)p) % 1024;
    _phase = p - (int)p;
    
    return (samplesToRender > 0);
}

} // namespace RadDSP
