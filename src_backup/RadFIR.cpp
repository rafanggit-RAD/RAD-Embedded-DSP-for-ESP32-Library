#include "RadFIR.h"
#include <string.h>

namespace RadDSP {

FIR::FIR() : _coeffs(nullptr), _delayLine(nullptr), _numCoeffs(0) {
}

FIR::FIR(const float* coeffs, int numCoeffs) : _coeffs(nullptr), _delayLine(nullptr), _numCoeffs(0) {
    setCoeffs(coeffs, numCoeffs);
}

FIR::~FIR() {
    if (_coeffs) free(_coeffs);
    if (_delayLine) free(_delayLine);
}

bool FIR::setCoeffs(const float* coeffs, int numCoeffs) {
    if (_coeffs) free(_coeffs);
    if (_delayLine) free(_delayLine);
    
    _numCoeffs = numCoeffs;
    
    if (_numCoeffs > 0) {
        _coeffs = (float*)malloc(_numCoeffs * sizeof(float));
        _delayLine = (float*)malloc(_numCoeffs * sizeof(float));
        
        if (_coeffs && _delayLine) {
            memcpy(_coeffs, coeffs, _numCoeffs * sizeof(float));
            memset(_delayLine, 0, _numCoeffs * sizeof(float));
            return true;
        }
    }
    
    _coeffs = nullptr;
    _delayLine = nullptr;
    _numCoeffs = 0;
    return false;
}

} // namespace RadDSP
