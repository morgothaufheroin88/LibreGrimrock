// Whitespace separated text streams, reconstructed from TextFile.cpp (0x080bb210-0x080bbdc0).
#pragma once
#include "core/FileSystem.h"
#include "core/Stream.h"

namespace core
{

class TextFileReader : public InputStream
{
  public:
    explicit TextFileReader(const char* filename);
    ~TextFileReader();
    int availableBytes();
    void readBytes(void* buffer, int size);
    void skip(int count);
    int getPosition() const;
    void seek(int pos);
    const char* getFilename() const;
    void readByte(unsigned char& v);
    void readShort(unsigned short& v);
    void readInt(unsigned int& v);
    void readFloat(float& v);
    void readDouble(double& v);
    void readBool(bool& v);
    // Next whitespace delimited token.
    void readString(String& s);
    void readLine(String& s);
    void skipWhiteSpaces();

  private:
    File* m_pFile;
};

class TextFileWriter : public OutputStream
{
  public:
    explicit TextFileWriter(const char* filename);
    ~TextFileWriter();
    int getPosition() const;
    void seek(int pos);
    const char* getFilename() const;
    void writeBytes(const void* buffer, int size);
    void writeByte(unsigned char v);
    void writeShort(unsigned short v);
    void writeInt(unsigned int v);
    void writeFloat(float v);
    void writeDouble(double v);
    void writeBool(bool v);
    void writeString(const String& s);
    void writeString(const char* s)
    {
        writeString(String(s));
    }

  private:
    File* m_pFile;
};

} // namespace core
