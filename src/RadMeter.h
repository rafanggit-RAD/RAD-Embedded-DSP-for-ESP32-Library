#ifndef RAD_METER_H
#define RAD_METER_H

#include <Arduino.h>
#include "RadControl.h"

namespace RadDSP {

class Meter : public Controllable {
public:
    Meter();
    
    /**
     * @brief Memproses blok sampel audio dan melacak nilai Peak Level
     * @param input Buffer audio masukan
     * @param length Panjang buffer sampel
     */
    void process(const float* input, int length);

    /**
     * @brief In-place process helper
     */
    inline void process(float* buffer, int length) {
        process((const float*)buffer, length);
    }

    void setParameter(uint8_t paramID, float value) override;
    float getParameter(uint8_t paramID) override;

private:
    float _peakDb;      // Nilai puncak sinyal saat ini dalam dBFS (-80.0 s.d 0.0)
    float _decayFactor; // Waktu redam (decay) jarum meter agar pergerakannya mulus
    float _currentPeak; // Nilai puncak linear sebelum dikonversi ke dB
};

} // namespace RadDSP

#endif // RAD_METER_H
