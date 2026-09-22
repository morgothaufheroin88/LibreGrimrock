// NativeSaveGameOutputStream of Legend of Grimrock 2 (0x0042ff90-0x00432090): the save game
// writer that the Lua SaveGameOutputStream (SaveGame.lua) was moved into native code for
// speed. The format is the one the Lua SaveGameInputStream reads back: chunks of
// (4 byte id, int size, body), a value stream tagged by type, strings through a string
// table that is written first as the "STAB" chunk, and the whole thing zlib compressed
// behind a "GRIM" + version header.
#include "core/ByteArrayStream.h"
#include "core/FileStream.h"
#include "core/HashMap.h"
#include "core/Utils.h"
#include "luax.h"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace core;

namespace
{
// Value tags of SaveGameInputStream.readValue
enum ValueTag
{
    Tag_String16 = 0,
    Tag_String32 = 1,
    Tag_Byte = 2,
    Tag_Short = 3,
    Tag_Int = 4,
    Tag_Double = 5,
    Tag_True = 6,
    Tag_False = 7,
    Tag_Nil = 8,
    Tag_Function = 9,
    Tag_Table = 10,
    Tag_Vec = 11,
    Tag_ZeroVec = 12,
    Tag_IdentityMat = 13,
    Tag_CommonMat = 14, // + matrix index 0..7, followed by the translation
    Tag_Mat = 22,
    Tag_Sphere = 23,
    Tag_Box = 24,
    Tag_Plane = 25,
    Tag_Ray = 26
};
// CommonMatrices of SaveGame.lua (0x0061a6a8): the four facings, their mirrors and the
// 1/100 scale, as rotation columns x, y, z.
constexpr int NumCommonMatrices = 8;
constexpr float CommonMatrices[NumCommonMatrices][9] = {
    {1, 0, 0, 0, 1, 0, 0, 0, 1},   {0, 0, -1, 0, 1, 0, 1, 0, 0},
    {-1, 0, 0, 0, 1, 0, 0, 0, -1}, {0, 0, 1, 0, 1, 0, -1, 0, 0},
    {0, 0, 1, 0, 1, 0, -1, 0, 0},  {-1, 0, 0, 0, 1, 0, 0, 0, -1},
    {0, 0, 1, 0, 1, 0, -1, 0, 0},  {0.01f, 0, 0, 0, 0.01f, 0, 0, 0, 0.01f}};
constexpr float CommonMatrixEpsilon = 1e-6f;
constexpr int CompressionLevel = 1;

// 0xc28 bytes: file name, the body stream, the chunk start stack, the string table and
// the serialised tables/functions keyed by their tostring() text.
class NativeSaveGameOutputStream
{
  public:
    explicit NativeSaveGameOutputStream(int version)
        : m_pStream(0), m_numStrings(0), m_version(version)
    {
    }
    ~NativeSaveGameOutputStream()
    {
        delete m_pStream;
    }
    // 0x00430b10
    void openFile(const char* filename)
    {
        m_filename = filename;
        if (!m_pStream)
            m_pStream = new ByteArrayOutputStream;
    }
    // 0x00430bd0
    void openBuffer()
    {
        if (!m_pStream)
            m_pStream = new ByteArrayOutputStream;
    }
    // 0x00430190: the string table chunk followed by the body, compressed into the file
    void close()
    {
        if (m_filename.size() > 0 && m_pStream)
        {
            ByteArrayOutputStream table;
            int tableSize = 0;
            for (int i = 0; i < m_strings.size(); ++i)
                tableSize += 4 + m_strings[i].size();
            table.writeBytes("STAB", 4);
            table.writeInt(tableSize);
            for (int i = 0; i < m_strings.size(); ++i)
            {
                table.writeInt(m_strings[i].size());
                table.writeBytes(m_strings[i].c_str(), m_strings[i].size());
            }
            int total = table.size() + m_pStream->size();
            char* data = new char[total];
            memcpy(data, table.data(), table.size());
            memcpy(data + table.size(), m_pStream->data(), m_pStream->size());
            int compressedSize = 0;
            char* compressed = compress(data, total, compressedSize, CompressionLevel);
            FileOutputStream out(m_filename.c_str());
            out.writeBytes("GRIM", 4);
            out.writeInt(m_version);
            out.writeBytes(compressed, compressedSize);
            delete[] data;
            delete[] compressed;
        }
        delete m_pStream;
        m_pStream = 0;
    }
    // 0x00430c90: the body of a buffer stream (no file name)
    const char* getBuffer(int& size)
    {
        if (m_filename.size() > 0 || !m_pStream)
        {
            size = 0;
            return 0;
        }
        size = m_pStream->size();
        return m_pStream->data();
    }
    // 0x00430cf0
    void openChunk(const char* id)
    {
        m_pStream->writeBytes(id, 4);
        m_pStream->writeInt(0);
        m_chunkStarts.push_back(m_pStream->getPosition());
    }
    // 0x00430dd0
    void closeChunk()
    {
        int end = m_pStream->getPosition();
        int start = m_chunkStarts.back();
        m_pStream->seek(start - 4);
        m_pStream->writeInt(end - start);
        m_pStream->seek(end);
        if (m_chunkStarts.size() > 0)
            m_chunkStarts.pop_back();
    }
    // 0x004314c0: the value on top of the Lua stack
    void writeValue(lua_State* L);
    ByteArrayOutputStream* getStream() const
    {
        return m_pStream;
    }

