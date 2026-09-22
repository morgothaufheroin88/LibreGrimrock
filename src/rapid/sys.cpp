// Reconstructed from Grimrock.bin.x86 sys.cpp.
#include "sys.h"
#include "Frame.h"
#include "RapidEngine.h"
#include "core/Dialog.h"
#include "core/String.h"
#include "core/Sys.h"
#include "core/Utils.h"
#include <cctype>
#include <cstdio>
#include <cstring>

using namespace core;

static long long g_startTime = 0;
static long long g_prevTime = 0;
static double g_time = 0.0;
static double g_deltaTime = 0.0;

constexpr double FirstFrameDeltaTime = 1.0 / 60.0;
constexpr double MaxDeltaTime = 0.1;

static luax::Enum g_encryptionMethods[] = {{"arc4-drop768", 0}, {0, 0}};

// 0x0812d570
void resetDeltaTime()
{
    g_prevTime = 0;
}

// 0x0812d640: delta time is clamped to MaxDeltaTime, the first frame gets 1/60.
void callDisplayFunc(lua_State* L, int errfunc)
{
    long long now = sysClock();
    g_time = sysGetSeconds(now - g_startTime);
    double dt;
    if (g_prevTime == 0)
        dt = FirstFrameDeltaTime;
    else
        dt = sysGetSeconds(now - g_prevTime);
    if (dt > MaxDeltaTime)
        dt = MaxDeltaTime;
    g_deltaTime = dt;
    g_prevTime = now;
    int top = lua_gettop(L);
    lua_getfield(L, LUA_REGISTRYINDEX, "rapid.display_func");
    if (lua_type(L, -1) == LUA_TFUNCTION)
    {
        if (lua_pcall(L, 0, LUA_MULTRET, errfunc) != 0)
        {
            g_pRapidEngine->luaError(lua_tostring(L, -1));
            // unreachable in the original as well: keep the func for sys.resume
            lua_getfield(L, LUA_REGISTRYINDEX, "rapid.display_func");
            lua_setfield(L, LUA_REGISTRYINDEX, "rapid.resume_func");
            lua_pushnil(L);
            lua_setfield(L, LUA_REGISTRYINDEX, "rapid.display_func");
        }
    }
    else
    {
        g_prevTime = 0;
    }
    lua_settop(L, top);
}

// ---- global functions ------------------------------------------------------------

// 0x0812eaf0: print through tostring, tab separated, to stdout
static int print(lua_State* L)
{
    int n = lua_gettop(L);
    String out;
    lua_getfield(L, LUA_GLOBALSINDEX, "tostring");
    for (int i = 1; i <= n; ++i)
    {
        lua_pushvalue(L, -1);
        lua_pushvalue(L, i);
        lua_call(L, 1, 1);
        const char* s = lua_tostring(L, -1);
        if (!s)
            return luaL_error(L, "'tostring' must return a string to 'print'");
        if (i > 1)
            out.append("\t");
        out.append(s);
        lua_pop(L, 1);
    }
    out.append("\n");
    printf("%s", out.c_str());
    return 0;
}
// 0x0812e570
static int dofile(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    if (luax::load(L, filename) != 0)
        luaL_error(L, "%s", lua_tostring(L, -1));
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0)
        luaL_error(L, "%s", lua_tostring(L, -1));
    return lua_gettop(L) - 1;
}
// 0x0812e510
static int loadfile(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    if (luax::load(L, filename) != 0)
    {
        lua_pushnil(L);
        lua_pushvalue(L, -2);
        return 2;
    }
    return 1;
}
// 0x0812df70: four bytes (or a 4 byte string) to a float
static int raw_bytes_to_float(lua_State* L)
{
    float value;
    if (lua_gettop(L) == 4)
    {
        unsigned char bytes[4];
        for (int i = 0; i < 4; ++i)
            bytes[i] = (unsigned char)luaL_checkinteger(L, i + 1);
        memcpy(&value, bytes, 4);
    }
    else
    {
        const char* bytes = luaL_checkstring(L, 1);
        if (lua_objlen(L, 1) != 4)
            luaL_error(L, "invalid number of bytes");
        memcpy(&value, bytes, 4);
    }
    lua_pushnumber(L, value);
    return 1;
}

