#ifndef RAD_GAIN_H
#define RAD_GAIN_H

#include <Arduino.h>
#include "esp_dsp.h"
#include "RadControl.h"
#include <math.h>

namespace RadDSP {

    class Gain : public Controllable {
    public:
        Gain() : _gainDB(0.0f), _mute(false), _invert(false), _multiplier(1.0f) {}

        void setParameter(uint8_t paramID, float value) override {
            switch (paramID) {
                case 0: _gainDB = value; break;
                case 1: _mute = (value > 0.5f); break;
                case 2: _invert = (value > 0.5f); break;
            }
            updateMultiplier();
        }

        float getParameter(uint8_t paramID) override {
            switch (paramID) {
                case 0: return _gainDB;
                case 1: return _mute ? 1.0f : 0.0f;
                case 2: return _invert ? 1.0f : 0.0f;
            }
            return 0.0f;
        }

        inline float* process(float* buffer, int length) {
            if (_multiplier != 1.0f) {
                dsps_mulc_f32(buffer, buffer, length, _multiplier, 1, 1);
            }
            return buffer;
        }
        
        inline void process(const float* input, float* output, int length) {
            dsps_mulc_f32(input, output, length, _multiplier, 1, 1);
        }

    private:
        float _gainDB;
        bool _mute;
        bool _invert;
        float _multiplier;

        void updateMultiplier() {
            if (_mute) {
                _multiplier = 0.0f;
            } else {
                _multiplier = pow(10.0f, _gainDB / 20.0f);
                if (_invert) _multiplier = -_multiplier;
            }
        }
    };
}

#endif
