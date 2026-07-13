#ifndef RAD_FIR_H
#define RAD_FIR_H

#include <Arduino.h>
#include <esp_dsp.h>
#include "RadControl.h"

namespace RadDSP {

#define MAX_FIR_TAPS 512

class FIR : public Controllable {
public:
    /**
     * @brief Default constructor for FIR filter (empty)
     */
    FIR();

    /**
     * @brief Constructor for FIR filter with initial coefficients
     * @param coeffs Pointer to the FIR coefficients array
     * @param numCoeffs Number of taps (coefficients)
     */
    FIR(const float* coeffs, int numCoeffs);
    
    ~FIR();

    void setParameter(uint8_t paramID, float value) override;
    float getParameter(uint8_t paramID) override;
    const char* getType() override { return "FIR"; }

    /**
     * @brief Set or update the FIR coefficients dynamically (SafeLoad Double Buffered)
     * @param coeffs Pointer to the new FIR coefficients array
     * @param numCoeffs Number of taps (coefficients)
     * @return true if memory allocation succeeded
     */
    bool setCoeffs(const float* coeffs, int numCoeffs);

    /**
     * @brief Process an array of samples
     * @param input Input buffer
     * @param output Output buffer
     * @param length Number of samples to process
     */
    inline void process(const float* input, float* output, int length) {
        // Atomically grab the active pointer
        float* activeCoeffs = _activeCoeffs;
        if (!activeCoeffs || _numCoeffs <= 0 || _bypass) {
            if (input != output) {
                memcpy(output, input, length * sizeof(float));
            }
            return;
        }
        
        for (int i = 0; i < length; i++) {
            _head--;
            if (_head < 0) _head = _numCoeffs - 1;
            
            // Double-write for linear reading (no modulo needed in loop)
            _delayLine[_head] = input[i];
            _delayLine[_head + _numCoeffs] = input[i];
            
            float* pDelay = &_delayLine[_head];
            
            // 8 FPU Accumulator Registers Unrolled!
            float acc0 = 0.0f, acc1 = 0.0f, acc2 = 0.0f, acc3 = 0.0f;
            float acc4 = 0.0f, acc5 = 0.0f, acc6 = 0.0f, acc7 = 0.0f;
            
            int j = 0;
            int blocks = _numCoeffs / 8;
            
            // Compiler will throw this into 8 distinct FPU registers (ILP)
            for (int b = 0; b < blocks; b++) {
                acc0 += activeCoeffs[j] * pDelay[j]; j++;
                acc1 += activeCoeffs[j] * pDelay[j]; j++;
                acc2 += activeCoeffs[j] * pDelay[j]; j++;
                acc3 += activeCoeffs[j] * pDelay[j]; j++;
                acc4 += activeCoeffs[j] * pDelay[j]; j++;
                acc5 += activeCoeffs[j] * pDelay[j]; j++;
                acc6 += activeCoeffs[j] * pDelay[j]; j++;
                acc7 += activeCoeffs[j] * pDelay[j]; j++;
            }
            
            // Sum all accumulators
            float sum = acc0 + acc1 + acc2 + acc3 + acc4 + acc5 + acc6 + acc7;
            
            // Remaining taps not divisible by 8
            for (; j < _numCoeffs; j++) {
                sum += activeCoeffs[j] * pDelay[j];
            }
            
            output[i] = sum * _gain;
        }
    }

private:
    float _coeffsA[MAX_FIR_TAPS];
    float _coeffsB[MAX_FIR_TAPS];
    float* volatile _activeCoeffs;
    bool _usingA;
    
    // Staging buffer for serial uploads
    float _stagingCoeffs[MAX_FIR_TAPS];
    int _targetTapIndex;
    int _numStagingTaps;

    float _delayLine[MAX_FIR_TAPS * 2];
    int _numCoeffs;
    int _head;
    float _gain;
    bool _bypass;
};

} // namespace RadDSP

#endif // RAD_FIR_H
