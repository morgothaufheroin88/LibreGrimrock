// Reconstructed from Grimrock.bin.x86 RapidEngine.cpp.
#include "RapidEngine.h"
#include "EngineBindings.h"
#include "Frame.h"
#include "Steam.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include "core/SharedPtr.h"
#include "core/Sys.h"
#include "core/TextFile.h"
#include "sys.h"
#include <SDL3/SDL.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>
#include <pthread.h>
#include <unistd.h>

using namespace core;

RapidEngine* g_pRapidEngine = 0;

// 0x0812bcd0: package loader for "a.b.c" -> "a/b/c.lua", falling back to "lib/a/b/c.lua",
// read through the mounted file systems (the archive).
static int zipLoader(lua_State* L)
{
    try
    {
        String name(luaL_checkstring(L, 1));
        name.replace(".", "/");
        name.append(".lua");
        if (!fileExists(name.c_str()))
        {
            String lib("lib/");
            lib.append(name.c_str());
            name = lib;
        }
        File* file = openRead(name.c_str());
        int length = file->getFileLength();
        char* data = new char[length + 1];
        file->read(data, length);
        data[length] = 0;
        closeFile(file);
        luaL_loadbuffer(L, data, length, name.c_str());
        delete[] data;
        return 1;
    }
    catch (Exception& e)
    {
        debugPrint("ERROR: %s\n", e.getReason());
        lua_pushnil(L);
        return 1;
    }
}

// 0x0812cd10
RapidEngine::RapidEngine()
    : m_quit(false), m_restart(false), m_companyName("My Company"),
      m_applicationName("My Application"), m_maxFrameRate(1000)
{
    g_pRapidEngine = this;
    m_rootDirectory = sysGetCurrentDirectory();
    m_libPath = m_rootDirectory;
    m_libPath.append("/lib");
    SDL_Init(SDL_INIT_VIDEO);
}
// 0x0812c1e0
RapidEngine::~RapidEngine()
{
    SDL_Quit();
    g_pRapidEngine = 0;
}
// 0x0812bf30
void RapidEngine::setWindowIcon(const char* filename)
{
    m_windowIcon.reset(new Image(filename));
}
// 0x0812bfd0
void RapidEngine::setProjectPath(const String& path)
{
    m_projectPath = path;
    m_projectPath.replace("\\", "/");
    int slash = m_projectPath.find_last("/");
    if (slash >= 0)
    {
        m_projectFolder = m_projectPath.substr(0, slash - 1);
        m_projectFile = m_projectPath.substr(slash + 1, -1);
    }
    else
    {
        m_projectFolder = "";
        m_projectFile = m_projectPath;
    }
}

// 0x0812c440: <documents>/<company>/<application>/error.log
String RapidEngine::writeErrorLog(const char* message)
{
    String dir = sysGetDocumentsDirectory();
    dir.append("/");
    dir.append(m_companyName.c_str());
    dir.append("/");
    dir.append(m_applicationName.c_str());
    sysCreateDirectory(dir.c_str(), true);
    String path = dir;
    path.append("/error.log");
    TextFileWriter writer(path.c_str());
    writer.writeString(String(message));
    writer.writeString(String("\r\n"));
    return path;
}
// 0x0812c810
void RapidEngine::luaError(const char* message)
{
    String logFile = writeErrorLog(message);
    String text(message);
    if (logFile.size() > 0)
        text =
            formatString("%s\n\nAn error log has been written to:\n%s\n\nPlease email this file to "
                         "feedback@almosthumangames.com\n",
                         message, logFile.c_str());
    sysMessageBox("Software Failure", text.c_str(), MessageBox_Ok);
    exit(-1);
}

// Debugging aid (GRIMROCK_DEBUG_STALLS=<ms>): a watchdog thread interrupts the main thread
// with SIGUSR1 when a frame takes longer than the given time and prints its backtrace, and
// the frame time is logged afterwards.
static volatile unsigned long g_frameCounter = 0;
static pthread_t g_mainThread;
static int g_stallThresholdMs = 0;

static void stallSignalHandler(int)
{
    void* frames[64];
    int count = backtrace(frames, 64);
    const char msg[] = "--- stall: main thread backtrace ---\n";
    if (write(2, msg, sizeof(msg) - 1) < 0)
        return;
    backtrace_symbols_fd(frames, count, 2);
}
static void* stallWatchdog(void*)
{
    unsigned long last = g_frameCounter;
    long long since = sysClock();
    bool reported = false;
    for (;;)
    {
        usleep(50000);
        unsigned long now = g_frameCounter;
        if (now != last)
        {
            last = now;
            since = sysClock();
            reported = false;
        }
        else if (!reported && sysGetSeconds(sysClock() - since) * 1000.0 > g_stallThresholdMs)
        {
            reported = true;
            pthread_kill(g_mainThread, SIGUSR1);
        }
    }
    return 0;
}
static void startStallWatchdog()
{
    const char* threshold = getenv("GRIMROCK_DEBUG_STALLS");
    if (!threshold)
        return;
    g_stallThresholdMs = atoi(threshold) > 0 ? atoi(threshold) : 500;
    g_mainThread = pthread_self();
    signal(SIGUSR1, stallSignalHandler);
    pthread_t thread;
    pthread_create(&thread, 0, stallWatchdog, 0);
    pthread_detach(thread);
}

// Refresh rate of the display the window is on, re-read once a second (the window may
// be moved to another display).
static float displayRefreshRate(double now)
{
    static float rate = 0.0f;
    static double nextQuery = 0.0;
    if (now >= nextQuery)
    {
        rate = sysGetDisplayRefreshRate();
        nextQuery = now + 1.0;
    }
    return rate;
}

