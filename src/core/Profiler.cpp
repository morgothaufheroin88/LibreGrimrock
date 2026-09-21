// Reconstructed from Grimrock.bin.x86 Profiler.cpp.
#include "core/Profiler.h"
#include "core/Array.h"
#include "core/Color.h"
#include "core/DebugDraw.h"
#include "core/Timer.h"
#include <cstdio>

namespace core
{

struct ProfileBlock
{
    const char* name;
    int count;
    float time;
    float startTime;
};

static Timer g_timer;
static Timer g_unclassifiedTimer;
static Array<ProfileBlock> g_blocks;
static Array<int> g_blockStack;
static float g_frameEnd;
static float g_frameStart;
static float g_unclassifiedTime;
static bool g_profiling;
static int g_nestingDepth;
static bool g_insideFrame;
static float g_timeToNextUpdate;

constexpr float UpdateInterval = 0.5f;

// 0x080b3d00: stats are collected for one frame every UpdateInterval seconds.
void Profiler::beginFrame()
{
    g_insideFrame = true;
    g_profiling = false;
    float timeLeft = g_timeToNextUpdate - g_timer.sample();
    g_timeToNextUpdate = timeLeft;
    if (timeLeft < 0.0f)
    {
        do
        {
            timeLeft += UpdateInterval;
        } while (timeLeft < 0.0f);
        g_timeToNextUpdate = timeLeft;
        g_profiling = true;
    }
    if (!g_profiling)
        return;
    g_frameStart = g_timer.get();
    g_blocks.resize(0);
    g_unclassifiedTime = 0.0f;
    g_unclassifiedTimer.reset();
}

// 0x080b3910
void Profiler::endFrame()
{
    g_insideFrame = false;
    if (!g_profiling)
        return;
    g_frameEnd = g_timer.get();
    g_unclassifiedTime += g_unclassifiedTimer.get();
}

// 0x080b3aa0
void Profiler::beginBlock(const char* name)
{
    if (!g_profiling)
        return;
    ProfileBlock* block = 0;
    for (int i = 0; i < g_blocks.size(); ++i)
    {
        if (g_blocks[i].name == name)
        {
            block = &g_blocks[i];
            break;
        }
    }
    if (!block)
    {
        block = &g_blocks.push_back();
        block->name = name;
        block->count = 0;
        block->time = 0.0f;
    }
    g_blockStack.push_back((int)(block - g_blocks.data()));
    block->count++;
    block->startTime = g_timer.get();
    if (g_nestingDepth == 0)
        g_unclassifiedTime += g_unclassifiedTimer.get();
    g_nestingDepth++;
}

// 0x080b3960
void Profiler::endBlock()
{
    if (!g_profiling)
        return;
    ProfileBlock& block = g_blocks[g_blockStack[g_blockStack.size() - 1]];
    if (g_blockStack.size() > 0)
        g_blockStack.pop_back();
    block.time += g_timer.get() - block.startTime;
    g_nestingDepth--;
    if (g_nestingDepth == 0)
        g_unclassifiedTimer.reset();
}

// 0x080b3780
void Profiler::draw()
{
    constexpr float TextX = 40.0f;
    constexpr float FirstLineY = 144.0f;
    constexpr float LineHeight = 14.0f;
    char text[256];
    float frameTime = g_frameEnd - g_frameStart;
    float y = FirstLineY;
    for (int i = 0; i < g_blocks.size(); ++i)
    {
        const ProfileBlock& block = g_blocks[i];
        float percent = block.time * 100.0f / frameTime;
        if (percent <= 0.0f)
            percent = 0.0f;
        float milliseconds = block.time * 1000.0f;
        if (milliseconds <= 0.0f)
            milliseconds = 0.0f;
        sprintf(text, "%-30s %3d / %5.2fms / %5.1f%%", block.name, block.count, milliseconds,
                percent);
        DebugDraw::drawText(text, Vec2(TextX, y), Color::White);
        y += LineHeight;
    }
    float percent = g_unclassifiedTime * 100.0f / frameTime;
    if (percent <= 0.0f)
        percent = 0.0f;
    float milliseconds = g_unclassifiedTime * 1000.0f;
    if (milliseconds <= 0.0f)
        milliseconds = 0.0f;
    sprintf(text, "%-30s       %5.2fms / %5.1f%%", "Unclassified", milliseconds, percent);
    DebugDraw::drawText(text, Vec2(TextX, y), Color::White);
}

} // namespace core
