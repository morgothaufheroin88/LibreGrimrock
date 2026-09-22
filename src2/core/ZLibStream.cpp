// Reconstructed from grimrock2.exe ZLibDecompressorInputStream.
#include "core/ZLibStream.h"
#include "core/Exception.h"
#include <cstring>

namespace core
{

// 0x0049e000
ZLibDecompressorInputStream::ZLibDecompressorInputStream(InputStream* host)
    : m_pHost(host), m_position(0), m_done(false)
{
    memset(&m_stream, 0, sizeof(m_stream));
    int result = inflateInit(&m_stream);
    if (result != Z_OK)
        throw Exception("inflateInit failed (%d)", result);
}
ZLibDecompressorInputStream::~ZLibDecompressorInputStream()
{
    inflateEnd(&m_stream);
}
// The inflated size is unknown until the end: what is left is at least one byte until
// the stream ends (the save game reader only checks for zero).
int ZLibDecompressorInputStream::availableBytes()
{
    if (m_done)
        return 0;
    if (m_stream.avail_in > 0 || m_pHost->availableBytes() > 0)
        return 1;
    return 0;
}
// 0x0049e0a0: inflates straight into the caller's buffer, refilling the input buffer
// from the host stream as needed.
void ZLibDecompressorInputStream::readBytes(void* buffer, int size)
{
    m_stream.next_out = (Bytef*)buffer;
    m_stream.avail_out = (uInt)size;
    while (m_stream.avail_out > 0)
    {
        if (m_stream.avail_in == 0)
        {
            int available = m_pHost->availableBytes();
            if (available <= 0)
                throw Exception("Read failed on file %s: unexpected end of compressed data",
                                m_pHost->getFilename());
            int count = available < BufferSize ? available : BufferSize;
            m_pHost->readBytes(m_buffer, count);
            m_stream.next_in = m_buffer;
            m_stream.avail_in = (uInt)count;
        }
        int result = inflate(&m_stream, Z_NO_FLUSH);
        if (result == Z_STREAM_END)
        {
            m_done = true;
            if (m_stream.avail_out > 0)
                throw Exception("Read failed on file %s: compressed data ended early",
                                m_pHost->getFilename());
            break;
        }
        if (result != Z_OK && result != Z_BUF_ERROR)
            throw Exception("Read failed on file %s: inflate error %d", m_pHost->getFilename(),
                            result);
    }
    m_position += size;
}
void ZLibDecompressorInputStream::skip(int count)
{
    unsigned char scratch[1024];
    while (count > 0)
    {
        int chunk = count < (int)sizeof(scratch) ? count : (int)sizeof(scratch);
        readBytes(scratch, chunk);
        count -= chunk;
    }
}
int ZLibDecompressorInputStream::getPosition() const
{
    return m_position;
}
void ZLibDecompressorInputStream::seek(int pos)
{
    if (pos < m_position)
        throw Exception("cannot seek backwards in a compressed stream");
    skip(pos - m_position);
}
const char* ZLibDecompressorInputStream::getFilename() const
{
    return m_pHost->getFilename();
}
void ZLibDecompressorInputStream::readByte(unsigned char& v)
{
    readBytes(&v, 1);
}
void ZLibDecompressorInputStream::readShort(unsigned short& v)
{
    readBytes(&v, 2);
}
void ZLibDecompressorInputStream::readInt(unsigned int& v)
{
    readBytes(&v, 4);
}
void ZLibDecompressorInputStream::readFloat(float& v)
{
    readBytes(&v, 4);
}
void ZLibDecompressorInputStream::readDouble(double& v)
{
    readBytes(&v, 8);
}
void ZLibDecompressorInputStream::readBool(bool& v)
{
    unsigned char b;
    readBytes(&b, 1);
    v = b != 0;
}

} // namespace core
