// Reconstructed from Grimrock.bin.x86 Utils.cpp.
#include "core/Utils.h"
#include "core/Exception.h"
#include "core/Stream.h"
#include <cstring>
#include <zlib.h>

namespace core
{

// XTEA with 32 rounds (the game archive cipher).
constexpr unsigned int XteaDelta = 0x9e3779b9;
constexpr unsigned int XteaRounds = 32;
constexpr unsigned int XteaFinalSum = XteaDelta * XteaRounds; // 0xc6ef3720
constexpr int XteaBlockSize = 8;

// 0x080ca020
void encryptXTEA(const char* src, char* dst, int length, const char* key)
{
    const unsigned int* keyWords = (const unsigned int*)key;
    int blocks = length / XteaBlockSize;
    for (int block = 0; block < blocks; ++block)
    {
        unsigned int v0, v1, sum = 0;
        memcpy(&v0, src + block * XteaBlockSize, 4);
        memcpy(&v1, src + block * XteaBlockSize + 4, 4);
        do
        {
            v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + keyWords[sum & 3]);
            sum += XteaDelta;
            v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + keyWords[(sum >> 11) & 3]);
        } while (sum != XteaFinalSum);
        memcpy(dst + block * XteaBlockSize, &v0, 4);
        memcpy(dst + block * XteaBlockSize + 4, &v1, 4);
    }
}
// 0x080ca0e0
void decryptXTEA(const char* src, char* dst, int length, const char* key)
{
    const unsigned int* keyWords = (const unsigned int*)key;
    int blocks = length / XteaBlockSize;
    for (int block = 0; block < blocks; ++block)
    {
        unsigned int v0, v1, sum = XteaFinalSum;
        memcpy(&v0, src + block * XteaBlockSize, 4);
        memcpy(&v1, src + block * XteaBlockSize + 4, 4);
        do
        {
            v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + keyWords[(sum >> 11) & 3]);
            sum -= XteaDelta;
            v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + keyWords[sum & 3]);
        } while (sum != 0);
        memcpy(dst + block * XteaBlockSize, &v0, 4);
        memcpy(dst + block * XteaBlockSize + 4, &v1, 4);
    }
}

// RC4 with the first 768 keystream bytes discarded (RC4-drop[768]).
constexpr int Arc4StateSize = 256;
constexpr int Arc4DropBytes = 0x300;

static void arc4(const char* src, char* dst, int length, const char* key, int keyLength)
{
    unsigned char state[Arc4StateSize];
    for (int i = 0; i < Arc4StateSize; ++i)
        state[i] = (unsigned char)i;
    unsigned int j = 0;
    for (int i = 0; i < Arc4StateSize; ++i)
    {
        j = ((unsigned char)key[i % keyLength] + j + state[i]) & 0xff;
        unsigned char swapped = state[j];
        state[j] = state[i];
        state[i] = swapped;
    }
    unsigned int x = 0, y = 0;
    for (int n = 0; n < Arc4DropBytes; ++n)
    {
        x = (x + 1) & 0xff;
        y = (state[x] + y) & 0xff;
        unsigned char swapped = state[y];
        state[y] = state[x];
        state[x] = swapped;
    }
    for (int i = 0; i < length; ++i)
    {
        x = (x + 1) & 0xff;
        y = (state[x] + y) & 0xff;
        unsigned char swapped = state[y];
        state[y] = state[x];
        state[x] = swapped;
        dst[i] = (char)(state[(unsigned char)(swapped + state[y])] ^ (unsigned char)src[i]);
    }
}
// 0x080ca190
void encryptARC4(const char* src, char* dst, int length, const char* key, int keyLength)
{
    arc4(src, dst, length, key, keyLength);
}
// 0x080ca530
void decryptARC4(const char* src, char* dst, int length, const char* key, int keyLength)
{
    arc4(src, dst, length, key, keyLength);
}

// 0x080ca2f0
char* uncompress(const char* src, int srcLength, int& length)
{
    unsigned int size;
    memcpy(&size, src, 4);
    length = (int)size;
    char* buffer = new char[size];
    uLongf destLen = size;
    int result = ::uncompress((Bytef*)buffer, &destLen, (const Bytef*)src + 4, srcLength - 4);
    switch (result)
    {
    case Z_OK:
        return buffer;
    case Z_MEM_ERROR:
        throw Exception("out of memory (Z_MEM_ERROR)");
    case Z_BUF_ERROR:
        throw Exception("not enough room in the output buffer (Z_BUF_ERROR)");
    case Z_DATA_ERROR:
        throw Exception("corrupted or incomplete data (Z_DATA_ERROR)");
    default:
        throw Exception("unspecified error in uncompress()");
    }
}
// 0x080ca430
char* compress(const char* src, int srcLength, int& length)
{
    uLongf bound = compressBound(srcLength);
    char* buffer = new char[bound + 4];
    unsigned int size = (unsigned int)srcLength;
    memcpy(buffer, &size, 4);
    int result = ::compress((Bytef*)buffer + 4, &bound, (const Bytef*)src, srcLength);
    switch (result)
    {
    case Z_OK:
        length = (int)bound + 4;
        return buffer;
    case Z_MEM_ERROR:
        throw Exception("out of memory (Z_MEM_ERROR)");
    case Z_BUF_ERROR:
        throw Exception("not enough room in the output buffer (Z_BUF_ERROR)");
    default:
        throw Exception("unspecified error in compress()");
    }
}

#if GRIMROCK_GAME >= 2
// 0x004513a0
char* compress(const char* src, int srcLength, int& length, int level)
{
    uLongf bound = compressBound(srcLength);
    char* buffer = new char[bound];
    int result = ::compress2((Bytef*)buffer, &bound, (const Bytef*)src, srcLength, level);
    switch (result)
    {
    case Z_OK:
        length = (int)bound;
        return buffer;
    case Z_MEM_ERROR:
        throw Exception("out of memory (Z_MEM_ERROR)");
    case Z_BUF_ERROR:
        throw Exception("not enough room in the output buffer (Z_BUF_ERROR)");
    default:
        throw Exception("unspecified error in compress()");
    }
}
// 0x00451430
void uncompress(const char* src, int srcLength, char* dst, int dstLength)
{
    uLongf destLen = dstLength;
    int result = ::uncompress((Bytef*)dst, &destLen, (const Bytef*)src, srcLength);
    switch (result)
    {
    case Z_OK:
        return;
    case Z_MEM_ERROR:
        throw Exception("out of memory (Z_MEM_ERROR)");
    case Z_BUF_ERROR:
        throw Exception("not enough room in the output buffer (Z_BUF_ERROR)");
    case Z_DATA_ERROR:
        throw Exception("corrupted or incomplete data (Z_DATA_ERROR)");
    default:
        throw Exception("unspecified error in uncompress()");
    }
}
#endif

} // namespace core

namespace core
{
// 0x080ca2c0 global constructor in Utils.cpp
Endian g_platformEndianness = Endian_Little;
} // namespace core
