#ifndef RAD_MATRIX_H
#define RAD_MATRIX_H

#include <Arduino.h>
#include "esp_dsp.h"
#include "RadControl.h"

namespace RadDSP {

    /**
     * @brief A Generic Adaptive Matrix Router.
     * Supports routing any of the NUM_IN Inputs to any of the NUM_OUT Outputs.
     */
    template <int NUM_IN, int NUM_OUT>
    class MatrixRouter : public Controllable {
    public:
        MatrixRouter() {
            // Initialize gain matrix to 0.0 (muted)
            for (int i = 0; i < NUM_IN; i++) {
                for (int j = 0; j < NUM_OUT; j++) {
                    _gainMatrix[i][j] = 0.0f;
                }
            }
            snprintf(_typeStr, sizeof(_typeStr), "MatrixRouter<%d,%d>", NUM_IN, NUM_OUT);
        }

        /**
         * ParamID format: (Input * NUM_OUT) + Output
         * Value: Gain in linear multiplier (0.0 to 1.0+)
         */
        void setParameter(uint8_t paramID, float value) override {
            int in = paramID / NUM_OUT;
            int out = paramID % NUM_OUT;
            if (in < NUM_IN && out < NUM_OUT) {
                _gainMatrix[in][out] = value;
            }
        }

        float getParameter(uint8_t paramID) override {
            int in = paramID / NUM_OUT;
            int out = paramID % NUM_OUT;
            if (in < NUM_IN && out < NUM_OUT) {
                return _gainMatrix[in][out];
            }
            return 0.0f;
        }

        const char* getType() override { return _typeStr; }

        void setRouteLinear(int inputIdx, int outputIdx, float linearGain) {
            if (inputIdx < NUM_IN && outputIdx < NUM_OUT) {
                _gainMatrix[inputIdx][outputIdx] = linearGain;
            }
        }

        /**
         * @brief Process the matrix mix (Mono version)
         */
        void process(float* inputs[], float* outputs[], int length) {
            // Clear outputs first (Safe Guard against Null Pointers)
            for (int outIdx = 0; outIdx < NUM_OUT; outIdx++) {
                if (outputs[outIdx] != nullptr) memset(outputs[outIdx], 0, length * sizeof(float));
            }

            // Matrix Accumulation
            for (int outIdx = 0; outIdx < NUM_OUT; outIdx++) {
                if (outputs[outIdx] == nullptr) continue; 
                
                for (int inIdx = 0; inIdx < NUM_IN; inIdx++) {
                    if (inputs[inIdx] == nullptr) continue; 
                    
                    float gain = _gainMatrix[inIdx][outIdx];
                    if (gain > 0.0001f) {
                        for (int i = 0; i < length; i++) {
                            outputs[outIdx][i] += inputs[inIdx][i] * gain;
                        }
                    }
                }
            }
        }

    private:
        float _gainMatrix[NUM_IN][NUM_OUT]; // [Input][Output]
        char _typeStr[32];
    };
}

#endif
