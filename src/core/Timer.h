// Reconstructed from Grimrock.bin.x86 Timer.cpp (0x080b1c70-0x080b1d30).
#pragma once

namespace core
{

class Timer
{
  public:
    Timer();
    void reset();
    void set(float time);
    float get();
    // Returns the time elapsed since the previous sample() call.
    float sample();

  private:
    long long m_startClock;
    float m_offset;
    float m_lastSample;
};

} // namespace core
