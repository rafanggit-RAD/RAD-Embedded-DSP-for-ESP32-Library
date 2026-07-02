#ifndef RAD_FFT_H
#define RAD_FFT_H

#include <Arduino.h>

namespace RadDSP {

class FFT {
public:
    /**
     * @brief Constructor for FFT
     * @param size Size of the FFT (must be power of 2, e.g., 512, 1024, 2048)
     */
    FFT(int size = 1024);
    
    ~FFT();

    /**
     * @brief Initialize the FFT buffers and tables
     * @return true if successful
     */
    bool begin();

    /**
     * @brief Apply Hann window to the input buffer
     * @param input Input buffer (size must be >= FFT size)
     */
    void applyWindow(float* input);

    /**
     * @brief Execute the FFT processing
     * @param input Array of float size (FFT size). Will be converted to complex.
     * @param output Array of float size (FFT size / 2) to store the magnitudes.
     */
    void process(float* input, float* output);

    /**
     * @brief Get the frequency at the specific bin
     * @param bin Bin index
     * @param sampleRate Sampling rate (e.g. 48000)
     * @return Frequency in Hz
     */
    float getFrequency(int bin, float sampleRate);

private:
    int _size;
    float* _complexBuffer;
    float* _windowBuffer;
    bool _initialized;
};

} // namespace RadDSP

#endif // RAD_FFT_H
