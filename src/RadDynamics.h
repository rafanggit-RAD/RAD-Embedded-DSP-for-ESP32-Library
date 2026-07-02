#ifndef RADDYNAMICS_H
#define RADDYNAMICS_H

#include "RadControl.h"
#include "RadLUT.h"
#include "RadBiquad.h"
#include <math.h>

namespace RadDSP {

    class Dynamics : public Controllable {
    public:
        Dynamics();
        ~Dynamics();

        // Controllable Interface
        void setParameter(uint8_t paramID, float value) override;
        float getParameter(uint8_t paramID) override;

        // Process audio block in-place
        float* process(float* buffer, int len);

        // Process audio with a separate sidechain buffer
        float* processSidechain(float* buffer, float* sidechainBuffer, int len);

    private:
        void updateCoefficients();
        void updateSidechainFilter();

        // Parameters
        float _type;            // 0=Comp, 1=Limiter, 2=Expander, 3=Gate
        float _threshold_db;
        float _ratio;
        float _attack_ms;
        float _hold_ms;
        float _release_ms;
        float _makeup_db;
        
        float _sc_filter_type;  // 0=None, 1=HPF, 2=LPF, 3=BPF
        float _sc_freq;         // Hz
        float _sc_q;            // Q-Factor fixed for sidechain, e.g. 0.707

        // Internal State
        float _env; // Mono envelope memory
        int _hold_samples; // Hold counter
        float _alphaA;
        float _alphaR;
        
        // Sidechain filter
        Biquad _scFilter;
        
        // Bypass flag (Param ID 100)
        bool _bypass;
    };

}

#endif
