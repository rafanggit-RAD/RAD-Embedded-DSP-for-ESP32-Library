#include "RadLUT.h"

namespace RadDSP {

    bool LUT::_initialized = false;
    float LUT::_lin2db_table[LIN2DB_SIZE];
    float LUT::_db2lin_table[DB2LIN_SIZE];

    void LUT::init() {
        if (_initialized) return;

        // Initialize lin2db table (0.000001 to 1.0)
        // Note: linear scale is highly non-linear in dB. 
        // We use a logarithmic distribution for the table input to get better resolution at low levels?
        // Actually, direct mapping is faster.
        // Let's map [0, 1.0] to array indices.
        // Index 0 -> 0.0 (returns -120dB)
        // Index LIN2DB_SIZE-1 -> 1.0 (returns 0dB)
        _lin2db_table[0] = -120.0f; // -120dB for exactly 0
        for (int i = 1; i < LIN2DB_SIZE; i++) {
            float lin = (float)i / (float)(LIN2DB_SIZE - 1);
            _lin2db_table[i] = 20.0f * log10f(lin);
            if (_lin2db_table[i] < -120.0f) _lin2db_table[i] = -120.0f;
        }

        // Initialize db2lin table (-120dB to +20dB)
        // Index 0 -> -120dB
        // Index DB2LIN_SIZE-1 -> +20dB
        for (int i = 0; i < DB2LIN_SIZE; i++) {
            float db = -120.0f + ((float)i / (float)(DB2LIN_SIZE - 1)) * 140.0f;
            _db2lin_table[i] = powf(10.0f, db / 20.0f);
        }

        _initialized = true;
    }

    float LUT::lin2db(float lin) {
        if (lin <= 0.000001f) return -120.0f;
        if (lin >= 1.0f) return 0.0f;

        float f_idx = lin * (LIN2DB_SIZE - 1);
        int idx = (int)f_idx;
        float frac = f_idx - idx;

        if (idx >= LIN2DB_SIZE - 1) return _lin2db_table[LIN2DB_SIZE - 1];

        // Linear interpolation
        return _lin2db_table[idx] + frac * (_lin2db_table[idx + 1] - _lin2db_table[idx]);
    }

    float LUT::db2lin(float db) {
        if (db <= -120.0f) return 0.0f;
        if (db >= 20.0f) return 10.0f; // +20dB is 10x multiplier

        // Map [-120, 20] to [0, DB2LIN_SIZE-1]
        float f_idx = (db + 120.0f) * ((DB2LIN_SIZE - 1) / 140.0f);
        int idx = (int)f_idx;
        float frac = f_idx - idx;

        if (idx >= DB2LIN_SIZE - 1) return _db2lin_table[DB2LIN_SIZE - 1];

        // Linear interpolation
        return _db2lin_table[idx] + frac * (_db2lin_table[idx + 1] - _db2lin_table[idx]);
    }

}