// ---- sys module ------------------------------------------------------------------

// 0x0812f1f0: switches to another project and restarts, keeping the arguments
static int sys_loadProject(lua_State* L)
{
    g_pRapidEngine->setProjectPath(String(luaL_checkstring(L, 1)));
    g_pRapidEngine->restart();
    return 0;
}
static int sys_getProjectFolder(lua_State* L)
{
    lua_pushstring(L, g_pRapidEngine->getProjectFolder().c_str());
    return 1;
}
// 0x0812f4d0: sys.restart([args])
static int sys_restart(lua_State* L)
{
#if GRIMROCK_GAME >= 2
    // 0x0040a4b0: restart([{switch, ...}])
    if (lua_gettop(L) > 0)
    {
        if (lua_type(L, 1) != LUA_TTABLE)
            return luaL_error(L, "invalid args");
        Array<String>& args = g_pRapidEngine->getArgList();
        args.clear();
        int count = (int)lua_objlen(L, 1);
        for (int i = 1; i <= count; ++i)
        {
            lua_rawgeti(L, 1, i);
            args.push_back(String(luaL_checkstring(L, -1)));
            lua_pop(L, 1);
        }
    }
    g_pRapidEngine->restart();
    return 0;
#else
    if (lua_gettop(L) > 0)
    {
        HashMap<String, String> args;
        if (lua_type(L, 1) != LUA_TTABLE)
            return luaL_error(L, "invalid args");
        lua_pushnil(L);
        while (lua_next(L, 1))
        {
            if (!lua_isstring(L, -2))
                luaL_error(L, "invalid arg key");
            if (!lua_isstring(L, -1))
                luaL_error(L, "invalid arg value");
            args.insert(String(lua_tostring(L, -2)), String(lua_tostring(L, -1)));
            lua_pop(L, 1);
        }
        g_pRapidEngine->getArgs() = args;
    }
    g_pRapidEngine->restart();
    return 0;
#endif
}
// 0x0812d5c0
static int sys_resume(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "rapid.resume_func");
    if (!lua_isnil(L, -1))
    {
        lua_setfield(L, LUA_REGISTRYINDEX, "rapid.display_func");
        lua_pushnil(L);
        lua_setfield(L, LUA_REGISTRYINDEX, "rapid.resume_func");
    }
    return 0;
}
// 0x0812e400
static int sys_args(lua_State* L)
{
    lua_newtable(L);
#if GRIMROCK_GAME >= 2
    // 0x0040a6d0: the switches as an array
    Array<String>& list = g_pRapidEngine->getArgList();
    for (int i = 0; i < list.size(); ++i)
    {
        lua_pushstring(L, list[i].c_str());
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
#else
    HashMap<String, String>& args = g_pRapidEngine->getArgs();
    for (HashMap<String, String>::iterator it = args.begin(); it != args.end(); ++it)
    {
        lua_pushstring(L, it.value().c_str());
        lua_setfield(L, -2, it.key().c_str());
    }
    return 1;
#endif
}
static int sys_rootDirectory(lua_State* L)
{
    lua_pushstring(L, g_pRapidEngine->getRootDirectory().c_str());
    return 1;
}
static int sys_platform(lua_State* L)
{
    lua_pushstring(L, "linux");
    return 1;
}
static int sys_setCompanyName(lua_State* L)
{
    g_pRapidEngine->setCompanyName(luaL_checkstring(L, 1));
    return 0;
}
static int sys_setApplicationName(lua_State* L)
{
    g_pRapidEngine->setApplicationName(luaL_checkstring(L, 1));
    return 0;
}
// 0x0812ee90: <documents>/<company>/<application>, created when missing
static int sys_getSystemFolder(lua_State* L)
{
    try
    {
        static luax::Enum folders[] = {{"documents", 0}, {0, 0}};
        int folder = luax::checkEnum(L, 1, folders);
        if (folder == 0)
        {
            String path = sysGetDocumentsDirectory();
            path.append("/");
            path.append(g_pRapidEngine->getCompanyName().c_str());
            path.append("/");
            path.append(g_pRapidEngine->getApplicationName().c_str());
            if (!sysFileExists(path.c_str()))
                sysCreateDirectory(path.c_str(), true);
            lua_pushstring(L, path.c_str());
        }
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int sys_getDesktopDisplayMode(lua_State* L)
{
    int width, height;
    sysGetDesktopDisplayMode(width, height);
    lua_pushnumber(L, width);
    lua_pushnumber(L, height);
    return 2;
}
#if GRIMROCK_GAME >= 2
static luax::Enum g_systemParameters[] = {{"work_area", 0x30}, {0, 0}}; // SPI_GETWORKAREA
static luax::Enum g_locales[] = {{"default", 0}, {"user", 1}, {0, 0}};
// grimrock2.exe 0x0040a7e0: left, top, right, bottom of the work area
static int sys_systemParametersInfo(lua_State* L)
{
    luax::checkEnum(L, 1, g_systemParameters);
    int left, top, right, bottom;
    if (!sysGetWorkArea(left, top, right, bottom))
        luaL_error(L, "SystemParametersInfo failed");
    lua_pushnumber(L, left);
    lua_pushnumber(L, top);
    lua_pushnumber(L, right);
    lua_pushnumber(L, bottom);
    return 4;
}
// grimrock2.exe 0x0040aaf0
static int sys_setLocale(lua_State* L)
{
    switch (luax::checkEnum(L, 1, g_locales))
    {
    case 0:
        sysSetLocale(false);
        break;
    case 1:
        sysSetLocale(true);
        break;
    default:
        luaL_error(L, "unknown locale");
    }
    return 0;
}
#endif
static int sys_clock(lua_State* L)
{
    lua_pushnumber(L, sysGetSeconds(sysClock()));
    return 1;
}
static int sys_time(lua_State* L)
{
    lua_pushnumber(L, g_time);
    return 1;
}
static int sys_deltaTime(lua_State* L)
{
    lua_pushnumber(L, g_deltaTime);
    return 1;
}
static int sys_sleep(lua_State* L)
{
    sysSleep(luaL_checkinteger(L, 1));
    return 0;
}
static int sys_setMaxFrameRate(lua_State* L)
{
    g_pRapidEngine->setMaxFrameRate(luaL_checkinteger(L, 1));
    return 0;
}
// 0x0812e170: returns the name of the pressed button
static int sys_messageBox(lua_State* L)
{
    static luax::Enum types[] = {{"ok", 0},           {"ok_cancel", 1},
                                 {"yes_no", 2},       {"yes_no_cancel", 3},
                                 {"retry_cancel", 4}, {0, 0}};
    static constexpr const char* buttons[5][3] = {{"ok", "", ""},
                                                  {"ok", "cancel", ""},
                                                  {"yes", "no", ""},
                                                  {"yes", "no", "cancel"},
                                                  {"retry", "cancel", ""}};
    const char* title = luaL_checkstring(L, 1);
    const char* message = luaL_checkstring(L, 2);
    int type = 0;
    if (lua_gettop(L) > 2)
        type = luax::checkEnum(L, 3, types);
    int result = sysMessageBox(title, message, (MessageBoxType)type);
    if (result < 1 || result > 3)
        result = 1;
    lua_pushstring(L, buttons[type][result - 1]);
    return 1;
}
// 0x0812e780: sys.fileDialog{parent=, type="open"|"save", title=, description=, extension=,
// initialDir=}
static int sys_fileDialog(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    Frame* parent = 0;
    lua_getfield(L, 1, "parent");
    if (!lua_isnil(L, -1))
        parent = checkFrame(L, lua_gettop(L));
    int mode = 0;
    lua_getfield(L, 1, "type");
    if (!lua_isnil(L, -1))
    {
        const char* type = luaL_checkstring(L, -1);
        if (strcmp(type, "open") == 0)
            mode = 0;
        else if (strcmp(type, "save") == 0)
            mode = 1;
        else
            luaL_error(L, "invalid dialog type");
    }
    lua_getfield(L, 1, "title");
    const char* title =
        lua_isnil(L, -1) ? (mode == 1 ? "Save File As" : "Open File") : luaL_checkstring(L, -1);
    lua_getfield(L, 1, "description");
    const char* description = lua_isnil(L, -1) ? "" : luaL_checkstring(L, -1);
    lua_getfield(L, 1, "extension");
    const char* extension = lua_isnil(L, -1) ? "*.*" : luaL_checkstring(L, -1);
    lua_getfield(L, 1, "initialDir");
    const char* initialDir = lua_isnil(L, -1) ? "" : luaL_checkstring(L, -1);
    lua_getfield(L, 1, "initialName");
    if (!lua_isnil(L, -1))
        luaL_checkstring(L, -1);
    String result = fileDialog(parent ? parent->getWindow() : 0, mode, title, description,
                               extension, initialDir);
    if (result.size() == 0)
        return 0;
    lua_pushstring(L, result.c_str());
    return 1;
}
// 0x0812e630
static int sys_browseFolderDialog(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    Frame* parent = 0;
    lua_getfield(L, 1, "parent");
    if (!lua_isnil(L, -1))
        parent = checkFrame(L, lua_gettop(L));
    lua_getfield(L, 1, "title");
    const char* title = lua_isnil(L, -1) ? "Select Folder" : luaL_checkstring(L, -1);
    String result = browseFolderDialog(parent ? parent->getWindow() : 0, title);
    if (result.size() == 0)
        return 0;
    lua_pushstring(L, result.c_str());
    return 1;
}
#if GRIMROCK_GAME >= 2
// grimrock2.exe 0x0040b260
static int sys_openURL(lua_State* L)
{
    sysOpenURL(luaL_checkstring(L, 1));
    return 0;
}
// grimrock2.exe 0x0040b290: bytes
static int sys_getMemoryStatus(lua_State* L)
{
    MemoryStatus status;
    sysGetMemoryStatus(status);
    lua_createtable(L, 0, 0);
    lua_pushnumber(L, (double)status.availablePhysical);
    lua_setfield(L, -2, "AvailablePhysical");
    lua_pushnumber(L, (double)status.availableVirtual);
    lua_setfield(L, -2, "AvailableVirtual");
    lua_pushnumber(L, (double)status.totalPhysical);
    lua_setfield(L, -2, "TotalPhysical");
    lua_pushnumber(L, (double)status.totalVirtual);
    lua_setfield(L, -2, "TotalVirtual");
    return 1;
}
// grimrock2.exe 0x0040b410: memory in MB, display adapters as GPU0, GPU1, ...
static int sys_getSystemInfo(lua_State* L)
{
    SystemInfo info;
    sysGetSystemInfo(info);
    lua_createtable(L, 0, 0);
    lua_pushstring(L, info.computerName.c_str());
    lua_setfield(L, -2, "ComputerName");
    lua_pushstring(L, info.osVersion.c_str());
    lua_setfield(L, -2, "OSVersion");
    lua_pushnumber(L, info.oemId);
    lua_setfield(L, -2, "OEMID");
    lua_pushnumber(L, info.processorCount);
    lua_setfield(L, -2, "PhysicalCPUCount");
    lua_pushnumber(L, info.logicalProcessorCount);
    lua_setfield(L, -2, "LogicalCPUCount");
    lua_pushnumber(L, info.pageSize);
    lua_setfield(L, -2, "PageSize");
    lua_pushstring(L, info.cpuVendor.c_str());
    lua_setfield(L, -2, "CPUVendor");
    lua_pushstring(L, info.cpuBrand.c_str());
    lua_setfield(L, -2, "CPUBrand");
    lua_pushnumber(L, (double)(info.totalPhysicalMemory >> 20));
    lua_setfield(L, -2, "TotalMem");
    lua_pushnumber(L, (double)(info.availablePhysicalMemory >> 20));
    lua_setfield(L, -2, "FreeMem");
    for (int i = 0; i < info.displayDevices.size(); ++i)
    {
        lua_pushstring(L, info.displayDevices[i].c_str());
        char key[16];
        snprintf(key, sizeof(key), "GPU%d", i);
        lua_setfield(L, -2, key);
    }
    return 1;
}
#endif
static int sys_exit(lua_State* L)
{
    g_pRapidEngine->quit();
    return 0;
}
// Key name -> virtual key code, single characters map to their upper case code.
static int keyCodeFromName(const char* name)
{
    if (!name || !*name)
        return 0;
    for (int key = 0; key < 512; ++key)
    {
        const char* n = getKeyName(key);
        if (n && strcmp(n, name) == 0)
            return key;
    }
    return toupper((unsigned char)*name);
}
static int sys_keyDown(lua_State* L)
{
    int key = keyCodeFromName(luaL_checkstring(L, 1));
    lua_pushboolean(L, g_pMainFrame->isKeyDown(key));
    return 1;
}
static int sys_keyPressed(lua_State* L)
{
    int key = keyCodeFromName(luaL_checkstring(L, 1));
    lua_pushboolean(L, g_pMainFrame->isKeyPressed(key));
    return 1;
}
static int sys_getKeyName(lua_State* L)
{
    const char* name = getKeyName(luaL_checkinteger(L, 1));
    lua_pushstring(L, name);
    return 1;
}
static int sys_mouseDown(lua_State* L)
{
    lua_pushboolean(L, g_pMainFrame->isMouseDown(luaL_checkinteger(L, 1)));
    return 1;
}
static int sys_mousePressed(lua_State* L)
{
    lua_pushboolean(L, g_pMainFrame->isMousePressed(luaL_checkinteger(L, 1)));
    return 1;
}
#if GRIMROCK_GAME >= 2
static int sys_mouseReleased(lua_State* L)
{
    lua_pushboolean(L, g_pMainFrame->isMouseReleased(luaL_checkinteger(L, 1)));
    return 1;
}
#endif
static int sys_mousePos(lua_State* L)
{
    lua_pushnumber(L, g_pMainFrame->getMouseX());
    lua_pushnumber(L, g_pMainFrame->getMouseY());
    return 2;
}
// 0x0812de50: returns the previous display function
static int sys_displayFunc(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "rapid.display_func");
    lua_pushvalue(L, 1);
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_setfield(L, LUA_REGISTRYINDEX, "rapid.display_func");
    lua_pushnil(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "rapid.resume_func");
    return 1;
}
static int sys_compress(lua_State* L)
{
    try
    {
        const char* data = luaL_checkstring(L, 1);
        int length;
#if GRIMROCK_GAME >= 2
        // 0x0040ba60: compress(data [, level]) without the size header
        int level = 6;
        if (lua_gettop(L) > 1)
            level = luaL_checkinteger(L, 2);
        char* out = compress(data, (int)lua_objlen(L, 1), length, level);
#else
        char* out = compress(data, (int)lua_objlen(L, 1), length);
#endif
        lua_pushlstring(L, out, length);
        delete[] out;
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int sys_uncompress(lua_State* L)
{
    try
    {
        const char* data = luaL_checkstring(L, 1);
        int length;
#if GRIMROCK_GAME >= 2
        // 0x0040bb30: uncompress(data, uncompressedSize)
        length = luaL_checkinteger(L, 2);
        char* out = new char[length > 0 ? length : 1];
        uncompress(data, (int)lua_objlen(L, 1), out, length);
#else
        char* out = uncompress(data, (int)lua_objlen(L, 1), length);
#endif
        lua_pushlstring(L, out, length);
        delete[] out;
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int sys_encrypt(lua_State* L)
{
    const char* data = luaL_checkstring(L, 1);
    int length = (int)lua_objlen(L, 1);
    const char* key = luaL_checkstring(L, 2);
    int keyLength = (int)lua_objlen(L, 2);
    if (lua_gettop(L) >= 3)
        luax::checkEnum(L, 3, g_encryptionMethods);
    char* out = new char[length];
    encryptARC4(data, out, length, key, keyLength);
    lua_pushlstring(L, out, length);
    delete[] out;
    return 1;
}
static int sys_decrypt(lua_State* L)
{
    const char* data = luaL_checkstring(L, 1);
    int length = (int)lua_objlen(L, 1);
    const char* key = luaL_checkstring(L, 2);
    int keyLength = (int)lua_objlen(L, 2);
    if (lua_gettop(L) >= 3)
        luax::checkEnum(L, 3, g_encryptionMethods);
    char* out = new char[length];
    decryptARC4(data, out, length, key, keyLength);
    lua_pushlstring(L, out, length);
    delete[] out;
    return 1;
}
// 0x0812d970: FNV-1a of the string
static int sys_fnv32(lua_State* L)
{
    const unsigned char* str = (const unsigned char*)luaL_checkstring(L, 1);
    int length = (int)lua_objlen(L, 1);
    unsigned int hash = Fnv1a32Basis;
    for (int i = 0; i < length; ++i)
        hash = (hash ^ str[i]) * Fnv1a32Prime;
    lua_pushnumber(L, (double)hash);
    return 1;
}
static int sys_setClipboard(lua_State* L)
{
    sysSetClipboard(luaL_checkstring(L, 1));
    return 0;
}
static int sys_getClipboard(lua_State* L)
{
    lua_pushstring(L, sysGetClipboard().c_str());
    return 1;
}
#if GRIMROCK_GAME >= 2
static int sys_proxyCount(lua_State* L)
{
    lua_pushnumber(L, luax::proxyCount(L));
    return 1;
}
#endif
static int sys_createGuid(lua_State* L)
{
    lua_pushstring(L, sysCreateGuid().c_str());
    return 1;
}

// 0x0812d800
void sys_mod(lua_State* L)
{
    static const luaL_Reg functions[] = {{"print", print},
                                         {"dofile", dofile},
                                         {"loadfile", loadfile},
                                         {"raw_bytes_to_float", raw_bytes_to_float},
                                         {0, 0}};
    static const luaL_Reg sys[] = {{"loadProject", sys_loadProject},
                                   {"getProjectFolder", sys_getProjectFolder},
                                   {"restart", sys_restart},
                                   {"resume", sys_resume},
                                   {"args", sys_args},
                                   {"rootDirectory", sys_rootDirectory},
                                   {"platform", sys_platform},
                                   {"setCompanyName", sys_setCompanyName},
                                   {"setApplicationName", sys_setApplicationName},
                                   {"getSystemFolder", sys_getSystemFolder},
                                   {"getDesktopDisplayMode", sys_getDesktopDisplayMode},
#if GRIMROCK_GAME >= 2
                                   {"systemParametersInfo", sys_systemParametersInfo},
                                   {"setLocale", sys_setLocale},
#endif
                                   {"clock", sys_clock},
                                   {"time", sys_time},
                                   {"deltaTime", sys_deltaTime},
                                   {"sleep", sys_sleep},
                                   {"setMaxFrameRate", sys_setMaxFrameRate},
                                   {"messageBox", sys_messageBox},
                                   {"fileDialog", sys_fileDialog},
                                   {"browseFolderDialog", sys_browseFolderDialog},
#if GRIMROCK_GAME >= 2
                                   {"openURL", sys_openURL},
                                   {"getMemoryStatus", sys_getMemoryStatus},
                                   {"getSystemInfo", sys_getSystemInfo},
#endif
                                   {"exit", sys_exit},
                                   {"keyDown", sys_keyDown},
                                   {"keyPressed", sys_keyPressed},
                                   {"getKeyName", sys_getKeyName},
                                   {"mouseDown", sys_mouseDown},
                                   {"mousePressed", sys_mousePressed},
#if GRIMROCK_GAME >= 2
                                   {"mouseReleased", sys_mouseReleased},
#endif
                                   {"mousePos", sys_mousePos},
                                   {"displayFunc", sys_displayFunc},
                                   {"compress", sys_compress},
                                   {"uncompress", sys_uncompress},
                                   {"encrypt", sys_encrypt},
                                   {"decrypt", sys_decrypt},
                                   {"fnv32", sys_fnv32},
                                   {"setClipboard", sys_setClipboard},
                                   {"getClipboard", sys_getClipboard},
                                   {"createGuid", sys_createGuid},
#if GRIMROCK_GAME >= 2
                                   {"proxyCount", sys_proxyCount},
#endif
                                   {0, 0}};
    g_startTime = sysClock();
    luax::registerFunctions(L, functions);
    luax::registerModule(L, "sys", sys);
}
