// Memory backed streams, reconstructed from ByteArrayStream.cpp (0x080ba8b0-0x080bb1c0).
#pragma once
#include "core/Array.h"
#include "core/Stream.h"

namespace core
{

class ByteArrayInputStream : public InputStream
{
  public:
    // 0x080badf0: copy = true duplicates the buffer, otherwise it must outlive the stream.
    ByteArrayInputStream(const char* data, int length, bool copy = false,
                         Endian endian = Endian_Platform);
    ~ByteArrayInputStream();
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
    const char* data() const
    {
        return m_pData;
    }

  private:
    const char* m_pData;
    const char* m_pPos;
    int m_length;
    bool m_swapBytes;
    bool m_ownsData;
};

class ByteArrayOutputStream : public OutputStream
{
  public:
    explicit ByteArrayOutputStream(Endian endian = Endian_Platform);
    ~ByteArrayOutputStream();
    int getPosition() const;
    void seek(int pos);
    const char* getFilename() const;
    void writeBytes(const void* buffer, int size);
    using OutputStream::writeByte;
    using OutputStream::writeInt;
    using OutputStream::writeShort;
    void writeByte(unsigned char v);
    void writeShort(unsigned short v);
    void writeInt(unsigned int v);
    void writeFloat(float v);
    void writeDouble(double v);
    void writeBool(bool v);
    const char* data() const
    {
        return m_data.data();
    }
    int size() const
    {
        return m_data.size();
    }

  private:
    Array<char> m_data;
    int m_pos;
    bool m_swapBytes;
};

} // namespace core
