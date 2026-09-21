// Reconstructed from Grimrock.bin.x86 Timer.cpp.
#include "core/Timer.h"
#include "core/Sys.h"

namespace core
{

// 0x080b1d00
Timer::Timer()
{
    m_startClock = sysClock();
    m_offset = 0.0f;
    m_lastSample = 0.0f;
}
// 0x080b1cd0
void Timer::reset()
{
    m_startClock = sysClock();
    m_offset = 0.0f;
    m_lastSample = 0.0f;
}
// 0x080b1c70
void Timer::set(float time)
{
    long long now = sysClock();
    m_offset = time;
    m_lastSample = time;
    m_startClock = now;
}
// 0x080b1ca0
float Timer::get()
{
    return (float)(sysGetSeconds(sysClock() - m_startClock) - m_offset);
}
// 0x080b1d30
float Timer::sample()
{
    float now = (float)(sysGetSeconds(sysClock() - m_startClock) - m_offset);
    float prev = m_lastSample;
    m_lastSample = now;
    return now - prev;
}

} // namespace core
