// Encryption and compression helpers, reconstructed from Utils.cpp (0x080ca020-0x080ca530).
#pragma once

namespace core
{

// XTEA, 32 rounds, 16 byte key, processes whole 8 byte blocks in place-compatible buffers.
void encryptXTEA(const char* src, char* dst, int length, const char* key);
void decryptXTEA(const char* src, char* dst, int length, const char* key);
// ARC4 with the first 768 keystream bytes discarded.
void encryptARC4(const char* src, char* dst, int length, const char* key, int keyLength);
void decryptARC4(const char* src, char* dst, int length, const char* key, int keyLength);
// zlib with a leading little-endian uncompressed size. Both return new[]'ed buffers.
char* uncompress(const char* src, int srcLength, int& length);
char* compress(const char* src, int srcLength, int& length);
#if GRIMROCK_GAME >= 2
// Grimrock 2 (0x004513a0 / 0x00451430): plain zlib streams, the caller keeps the size.
char* compress(const char* src, int srcLength, int& length, int level);
void uncompress(const char* src, int srcLength, char* dst, int dstLength);
#endif

} // namespace core
