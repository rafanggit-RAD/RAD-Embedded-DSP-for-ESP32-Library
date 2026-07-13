#include "RadMeter.h"
#include <math.h>

namespace RadDSP {

Meter::Meter() : _peakDb(-80.0f), _decayFactor(0.95f), _currentPeak(0.0f) {}

float Meter::process(const float* input, int length, int mode) {
    float localMax = 0.0f;
    for (int i = 0; i < length; i++) {
        float absVal = fabsf(input[i]);
        if (absVal > localMax) {
            localMax = absVal;
        }
    }

    // Instant attack, smooth decay
    if (localMax >= _currentPeak) {
        _currentPeak = localMax;
    } else {
        _currentPeak = (_currentPeak * _decayFactor) + (localMax * (1.0f - _decayFactor));
    }

    // Konversi nilai puncak linear ke dBFS
    if (_currentPeak > 0.0001f) {
        _peakDb = 20.0f * log10f(_currentPeak);
    } else {
        _peakDb = -80.0f;
    }

    // Batasi nilai agar berada di range aman desibel
    if (_peakDb < -80.0f) _peakDb = -80.0f;
    if (_peakDb > 6.0f)  _peakDb = 6.0f;

    // Kembalikan nilai puncak berdasarkan mode yang dipilih
    if (mode == 0) {
        return _currentPeak; // Linear
    } else {
        return _peakDb;      // dBFS
    }
}

void Meter::setParameter(uint8_t paramID, float value) {
    if (paramID == 1) {
        _decayFactor = value;
        if (_decayFactor < 0.5f) _decayFactor = 0.5f;
        if (_decayFactor > 0.999f) _decayFactor = 0.999f;
    }
}

float Meter::getParameter(uint8_t paramID) {
    if (paramID == 0) {
        return _peakDb;
    } else if (paramID == 1) {
        return _decayFactor;
    }
    return -80.0f;
}

} // namespace RadDSP
