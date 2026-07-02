#ifndef RAD_FIR_H
#define RAD_FIR_H

#include <Arduino.h>
#include <esp_dsp.h>

namespace RadDSP {

class FIR {
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

    /**
     * @brief Set or update the FIR coefficients dynamically
     * @param coeffs Pointer to the new FIR coefficients array
     * @param numCoeffs Number of taps (coefficients)
     * @return true if memory allocation succeeded
     */
    bool setCoeffs(const float* coeffs, int numCoeffs);

    /**
     * @brief Process an array of samples
     * @param input Input buffer
     * @param output Output buffer (can be same as input)
     * @param length Number of samples to process
     * @note Implementasi ini menggunakan loop unrolling 8-jalur manual
     * untuk memaksa compiler menggunakan 8 register FPU secara paralel,
     * yang menghasilkan kecepatan sangat tinggi tanpa bergantung pada struct ESP-DSP.
     */
    inline void process(const float* input, float* output, int length) {
        if (!_coeffs || !_delayLine) return;

        for (int i = 0; i < length; i++) {
            // Shift delay line
            memmove(&_delayLine[1], &_delayLine[0], (_numCoeffs - 1) * sizeof(float));
            _delayLine[0] = input[i];

            // 8 FPU Accumulator Registers Unrolled!
            float acc0 = 0.0f, acc1 = 0.0f, acc2 = 0.0f, acc3 = 0.0f;
            float acc4 = 0.0f, acc5 = 0.0f, acc6 = 0.0f, acc7 = 0.0f;
            
            int j = 0;
            int blocks = _numCoeffs / 8;
            
            // Compiler akan melempar ini ke 8 FPU registers yang berbeda (ILP)
            for (int b = 0; b < blocks; b++) {
                acc0 += _coeffs[j] * _delayLine[j]; j++;
                acc1 += _coeffs[j] * _delayLine[j]; j++;
                acc2 += _coeffs[j] * _delayLine[j]; j++;
                acc3 += _coeffs[j] * _delayLine[j]; j++;
                acc4 += _coeffs[j] * _delayLine[j]; j++;
                acc5 += _coeffs[j] * _delayLine[j]; j++;
                acc6 += _coeffs[j] * _delayLine[j]; j++;
                acc7 += _coeffs[j] * _delayLine[j]; j++;
            }
            
            // Sum all accumulators
            float sum = acc0 + acc1 + acc2 + acc3 + acc4 + acc5 + acc6 + acc7;
            
            // Sisa taps yang tidak habis dibagi 8
            for (; j < _numCoeffs; j++) {
                sum += _coeffs[j] * _delayLine[j];
            }
            
            output[i] = sum;
        }
    }

private:
    float* _coeffs;
    float* _delayLine;
    int _numCoeffs;
};

} // namespace RadDSP

#endif // RAD_FIR_H
