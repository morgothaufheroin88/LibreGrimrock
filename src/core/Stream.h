// core::InputStream / core::OutputStream, reconstructed from ByteArrayStream.cpp
// (0x080b4ee0-0x080b5560). The virtual layout matches the original vtables.
#pragma once
#include "core/Matrix.h"
#include "core/String.h"

namespace core
{

enum Endian
{
    Endian_Platform = 0,
    Endian_Little = 1,
    Endian_Big = 2
};
extern Endian g_platformEndianness;

class InputStream
{
  public:
    virtual ~InputStream() {}
    virtual int availableBytes() = 0;
    virtual void readBytes(void* buffer, int size) = 0;
    virtual void skip(int count) = 0;
    virtual int getPosition() const = 0;
    virtual void seek(int pos) = 0;
    virtual const char* getFilename() const = 0;
    // 0x080b4ef0 / 0x080b4f10 / 0x080b4f30: signed variants forward to the unsigned ones.
    virtual void readByte(signed char& v)
    {
        readByte((unsigned char&)v);
    }
    virtual void readShort(short& v)
    {
        readShort((unsigned short&)v);
    }
    virtual void readInt(int& v)
    {
        readInt((unsigned int&)v);
    }
    virtual void readByte(unsigned char& v) = 0;
    virtual void readShort(unsigned short& v) = 0;
    virtual void readInt(unsigned int& v) = 0;
    virtual void readFloat(float& v) = 0;
    virtual void readDouble(double& v) = 0;
    virtual void readBool(bool& v) = 0;
    // 0x080b5490: int length followed by the characters.
    virtual void readString(String& s);
    virtual void readVector2(Vec2& v);
    virtual void readVector3(Vec3& v);
    virtual void readVector4(Vec4& v);
    virtual void readMatrix3x3(Matrix3x3& m);
    virtual void readMatrix4x3(Matrix4x3& m);
    virtual void readMatrix4x4(Matrix4x4& m);
    virtual void readQuaternion(Quat& q);

    unsigned char readByte()
    {
        unsigned char v;
        readByte(v);
        return v;
    }
    unsigned short readShort()
    {
        unsigned short v;
        readShort(v);
        return v;
    }
    int readInt()
    {
        int v;
        readInt(v);
        return v;
    }
    float readFloat()
    {
        float v;
        readFloat(v);
        return v;
    }
    bool readBool()
    {
        bool v;
        readBool(v);
        return v;
    }
    String readString()
    {
        String s;
        readString(s);
        return s;
    }
};

class OutputStream
{
  public:
    virtual ~OutputStream() {}
    virtual int getPosition() const = 0;
    virtual void seek(int pos) = 0;
    virtual const char* getFilename() const = 0;
    virtual void writeBytes(const void* buffer, int size) = 0;
    virtual void writeByte(signed char v)
    {
        writeByte((unsigned char)v);
    }
    virtual void writeShort(short v)
    {
        writeShort((unsigned short)v);
    }
    virtual void writeInt(int v)
    {
        writeInt((unsigned int)v);
    }
    virtual void writeByte(unsigned char v) = 0;
    virtual void writeShort(unsigned short v) = 0;
    virtual void writeInt(unsigned int v) = 0;
    virtual void writeFloat(float v) = 0;
    virtual void writeDouble(double v) = 0;
    virtual void writeBool(bool v) = 0;
    // 0x080b5440 / 0x080b51d0
    virtual void writeString(const char* s);
    virtual void writeString(const String& s);
    virtual void writeVector2(const Vec2& v);
    virtual void writeVector3(const Vec3& v);
    virtual void writeVector4(const Vec4& v);
    virtual void writeMatrix3x3(const Matrix3x3& m);
    virtual void writeMatrix4x3(const Matrix4x3& m);
    virtual void writeMatrix4x4(const Matrix4x4& m);
    virtual void writeQuaternion(const Quat& q);
};

inline unsigned short swapBytes(unsigned short v)
{
    return (unsigned short)((v >> 8) | (v << 8));
}
inline unsigned int swapBytes(unsigned int v)
{
    return (v << 24) | (v >> 24) | ((v >> 8) & 0xff00) | ((v & 0xff00) << 8);
}

} // namespace core
