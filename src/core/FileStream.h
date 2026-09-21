// File backed binary streams, reconstructed from FileStream.cpp (0x080c97e0-0x080c9fc0).
#pragma once
#include "core/FileSystem.h"
#include "core/Stream.h"

namespace core
{

class FileInputStream : public InputStream
{
  public:
    // 0x080c9ec0: opens through the mounted file systems; throws FileNotFoundException.
    explicit FileInputStream(const char* filename, Endian endian = Endian_Platform);
    ~FileInputStream();
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
    File* m_pFile;
    bool m_swapBytes;
    String m_filename;
};

class FileOutputStream : public OutputStream
{
  public:
    explicit FileOutputStream(const char* filename, Endian endian = Endian_Platform);
    ~FileOutputStream();
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

  private:
    File* m_pFile;
    bool m_swapBytes;
    String m_filename;
};

} // namespace core
