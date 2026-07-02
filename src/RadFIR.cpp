#include "RadFIR.h"
#include <string.h>

namespace RadDSP {

FIR::FIR() : _activeCoeffs(nullptr), _usingA(true), _targetTapIndex(0), _numStagingTaps(0), _numCoeffs(0), _gain(1.0f), _bypass(false) {
    memset(_stagingCoeffs, 0, sizeof(_stagingCoeffs));
    memset(_delayLine, 0, sizeof(_delayLine));
    _head = 0;
}

FIR::FIR(const float* coeffs, int numCoeffs) : _activeCoeffs(nullptr), _usingA(true), _targetTapIndex(0), _numStagingTaps(0), _numCoeffs(0), _gain(1.0f), _bypass(false) {
    memset(_stagingCoeffs, 0, sizeof(_stagingCoeffs));
    memset(_delayLine, 0, sizeof(_delayLine));
    _head = 0;
    setCoeffs(coeffs, numCoeffs);
}

FIR::~FIR() {
    // Statis: Tidak perlu free()
}

bool FIR::setCoeffs(const float* coeffs, int numCoeffs) {
    if (numCoeffs <= 0) return false;
    if (numCoeffs > MAX_FIR_TAPS) numCoeffs = MAX_FIR_TAPS; // Cap to 512 taps

    // If size changed, we must reallocate everything (not safe during playback)
    // However, if size is same, we just update the inactive buffer safely.
    if (_numCoeffs != numCoeffs) {
        // Size changed, aman untuk reset memori _delayLine tanpa malloc/free
        memset((void*)_delayLine, 0, sizeof(_delayLine));
        _head = 0;
        _numCoeffs = numCoeffs;
    }
    
    if (_usingA) {
        // Sedang pakai A, copy ke B
        memcpy((void*)_coeffsB, coeffs, _numCoeffs * sizeof(float));
        _activeCoeffs = _coeffsB; // Atomic swap
        _usingA = false;
    } else {
        // Sedang pakai B, copy ke A
        memcpy((void*)_coeffsA, coeffs, _numCoeffs * sizeof(float));
        _activeCoeffs = _coeffsA; // Atomic swap
        _usingA = true;
    }
    
    return true;
}

void FIR::setParameter(uint8_t paramID, float value) {
    if (paramID == 0) {
        _targetTapIndex = (int)value;
        if (_targetTapIndex < 0) _targetTapIndex = 0;
        if (_targetTapIndex >= MAX_FIR_TAPS) _targetTapIndex = MAX_FIR_TAPS - 1;
    } else if (paramID == 1) {
        _stagingCoeffs[_targetTapIndex] = value;
    } else if (paramID == 2) {
        if (value > 0.5f) { // Commit command
            setCoeffs(_stagingCoeffs, _numStagingTaps);
        }
    } else if (paramID == 3) {
        _numStagingTaps = (int)value;
        if (_numStagingTaps < 1) _numStagingTaps = 1;
        if (_numStagingTaps > MAX_FIR_TAPS) _numStagingTaps = MAX_FIR_TAPS;
    } else if (paramID == 4) {
        _gain = powf(10.0f, value / 20.0f);
    } else if (paramID == 100) {
        _bypass = (value > 0.5f);
    }
}

float FIR::getParameter(uint8_t paramID) {
    if (paramID == 0) return (float)_targetTapIndex;
    if (paramID == 1) return _stagingCoeffs[_targetTapIndex];
    if (paramID == 2) return 0.0f; // Write-only trigger
    if (paramID == 3) return (float)_numStagingTaps;
    if (paramID == 4) return 20.0f * log10f(_gain > 0.00001f ? _gain : 0.00001f);
    if (paramID == 100) return _bypass ? 1.0f : 0.0f;
    
    return 0.0f;
}

} // namespace RadDSP
