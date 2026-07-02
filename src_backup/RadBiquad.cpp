#include "RadBiquad.h"
#include "RadDSP.h"
#include <esp_dsp.h>
#include <math.h>
#include <string.h>

namespace RadDSP {

Biquad::Biquad() : _type(0), _freq(1000.0f), _gain(0.0f), _q(0.707f) {
    memset(_coeffsA, 0, sizeof(_coeffsA));
    memset(_coeffsB, 0, sizeof(_coeffsB));
    memset(_delayLines, 0, sizeof(_delayLines));
    _coeffsA[0] = 1.0f;
    _coeffsB[0] = 1.0f;
    _useA = true;
    _activeCoeffs = _coeffsA;
}

Biquad::~Biquad() {
}

void Biquad::setLowpass(float cutoff, float q) {
    _type = 0; _freq = cutoff; _q = q; _gain = 0.0f;
    float f = cutoff / getSampleRate();
    float* target = _useA ? _coeffsB : _coeffsA;
    dsps_biquad_gen_lpf_f32(target, f, q);
    _activeCoeffs = target;
    _useA = !_useA;
}

void Biquad::setHighpass(float cutoff, float q) {
    _type = 1; _freq = cutoff; _q = q; _gain = 0.0f;
    float f = cutoff / getSampleRate();
    float* target = _useA ? _coeffsB : _coeffsA;
    dsps_biquad_gen_hpf_f32(target, f, q);
    _activeCoeffs = target;
    _useA = !_useA;
}

void Biquad::setBandpass(float centerFreq, float q) {
    _type = 2; _freq = centerFreq; _q = q; _gain = 0.0f;
    float f = centerFreq / getSampleRate();
    float* target = _useA ? _coeffsB : _coeffsA;
    dsps_biquad_gen_bpf_f32(target, f, q);
    _activeCoeffs = target;
    _useA = !_useA;
}

void Biquad::setPeakingEQ(float centerFreq, float gain, float q) {
    _type = 3; _freq = centerFreq; _q = q; _gain = gain;
    float f = centerFreq / getSampleRate();
    float linearGain = powf(10.0f, gain / 20.0f);
    float* target = _useA ? _coeffsB : _coeffsA;
    
    float w0 = 2.0f * M_PI * f;
    float alpha = sinf(w0) / (2.0f * q);
    float A = sqrtf(linearGain);
    
    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cosf(w0);
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cosf(w0);
    float a2 = 1.0f - alpha / A;
    
    target[0] = b0 / a0;
    target[1] = b1 / a0;
    target[2] = b2 / a0;
    target[3] = a1 / a0; 
    target[4] = a2 / a0;
    
    _activeCoeffs = target;
    _useA = !_useA;
}

void Biquad::setLowShelf(float cutoff, float gain, float q) {
    _type = 4; _freq = cutoff; _q = q; _gain = gain;
    float f = cutoff / getSampleRate();
    float linearGain = powf(10.0f, gain / 20.0f);
    float* target = _useA ? _coeffsB : _coeffsA;
    
    float w0 = 2.0f * M_PI * f;
    float alpha = sinf(w0) / (2.0f * q);
    float A = sqrtf(linearGain);
    
    float b0 = A * ((A + 1.0f) - (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha);
    float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosf(w0));
    float b2 = A * ((A + 1.0f) - (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha);
    float a0 = (A + 1.0f) + (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha;
    float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosf(w0));
    float a2 = (A + 1.0f) + (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha;
    
    target[0] = b0 / a0;
    target[1] = b1 / a0;
    target[2] = b2 / a0;
    target[3] = a1 / a0;
    target[4] = a2 / a0;
    
    _activeCoeffs = target;
    _useA = !_useA;
}

void Biquad::setHighShelf(float cutoff, float gain, float q) {
    _type = 5; _freq = cutoff; _q = q; _gain = gain;
    float f = cutoff / getSampleRate();
    float linearGain = powf(10.0f, gain / 20.0f);
    float* target = _useA ? _coeffsB : _coeffsA;
    
    float w0 = 2.0f * M_PI * f;
    float alpha = sinf(w0) / (2.0f * q);
    float A = sqrtf(linearGain);
    
    float b0 = A * ((A + 1.0f) + (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha);
    float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosf(w0));
    float b2 = A * ((A + 1.0f) + (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha);
    float a0 = (A + 1.0f) - (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha;
    float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosf(w0));
    float a2 = (A + 1.0f) - (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha;
    
    target[0] = b0 / a0;
    target[1] = b1 / a0;
    target[2] = b2 / a0;
    target[3] = a1 / a0;
    target[4] = a2 / a0;
    
    _activeCoeffs = target;
    _useA = !_useA;
}


void Biquad::setParameter(uint8_t paramID, float value) {
    // Parameter Mapping: 0=Type, 1=Freq, 2=Gain, 3=Q
    if (paramID == 0) _type = (uint8_t)value;
    else if (paramID == 1) _freq = value;
    else if (paramID == 2) _gain = value;
    else if (paramID == 3) _q = value;

    // Recalculate
    if (_type == 0) setLowpass(_freq, _q);
    else if (_type == 1) setHighpass(_freq, _q);
    else if (_type == 2) setBandpass(_freq, _q);
    else if (_type == 3) setPeakingEQ(_freq, _gain, _q);
    else if (_type == 4) setLowShelf(_freq, _gain, _q);
    else if (_type == 5) setHighShelf(_freq, _gain, _q);
}

float Biquad::getParameter(uint8_t paramID) {
    if (paramID == 0) return (float)_type;
    if (paramID == 1) return _freq;
    if (paramID == 2) return _gain;
    if (paramID == 3) return _q;
    return 0.0f;
}

} // namespace RadDSP
