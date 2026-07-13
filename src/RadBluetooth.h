#ifndef RAD_BLUETOOTH_H
#define RAD_BLUETOOTH_H

#include <Arduino.h>
#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "freertos/ringbuf.h"

namespace RadDSP {

    class Bluetooth {
    public:
        Bluetooth();
        ~Bluetooth();

        /**
         * @brief Initialize ESP32 Native Bluetooth A2DP Sink
         * @param deviceName Name of the Bluetooth device
         */
        bool begin(const char* deviceName = "Precious DSP");

        /**
         * @brief Manually release Bluetooth controller memory to free up RAM if BT is not used
         * @return true if memory release succeeded
         */
        bool releaseMemory();

        /**
         * @brief Read audio data from the Bluetooth RingBuffer
         * @param leftBuffer Output left channel buffer (float -1.0 to 1.0)
         * @param rightBuffer Output right channel buffer (float -1.0 to 1.0)
         * @param length Number of samples per channel to read
         * @return true if data was successfully read, false if buffer is empty
         */
        bool readAudio(float* leftBuffer, float* rightBuffer, int length, uint32_t systemSampleRate = 48000);

        /**
         * @brief Check if Bluetooth A2DP Sink is connected to a source
         * @return true if connected, false otherwise
         */
        bool isConnected() const;

        /**
         * @brief Get the name of the connected Bluetooth device
         * @return Device name string, or "None" if not connected
         */
        const char* getConnectedDeviceName() const;

        /**
         * @brief Get the MAC address of the connected Bluetooth device
         * @return MAC address string, or "00:00:00:00:00:00" if not connected
         */
        const char* getConnectedDeviceMac() const;

        /**
         * @brief Get the current fill level of the Bluetooth ring buffer in bytes.
         * @return Number of bytes currently in the buffer, or 0 if not initialized.
         */
        static size_t getRingBufferFillBytes();

        /**
         * @brief Get the total capacity of the Bluetooth ring buffer in bytes.
         * @return Capacity in bytes, or 0 if not initialized.
         */
        static size_t getRingBufferCapacity();

    private:
        static void a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
        static void a2d_data_cb(const uint8_t *data, uint32_t len);
        static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
        
        static char _connectedName[64];
        static char _connectedMac[20];
        
        static RingbufHandle_t _ringBuf;
        static size_t _ringBufSize;
        static volatile uint32_t _bluetoothSampleRate;
        static volatile bool _connected;
        static esp_bd_addr_t _lastBda;
        static bool _hasLastBda;
        bool _initialized;

        static void loadLastDevice();
        static void saveLastDevice(esp_bd_addr_t bda);
        static void connectToLastDevice();

        // ASRC Circular Buffer & Phase State
        float _inBufL[1024];
        float _inBufR[1024];
        int _inBufHead;
        int _inBufTail;
        double _phase;
        float _lastRatio;
    };
}

#endif
