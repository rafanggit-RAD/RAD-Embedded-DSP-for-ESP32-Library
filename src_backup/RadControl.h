#ifndef RAD_CONTROL_H
#define RAD_CONTROL_H

#include <Arduino.h>

namespace RadDSP {

    /**
     * @brief Interface abstrak untuk modul DSP yang bisa dikontrol
     */
    class Controllable {
    public:
        // paramID: 0=Type, 1=Freq, 2=Gain, 3=Q (Tergantung implementasi modul)
        virtual void setParameter(uint8_t paramID, float value) = 0;
        virtual float getParameter(uint8_t paramID) = 0;
    };

    /**
     * @brief Controller untuk mem-parsing komando dari Serial dan I2C
     */
    class Controller {
    public:
        Controller();

        /**
         * @brief Mendaftarkan modul DSP ke dalam sistem kontrol
         * @param id ID unik untuk modul ini (0-255)
         * @param module Pointer ke objek filter (misal &eq1)
         */
        void attach(uint8_t id, Controllable* module);

        /**
         * @brief Mengaktifkan kontrol via Serial (Format: ID,Param,Value)
         */
        void beginSerial(long baudRate = 115200);

        /**
         * @brief Mengaktifkan kontrol via I2C Slave
         */
        void beginI2C(uint8_t address, int sdaPin = 21, int sclPin = 22);

        /**
         * @brief Mengecek data masuk dari Serial. Panggil fungsi ini di dalam loop()!
         */
        void poll();

        /**
         * @brief Menandai awal dari blok pemrosesan DSP
         * @param coreID Core ID (0 atau 1)
         */
        void markProcessStart(uint8_t coreID = 1);

        /**
         * @brief Menandai akhir dari blok pemrosesan DSP dan menghitung Utilisasi Core
         * @param coreID Core ID (0 atau 1)
         * @param sampleCount Jumlah sampel per blok (misal 64)
         * @param sampleRate Sample rate (misal 48000)
         */
        void markProcessEnd(uint8_t coreID, int sampleCount, int sampleRate);

        /**
         * @brief Internal use: Mengeksekusi parameter ke modul yang dituju
         */
        void executeCommand(uint8_t id, uint8_t paramID, float value);

    private:
        Controllable* _modules[256];
        bool _serialEnabled;
        char _serialBuffer[128];
        uint8_t _bufLen;
        uint32_t _processStartTs[2];
        float _dspLoad[2];
    };

}

#endif
