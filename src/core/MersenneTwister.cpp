// Reconstructed from Grimrock.bin.x86 MersenneTwister.cpp (reference MT19937 code).
#include "core/MersenneTwister.h"

namespace core
{

constexpr unsigned int MATRIX_A = 0x9908b0dfU;   // constant vector a
constexpr unsigned int UPPER_MASK = 0x80000000U; // most significant w-r bits
constexpr unsigned int LOWER_MASK = 0x7fffffffU; // least significant r bits
constexpr unsigned int DefaultSeed = 5489U;
constexpr unsigned int InitMultiplier = 1812433253U;
constexpr unsigned int InitArraySeed = 19650218U;
constexpr unsigned int InitArrayMultiplier1 = 1664525U;
constexpr unsigned int InitArrayMultiplier2 = 1566083941U;
constexpr int TemperingShiftU = 11;
constexpr int TemperingShiftS = 7;
constexpr int TemperingShiftT = 15;
constexpr int TemperingShiftL = 18;
constexpr unsigned int TemperingMaskB = 0x9d2c5680U;
constexpr unsigned int TemperingMaskC = 0xefc60000U;

// 0x080cbab0
MersenneTwister::MersenneTwister()
{
    unsigned long initKey[4] = {0x123, 0x234, 0x345, 0x456};
    initArray(initKey, 4);
}
// 0x080cb780
MersenneTwister::~MersenneTwister() {}

// 0x080cb790
void MersenneTwister::initGen(unsigned long seed)
{
    mt[0] = (unsigned int)seed;
    for (mti = 1; mti < N; mti++)
        mt[mti] = (InitMultiplier * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti);
}

// 0x080cb7f0
void MersenneTwister::initArray(unsigned long* initKey, int keyLength)
{
    initGen(InitArraySeed);
    int i = 1, j = 0;
    int k = (N > keyLength ? N : keyLength);
    for (; k; k--)
    {
        mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * InitArrayMultiplier1)) +
                (unsigned int)initKey[j] + j;
        i++;
        j++;
        if (i >= N)
        {
            mt[0] = mt[N - 1];
            i = 1;
        }
        if (j >= keyLength)
            j = 0;
    }
    for (k = N - 1; k; k--)
    {
        mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * InitArrayMultiplier2)) - i;
        i++;
        if (i >= N)
        {
            mt[0] = mt[N - 1];
            i = 1;
        }
    }
    mt[0] = 0x80000000U;
}

// 0x080cb910
unsigned int MersenneTwister::genrand_int32()
{
    static constexpr unsigned int mag01[2] = {0x0U, MATRIX_A};
    unsigned int y;
    if (mti >= N)
    {
        int kk;
        if (mti == N + 1)
            initGen(DefaultSeed);
        for (kk = 0; kk < N - M; kk++)
        {
            y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
            mt[kk] = mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1U];
        }
        for (; kk < N - 1; kk++)
        {
            y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
            mt[kk] = mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1U];
        }
        y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
        mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1U];
        mti = 0;
    }
    // tempering
    y = mt[mti++];
    y ^= (y >> TemperingShiftU);
    y ^= (y << TemperingShiftS) & TemperingMaskB;
    y ^= (y << TemperingShiftT) & TemperingMaskC;
    y ^= (y >> TemperingShiftL);
    return y;
}

} // namespace core
