// Contract tests for the core foundation (String/Array/HashMap/SharedPtr/FileSystem/Archive).
#include "core/ArchiveFileSystem.h"
#include "core/FileSystem.h"
#include "core/HashMap.h"
#include "core/Image.h"
#include "core/Matrix.h"
#include "core/Noise.h"
#include "core/Prim.h"
#include "core/SharedPtr.h"
#include "core/String.h"
#include "core/Utils.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int failures = 0;
#define REQUIRE(c)                                                                                 \
    do                                                                                             \
    {                                                                                              \
        if (!(c))                                                                                  \
        {                                                                                          \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                    \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

struct Counted
{
    static int alive;
    Counted()
    {
        ++alive;
    }
    virtual ~Counted()
    {
        --alive;
    }
};
int Counted::alive = 0;

static void testString()
{
    core::String s("hello");
    REQUIRE(s.size() == 5 && strcmp(s.c_str(), "hello") == 0);
    s.append(" world");
    REQUIRE(s == "hello world");
    REQUIRE(s.find("world") == 6 && s.find("zzz") == -1);
    REQUIRE(s.find_last("o") == 7 && s.count("l") == 3);
    REQUIRE(s.substr(6, 8) == "wor");
    REQUIRE(s.substr(6) == "world");
    s.replace("world", "there");
    REQUIRE(s == "hello there");
    s.replace("l", "L");
    REQUIRE(s == "heLLo there");
    s.erase(0, 5); // inclusive range, as in the original
    REQUIRE(s == "there");
    s.erase(4, 4); // erasing the last character must not read past the terminator
    REQUIRE(s == "ther" && s.size() == 4);
    {
        core::String shader("#version 120\n#define X\n");
        shader.replace("#version 120\n", "");
        REQUIRE(shader == "#define X\n");
    }
    s = "there";
    s.insert(0, "hi ");
    REQUIRE(s == "hi there");
    s.toupper();
    REQUIRE(s == "HI THERE" && s.startsWith("HI"));
    core::String e;
    REQUIRE(e.size() == 0 && strcmp(e.c_str(), "") == 0);
    core::Array<core::String> parts = core::split("a,b,,c", ",");
    REQUIRE(parts.size() == 4 && parts[0] == "a" && parts[2] == "" && parts[3] == "c");
    REQUIRE(core::formatString("%d-%s", 42, "x") == "42-x");
    REQUIRE(core::hashString("") == 0x811c9dc5u && core::hashString("hello") == 0x4f9f2cabu);
}

static void testArray()
{
    core::Array<int> a;
    for (int i = 0; i < 100; ++i)
        a.push_back(i);
    REQUIRE(a.size() == 100 && a[99] == 99 && a.capacity() >= 100);
    a.erase(0);
    REQUIRE(a.size() == 99 && a[0] == 1);
    a.insert(0, 7);
    REQUIRE(a[0] == 7 && a[1] == 1);
    a.resize(3);
    REQUIRE(a.size() == 3);
    core::Array<core::String> s;
    s.push_back(core::String("x"));
    s.push_back(core::String("y"));
    core::Array<core::String> t(s);
    REQUIRE(t.size() == 2 && t[1] == "y");
}

static void testHashMap()
{
    core::HashMap<core::String, int> m;
    m.insert(core::String("one"), 1);
    m.insert(core::String("two"), 2);
    m.insert(core::String("one"), 11);
    REQUIRE(m.size() == 2);
    REQUIRE(*m.findValue(core::String("one")) == 11);
    REQUIRE(m.findValue(core::String("three")) == 0);
    REQUIRE(m.remove(core::String("two")) && m.size() == 1);
    int n = 0;
    for (core::HashMap<core::String, int>::iterator it = m.begin(); it != m.end(); ++it)
        ++n;
    REQUIRE(n == 1);
    core::HashMap<void*, int> pm;
    pm.insert(&n, 5);
    REQUIRE(*pm.findValue(&n) == 5);
}

static void testSharedPtr()
{
    {
        core::SharedPtr<Counted> a(new Counted);
        REQUIRE(Counted::alive == 1 && a.useCount() == 1);
        core::SharedPtr<Counted> b = a;
        REQUIRE(a.useCount() == 2);
        b.reset();
        REQUIRE(a.useCount() == 1 && Counted::alive == 1);
        Counted* raw = a.get();
        core::SharedPtr<Counted> c(raw); // second owner found through the global map
        REQUIRE(a.useCount() == 2 && c.useCount() == 2);
        a.reset(new Counted);
        REQUIRE(Counted::alive == 2 && c.useCount() == 1);
    }
    REQUIRE(Counted::alive == 0);
    REQUIRE(core::SharedPtrBase::objectCount() == 0);
}

static void testMath()
{
    core::Matrix4x3 m;
    m.makeRotation(0.3f, 0.7f, -0.2f, core::Matrix3x3::XYZ);
    m.pos.set(1, 2, 3);
    core::Matrix4x3 inv = m;
    inv.invertOrthonormal();
    core::Vec3 p(4, 5, 6);
    core::Vec3 q = inv.transformPoint(m.transformPoint(p));
    REQUIRE(std::fabs(q.x - 4) < 1e-4f && std::fabs(q.y - 5) < 1e-4f && std::fabs(q.z - 6) < 1e-4f);
    core::Matrix4x3 inv2 = m;
    inv2.invert();
    q = inv2.transformPoint(m.transformPoint(p));
    REQUIRE(std::fabs(q.x - 4) < 1e-3f && std::fabs(q.z - 6) < 1e-3f);
    core::Matrix4x4 m4(m);
    core::Matrix4x4 m4i = m4;
    m4i.invert();
    core::Vec4 r = m4i.transform(m4.transform(core::Vec4(1, 2, 3, 1)));
    REQUIRE(std::fabs(r.x - 1) < 1e-3f && std::fabs(r.y - 2) < 1e-3f);
    core::AABox3 box(core::Vec3(-1, -1, -1), core::Vec3(1, 1, 1));
    float t;
    REQUIRE(core::intersectRayBox(core::Ray3(core::Vec3(0, 0, -5), core::Vec3(0, 0, 1)), box, t));
    REQUIRE(std::fabs(t - 4) < 1e-5f);
    REQUIRE(!core::testRayBox(core::Ray3(core::Vec3(0, 5, -5), core::Vec3(0, 0, 1)), box));
    REQUIRE(core::noise(0.5f, 0.5f, 0.5f) > -1.0f && core::noise(0.5f, 0.5f, 0.5f) < 1.0f);
}

static void testUtils()
{
    const char key[5] = {1, 2, 3, 4, 5};
    char zero[16] = {0};
    char out[16];
    core::decryptARC4(zero, out, 16, key, 5);
    const unsigned char expected[16] = {0xeb, 0x62, 0x63, 0x8d, 0x4f, 0x0b, 0xa1, 0xfe,
                                        0x9f, 0xca, 0x20, 0xe0, 0x5b, 0xf8, 0xff, 0x2b};
    REQUIRE(memcmp(out, expected, 16) == 0); // RFC 6229 keystream at offset 768
    const char plain[] = "Grimrock!Grimrock!";
    char xkey[16];
    for (int i = 0; i < 16; ++i)
        xkey[i] = (char)i;
    char enc[24], dec[24];
    memset(enc, 0, sizeof(enc));
    memset(dec, 0, sizeof(dec));
    core::encryptXTEA(plain, enc, 16, xkey);
    core::decryptXTEA(enc, dec, 16, xkey);
    REQUIRE(memcmp(dec, plain, 16) == 0 && memcmp(enc, plain, 16) != 0);
    int clen = 0;
    char* c = core::compress(plain, (int)sizeof(plain), clen);
    int ulen = 0;
    char* u = core::uncompress(c, clen, ulen);
    REQUIRE(ulen == (int)sizeof(plain) && memcmp(u, plain, ulen) == 0);
    delete[] c;
    delete[] u;
}

// 0x080cb680 clears new images; the font atlas relies on it for its padding.
static void testImage()
{
    for (int i = 0; i < 8; ++i)
    {
        core::Image image(64, 32);
        bool clear = true;
        for (int y = 0; y < 32 && clear; ++y)
            for (int x = 0; x < 64; ++x)
            {
                core::Color c = image.getPixel(x, y);
                if (c.r || c.g || c.b || c.a)
                {
                    clear = false;
                    break;
                }
            }
        REQUIRE(clear);
        image.setPixel(3, 4, core::Color(1, 2, 3, 4));
        core::Image copy(image);
        core::Color c = copy.getPixel(3, 4);
        REQUIRE(c.r == 1 && c.g == 2 && c.b == 3 && c.a == 4);
    }
}

static void testArchive()
{
    // GRIMROCK_DAT points at the game archive; the test is skipped without it
    const char* path = getenv("GRIMROCK_DAT");
    if (!path)
        path = "grimrock.dat";
    if (!core::sysFileExists(path))
    {
        printf("skipping archive test, %s missing\n", path);
        return;
    }
    core::ArchiveFileSystem archive(path);
    static const char key[16] = {
        0x4b, (char)0x8a, 0x11,       0x56,       (char)0xfd, 0x42,       (char)0xce, (char)0xf3,
        0x00, (char)0xd7, (char)0xa2, (char)0xdf, (char)0xef, (char)0xd4, (char)0xcc, (char)0xf7};
    archive.setEncryptionKey(key, 16);
    REQUIRE(archive.items().size() == 1867);
    REQUIRE(archive.fileExists("init.lua"));
    core::mount(archive);
    core::File* f = core::openRead("init.lua");
    REQUIRE(f != 0);
    int len = f->getFileLength();
    REQUIRE(len > 0);
    char* data = new char[len];
    f->read(data, len);
    REQUIRE(memcmp(data, "\x1bLJ", 3) == 0); // LuaJIT bytecode
    delete[] data;
    delete f;
    int rlen = 0;
    char* r = core::readFile("AI.lua", rlen);
    REQUIRE(rlen > 0 && memcmp(r, "\x1bLJ", 3) == 0);
    delete[] r;
    core::unmount(archive);
    REQUIRE(!core::fileExists("init.lua"));
}

int main()
{
    testString();
    testArray();
    testHashMap();
    testSharedPtr();
    testMath();
    testUtils();
    testImage();
    try
    {
        testArchive();
    }
    catch (core::Exception& e)
    {
        printf("exception: %s\n", e.getReason());
        ++failures;
    }
    printf("%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
