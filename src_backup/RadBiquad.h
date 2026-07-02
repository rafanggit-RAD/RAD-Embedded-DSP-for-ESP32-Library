#ifndef RAD_BIQUAD_H
#define RAD_BIQUAD_H

#include <Arduino.h>
#include <esp_dsp.h>
#include "RadControl.h"

namespace RadDSP {

class Biquad : public Controllable {
public:
    Biquad();
    ~Biquad();

    /**
     * @brief Implementasi dari RadDSP::Controllable
     */
    void setParameter(uint8_t paramID, float value) override;
    float getParameter(uint8_t paramID) override;

    /**
     * @brief Set as Lowpass filter
     * @param cutoff Cutoff frequency in Hz
     * @param q Q-factor (e.g. 0.707 for Butterworth)
     */
    void setLowpass(float cutoff, float q = 0.707f);

    /**
     * @brief Set as Highpass filter
     * @param cutoff Cutoff frequency in Hz
     * @param q Q-factor
     */
    void setHighpass(float cutoff, float q = 0.707f);

    /**
     * @brief Set as Bandpass filter
     * @param centerFreq Center frequency in Hz
     * @param q Q-factor
     */
    void setBandpass(float centerFreq, float q = 1.0f);

    /**
     * @brief Set as Peaking Equalizer (Parametric EQ)
     * @param centerFreq Center frequency in Hz
     * @param gain Gain in dB (positive for boost, negative for cut)
     * @param q Q-factor
     */
    void setPeakingEQ(float centerFreq, float gain, float q = 1.0f);

    /**
     * @brief Set as Low Shelf filter
     * @param cutoff Cutoff frequency in Hz
     * @param gain Gain in dB
     * @param q Q-factor
     */
    void setLowShelf(float cutoff, float gain, float q = 0.707f);

    /**
     * @brief Set as High Shelf filter
     * @param cutoff Cutoff frequency in Hz
     * @param gain Gain in dB
     * @param q Q-factor
     */
    void setHighShelf(float cutoff, float gain, float q = 0.707f);

    /**
     * @brief Process an array of samples
     * @param input Input buffer
     * @param output Output buffer
     * @param length Number of samples to process
     * @param channel Channel index for multi-channel processing (0 or 1, default: 0)
     */
    inline void process(const float* input, float* output, int length, uint8_t channel = 0) {
        if (channel < 2) {
            dsps_biquad_f32(input, output, length, _activeCoeffs, _delayLines[channel]);
        }
    }

    /**
     * @brief Process an array of samples
     * @param buffer Input/Output buffer
     * @param length Number of samples to process
     * @param channel Channel index for multi-channel processing (0 or 1, default: 0)
     */
    inline float* process(float* buffer, int length, uint8_t channel = 0) {
        if (channel < 2) {
            dsps_biquad_f32(buffer, buffer, length, _activeCoeffs, _delayLines[channel]);
        }
        return buffer;
    }

private:
    uint8_t _type; // 0=LP, 1=HP, 2=BP, 3=PEQ, 4=LS, 5=HS
    float _freq;
    float _gain;
    float _q;

    float _coeffsA[5]; // Ping buffer
    float _coeffsB[5]; // Pong buffer
    float* _activeCoeffs; // Atomic pointer
    float _delayLines[2][4]; // Support up to 2 channels
    bool _useA;
};

} // namespace RadDSP

#endif // RAD_BIQUAD_H
