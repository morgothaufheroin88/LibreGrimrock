// Reconstructed from Grimrock.bin.x86 ByteArrayStream.cpp.
#include "core/ByteArrayStream.h"
#include "core/Exception.h"
#include <cstring>

namespace core
{

// ---- InputStream / OutputStream shared helpers -------------------------------------

// 0x080b5490
void InputStream::readString(String& s)
{
    int length;
    readInt(length);
    char* buffer = new char[length + 1];
    readBytes(buffer, length);
    buffer[length] = 0;
    s = buffer;
    delete[] buffer;
}
void InputStream::readVector2(Vec2& v)
{
    readFloat(v.x);
    readFloat(v.y);
}
void InputStream::readVector3(Vec3& v)
{
    readFloat(v.x);
    readFloat(v.y);
    readFloat(v.z);
}
void InputStream::readVector4(Vec4& v)
{
    readFloat(v.x);
    readFloat(v.y);
    readFloat(v.z);
    readFloat(v.w);
}
void InputStream::readMatrix3x3(Matrix3x3& m)
{
    float* f = m.data();
    for (int i = 0; i < 9; ++i)
        readFloat(f[i]);
}
void InputStream::readMatrix4x3(Matrix4x3& m)
{
    float* f = m.data();
    for (int i = 0; i < 12; ++i)
        readFloat(f[i]);
}
void InputStream::readMatrix4x4(Matrix4x4& m)
{
    for (int i = 0; i < 16; ++i)
        readFloat(m.m[i]);
}
void InputStream::readQuaternion(Quat& q)
{
    readFloat(q.x);
    readFloat(q.y);
    readFloat(q.z);
    readFloat(q.w);
}

// 0x080b5440
void OutputStream::writeString(const char* s)
{
    int length = (int)strlen(s);
    writeInt(length);
    writeBytes(s, length);
}
// 0x080b51d0
void OutputStream::writeString(const String& s)
{
    writeInt(s.size());
    writeBytes(s.c_str(), s.size());
}
void OutputStream::writeVector2(const Vec2& v)
{
    writeFloat(v.x);
    writeFloat(v.y);
}
void OutputStream::writeVector3(const Vec3& v)
{
    writeFloat(v.x);
    writeFloat(v.y);
    writeFloat(v.z);
}
void OutputStream::writeVector4(const Vec4& v)
{
    writeFloat(v.x);
    writeFloat(v.y);
    writeFloat(v.z);
    writeFloat(v.w);
}
void OutputStream::writeMatrix3x3(const Matrix3x3& m)
{
    const float* f = m.data();
    for (int i = 0; i < 9; ++i)
        writeFloat(f[i]);
}
void OutputStream::writeMatrix4x3(const Matrix4x3& m)
{
    const float* f = m.data();
    for (int i = 0; i < 12; ++i)
        writeFloat(f[i]);
}
void OutputStream::writeMatrix4x4(const Matrix4x4& m)
{
    for (int i = 0; i < 16; ++i)
        writeFloat(m.m[i]);
}
void OutputStream::writeQuaternion(const Quat& q)
{
    writeFloat(q.x);
    writeFloat(q.y);
    writeFloat(q.z);
    writeFloat(q.w);
}

// ---- ByteArrayInputStream ------------------------------------------------------------

// 0x080badf0
ByteArrayInputStream::ByteArrayInputStream(const char* data, int length, bool copy, Endian endian)
{
    if (endian == Endian_Platform)
        endian = g_platformEndianness;
    const char* source = data;
    if (copy)
    {
        char* buffer = new char[length];
        memcpy(buffer, data, length);
        source = buffer;
    }
    m_pData = source;
    m_pPos = source;
    m_length = length;
    m_swapBytes = g_platformEndianness != endian;
    m_ownsData = copy;
}
// 0x080bad80
ByteArrayInputStream::~ByteArrayInputStream()
{
    if (m_ownsData && m_pData)
        delete[] m_pData;
}
// 0x080ba8b0
int ByteArrayInputStream::availableBytes()
{
    return m_length - (int)(m_pPos - m_pData);
}
// 0x080bb0f0
void ByteArrayInputStream::readBytes(void* buffer, int size)
{
    if (size >= 0 && size <= availableBytes())
    {
        memcpy(buffer, m_pPos, size);
        m_pPos += size;
        return;
    }
    throw Exception("not enough bytes to read");
}
// 0x080ba8d0
void ByteArrayInputStream::skip(int count)
{
    seek(getPosition() + count);
}
// 0x080ba910
int ByteArrayInputStream::getPosition() const
{
    return (int)(m_pPos - m_pData);
}
// 0x080bb070
void ByteArrayInputStream::seek(int pos)
{
    if (pos >= 0 && pos <= m_length)
    {
        m_pPos = m_pData + pos;
        return;
    }
    throw Exception("Seek out of range");
}
// 0x080ba920
const char* ByteArrayInputStream::getFilename() const
{
    return "";
}
// 0x080ba930
void ByteArrayInputStream::readByte(unsigned char& v)
{
    readBytes(&v, 1);
}
// 0x080ba960
void ByteArrayInputStream::readShort(unsigned short& v)
{
    readBytes(&v, 2);
    if (m_swapBytes)
        v = swapBytes(v);
}
// 0x080ba9b0
void ByteArrayInputStream::readInt(unsigned int& v)
{
    readBytes(&v, 4);
    if (m_swapBytes)
        v = swapBytes(v);
}
// 0x080baa10
void ByteArrayInputStream::readFloat(float& v)
{
    unsigned int bits;
    readBytes(&bits, 4);
    if (m_swapBytes)
        bits = swapBytes(bits);
    memcpy(&v, &bits, 4);
}
// 0x080baa70
void ByteArrayInputStream::readDouble(double& v)
{
    unsigned char b[8];
    readBytes(b, 8);
    if (m_swapBytes)
    {
        for (int i = 0; i < 4; ++i)
        {
            unsigned char swapped = b[i];
            b[i] = b[7 - i];
            b[7 - i] = swapped;
        }
    }
    memcpy(&v, b, 8);
}
// 0x080bab00
void ByteArrayInputStream::readBool(bool& v)
{
    char byte;
    readBytes(&byte, 1);
    v = byte != 0;
}

// ---- ByteArrayOutputStream -----------------------------------------------------------

// 0x080bab30
ByteArrayOutputStream::ByteArrayOutputStream(Endian endian) : m_pos(0)
{
    if (endian == Endian_Platform)
        endian = g_platformEndianness;
    m_swapBytes = g_platformEndianness != endian;
}
// 0x080bafc0
ByteArrayOutputStream::~ByteArrayOutputStream() {}
// 0x080bab70
int ByteArrayOutputStream::getPosition() const
{
    return m_pos;
}
// 0x080bb000
void ByteArrayOutputStream::seek(int pos)
{
    if (pos >= 0 && pos <= m_data.size())
    {
        m_pos = pos;
        return;
    }
    throw Exception("Seek out of range");
}
// 0x080bab80
const char* ByteArrayOutputStream::getFilename() const
{
    return "";
}
// 0x080bae90
void ByteArrayOutputStream::writeBytes(const void* buffer, int size)
{
    if (m_pos + size > m_data.size())
        m_data.resize(m_pos + size);
    if (size > 0)
        memcpy(m_data.data() + m_pos, buffer, size);
    m_pos += size;
}
// 0x080bab90
void ByteArrayOutputStream::writeByte(unsigned char v)
{
    writeBytes(&v, 1);
}
// 0x080babc0
void ByteArrayOutputStream::writeShort(unsigned short v)
{
    if (m_swapBytes)
        v = swapBytes(v);
    writeBytes(&v, 2);
}
// 0x080bac00
void ByteArrayOutputStream::writeInt(unsigned int v)
{
    if (m_swapBytes)
        v = swapBytes(v);
    writeBytes(&v, 4);
}
// 0x080bac60
void ByteArrayOutputStream::writeFloat(float v)
{
    unsigned int bits;
    memcpy(&bits, &v, 4);
    if (m_swapBytes)
        bits = swapBytes(bits);
    writeBytes(&bits, 4);
}
// 0x080bacc0
void ByteArrayOutputStream::writeDouble(double v)
{
    unsigned char b[8];
    memcpy(b, &v, 8);
    if (m_swapBytes)
    {
        for (int i = 0; i < 4; ++i)
        {
            unsigned char swapped = b[i];
            b[i] = b[7 - i];
            b[7 - i] = swapped;
        }
    }
    writeBytes(b, 8);
}
// 0x080bad50
void ByteArrayOutputStream::writeBool(bool v)
{
    char byte = v ? 1 : 0;
    writeBytes(&byte, 1);
}

} // namespace core
