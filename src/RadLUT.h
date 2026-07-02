#ifndef RADLUT_H
#define RADLUT_H

#include <stdint.h>
#include <math.h>

namespace RadDSP {

    class LUT {
    public:
        // Initialize the lookup tables. Call once in setup()
        static void init();

        // Convert linear amplitude to dB (-120dB to 0dB)
        static float lin2db(float lin);

        // Convert dB (-120dB to 20dB) to linear amplitude
        static float db2lin(float db);

    private:
        static bool _initialized;
        
        // lin2db table: maps linear range [0.000001, 1.0] to [-120, 0] dB
        static const int LIN2DB_SIZE = 1024;
        static float _lin2db_table[LIN2DB_SIZE];
        
        // db2lin table: maps dB range [-120.0, 20.0] to linear
        static const int DB2LIN_SIZE = 1024;
        static float _db2lin_table[DB2LIN_SIZE];
    };

}

#endif
