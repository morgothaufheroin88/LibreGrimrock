// core::Matrix3x3 / Matrix4x3 / Matrix4x4, reconstructed from Matrix3x3.cpp (0x080b0e50-),
// Matrix4x3.cpp (0x080c6f70-) and Matrix4x4.cpp (0x080b2690-).
// All matrices are stored column-major: Matrix3x3 is three basis vectors (x, y, z),
// Matrix4x3 adds the translation (pos). Points transform as x*px + y*py + z*pz + pos.
#pragma once
#include "core/Vector.h"

namespace core
{

class Matrix3x3
{
  public:
    enum Order
    {
        XYZ = 0,
        XZY = 1,
        YXZ = 2,
        YZX = 3,
        ZXY = 4,
        ZYX = 5
    };

    Vec3 x, y, z;

    Matrix3x3() : x(1, 0, 0), y(0, 1, 0), z(0, 0, 1) {}
    Matrix3x3(const Vec3& x_, const Vec3& y_, const Vec3& z_) : x(x_), y(y_), z(z_) {}
    Matrix3x3(float m00, float m01, float m02, float m10, float m11, float m12, float m20,
              float m21, float m22)
        : x(m00, m10, m20), y(m01, m11, m21), z(m02, m12, m22)
    {
    }

    float* data()
    {
        return &x.x;
    }
    const float* data() const
    {
        return &x.x;
    }
    float& operator()(int row, int col)
    {
        return data()[col * 3 + row];
    }
    float operator()(int row, int col) const
    {
        return data()[col * 3 + row];
    }
    Vec3& column(int i)
    {
        return (&x)[i];
    }
    const Vec3& column(int i) const
    {
        return (&x)[i];
    }

    void makeNull();
    void makeIdentity();
    void makeScaling(float sx, float sy, float sz);
    void makeRotation(const Vec3& axis, float angle);
    void makeRotation(float rx, float ry, float rz, Order order);
    void transpose();
    void scale(float factor);
    void invert();
    void lookAt(const Vec3& dir);
    void orthonormalize();
    void rotateAbout(int axis, float angle);
    void dump();

    Vec3 transform(const Vec3& v) const
    {
        return Vec3(x.x * v.x + y.x * v.y + z.x * v.z, x.y * v.x + y.y * v.y + z.y * v.z,
                    x.z * v.x + y.z * v.y + z.z * v.z);
    }
    Vec3 operator*(const Vec3& v) const
    {
        return transform(v);
    }
    Matrix3x3 operator*(const Matrix3x3& m) const
    {
        return Matrix3x3(transform(m.x), transform(m.y), transform(m.z));
    }
    Matrix3x3& operator*=(const Matrix3x3& m)
    {
        *this = *this * m;
        return *this;
    }

    static Matrix3x3 createScaling(float sx, float sy, float sz);
    static Matrix3x3 createRotationX(float angle);
    static Matrix3x3 createRotationY(float angle);
    static Matrix3x3 createRotationZ(float angle);
    static Matrix3x3 createRotation(float rx, float ry, float rz, Order order);
    static Matrix3x3 createRotation(const Vec3& axis, float angle);
    static const Matrix3x3 sm_mIdentity;
};

class Matrix4x3 : public Matrix3x3
{
  public:
    Vec3 pos;

    Matrix4x3() : Matrix3x3(), pos(0, 0, 0) {}
    Matrix4x3(const Matrix3x3& m, const Vec3& p) : Matrix3x3(m), pos(p) {}
    explicit Matrix4x3(const Matrix3x3& m) : Matrix3x3(m), pos(0, 0, 0) {}

    void makeIdentity();
    void makeNull();
    void makeRotation(float rx, float ry, float rz, Matrix3x3::Order order);
    void makeTranslation(const Vec3& p)
    {
        Matrix3x3::makeIdentity();
        pos = p;
    }
    void invertOrthonormal();
    void invert();
    void dump();

    Vec3 transformPoint(const Vec3& v) const
    {
        return Vec3(x.x * v.x + y.x * v.y + z.x * v.z + pos.x,
                    x.y * v.x + y.y * v.y + z.y * v.z + pos.y,
                    x.z * v.x + y.z * v.y + z.z * v.z + pos.z);
    }
    Vec3 transformVector(const Vec3& v) const
    {
        return Matrix3x3::transform(v);
    }
    const Matrix3x3& rotation() const
    {
        return *this;
    }
    Matrix3x3& rotation()
    {
        return *this;
    }
    // Composition: (this * m) applies m first, then this.
    Matrix4x3 operator*(const Matrix4x3& m) const
    {
        return Matrix4x3(
            Matrix3x3(transformVector(m.x), transformVector(m.y), transformVector(m.z)),
            transformPoint(m.pos));
    }
    Matrix4x3& operator*=(const Matrix4x3& m)
    {
        *this = *this * m;
        return *this;
    }
    static const Matrix4x3 sm_mIdentity;
};

class Matrix4x4
{
  public:
    float m[16]; // column-major

    Matrix4x4()
    {
        makeIdentity();
    }
    // Row-major argument order (m00 m01 m02 m03 is the first row), as in the original.
    Matrix4x4(float m00, float m01, float m02, float m03, float m10, float m11, float m12,
              float m13, float m20, float m21, float m22, float m23, float m30, float m31,
              float m32, float m33);
    Matrix4x4(const Matrix4x3& mat);

    float& operator()(int row, int col)
    {
        return m[col * 4 + row];
    }
    float operator()(int row, int col) const
    {
        return m[col * 4 + row];
    }
    void makeIdentity()
    {
        for (int i = 0; i < 16; ++i)
            m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
    void makeNull()
    {
        for (int i = 0; i < 16; ++i)
            m[i] = 0.0f;
    }
    Matrix4x4& operator*=(float s);
    Matrix4x4& operator/=(float s);
    Matrix4x4 operator*(const Matrix4x4& o) const;
    void transpose();
    void invert();
    void dump();
    Vec4 transform(const Vec4& v) const
    {
        return Vec4(m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
                    m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
                    m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
                    m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w);
    }
    Vec3 transformPoint(const Vec3& v) const
    {
        Vec4 r = transform(Vec4(v, 1.0f));
        return Vec3(r.x / r.w, r.y / r.w, r.z / r.w);
    }
    static const Matrix4x4 sm_mIdentity;
};

} // namespace core
