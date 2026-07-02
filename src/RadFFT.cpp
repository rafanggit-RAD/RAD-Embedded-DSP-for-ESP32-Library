#include "RadFFT.h"
#include <esp_dsp.h>
#include <math.h>

namespace RadDSP {

FFT::FFT(int size) : _size(size), _complexBuffer(nullptr), _windowBuffer(nullptr), _initialized(false) {
}

FFT::~FFT() {
    if (_complexBuffer) {
        free(_complexBuffer);
    }
    if (_windowBuffer) {
        free(_windowBuffer);
    }
}

bool FFT::begin() {
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK) {
        // Init fails typically only if already initialized, which is fine, 
        // or memory issues. We'll proceed.
    }

    _complexBuffer = (float*)malloc(_size * 2 * sizeof(float));
    _windowBuffer = (float*)malloc(_size * sizeof(float));

    if (!_complexBuffer || !_windowBuffer) {
        return false;
    }

    // Generate Hann window
    dsps_wind_hann_f32(_windowBuffer, _size);
    
    _initialized = true;
    return true;
}

void FFT::applyWindow(float* input) {
    if (!_initialized) return;
    for (int i = 0; i < _size; i++) {
        input[i] *= _windowBuffer[i];
    }
}

void FFT::process(float* input, float* output) {
    if (!_initialized) return;

    // Convert real input to complex buffer for ESP-DSP
    for (int i = 0 ; i < _size ; i++) {
        _complexBuffer[i * 2 + 0] = input[i]; // Real part
        _complexBuffer[i * 2 + 1] = 0;        // Imaginary part
    }

    // Perform FFT
    dsps_fft2r_fc32(_complexBuffer, _size);
    
    // Bit reverse
    dsps_bit_rev_fc32(_complexBuffer, _size);
    
    // Calculate magnitude
    // dsps_cplx2reC_fc32 can be used, but manual is fine for outputting half size
    for (int i = 0 ; i < _size / 2 ; i++) {
        float re = _complexBuffer[i * 2 + 0];
        float im = _complexBuffer[i * 2 + 1];
        output[i] = sqrtf(re * re + im * im) / (_size / 2.0f);
    }
}

float FFT::getFrequency(int bin, float sampleRate) {
    return (float)bin * sampleRate / (float)_size;
}

} // namespace RadDSP
