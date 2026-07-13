#ifndef RAD_METER_H
#define RAD_METER_H

#include <Arduino.h>
#include "RadControl.h"

namespace RadDSP {

class Meter : public Controllable {
public:
    Meter();
    
    /**
     * @brief Memproses blok sampel audio, melacak Peak Level, dan mengembalikan nilai puncak terkini
     * @param input Buffer audio masukan
     * @param length Panjang buffer sampel
     * @param mode 0 untuk skala Linear, 1 untuk skala desibel (dBFS)
     * @return Nilai puncak terkini sesuai mode pilihan
     */
    float process(const float* input, int length, int mode = 1);

    /**
     * @brief Helper untuk pemrosesan in-place
     */
    inline float process(float* buffer, int length, int mode = 1) {
        return process((const float*)buffer, length, mode);
    }

    void setParameter(uint8_t paramID, float value) override;
    float getParameter(uint8_t paramID) override;
    const char* getType() override { return "Meter"; }

private:
    float _peakDb;      // Nilai puncak sinyal saat ini dalam dBFS (-80.0 s.d 6.0)
    float _decayFactor; // Waktu redam (decay) jarum meter agar pergerakannya mulus
    float _currentPeak; // Nilai puncak linear sebelum dikonversi ke dB
};

} // namespace RadDSP

#endif // RAD_METER_H
