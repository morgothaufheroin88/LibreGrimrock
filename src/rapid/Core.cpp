// The core Lua module (file systems, streams, dates, images, profiler), reconstructed
// from Core.cpp (0x081348a0-0x08138910).
#include "core/ArchiveFileSystem.h"
#include "core/ByteArrayStream.h"
#include "core/Exception.h"
#include "core/FileStream.h"
#include "core/FileSystem.h"
#include "core/Image.h"
#include "core/MersenneTwister.h"
#include "core/Profiler.h"
#if GRIMROCK_GAME >= 2
#include "core/DirectoryWatcher.h"
#include "core/ZLibStream.h"
#endif
#include "core/Sys.h"
#include "luax.h"
#include "sys.h"
#include <cmath>
#include <cstring>

using namespace core;

// ---- FileSystem ------------------------------------------------------------------

static int FileSystem_mount(lua_State* L)
{
    try
    {
        mount(*luax::checkObject<FileSystem>(L, 1), -1);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_unmount(lua_State* L)
{
    try
    {
        unmount(*luax::checkObject<FileSystem>(L, 1));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_addSearchPath(lua_State* L)
{
    addSearchPath(luaL_checkstring(L, 1));
    return 0;
}
static int FileSystem_removeSearchPath(lua_State* L)
{
    removeSearchPath(luaL_checkstring(L, 1));
    return 0;
}
static int FileSystem_fileExists(lua_State* L)
{
    try
    {
        lua_pushboolean(L, fileExists(luaL_checkstring(L, 1)));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_fileLength(lua_State* L)
{
    try
    {
        lua_pushnumber(L, fileLength(luaL_checkstring(L, 1)));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_fileDate(lua_State* L)
{
    try
    {
        FileDate* date = new FileDate(fileDate(luaL_checkstring(L, 1)));
        luax::createObject<FileDate>(L, date, luax::ProxyDestructor<FileDate>::destroy);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_readFile(lua_State* L)
{
    try
    {
        File* file = openRead(luaL_checkstring(L, 1));
        int length = file->getFileLength();
        char* data = new char[length];
        file->read(data, length);
        closeFile(file);
        lua_pushlstring(L, data, length);
        delete[] data;
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_setCurrentDirectory(lua_State* L)
{
    try
    {
        setCurrentDirectory(luaL_checkstring(L, 1));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_getCurrentDirectory(lua_State* L)
{
    lua_pushstring(L, getCurrentDirectory().c_str());
    return 1;
}
static int FileSystem_copyFile(lua_State* L)
{
    try
    {
        copyFile(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_moveFile(lua_State* L)
{
    try
    {
        moveFile(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_deleteFile(lua_State* L)
{
    try
    {
        deleteFile(luaL_checkstring(L, 1));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_createPath(lua_State* L)
{
    try
    {
        createPath(luaL_checkstring(L, 1));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int FileSystem_setFileDate(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        FileDate* date = luax::checkObject<FileDate>(L, 2);
        setFileDate(filename, *date);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x08135fa0: findFiles(path, pattern[, recursive])
static int FileSystem_findFiles(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const char* pattern = luaL_checkstring(L, 2);
    bool recursive = false;
    if (lua_type(L, 3) != LUA_TNONE)
    {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        recursive = lua_toboolean(L, 3) != 0;
    }
    Array<String> files;
    files.reserve(256);
    findFiles(path, pattern, files, recursive);
    lua_newtable(L);
    for (int i = 0; i < files.size(); ++i)
    {
        lua_pushstring(L, files[i].c_str());
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}
static int FileSystem_getFileExtension(lua_State* L)
{
    lua_pushstring(L, getFileExtension(luaL_checkstring(L, 1)).c_str());
    return 1;
}
static int FileSystem_getPath(lua_State* L)
{
    lua_pushstring(L, getPath(luaL_checkstring(L, 1)).c_str());
    return 1;
}
static int FileSystem_stripPath(lua_State* L)
{
    lua_pushstring(L, stripPath(luaL_checkstring(L, 1)).c_str());
    return 1;
}
static int FileSystem_stripExtension(lua_State* L)
{
    lua_pushstring(L, stripExtension(luaL_checkstring(L, 1)).c_str());
    return 1;
}
// 0x081348a0: "dir/name.ext" -> dir, name, ext (nil when missing)
#if GRIMROCK_GAME >= 2
// 0x0040cf50: "name####.ext" -> the first free numbered name
static int FileSystem_getTempFilename(lua_State* L)
{
    lua_pushstring(L, getTempFilename(luaL_checkstring(L, 1)).c_str());
    return 1;
}
#endif
static int FileSystem_splitPath(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const char* lastSlash = 0;
    const char* lastDot = 0;
    for (const char* p = path; *p; ++p)
    {
        if (*p == '\\' || *p == '/')
        {
            lastSlash = p;
            lastDot = 0;
        }
        else if (*p == '.')
        {
            lastDot = p;
        }
    }
    const char* name = path;
    if (lastSlash)
    {
        lua_pushlstring(L, path, lastSlash - path);
        name = lastSlash + 1;
    }
    else
    {
        lua_pushnil(L);
    }
    if (lastDot)
    {
        lua_pushlstring(L, name, lastDot - name);
        lua_pushstring(L, lastDot + 1);
    }
    else
    {
        lua_pushstring(L, name);
        lua_pushnil(L);
    }
    return 3;
}
static const luaL_Reg FileSystem_methods[] = {
    {"mount", FileSystem_mount},
    {"unmount", FileSystem_unmount},
    {"addSearchPath", FileSystem_addSearchPath},
    {"removeSearchPath", FileSystem_removeSearchPath},
    {"fileExists", FileSystem_fileExists},
    {"fileLength", FileSystem_fileLength},
    {"fileDate", FileSystem_fileDate},
    {"readFile", FileSystem_readFile},
    {"setCurrentDirectory", FileSystem_setCurrentDirectory},
    {"getCurrentDirectory", FileSystem_getCurrentDirectory},
    {"copyFile", FileSystem_copyFile},
    {"moveFile", FileSystem_moveFile},
    {"deleteFile", FileSystem_deleteFile},
    {"createPath", FileSystem_createPath},
    {"setFileDate", FileSystem_setFileDate},
    {"findFiles", FileSystem_findFiles},
    {"getFileExtension", FileSystem_getFileExtension},
    {"getPath", FileSystem_getPath},
    {"stripPath", FileSystem_stripPath},
    {"stripExtension", FileSystem_stripExtension},
    {"splitPath", FileSystem_splitPath},
#if GRIMROCK_GAME >= 2
    {"getTempFilename", FileSystem_getTempFilename},
#endif
    {0, 0}};

static int ArchiveFileSystem_create(lua_State* L)
{
    try
    {
        luax::createSharedObject<ArchiveFileSystem>(L,
                                                    new ArchiveFileSystem(luaL_checkstring(L, 1)));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static const luaL_Reg ArchiveFileSystem_methods[] = {{"create", ArchiveFileSystem_create}, {0, 0}};

// ---- streams ---------------------------------------------------------------------

static int InputStream_availableBytes(lua_State* L)
{
    try
    {
        lua_pushnumber(L, luax::checkObject<InputStream>(L, 1)->availableBytes());
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_skip(lua_State* L)
{
    try
    {
        luax::checkObject<InputStream>(L, 1)->skip(luaL_checkinteger(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_getPosition(lua_State* L)
{
    try
    {
        lua_pushnumber(L, luax::checkObject<InputStream>(L, 1)->getPosition());
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_seek(lua_State* L)
{
    try
    {
        luax::checkObject<InputStream>(L, 1)->seek(luaL_checkinteger(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_getFilename(lua_State* L)
{
    try
    {
        lua_pushstring(L, luax::checkObject<InputStream>(L, 1)->getFilename());
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_readBytes(lua_State* L)
{
    try
    {
        InputStream* stream = luax::checkObject<InputStream>(L, 1);
        int count = luaL_checkinteger(L, 2);
        char* data = new char[count];
        stream->readBytes(data, count);
        lua_pushlstring(L, data, count);
        delete[] data;
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_readByte(lua_State* L)
{
    try
    {
        unsigned char v;
        luax::checkObject<InputStream>(L, 1)->readByte(v);
        lua_pushnumber(L, v);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_readShort(lua_State* L)
{
    try
    {
        unsigned short v;
        luax::checkObject<InputStream>(L, 1)->readShort(v);
        lua_pushnumber(L, v);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_readInt(lua_State* L)
{
    try
    {
        unsigned int v;
        luax::checkObject<InputStream>(L, 1)->readInt(v);
        lua_pushnumber(L, v);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_readFloat(lua_State* L)
{
    try
    {
        float v;
        luax::checkObject<InputStream>(L, 1)->readFloat(v);
        lua_pushnumber(L, v);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_readDouble(lua_State* L)
{
    try
    {
        double v;
        luax::checkObject<InputStream>(L, 1)->readDouble(v);
        lua_pushnumber(L, v);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int InputStream_readBool(lua_State* L)
{
    try
    {
        bool v;
        luax::checkObject<InputStream>(L, 1)->readBool(v);
        lua_pushboolean(L, v);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static const luaL_Reg InputStream_methods[] = {{"availableBytes", InputStream_availableBytes},
                                               {"skip", InputStream_skip},
                                               {"getPosition", InputStream_getPosition},
                                               {"seek", InputStream_seek},
                                               {"getFilename", InputStream_getFilename},
                                               {"readBytes", InputStream_readBytes},
                                               {"readByte", InputStream_readByte},
                                               {"readShort", InputStream_readShort},
                                               {"readInt", InputStream_readInt},
                                               {"readFloat", InputStream_readFloat},
                                               {"readDouble", InputStream_readDouble},
                                               {"readBool", InputStream_readBool},
                                               {0, 0}};

static int OutputStream_getPosition(lua_State* L)
{
    try
    {
        lua_pushnumber(L, luax::checkObject<OutputStream>(L, 1)->getPosition());
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int OutputStream_seek(lua_State* L)
{
    try
    {
        luax::checkObject<OutputStream>(L, 1)->seek(luaL_checkinteger(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int OutputStream_getFilename(lua_State* L)
{
    try
    {
        lua_pushstring(L, luax::checkObject<OutputStream>(L, 1)->getFilename());
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int OutputStream_writeBytes(lua_State* L)
{
    try
    {
        OutputStream* out = luax::checkObject<OutputStream>(L, 1);
        const char* data = luaL_checkstring(L, 2);
        out->writeBytes(data, (int)lua_objlen(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int OutputStream_writeByte(lua_State* L)
{
    try
    {
        luax::checkObject<OutputStream>(L, 1)->writeByte(
            (unsigned char)(luaL_checkinteger(L, 2) & 0xff));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int OutputStream_writeShort(lua_State* L)
{
    try
    {
        luax::checkObject<OutputStream>(L, 1)->writeShort(
            (unsigned short)(luaL_checkinteger(L, 2) & 0xffff));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int OutputStream_writeInt(lua_State* L)
{
    try
    {
        luax::checkObject<OutputStream>(L, 1)->writeInt((int)luaL_checkinteger(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int OutputStream_writeFloat(lua_State* L)
{
    try
    {
        luax::checkObject<OutputStream>(L, 1)->writeFloat((float)luaL_checknumber(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int OutputStream_writeDouble(lua_State* L)
{
    try
    {
        luax::checkObject<OutputStream>(L, 1)->writeDouble(luaL_checknumber(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int OutputStream_writeBool(lua_State* L)
{
    try
    {
        OutputStream* out = luax::checkObject<OutputStream>(L, 1);
        out->writeBool(luax::checkBool(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static const luaL_Reg OutputStream_methods[] = {{"getPosition", OutputStream_getPosition},
                                                {"seek", OutputStream_seek},
                                                {"getFilename", OutputStream_getFilename},
                                                {"writeBytes", OutputStream_writeBytes},
                                                {"writeByte", OutputStream_writeByte},
                                                {"writeShort", OutputStream_writeShort},
                                                {"writeInt", OutputStream_writeInt},
                                                {"writeFloat", OutputStream_writeFloat},
                                                {"writeDouble", OutputStream_writeDouble},
                                                {"writeBool", OutputStream_writeBool},
                                                {0, 0}};

static int FileInputStream_create(lua_State* L)
{
    try
    {
        luax::createSharedObject<FileInputStream>(L, new FileInputStream(luaL_checkstring(L, 1)));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static const luaL_Reg FileInputStream_methods[] = {{"create", FileInputStream_create}, {0, 0}};
static int FileOutputStream_create(lua_State* L)
{
    try
    {
        luax::createSharedObject<FileOutputStream>(L, new FileOutputStream(luaL_checkstring(L, 1)));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static const luaL_Reg FileOutputStream_methods[] = {{"create", FileOutputStream_create}, {0, 0}};
static int ByteArrayInputStream_create(lua_State* L)
{
    try
    {
        const char* data = luaL_checkstring(L, 1);
        luax::createSharedObject<ByteArrayInputStream>(
            L, new ByteArrayInputStream(data, (int)lua_objlen(L, 1), true));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static const luaL_Reg ByteArrayInputStream_methods[] = {{"create", ByteArrayInputStream_create},
                                                        {0, 0}};
static int ByteArrayOutputStream_create(lua_State* L)
{
    try
    {
        luax::createSharedObject<ByteArrayOutputStream>(L, new ByteArrayOutputStream);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int ByteArrayOutputStream_getData(lua_State* L)
{
    ByteArrayOutputStream* out = luax::checkObject<ByteArrayOutputStream>(L, 1);
    lua_pushlstring(L, out->data(), out->size());
    return 1;
}
static const luaL_Reg ByteArrayOutputStream_methods[] = {
    {"create", ByteArrayOutputStream_create}, {"getData", ByteArrayOutputStream_getData}, {0, 0}};

// ---- FileDate --------------------------------------------------------------------

// 100ns ticks between 1601-01-01 and 1970-01-01
static const long long UnixEpochOffset = 0x19db1ded53e8000LL;

static int FileDate_createFromUnixTime(lua_State* L)
{
    long long unixTime = (long long)lrint(luaL_checknumber(L, 1));
    FileDate* date = new FileDate(unixTime * 10000000LL + UnixEpochOffset);
    luax::createObject<FileDate>(L, date, luax::ProxyDestructor<FileDate>::destroy);
    return 1;
}
static int FileDate_compare(lua_State* L)
{
    FileDate* first = luax::checkObject<FileDate>(L, 1);
    FileDate* second = luax::checkObject<FileDate>(L, 2);
    lua_pushnumber(L, *first < *second ? -1.0 : (*first == *second ? 0.0 : 1.0));
    return 1;
}
static int FileDate_toUnixTime(lua_State* L)
{
    FileDate* date = luax::checkObject<FileDate>(L, 1);
    lua_pushnumber(L, (double)(int)(*date / 10000000LL - UnixEpochOffset / 10000000LL));
    return 1;
}
static const luaL_Reg FileDate_methods[] = {{"createFromUnixTime", FileDate_createFromUnixTime},
                                            {"compare", FileDate_compare},
                                            {"toUnixTime", FileDate_toUnixTime},
                                            {0, 0}};

// ---- Image -----------------------------------------------------------------------

static int Image_create(lua_State* L)
{
    int width = luaL_checkinteger(L, 1);
    int height = luaL_checkinteger(L, 2);
    if (width < 1 || height < 1)
        luaL_error(L, "invalid size");
    luax::createObject<Image>(L, new Image(width, height), luax::ProxyDestructor<Image>::destroy);
    return 1;
}
static int Image_load(lua_State* L)
{
    try
    {
        luax::createObject<Image>(L, new Image(luaL_checkstring(L, 1)),
                                  luax::ProxyDestructor<Image>::destroy);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static int Image_save(lua_State* L)
{
    try
    {
        Image* image = luax::checkObject<Image>(L, 1);
        const char* filename = luaL_checkstring(L, 2);
        bool alpha = true;
        if (lua_type(L, 3) != LUA_TNONE)
            alpha = luax::checkBool(L, 3);
        image->save(filename, alpha);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x08136320: copy(src, srcX, srcY, width, height, dst, dstX, dstY)
static int Image_copy(lua_State* L)
{
    Image* src = luax::checkObject<Image>(L, 1);
    int srcX = luaL_checkinteger(L, 2);
    int srcY = luaL_checkinteger(L, 3);
    int width = luaL_checkinteger(L, 4);
    int height = luaL_checkinteger(L, 5);
    Image* dst = luax::checkObject<Image>(L, 6);
    int dstX = luaL_checkinteger(L, 7);
    int dstY = luaL_checkinteger(L, 8);
    if (srcX < 0 || srcY < 0 || width < 0 || height < 0 || srcX + width > src->getWidth() ||
        srcY + height > src->getHeight())
        luaL_error(L, "invalid source rectangle");
    if (dstX < 0 || dstY < 0 || dstX + width > dst->getWidth() || dstY + height > dst->getHeight())
        luaL_error(L, "invalid destination rectangle");
    dst->copy(dstX, dstY, src, srcX, srcY, width, height);
    return 0;
}
static int Image_getWidth(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Image>(L, 1)->getWidth());
    return 1;
}
static int Image_getHeight(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Image>(L, 1)->getHeight());
    return 1;
}
static int Image_setPixel(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    if (x < 0 || y < 0 || x >= image->getWidth() || y >= image->getHeight())
        luaL_error(L, "coordinate out of range");
    if (lua_type(L, 4) != LUA_TTABLE)
        luaL_typerror(L, 4, "color");
    float c[4];
    for (int i = 0; i < 4; ++i)
    {
        lua_rawgeti(L, 4, i + 1);
        c[i] = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    image->setPixel(x, y,
                    Color((unsigned char)lrintf(c[0]), (unsigned char)lrintf(c[1]),
                          (unsigned char)lrintf(c[2]), (unsigned char)lrintf(c[3])));
    return 0;
}
static int Image_getPixel(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    if (x < 0 || y < 0 || x >= image->getWidth() || y >= image->getHeight())
        luaL_error(L, "coordinate out of range");
    Color pixel = image->getPixel(x, y);
    luax::pushVector(L, Vec4(pixel.r, pixel.g, pixel.b, pixel.a));
    return 1;
}
#if GRIMROCK_GAME >= 2
// 0x0040eb10
static int Image_clear(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    image->clear(luax::checkColor(L, 2));
    return 0;
}
// 0x0040eb60: fillRect(x, y, width, height, color)
static int Image_fillRect(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    int width = luaL_checkinteger(L, 4);
    int height = luaL_checkinteger(L, 5);
    image->fillRect(x, y, width, height, luax::checkColor(L, 6));
    return 0;
}
// 0x0040ed10: r, g, b, a as four numbers
static int Image_getPixelRGBA(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    if (x < 0 || y < 0 || x >= image->getWidth() || y >= image->getHeight())
        luaL_error(L, "coordinate out of range");
    Color pixel = image->getPixel(x, y);
    lua_pushnumber(L, pixel.r);
    lua_pushnumber(L, pixel.g);
    lua_pushnumber(L, pixel.b);
    lua_pushnumber(L, pixel.a);
    return 4;
}
// 0x0040ee00 / 0x0040eed0
static int Image_sampleNearestClamp(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    luax::pushVector(L, image->sampleNearestClamp((float)luaL_checknumber(L, 2),
                                                  (float)luaL_checknumber(L, 3)));
    return 1;
}
static int Image_sampleNearestClampRGBA(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    Vec4 c = image->sampleNearestClamp((float)luaL_checknumber(L, 2),
                                       (float)luaL_checknumber(L, 3));
    lua_pushnumber(L, c.x);
    lua_pushnumber(L, c.y);
    lua_pushnumber(L, c.z);
    lua_pushnumber(L, c.w);
    return 4;
}
// 0x0040ef90 / 0x0040f060
static int Image_sampleLinearClamp(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    luax::pushVector(L, image->sampleLinearClamp((float)luaL_checknumber(L, 2),
                                                 (float)luaL_checknumber(L, 3)));
    return 1;
}
static int Image_sampleLinearClampRGBA(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    Vec4 c = image->sampleLinearClamp((float)luaL_checknumber(L, 2),
                                      (float)luaL_checknumber(L, 3));
    lua_pushnumber(L, c.x);
    lua_pushnumber(L, c.y);
    lua_pushnumber(L, c.z);
    lua_pushnumber(L, c.w);
    return 4;
}
// 0x0040f120
static int Image_resample(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    int width = luaL_checkinteger(L, 2);
    int height = luaL_checkinteger(L, 3);
    if (width < 1 || height < 1)
        luaL_error(L, "invalid image size");
    image->resample(width, height);
    return 0;
}
// 0x0040f170
static int Image_blur(lua_State* L)
{
    luax::checkObject<Image>(L, 1)->blur(0);
    return 0;
}
#endif
static int Image_setImageData(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    const char* data = luaL_checkstring(L, 2);
    size_t size = lua_objlen(L, 2);
    if (size != (size_t)(image->getWidth() * image->getHeight() * 4))
        return luaL_error(L, "invalid data size");
    memcpy(image->getData(), data, size);
    return 0;
}
static int Image_getImageData(lua_State* L)
{
    Image* image = luax::checkObject<Image>(L, 1);
    lua_pushlstring(L, (const char*)image->getData(), image->getWidth() * image->getHeight() * 4);
    return 1;
}
static const luaL_Reg Image_methods[] = {{"create", Image_create},
                                         {"load", Image_load},
                                         {"save", Image_save},
                                         {"copy", Image_copy},
                                         {"getWidth", Image_getWidth},
                                         {"getHeight", Image_getHeight},
#if GRIMROCK_GAME >= 2
                                         {"clear", Image_clear},
                                         {"fillRect", Image_fillRect},
#endif
                                         {"setPixel", Image_setPixel},
                                         {"getPixel", Image_getPixel},
#if GRIMROCK_GAME >= 2
                                         {"getPixelRGBA", Image_getPixelRGBA},
                                         {"sampleNearestClamp", Image_sampleNearestClamp},
                                         {"sampleNearestClampRGBA", Image_sampleNearestClampRGBA},
                                         {"sampleLinearClamp", Image_sampleLinearClamp},
                                         {"sampleLinearClampRGBA", Image_sampleLinearClampRGBA},
                                         {"resample", Image_resample},
                                         {"blur", Image_blur},
#endif
                                         {"setImageData", Image_setImageData},
                                         {"getImageData", Image_getImageData},
                                         {0, 0}};
static constexpr const char* Image_properties[] = {"Width", "Height", 0};

// ---- Profiler --------------------------------------------------------------------

static int Profiler_beginFrame(lua_State* L)
{
    Profiler::beginFrame();
    return 0;
}
static int Profiler_endFrame(lua_State* L)
{
    Profiler::endFrame();
    return 0;
}
static int Profiler_beginBlock(lua_State* L)
{
    Profiler::beginBlock(luaL_checkstring(L, 1));
    return 0;
}
static int Profiler_endBlock(lua_State* L)
{
    Profiler::endBlock();
    return 0;
}
static int Profiler_draw(lua_State* L)
{
    Profiler::draw();
    return 0;
}
#if GRIMROCK_GAME >= 2
// 0x0040f2f0
static int Profiler_getBlockCount(lua_State* L)
{
    lua_pushnumber(L, Profiler::getBlockCount());
    return 1;
}
// 0x0040f320: name, call count and time of one block
static int Profiler_getBlockData(lua_State* L)
{
    int index = luaL_checkinteger(L, 1);
    if (index < 1 || index > Profiler::getBlockCount())
        luaL_error(L, "invalid profiler block index");
    const char* name;
    int count;
    float time;
    Profiler::getBlockData(index - 1, name, count, time);
    lua_pushstring(L, name);
    lua_pushnumber(L, count);
    lua_pushnumber(L, time);
    return 3;
}
#endif
static const luaL_Reg Profiler_methods[] = {{"beginFrame", Profiler_beginFrame},
                                            {"endFrame", Profiler_endFrame},
                                            {"beginBlock", Profiler_beginBlock},
                                            {"endBlock", Profiler_endBlock},
                                            {"draw", Profiler_draw},
#if GRIMROCK_GAME >= 2
                                            {"getBlockCount", Profiler_getBlockCount},
                                            {"getBlockData", Profiler_getBlockData},
#endif
                                            {0, 0}};

#if GRIMROCK_GAME >= 2
// ---- MersenneTwister --------------------------------------------------------------

LUAX_CLASS(MersenneTwister, "MersenneTwister")

// 0x0040f500: MersenneTwister.create([seed])
static int MersenneTwister_create(lua_State* L)
{
    MersenneTwister* rng = new MersenneTwister;
    if (lua_gettop(L) > 0)
        rng->initGen((unsigned long)(long long)luaL_checknumber(L, 1));
    luax::createSharedObject<MersenneTwister>(L, rng);
    return 1;
}
// 0x0040f5b0: [0,1)
static int MersenneTwister_random(lua_State* L)
{
    MersenneTwister* rng = luax::checkObject<MersenneTwister>(L, 1);
    lua_pushnumber(L, rng->genrand_real2());
    return 1;
}
// 0x0040f620: randomInt(lo, hi), inclusive and in either order
static int MersenneTwister_randomInt(lua_State* L)
{
    MersenneTwister* rng = luax::checkObject<MersenneTwister>(L, 1);
    int lo = luaL_checkinteger(L, 2);
    int hi = luaL_checkinteger(L, 3);
    if (hi < lo)
    {
        int range = lo - hi + 1;
        if (range < 2)
            range = 1;
        lua_pushnumber(L, hi + (int)(rng->genrand_int31() % range));
    }
    else
    {
        int range = hi - lo + 1;
        if (range < 2)
            range = 1;
        lua_pushnumber(L, lo + (int)(rng->genrand_int31() % range));
    }
    return 1;
}
static const luaL_Reg MersenneTwister_methods[] = {{"create", MersenneTwister_create},
                                                   {"random", MersenneTwister_random},
                                                   {"randomInt", MersenneTwister_randomInt},
                                                   {0, 0}};

// ---- ZLibDecompressorInputStream ---------------------------------------------------

LUAX_CLASS(ZLibDecompressorInputStream, "ZLibDecompressorInputStream")

// 0x0040e300
static int ZLibDecompressorInputStream_create(lua_State* L)
{
    try
    {
        InputStream* host = luax::checkObject<InputStream>(L, 1);
        luax::createSharedObject<ZLibDecompressorInputStream>(
            L, new ZLibDecompressorInputStream(host));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static const luaL_Reg ZLibDecompressorInputStream_methods[] = {
    {"create", ZLibDecompressorInputStream_create}, {0, 0}};

// ---- DirectoryWatcher / FileResource ------------------------------------------------

LUAX_CLASS(DirectoryWatcher, "DirectoryWatcher")

// 0x0040e550
static int DirectoryWatcher_create(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    luax::createSharedObject<DirectoryWatcher>(L, new DirectoryWatcher(path));
    return 1;
}
// 0x0040e600: the next changed file, nothing when the directory is unchanged
static int DirectoryWatcher_getChangedFile(lua_State* L)
{
    DirectoryWatcher* watcher = luax::checkObject<DirectoryWatcher>(L, 1);
    String filename;
    if (!watcher->getChangedFile(filename))
        return 0;
    lua_pushstring(L, filename.c_str());
    return 1;
}
static const luaL_Reg DirectoryWatcher_methods[] = {
    {"create", DirectoryWatcher_create},
    {"getChangedFile", DirectoryWatcher_getChangedFile},
    {0, 0}};

// 0x0040e6b0: reloads the resources loaded from the file
static int FileResource_fileChanged(lua_State* L)
{
    fileChanged(luaL_checkstring(L, 1));
    return 0;
}
static const luaL_Reg FileResource_methods[] = {{"fileChanged", FileResource_fileChanged},
                                                {0, 0}};
#endif

// ---- math.random / math.randomseed ------------------------------------------------

// 0x082c3d20: the Mersenne Twister replacing LuaJIT's PRNG.
static MersenneTwister g_rng;
constexpr long double Int32ToUnit = 2.3283064370807974e-10L; // 1 / 2^32

// 0x08135720
static int math_randomseed(lua_State* L)
{
    g_rng.initGen((unsigned long)luaL_checkinteger(L, 1));
    return 0;
}
// 0x08137a70: random() in [0,1), random(n) in [1,n] (or [n,1] for n < 1),
// random(m, n) in [m,n] (either order); integers come from the upper 31 bits.
static int math_random(lua_State* L)
{
    int n = lua_gettop(L);
    if (n == 0)
    {
        unsigned int random = g_rng.genrand_int32();
        lua_pushnumber(L, (float)((long double)random * Int32ToUnit));
        return 1;
    }
    int result;
    if (n == 1)
    {
        int upper = luaL_checkinteger(L, 1);
        int random = (int)(g_rng.genrand_int32() >> 1);
        if (upper < 1)
            result = random % (2 - upper) + upper;
        else
            result = random % upper + 1;
    }
    else
    {
        if (n != 2)
            luaL_error(L, "invalid number of arguments");
        int upper = luaL_checkinteger(L, 2);
        int lower = luaL_checkinteger(L, 1);
        int random = (int)(g_rng.genrand_int32() >> 1);
        if (upper < lower)
        {
            int range = (lower + 1) - upper;
            if (range <= 0)
                range = 1;
            result = random % range + upper;
        }
        else
        {
            int range = (upper + 1) - lower;
            if (range <= 0)
                range = 1;
            result = random % range + lower;
        }
    }
    lua_pushnumber(L, result);
    return 1;
}
static const luaL_Reg math_functions[] = {
    {"random", math_random}, {"randomseed", math_randomseed}, {0, 0}};

// 0x08134480
void core_mod(lua_State* L)
{
    luax::registerClass(L, "FileSystem", FileSystem_methods, 0);
    luax::registerSubclass(L, "ArchiveFileSystem", "FileSystem", ArchiveFileSystem_methods, 0);
    luax::registerClass(L, "InputStream", InputStream_methods, 0);
    luax::registerClass(L, "OutputStream", OutputStream_methods, 0);
    luax::registerSubclass(L, "FileInputStream", "InputStream", FileInputStream_methods, 0);
    luax::registerSubclass(L, "FileOutputStream", "OutputStream", FileOutputStream_methods, 0);
    luax::registerSubclass(L, "ByteArrayInputStream", "InputStream", ByteArrayInputStream_methods,
                           0);
    luax::registerSubclass(L, "ByteArrayOutputStream", "OutputStream",
                           ByteArrayOutputStream_methods, 0);
    luax::registerClass(L, "FileDate", FileDate_methods, 0);
    luax::registerClass(L, "Image", Image_methods, Image_properties);
    luax::registerClass(L, "Profiler", Profiler_methods, 0);
#if GRIMROCK_GAME >= 2
    luax::registerClass(L, "MersenneTwister", MersenneTwister_methods, 0);
    luax::registerSubclass(L, "ZLibDecompressorInputStream", "InputStream",
                           ZLibDecompressorInputStream_methods, 0);
    luax::registerClass(L, "DirectoryWatcher", DirectoryWatcher_methods, 0);
    luax::registerClass(L, "FileResource", FileResource_methods, 0);
#endif
    lua_getfield(L, LUA_GLOBALSINDEX, "math");
    for (const luaL_Reg* f = math_functions; f->name; ++f)
    {
        lua_pushcfunction(L, f->func);
        lua_setfield(L, -2, f->name);
    }
    lua_pop(L, 1);
}