  private:
    // 0x004305d0: strings go through the table, handle 1 upwards
    void writeString(const char* s, int length)
    {
        String key(s, length);
        int* handle = m_stringHandles.findValue(key);
        int h;
        if (!handle)
        {
            h = ++m_numStrings;
            m_stringHandles.insert(key, h);
            m_strings.push_back(key);
        }
        else
        {
            h = *handle;
        }
        if (h < 0x10000)
        {
            m_pStream->writeByte((unsigned char)Tag_String16);
            m_pStream->writeShort((unsigned short)h);
        }
        else
        {
            m_pStream->writeByte((unsigned char)Tag_String32);
            m_pStream->writeInt((unsigned int)h);
        }
    }
    // 0x00430470: the smallest integer tag that holds the number
    void writeNumber(double v)
    {
        if (floor(v) == v && v >= 0.0)
        {
            if (v < 256.0)
            {
                m_pStream->writeByte((unsigned char)Tag_Byte);
                m_pStream->writeByte((unsigned char)(int)(v));
                return;
            }
            if (v < 65536.0)
            {
                m_pStream->writeByte((unsigned char)Tag_Short);
                m_pStream->writeShort((unsigned short)(int)(v));
                return;
            }
            if (v < 4294967296.0)
            {
                m_pStream->writeByte((unsigned char)Tag_Int);
                m_pStream->writeInt((unsigned int)(long long)(v));
                return;
            }
        }
        m_pStream->writeByte((unsigned char)Tag_Double);
        m_pStream->writeDouble(v);
    }
    void writeVec(const Vec3& v)
    {
        m_pStream->writeDouble(v.x);
        m_pStream->writeDouble(v.y);
        m_pStream->writeDouble(v.z);
    }
    // 0x00430e70 / 0x00431070: tables and functions are written once and referred to by
    // the name Lua's tostring() gives them ("t0x..." / "f0x..." without the type word)
    bool serializedName(lua_State* L, int index, char prefix, String& name, bool& known)
    {
        lua_getfield(L, LUA_GLOBALSINDEX, "tostring");
        lua_pushvalue(L, index);
        lua_call(L, 1, 1);
        size_t length = 0;
        const char* text = luaL_checklstring(L, -1, &length);
        String key(text, (int)length);
        lua_pop(L, 1);
        int* existing = m_serialized.findValue(key);
        if (existing)
        {
            name = m_serializedNames[*existing];
            known = true;
            return true;
        }
        int at = key.find(" 0x");
        String hex = at >= 0 ? key.substr(at + 3) : key;
        name = String(&prefix, 1) + hex;
        m_serialized.insert(key, m_serializedNames.size());
        m_serializedNames.push_back(name);
        known = false;
        return true;
    }
    static int dumpWriter(lua_State* L, const void* p, size_t size, void* ud)
    {
        Array<char>& out = *(Array<char>*)ud;
        int at = out.size();
        out.resize(at + (int)size);
        memcpy(out.data() + at, p, size);
        return 0;
    }

