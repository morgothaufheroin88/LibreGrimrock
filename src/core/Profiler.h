// Reconstructed from Grimrock.bin.x86 Profiler.cpp (0x080b3780-0x080b3d00).
#pragma once

namespace core
{

class Profiler
{
  public:
    static void beginFrame();
    static void endFrame();
    static void beginBlock(const char* name);
    static void endBlock();
    static void draw();
#if GRIMROCK_GAME >= 2
    // 0x0040f2f0 / 0x0040f320: the collected blocks of the frame, for Profiler.lua
    static int getBlockCount();
    static void getBlockData(int index, const char*& name, int& count, float& time);
#endif
};

class ProfileScope
{
  public:
    explicit ProfileScope(const char* name)
    {
        Profiler::beginBlock(name);
    }
    ~ProfileScope()
    {
        Profiler::endBlock();
    }
};

} // namespace core