// 0x0812c950
void RapidEngine::enterMainLoop()
{
    startStallWatchdog();
    if (fileExists("init.lua"))
        setProjectPath(String("init.lua"));
    for (;;)
    {
        m_quit = false;
        m_restart = false;
        sysSetCurrentDirectory(m_rootDirectory.c_str());
        lua_State* L = luaL_newstate();
        luaL_openlibs(L);
        luax::init(L);
        sys_mod(L);
        core_mod(L);
        frame_mod(L);
        engine_mod(L);
        steam_mod(L);
#if GRIMROCK_GAME >= 2
        savegame_mod(L);
#endif
        lua_getfield(L, LUA_GLOBALSINDEX, "debug");
        int debugIndex = lua_gettop(L);
        lua_getfield(L, -1, "traceback");
        int traceback = debugIndex + 1;
        lua_getfield(L, LUA_GLOBALSINDEX, "package");
        String path = formatString("./?.lua;%s/?.lua", m_libPath.c_str());
        lua_pushstring(L, path.c_str());
        lua_setfield(L, -2, "path");
        lua_pop(L, 1);
        lua_getfield(L, LUA_GLOBALSINDEX, "package");
        lua_getfield(L, -1, "loaders");
        int numLoaders = (int)lua_objlen(L, -1);
        lua_pushcfunction(L, zipLoader);
        lua_rawseti(L, -2, numLoaders + 1);
        lua_pop(L, 2);
        if (m_projectFile.size() > 0)
        {
            if (m_projectFolder.size() > 0)
                sysSetCurrentDirectory(m_projectFolder.c_str());
            if (luax::load(L, m_projectFile.c_str()) != 0 ||
                lua_pcall(L, 0, LUA_MULTRET, traceback) != 0)
                luaError(lua_tostring(L, -1));
        }
        resetDeltaTime();
        while (!m_quit && !m_restart)
        {
            double start = sysGetSeconds(sysClock());
            if (g_pMainFrame && !Frame::updateFrames())
            {
                m_quit = true;
                break;
            }
            callDisplayFunc(L, traceback);
            ++g_frameCounter;
            // Debugging aid: GRIMROCK_DEBUG_LUA runs a chunk from the given frame on
            // (GRIMROCK_DEBUG_LUA_FRAME, 600 by default) until it returns a true value,
            // which is how the save/load paths are exercised without clicking through the
            // menus: the chunk waits for the state it needs and then reports it is done.
            static const char* debugLua = getenv("GRIMROCK_DEBUG_LUA");
            static const int debugLuaFrame =
                getenv("GRIMROCK_DEBUG_LUA_FRAME") ? atoi(getenv("GRIMROCK_DEBUG_LUA_FRAME")) : 600;
            static bool debugLuaDone = false;
            if (debugLua && !debugLuaDone && g_frameCounter >= (unsigned long)debugLuaFrame)
            {
                if (g_frameCounter == (unsigned long)debugLuaFrame)
                    debugPrint("GRIMROCK_DEBUG_LUA: %s\n", debugLua);
                if (luaL_loadstring(L, debugLua) != 0 || lua_pcall(L, 0, 1, traceback) != 0)
                {
                    debugPrint("GRIMROCK_DEBUG_LUA failed: %s\n", lua_tostring(L, -1));
                    debugLuaDone = true;
                }
                else
                    debugLuaDone = lua_toboolean(L, -1) != 0;
                lua_pop(L, 1);
            }
            if (g_stallThresholdMs)
            {
                static double nextReport = 0.0;
                double now = sysGetSeconds(sysClock());
                if (now >= nextReport)
                {
                    nextReport = now + 10.0;
                    FILE* statm = fopen("/proc/self/statm", "r");
                    long pages = 0, resident = 0;
                    if (statm)
                    {
                        if (fscanf(statm, "%ld %ld", &pages, &resident) != 2)
                            resident = 0;
                        fclose(statm);
                    }
                    debugPrint("--- heap: lua %d KB, rss %ld MB, shared objects %d\n",
                               lua_gc(L, LUA_GCCOUNT, 0),
                               resident * sysconf(_SC_PAGESIZE) / (1024 * 1024),
                               SharedPtrBase::objectCount());
                }
                double frameMs = sysGetSeconds(sysClock()) * 1000.0 - start * 1000.0;
                if (frameMs > g_stallThresholdMs)
                    debugPrint("--- stall: frame took %.0f ms\n", frameMs);
            }
            // Frame rate limiter. The original spins until 1/maxFrameRate has passed
            // (sys.setMaxFrameRate, 120 in the shipped config); here the cap is the
            // refresh rate of the display, and the wait sleeps instead of burning a core.
            double frameTime = 1.0 / (double)(m_maxFrameRate > 0 ? m_maxFrameRate : 1000);
            float refreshRate = displayRefreshRate(start);
            if (refreshRate > 0.0f)
                frameTime = 1.0 / (double)refreshRate;
            for (;;)
            {
                double remaining = frameTime - (sysGetSeconds(sysClock()) - start);
                if (remaining <= 0.0)
                    break;
                if (remaining > 0.0015)
                    usleep((useconds_t)((remaining - 0.001) * 1e6));
            }
        }
        lua_close(L);
        shutdownEngine();
        shutdownFrame();
        if (m_quit)
        {
            shutdownSteam();
            return;
        }
    }
}
