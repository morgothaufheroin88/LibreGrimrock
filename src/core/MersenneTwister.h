// MT19937, reconstructed from MersenneTwister.cpp (0x080cb780-0x080cbab0).
#pragma once

namespace core
{

class MersenneTwister
{
  public:
    static constexpr int N = 624;                        // degree of recurrence (state size)
    static constexpr int M = 397;                        // middle word offset
    static constexpr double Int32Range = 4294967295.0;   // 2^32 - 1
    static constexpr double Int32Modulus = 4294967296.0; // 2^32
    MersenneTwister();
    explicit MersenneTwister(unsigned long seed)
    {
        initGen(seed);
    }
    ~MersenneTwister();
    void initGen(unsigned long seed);
    void initArray(unsigned long* initKey, int keyLength);
    unsigned int genrand_int32();
    // [0,0x7fffffff]
    int genrand_int31()
    {
        return (int)(genrand_int32() >> 1);
    }
    // [0,1]
    double genrand_real1()
    {
        return genrand_int32() * (1.0 / Int32Range);
    }
    // [0,1)
    double genrand_real2()
    {
        return genrand_int32() * (1.0 / Int32Modulus);
    }
    // (0,1)
    double genrand_real3()
    {
        return (((double)genrand_int32()) + 0.5) * (1.0 / Int32Modulus);
    }

  private:
    unsigned int mt[N];
    int mti;
};

} // namespace core
