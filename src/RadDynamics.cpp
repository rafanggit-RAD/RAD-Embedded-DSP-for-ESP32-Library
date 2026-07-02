#include "RadDynamics.h"
#include "RadDSP.h"
#include <string.h>

namespace RadDSP {

    Dynamics::Dynamics() {
        _type = 0.0f; // Compressor
        _threshold_db = -12.0f;
        _ratio = 4.0f;
        _attack_ms = 10.0f;
        _hold_ms = 0.0f;
        _release_ms = 100.0f;
        _makeup_db = 0.0f;
        
        _sc_filter_type = 0.0f;
        _sc_freq = 1000.0f;
        _sc_q = 0.707f;

        _env = 0.0f;
        _hold_samples = 0;
        
        _bypass = false;
        
        updateCoefficients();
        updateSidechainFilter();
    }

    Dynamics::~Dynamics() {}

    void Dynamics::updateCoefficients() {
        float sr = getSampleRate();
        if (sr <= 0) sr = 48000.0f;
        
        float attack_sec = _attack_ms / 1000.0f;
        if (attack_sec < 0.0001f) attack_sec = 0.0001f;
        _alphaA = expf(-1.0f / (attack_sec * sr));

        float release_sec = _release_ms / 1000.0f;
        if (release_sec < 0.0001f) release_sec = 0.0001f;
        _alphaR = expf(-1.0f / (release_sec * sr));
    }

    void Dynamics::updateSidechainFilter() {
        if (_sc_filter_type >= 0.5f && _sc_filter_type < 1.5f) {
            _scFilter.setHighpass(_sc_freq, _sc_q);
        } else if (_sc_filter_type >= 1.5f && _sc_filter_type < 2.5f) {
            _scFilter.setLowpass(_sc_freq, _sc_q);
        } else if (_sc_filter_type >= 2.5f) {
            _scFilter.setBandpass(_sc_freq, _sc_q);
        }
    }

    void Dynamics::setParameter(uint8_t paramID, float value) {
        if (paramID == 100) {
            _bypass = (value >= 0.5f);
            return;
        }

        switch (paramID) {
            case 0: _type = value; break;
            case 1: _threshold_db = value; break;
            case 2: _ratio = value; if (_ratio < 1.0f) _ratio = 1.0f; break;
            case 3: _attack_ms = value; updateCoefficients(); break;
            case 4: _hold_ms = value; break;
            case 5: _release_ms = value; updateCoefficients(); break;
            case 6: _makeup_db = value; break;
            case 7: _sc_filter_type = value; updateSidechainFilter(); break;
            case 8: _sc_freq = value; updateSidechainFilter(); break;
        }
    }

    float Dynamics::getParameter(uint8_t paramID) {
        if (paramID == 100) return _bypass ? 1.0f : 0.0f;
        
        switch (paramID) {
            case 0: return _type;
            case 1: return _threshold_db;
            case 2: return _ratio;
            case 3: return _attack_ms;
            case 4: return _hold_ms;
            case 5: return _release_ms;
            case 6: return _makeup_db;
            case 7: return _sc_filter_type;
            case 8: return _sc_freq;
        }
        return 0.0f;
    }

    float* Dynamics::process(float* buffer, int len) {
        return processSidechain(buffer, buffer, len);
    }

    float* Dynamics::processSidechain(float* buffer, float* sidechainBuffer, int len) {
        if (_bypass) return buffer;

        float sc[len];
        memcpy(sc, sidechainBuffer, len * sizeof(float));

        if (_sc_filter_type > 0.5f) {
            _scFilter.process(sc, len);
        }

        float sr = getSampleRate();
        if (sr <= 0) sr = 48000.0f;
        int hold_samples_max = (int)((_hold_ms / 1000.0f) * sr);
        
        float env = _env;
        int hold_counter = _hold_samples;

        for (int i = 0; i < len; i++) {
            float in_val = buffer[i];
            float sc_val = fabsf(sc[i]);

            if (sc_val > env) {
                env = _alphaA * env + (1.0f - _alphaA) * sc_val;
                hold_counter = hold_samples_max;
            } else {
                if (hold_counter > 0) {
                    hold_counter--;
                } else {
                    env = _alphaR * env + (1.0f - _alphaR) * sc_val;
                }
            }

            float env_db = LUT::lin2db(env);
            float gain_reduction_db = 0.0f;
            
            int t = (int)_type;
            
            if (t == 0) {
                if (env_db > _threshold_db) {
                    gain_reduction_db = (_threshold_db - env_db) * (1.0f - 1.0f / _ratio);
                }
            } else if (t == 1) {
                if (env_db > _threshold_db) {
                    gain_reduction_db = _threshold_db - env_db;
                }
            } else if (t == 2) {
                if (env_db < _threshold_db) {
                    gain_reduction_db = (env_db - _threshold_db) * (1.0f - 1.0f / _ratio);
                }
            } else if (t == 3) {
                if (env_db < _threshold_db) {
                    gain_reduction_db = -80.0f;
                }
            }

            float linear_gain = LUT::db2lin(gain_reduction_db + _makeup_db);
            buffer[i] = in_val * linear_gain;
        }

        _env = env;
        _hold_samples = hold_counter;

        return buffer;
    }

}
