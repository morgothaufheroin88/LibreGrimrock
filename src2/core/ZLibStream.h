// Inflating input stream of Legend of Grimrock 2, reconstructed from grimrock2.exe
// ZLibDecompressorInputStream (0x0049e000-0x0049e5a0, 0x1001c bytes: a z_stream and a
// 64 KB input buffer fed from the host stream).
#pragma once
#include "core/Array.h"
#include "core/Stream.h"
#include <zlib.h>

namespace core
{

class ZLibDecompressorInputStream : public InputStream
{
  public:
    static constexpr int BufferSize = 0x10000;
    // 0x0049e000: the host stream is not owned
    explicit ZLibDecompressorInputStream(InputStream* host);
    ~ZLibDecompressorInputStream();
    int availableBytes();
    void readBytes(void* buffer, int size);
    void skip(int count);
    int getPosition() const;
    void seek(int pos);
    const char* getFilename() const;
    using InputStream::readByte;
    using InputStream::readInt;
    using InputStream::readShort;
    void readByte(unsigned char& v);
    void readShort(unsigned short& v);
    void readInt(unsigned int& v);
    void readFloat(float& v);
    void readDouble(double& v);
    void readBool(bool& v);

  private:
    InputStream* m_pHost;
    z_stream m_stream;
    unsigned char m_buffer[BufferSize];
    int m_position; // uncompressed bytes handed out so far
    bool m_done;
};

} // namespace core
