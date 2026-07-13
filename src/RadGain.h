#ifndef RAD_GAIN_H
#define RAD_GAIN_H

#include <Arduino.h>
#include "esp_dsp.h"
#include "RadControl.h"
#include <math.h>

namespace RadDSP {

    class MonoGain : public Controllable {
    public:
        MonoGain() : _gainDB(0.0f), _mute(false), _invert(false), _multiplier(1.0f) {}

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

        const char* getType() override { return "MonoGain"; }

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

    // Alias untuk kompatibilitas ke belakang
    typedef MonoGain Gain;

    class StereoGain : public Controllable {
    public:
        StereoGain() : _gainDB(0.0f), _mute(false), _invert(false), _balance(0.0f), _multiplier(1.0f) {}

        void setParameter(uint8_t paramID, float value) override {
            switch (paramID) {
                case 0: _gainDB = value; break;
                case 1: _mute = (value > 0.5f); break;
                case 2: _invert = (value > 0.5f); break;
                case 3: _balance = value; break;
            }
            updateMultiplier();
        }

        float getParameter(uint8_t paramID) override {
            switch (paramID) {
                case 0: return _gainDB;
                case 1: return _mute ? 1.0f : 0.0f;
                case 2: return _invert ? 1.0f : 0.0f;
                case 3: return _balance;
            }
            return 0.0f;
        }

        const char* getType() override { return "StereoGain"; }

        inline void process(float* left, float* right, int length) {
            float multL = _multiplier * ((_balance <= 0.0f) ? 1.0f : (1.0f - _balance));
            float multR = _multiplier * ((_balance >= 0.0f) ? 1.0f : (1.0f + _balance));
            
            if (multL != 1.0f) {
                dsps_mulc_f32(left, left, length, multL, 1, 1);
            }
            if (multR != 1.0f) {
                dsps_mulc_f32(right, right, length, multR, 1, 1);
            }
        }

        inline void process(const float* inputL, const float* inputR, float* outputL, float* outputR, int length) {
            float multL = _multiplier * ((_balance <= 0.0f) ? 1.0f : (1.0f - _balance));
            float multR = _multiplier * ((_balance >= 0.0f) ? 1.0f : (1.0f + _balance));
            dsps_mulc_f32(inputL, outputL, length, multL, 1, 1);
            dsps_mulc_f32(inputR, outputR, length, multR, 1, 1);
        }

    private:
        float _gainDB;
        bool _mute;
        bool _invert;
        float _balance;
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
