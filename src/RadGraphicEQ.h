#ifndef RAD_GRAPHIC_EQ_H
#define RAD_GRAPHIC_EQ_H

#include <Arduino.h>
#include <math.h>
#include "RadControl.h"
#include "RadBiquad.h"

namespace RadDSP {

/**
 * @brief N-Band Graphic Equalizer
 * @tparam N Number of bands (typically 1 to 48)
 */
template<int N>
class GraphicEQ : public Controllable {
public:
    GraphicEQ() : _bypass(false) {
        float fMin = 20.0f;
        float fMax = 20000.0f;
        
        // Auto calculate Q-factor based on bands per octave (octave spacing)
        float qVal = 1.0f;
        if (N > 1) {
            // log2f(1000.0) is approx 9.965784f
            float octaves = log2f(fMax / fMin) / (N - 1);
            float twoToOct = powf(2.0f, octaves);
            qVal = sqrtf(twoToOct) / (twoToOct - 1.0f);
        } else {
            qVal = 1.0f;
        }

        for (int i = 0; i < N; i++) {
            _gains[i] = 0.0f;
            if (N == 1) {
                _freqs[i] = 1000.0f;
            } else {
                _freqs[i] = fMin * powf(fMax / fMin, (float)i / (N - 1));
            }
            // Set peaking EQ coefficients with zero gain initially
            _eqs[i].setPeakingEQ(_freqs[i], 0.0f, qVal);
        }
        
        snprintf(_typeStr, sizeof(_typeStr), "GraphicEQ<%d>", N);
    }

    ~GraphicEQ() {}

    void setParameter(uint8_t paramID, float value) override {
        if (paramID < N) {
            _gains[paramID] = value;
            
            // Re-calculate parameters to update coefficients
            float fMin = 20.0f;
            float fMax = 20000.0f;
            float qVal = 1.0f;
            if (N > 1) {
                float octaves = log2f(fMax / fMin) / (N - 1);
                float twoToOct = powf(2.0f, octaves);
                qVal = sqrtf(twoToOct) / (twoToOct - 1.0f);
            }
            _eqs[paramID].setPeakingEQ(_freqs[paramID], value, qVal);
        } else if (paramID == 100) {
            _bypass = (value > 0.5f);
        }
    }

    float getParameter(uint8_t paramID) override {
        if (paramID < N) {
            return _gains[paramID];
        } else if (paramID == 100) {
            return _bypass ? 1.0f : 0.0f;
        }
        return 0.0f;
    }

    const char* getType() override {
        return _typeStr;
    }

    inline float* process(float* buffer, int length) {
        if (_bypass) return buffer;
        for (int i = 0; i < N; i++) {
            _eqs[i].process(buffer, length);
        }
        return buffer;
    }

    inline void process(const float* input, float* output, int length) {
        if (_bypass) {
            if (input != output) {
                memcpy(output, input, length * sizeof(float));
            }
            return;
        }
        _eqs[0].process(input, output, length);
        for (int i = 1; i < N; i++) {
            _eqs[i].process(output, length);
        }
    }

private:
    Biquad _eqs[N];
    float _freqs[N];
    float _gains[N];
    bool _bypass;
    char _typeStr[24];
};

} // namespace RadDSP

#endif // RAD_GRAPHIC_EQ_H
