// Reconstructed from Grimrock.bin.x86 FileStream.cpp.
#include "core/FileStream.h"
#include <cstring>

namespace core
{

// 0x080c9ec0
FileInputStream::FileInputStream(const char* filename, Endian endian) : m_filename(filename)
{
    if (endian == Endian_Platform)
        endian = g_platformEndianness;
    m_pFile = openRead(filename);
    m_swapBytes = g_platformEndianness != endian;
}
// 0x080c9dc0
FileInputStream::~FileInputStream()
{
    delete m_pFile;
}
// 0x080c97e0
int FileInputStream::availableBytes()
{
    return m_pFile->getFileLength() - m_pFile->getFilePosition();
}
// 0x080c9820
void FileInputStream::readBytes(void* buffer, int size)
{
    m_pFile->read(buffer, size);
}
// 0x080c9840
void FileInputStream::skip(int count)
{
    m_pFile->skip(count);
}
// 0x080c9860
int FileInputStream::getPosition() const
{
    return m_pFile->getFilePosition();
}
// 0x080c9880
void FileInputStream::seek(int pos)
{
    m_pFile->seek(pos);
}
// 0x080c98a0
const char* FileInputStream::getFilename() const
{
    return m_filename.c_str();
}
// 0x080c98c0
void FileInputStream::readByte(unsigned char& v)
{
    readBytes(&v, 1);
}
// 0x080c98f0
void FileInputStream::readShort(unsigned short& v)
{
    readBytes(&v, 2);
    if (m_swapBytes)
        v = swapBytes(v);
}
// 0x080c9940
void FileInputStream::readInt(unsigned int& v)
{
    readBytes(&v, 4);
    if (m_swapBytes)
        v = swapBytes(v);
}
// 0x080c99a0
void FileInputStream::readFloat(float& v)
{
    unsigned int bits;
    readBytes(&bits, 4);
    if (m_swapBytes)
        bits = swapBytes(bits);
    memcpy(&v, &bits, 4);
}
// 0x080c9a00
void FileInputStream::readDouble(double& v)
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
// 0x080c9a90
void FileInputStream::readBool(bool& v)
{
    char byte;
    readBytes(&byte, 1);
    v = byte != 0;
}

// 0x080c9e20
FileOutputStream::FileOutputStream(const char* filename, Endian endian) : m_filename(filename)
{
    if (endian == Endian_Platform)
        endian = g_platformEndianness;
    m_pFile = openWrite(filename);
    m_swapBytes = g_platformEndianness != endian;
}
// 0x080c9d60
FileOutputStream::~FileOutputStream()
{
    delete m_pFile;
}
// 0x080c9ac0
int FileOutputStream::getPosition() const
{
    return m_pFile->getFilePosition();
}
// 0x080c9ae0
void FileOutputStream::seek(int pos)
{
    m_pFile->seek(pos);
}
// 0x080c9b00
const char* FileOutputStream::getFilename() const
{
    return m_filename.c_str();
}
// 0x080c9b20
void FileOutputStream::writeBytes(const void* buffer, int size)
{
    m_pFile->write(buffer, size);
}
// 0x080c9b40
void FileOutputStream::writeByte(unsigned char v)
{
    writeBytes(&v, 1);
}
// 0x080c9b70
void FileOutputStream::writeShort(unsigned short v)
{
    if (m_swapBytes)
        v = swapBytes(v);
    writeBytes(&v, 2);
}
// 0x080c9bb0
void FileOutputStream::writeInt(unsigned int v)
{
    if (m_swapBytes)
        v = swapBytes(v);
    writeBytes(&v, 4);
}
// 0x080c9c10
void FileOutputStream::writeFloat(float v)
{
    unsigned int bits;
    memcpy(&bits, &v, 4);
    if (m_swapBytes)
        bits = swapBytes(bits);
    writeBytes(&bits, 4);
}
// 0x080c9c70
void FileOutputStream::writeDouble(double v)
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
// 0x080c9d00
void FileOutputStream::writeBool(bool v)
{
    char byte = v ? 1 : 0;
    writeBytes(&byte, 1);
}

} // namespace core
