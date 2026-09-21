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
