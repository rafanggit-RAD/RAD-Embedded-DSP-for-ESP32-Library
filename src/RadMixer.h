#ifndef RAD_MIXER_H
#define RAD_MIXER_H

#include <Arduino.h>
#include "RadControl.h"
#include <math.h>

namespace RadDSP {

    /**
     * @brief N-Channel Mono Mixer template class using Variadic Pack Processing
     * @tparam N Number of input audio channels to mix
     */
    template<int N>
    class Mixer : public Controllable {
    public:
        Mixer() {
            for (int i = 0; i < N; i++) {
                _gainsDB[i] = 0.0f;
                _mutes[i] = false;
                _muls[i] = 1.0f;
            }
            memset(_outBuf, 0, sizeof(_outBuf));
        }

        void setParameter(uint8_t paramID, float value) override {
            if (paramID < N) {
                _gainsDB[paramID] = value;
                updateMul(paramID);
            } else if (paramID >= 100 && paramID < 100 + N) {
                _mutes[paramID - 100] = (value > 0.5f);
                updateMul(paramID - 100);
            }
        }

        float getParameter(uint8_t paramID) override {
            if (paramID < N) return _gainsDB[paramID];
            if (paramID >= 100 && paramID < 100 + N) return _mutes[paramID - 100] ? 1.0f : 0.0f;
            return 0.0f;
        }

        void updateMul(int ch) {
            _muls[ch] = _mutes[ch] ? 0.0f : powf(10.0f, _gainsDB[ch] / 20.0f);
        }

        #pragma GCC optimize ("O3")
        template<typename... Args>
        float* process(int length, Args... inputs) {
            static_assert(sizeof...(inputs) == N, "Number of inputs must match the template parameter N!");
            
            // Kumpulkan input pack ke array lokal pointer
            const float* inputList[N] = { inputs... };

            // Bersihkan buffer output internal
            int safeLength = length > 256 ? 256 : length;
            memset(_outBuf, 0, safeLength * sizeof(float));

            // Lakukan pencampuran dan akumulasi gain per channel
            for (int ch = 0; ch < N; ch++) {
                register float g = _muls[ch];
                const float* in = inputList[ch];
                if (in) {
                    for (int i = 0; i < safeLength; i++) {
                        _outBuf[i] += in[i] * g;
                    }
                }
            }
            return _outBuf;
        }

    private:
        float _gainsDB[N];
        bool _mutes[N];
        float _muls[N];
        float _outBuf[256];
    };

    /**
     * @brief M-Channel Mono Splitter template class using Variadic Pack Processing
     * Splits 1 input audio channel into M output channels with individual gain controls.
     * @tparam M Number of output audio channels to split into
     */
    template<int M>
    class Splitter : public Controllable {
    public:
        Splitter() {
            for (int i = 0; i < M; i++) {
                _gainsDB[i] = 0.0f;
                _muls[i] = 1.0f;
            }
        }

        void setParameter(uint8_t paramID, float value) override {
            if (paramID < M) {
                _gainsDB[paramID] = value;
                _muls[paramID] = powf(10.0f, value / 20.0f);
            }
        }

        float getParameter(uint8_t paramID) override {
            if (paramID < M) return _gainsDB[paramID];
            return 0.0f;
        }

        #pragma GCC optimize ("O3")
        template<typename... Args>
        void process(int length, const float* input, Args... outputs) {
            static_assert(sizeof...(outputs) == M, "Number of outputs must match the template parameter M!");
            
            // Kumpulkan output pack ke array lokal pointer
            float* outputList[M] = { outputs... };
            int safeLength = length > 256 ? 256 : length;

            // Salin input ke masing-masing output dengan gain independen
            for (int ch = 0; ch < M; ch++) {
                register float g = _muls[ch];
                float* out = outputList[ch];
                if (out && input) {
                    for (int i = 0; i < safeLength; i++) {
                        out[i] = input[i] * g;
                    }
                }
            }
        }

    private:
        float _gainsDB[M];
        float _muls[M];
    };
}

#endif