    String m_filename;
    ByteArrayOutputStream* m_pStream;
    Array<int> m_chunkStarts;
    HashMap<String, int> m_stringHandles;
    Array<String> m_strings;
    int m_numStrings;
    HashMap<String, int> m_serialized; // tostring text -> index in m_serializedNames
    Array<String> m_serializedNames;
    int m_version;
};

// 0x00430010
static int commonMatrixIndex(const Matrix4x3& m)
{
    for (int i = 0; i < NumCommonMatrices; ++i)
    {
        const float* c = CommonMatrices[i];
        if (fabsf(m.x.x - c[0]) < CommonMatrixEpsilon &&
            fabsf(m.x.y - c[1]) < CommonMatrixEpsilon &&
            fabsf(m.x.z - c[2]) < CommonMatrixEpsilon &&
            fabsf(m.y.x - c[3]) < CommonMatrixEpsilon &&
            fabsf(m.y.y - c[4]) < CommonMatrixEpsilon &&
            fabsf(m.y.z - c[5]) < CommonMatrixEpsilon &&
            fabsf(m.z.x - c[6]) < CommonMatrixEpsilon &&
            fabsf(m.z.y - c[7]) < CommonMatrixEpsilon && fabsf(m.z.z - c[8]) < CommonMatrixEpsilon)
            return i;
    }
    return -1;
}
// the class tables of vec, mat, Sphere, Box, Plane and Ray are the metatables of their
// values; index is made absolute because the lookup pushes onto the stack
static bool hasMetatable(lua_State* L, int index, const char* name)
{
    if (index < 0)
        index = lua_gettop(L) + 1 + index;
    lua_getfield(L, LUA_GLOBALSINDEX, name);
    bool same = lua_rawequal(L, -1, index) == 1;
    lua_pop(L, 1);
    return same;
}

void NativeSaveGameOutputStream::writeValue(lua_State* L)
{
    ByteArrayOutputStream* out = m_pStream;
    switch (lua_type(L, -1))
    {
    case LUA_TNIL:
        out->writeByte((unsigned char)Tag_Nil);
        return;
    case LUA_TBOOLEAN:
        out->writeByte((unsigned char)(lua_toboolean(L, -1) ? Tag_True : Tag_False));
        return;
    case LUA_TNUMBER:
        writeNumber(luaL_checknumber(L, -1));
        return;
    case LUA_TSTRING:
    {
        size_t length = lua_objlen(L, -1);
        writeString(luaL_checklstring(L, -1, &length), (int)length);
        return;
    }
    case LUA_TFUNCTION:
    {
        out->writeByte((unsigned char)Tag_Function);
        String name;
        bool known;
        serializedName(L, -1, 'f', name, known);
        writeString(name.c_str(), name.size());
        if (known)
        {
            out->writeByte((unsigned char)Tag_Nil);
            return;
        }
        Array<char> dump;
        lua_dump(L, dumpWriter, &dump);
        writeString(dump.data(), dump.size());
        return;
    }
    case LUA_TTABLE:
        break;
    default:
        return;
    }
    // tables: plain ones are serialised member by member, the vec/mat/Sphere/Box/Plane/Ray
    // classes by value
    int index = lua_gettop(L);
    if (!lua_getmetatable(L, index))
    {
        out->writeByte((unsigned char)Tag_Table);
        String name;
        bool known;
        serializedName(L, index, 't', name, known);
        writeString(name.c_str(), name.size());
        if (!known)
        {
            lua_pushnil(L);
            while (lua_next(L, index))
            {
                lua_pushvalue(L, -2);
                writeValue(L);
                lua_pop(L, 1);
                writeValue(L);
                lua_pop(L, 1);
            }
        }
        out->writeByte((unsigned char)Tag_Nil);
        return;
    }
    // the metatable is on the stack
    if (hasMetatable(L, -1, "vec"))
    {
        Vec3 v = luax::checkVector3(L, index);
        if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
        {
            out->writeByte((unsigned char)Tag_ZeroVec);
        }
        else
        {
            out->writeByte((unsigned char)Tag_Vec);
            writeVec(v);
        }
    }
    else if (hasMetatable(L, -1, "mat"))
    {
        Matrix4x3 m = luax::checkMatrix4x3(L, index);
        if (m.x.x == 1.0f && m.x.y == 0.0f && m.x.z == 0.0f && m.y.x == 0.0f && m.y.y == 1.0f &&
            m.y.z == 0.0f && m.z.x == 0.0f && m.z.y == 0.0f && m.z.z == 1.0f && m.pos.x == 0.0f &&
            m.pos.y == 0.0f && m.pos.z == 0.0f)
        {
            out->writeByte((unsigned char)Tag_IdentityMat);
        }
        else
        {
            int common = commonMatrixIndex(m);
            if (common >= 0)
            {
                out->writeByte((unsigned char)(Tag_CommonMat + common));
            }
            else
            {
                out->writeByte((unsigned char)Tag_Mat);
                const float* f = &m.x.x;
                for (int i = 0; i < 9; ++i)
                    out->writeFloat(f[i]);
            }
            out->writeFloat(m.pos.x);
            out->writeFloat(m.pos.y);
            out->writeFloat(m.pos.z);
        }
    }
    else if (hasMetatable(L, -1, "Sphere"))
    {
        lua_getfield(L, index, "pos");
        Vec3 pos = luax::checkVector3(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, index, "radius");
        double radius = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        out->writeByte((unsigned char)Tag_Sphere);
        writeVec(pos);
        out->writeDouble(radius);
    }
    else if (hasMetatable(L, -1, "Box"))
    {
        lua_getfield(L, index, "pos");
        Vec3 pos = luax::checkVector3(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, index, "hsize");
        Vec3 hsize = luax::checkVector3(L, -1);
        lua_pop(L, 1);
        out->writeByte((unsigned char)Tag_Box);
        writeVec(pos);
        writeVec(hsize);
    }
    else if (hasMetatable(L, -1, "Plane"))
    {
        lua_getfield(L, index, "n");
        Vec3 n = luax::checkVector3(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, index, "d");
        double d = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        out->writeByte((unsigned char)Tag_Plane);
        writeVec(n);
        out->writeDouble(d);
    }
    else if (hasMetatable(L, -1, "Ray"))
    {
        lua_getfield(L, index, "pos");
        Vec3 pos = luax::checkVector3(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, index, "dir");
        Vec3 dir = luax::checkVector3(L, -1);
        lua_pop(L, 1);
        out->writeByte((unsigned char)Tag_Ray);
        writeVec(pos);
        writeVec(dir);
    }
    lua_pop(L, 1); // the metatable
}
} // namespace

