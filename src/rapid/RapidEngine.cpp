// Reconstructed from Grimrock.bin.x86 RapidEngine.cpp.
#include "RapidEngine.h"
#include "EngineBindings.h"
#include "Frame.h"
#include "Steam.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include "core/Sys.h"
#include "core/TextFile.h"
#include "sys.h"
#include <SDL2/SDL.h>
#include <cstdlib>

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
    SDL_SetHint("SDL_VIDEO_X11_XRANDR", "1");
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

// 0x0812c950
void RapidEngine::enterMainLoop()
{
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
            // frame rate limiter
            int fps = m_maxFrameRate;
            if (fps <= 0)
                fps = 1;
            else if (fps > 1000)
                fps = 1000;
            while (sysGetSeconds(sysClock()) - start < 1.0 / (float)fps)
            {
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
