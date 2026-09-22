// Reconstructed from Grimrock.bin.x86 TextFile.cpp.
#include "core/TextFile.h"
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace core
{

// 0x080bb600
TextFileReader::TextFileReader(const char* filename)
{
    m_pFile = openRead(filename);
}
// 0x080bb520
TextFileReader::~TextFileReader()
{
    delete m_pFile;
}
// 0x080bb210
int TextFileReader::availableBytes()
{
    return m_pFile->getFileLength() - m_pFile->getFilePosition();
}
// 0x080bb250
void TextFileReader::readBytes(void* buffer, int size)
{
    m_pFile->read(buffer, size);
}
// 0x080bb270
void TextFileReader::skip(int count)
{
    m_pFile->skip(count);
}
// 0x080bb290
int TextFileReader::getPosition() const
{
    return m_pFile->getFilePosition();
}
// 0x080bb2b0
void TextFileReader::seek(int pos)
{
    m_pFile->seek(pos);
}
// 0x080bb2d0
const char* TextFileReader::getFilename() const
{
    return m_pFile->getFilename();
}
// 0x080bb2f0
void TextFileReader::readByte(unsigned char& v)
{
    readBytes(&v, 1);
}
// 0x080bb6c0
void TextFileReader::readShort(unsigned short& v)
{
    String token;
    readString(token);
    v = (unsigned short)strtol(token.c_str(), 0, 10);
}
// 0x080bb7d0
void TextFileReader::readInt(unsigned int& v)
{
    String token;
    readString(token);
    v = (unsigned int)strtol(token.c_str(), 0, 10);
}
// 0x080bb750
void TextFileReader::readFloat(float& v)
{
    String token;
    readString(token);
    v = (float)strtod(token.c_str(), 0);
}
// 0x080bb900
void TextFileReader::readDouble(double& v)
{
    String token;
    readString(token);
    v = strtod(token.c_str(), 0);
}
// 0x080bb860: "true" or "1"
void TextFileReader::readBool(bool& v)
{
    String token;
    readString(token);
    v = strncmp(token.c_str(), "true", 5) == 0 || strncmp(token.c_str(), "1", 2) == 0;
}
// 0x080bb5a0
void TextFileReader::skipWhiteSpaces()
{
    while (availableBytes() > 0)
    {
        unsigned char ch;
        readBytes(&ch, 1);
        if (!isspace(ch))
        {
            m_pFile->skip(-1);
            return;
        }
    }
}
// 0x080bbcd0
void TextFileReader::readString(String& s)
{
    s.clear();
    skipWhiteSpaces();
    while (availableBytes() != 0)
    {
        char ch;
        readBytes(&ch, 1);
        if (isspace((unsigned char)ch))
            break;
        s.push_back(&ch, 1);
    }
}
// 0x080bb430
void TextFileReader::readLine(String& s)
{
    s.clear();
    for (;;)
    {
        char ch;
        readBytes(&ch, 1);
        if (ch == 0 || ch == '\n')
            break;
        s.push_back(&ch, 1);
    }
}

// 0x080bb560
TextFileWriter::TextFileWriter(const char* filename)
{
    m_pFile = openWrite(filename);
}
// 0x080bb4e0
TextFileWriter::~TextFileWriter()
{
    delete m_pFile;
}
// 0x080bb320
int TextFileWriter::getPosition() const
{
    return m_pFile->getFilePosition();
}
// 0x080bb340
void TextFileWriter::seek(int pos)
{
    m_pFile->seek(pos);
}
// 0x080bb360
const char* TextFileWriter::getFilename() const
{
    return m_pFile->getFilename();
}
// 0x080bb380
void TextFileWriter::writeBytes(const void* buffer, int size)
{
    m_pFile->write(buffer, size);
}
// 0x080bb3a0
void TextFileWriter::writeByte(unsigned char v)
{
    writeBytes(&v, 1);
}
// 0x080bb3d0
void TextFileWriter::writeShort(unsigned short v)
{
    writeBytes(&v, 2);
}
// 0x080bba50
void TextFileWriter::writeInt(unsigned int v)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", (int)v);
    writeString(String(buffer));
}
// 0x080bb980
void TextFileWriter::writeFloat(float v)
{
    // "%f" writes every integer digit: FLT_MAX takes 47 characters (the original's 32
    // byte buffer overflowed on large values)
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%f", (double)v);
    writeString(String(buffer));
}
// 0x080bbb20
void TextFileWriter::writeDouble(double v)
{
    char buffer[DBL_MAX_10_EXP + 16]; // every integer digit of DBL_MAX, sign, point, six decimals
    snprintf(buffer, sizeof(buffer), "%f", v);
    writeString(String(buffer));
}
// 0x080bbbf0
void TextFileWriter::writeBool(bool v)
{
    writeString(v ? String("true") : String("false"));
}
// 0x080bb3f0
void TextFileWriter::writeString(const String& s)
{
    writeBytes(s.c_str(), s.size());
}

} // namespace core