LUAX_CLASS(NativeSaveGameOutputStream, "NativeSaveGameOutputStream")

// 0x00430ac0: NativeSaveGameOutputStream.create(version)
static int NativeSaveGameOutputStream_create(lua_State* L)
{
    int version = luaL_checkinteger(L, 1);
    luax::createSharedObject<NativeSaveGameOutputStream>(L,
                                                         new NativeSaveGameOutputStream(version));
    return 1;
}
// 0x00430b10
static int NativeSaveGameOutputStream_openFile(lua_State* L)
{
    NativeSaveGameOutputStream* stream = luax::checkObject<NativeSaveGameOutputStream>(L, 1);
    stream->openFile(luaL_checkstring(L, 2));
    return 0;
}
// 0x00430bd0
static int NativeSaveGameOutputStream_openBuffer(lua_State* L)
{
    luax::checkObject<NativeSaveGameOutputStream>(L, 1)->openBuffer();
    return 0;
}
// 0x00430c60
static int NativeSaveGameOutputStream_close(lua_State* L)
{
    try
    {
        luax::checkObject<NativeSaveGameOutputStream>(L, 1)->close();
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00430c90
static int NativeSaveGameOutputStream_getBuffer(lua_State* L)
{
    NativeSaveGameOutputStream* stream = luax::checkObject<NativeSaveGameOutputStream>(L, 1);
    int size = 0;
    const char* data = stream->getBuffer(size);
    if (!data)
        lua_pushnil(L);
    else
        lua_pushlstring(L, data, size);
    return 1;
}
static int checkStreamOpen(lua_State* L, NativeSaveGameOutputStream* stream)
{
    if (!stream->getStream())
        return luaL_error(L, "save game stream is not open");
    return 0;
}
// 0x00430cf0
static int NativeSaveGameOutputStream_openChunk(lua_State* L)
{
    NativeSaveGameOutputStream* stream = luax::checkObject<NativeSaveGameOutputStream>(L, 1);
    const char* id = luaL_checkstring(L, 2);
    if (lua_objlen(L, 2) != 4)
        luaL_argerror(L, 2, "invalid chunk id");
    checkStreamOpen(L, stream);
    stream->openChunk(id);
    return 0;
}
// 0x00430dd0
static int NativeSaveGameOutputStream_closeChunk(lua_State* L)
{
    NativeSaveGameOutputStream* stream = luax::checkObject<NativeSaveGameOutputStream>(L, 1);
    checkStreamOpen(L, stream);
    stream->closeChunk();
    return 0;
}
// 0x004314c0
static int NativeSaveGameOutputStream_writeValue(lua_State* L)
{
    NativeSaveGameOutputStream* stream = luax::checkObject<NativeSaveGameOutputStream>(L, 1);
    checkStreamOpen(L, stream);
    lua_settop(L, 2);
    stream->writeValue(L);
    return 0;
}

const luaL_Reg NativeSaveGameOutputStream_methods[] = {
    {"create", NativeSaveGameOutputStream_create},
    {"openFile", NativeSaveGameOutputStream_openFile},
    {"openBuffer", NativeSaveGameOutputStream_openBuffer},
    {"close", NativeSaveGameOutputStream_close},
    {"getBuffer", NativeSaveGameOutputStream_getBuffer},
    {"openChunk", NativeSaveGameOutputStream_openChunk},
    {"closeChunk", NativeSaveGameOutputStream_closeChunk},
    {"writeValue", NativeSaveGameOutputStream_writeValue},
    {0, 0}};

// 0x00431640
void savegame_mod(lua_State* L)
{
    luax::registerClass(L, "NativeSaveGameOutputStream", NativeSaveGameOutputStream_methods, 0);
}
