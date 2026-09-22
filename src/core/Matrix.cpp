// Reconstructed from Grimrock.bin.x86 Matrix3x3.cpp, Matrix4x3.cpp, Matrix4x4.cpp.
#include "core/Matrix.h"
#include <cmath>
#include <cstdio>
#include <utility>
using std::swap;

namespace core
{

const Matrix3x3 Matrix3x3::sm_mIdentity;
const Matrix4x3 Matrix4x3::sm_mIdentity;
const Matrix4x4 Matrix4x4::sm_mIdentity;

// ---- Matrix3x3 ----------------------------------------------------------------

// 0x080b0ee0
void Matrix3x3::makeNull()
{
    x.set(0, 0, 0);
    y.set(0, 0, 0);
    z.set(0, 0, 0);
}
// 0x080b0eb0
void Matrix3x3::makeIdentity()
{
    x.set(1, 0, 0);
    y.set(0, 1, 0);
    z.set(0, 0, 1);
}
// 0x080b0e80
void Matrix3x3::makeScaling(float sx, float sy, float sz)
{
    x.set(sx, 0, 0);
    y.set(0, sy, 0);
    z.set(0, 0, sz);
}
// 0x080b1120
void Matrix3x3::makeRotation(const Vec3& axis, float angle)
{
    float sinA = std::sin(angle), cosA = std::cos(angle);
    float oneMinusCos = 1.0f - cosA;
    float xy = axis.x * axis.y * oneMinusCos, yz = axis.y * axis.z * oneMinusCos,
          zx = axis.z * axis.x * oneMinusCos;
    x.x = (1.0f - axis.x * axis.x) * cosA + axis.x * axis.x;
    y.x = xy - axis.z * sinA;
    z.x = zx + axis.y * sinA;
    x.y = xy + axis.z * sinA;
    y.y = (1.0f - axis.y * axis.y) * cosA + axis.y * axis.y;
    z.y = yz - axis.x * sinA;
    x.z = zx - axis.y * sinA;
    y.z = yz + axis.x * sinA;
    z.z = (1.0f - axis.z * axis.z) * cosA + axis.z * axis.z;
}
// 0x080b11f0
void Matrix3x3::makeRotation(float rx, float ry, float rz, Order order)
{
    float sx = std::sin(rx), cx = std::cos(rx);
    float sy = std::sin(ry), cy = std::cos(ry);
    float sz = std::sin(rz), cz = std::cos(rz);
    float* v = data();
    switch (order)
    {
    case XYZ:
    {
        const float t[9] = {cz * cy,
                            -sz * cy,
                            sy,
                            cz * sy * sx + sz * cx,
                            cz * cx - sz * sy * sx,
                            -cy * sx,
                            sz * sx - cz * sy * cx,
                            sz * sy * cx + cz * sx,
                            cy * cx};
        for (int i = 0; i < 9; ++i)
            v[i] = t[i];
        break;
    }
    case XZY:
    {
        const float t[9] = {cy * cz,
                            -sz,
                            cz * sy,
                            sy * sx + cy * sz * cx,
                            cz * cx,
                            sz * sy * cx - cy * sx,
                            cy * sz * sx - sy * cx,
                            cz * sx,
                            sz * sy * sx + cy * cx};
        for (int i = 0; i < 9; ++i)
            v[i] = t[i];
        break;
    }
    case YXZ:
    {
        const float t[9] = {
            sz * sx * sy + cz * cy, cz * sx * sy - sz * cy, sy * cx, sz * cx, cz * cx, -sx,
            sz * sx * cy - cz * sy, cz * sx * cy + sz * sy, cy * cx};
        for (int i = 0; i < 9; ++i)
            v[i] = t[i];
        break;
    }
    case YZX:
    {
        const float t[9] = {
            cz * cy,  sx * sy - cx * sz * cy, cx * sy + sx * sz * cy, sz, cx * cz, -sx * cz,
            -cz * sy, sx * cy + cx * sz * sy, cx * cy - sx * sz * sy};
        for (int i = 0; i < 9; ++i)
            v[i] = t[i];
        break;
    }
    case ZXY:
    {
        const float t[9] = {cy * cz - sy * sx * sz,
                            -cx * sz,
                            sx * cy * sz + sy * cz,
                            sy * sx * cz + cy * sz,
                            cx * cz,
                            sz * sy - sx * cy * cz,
                            -sy * cx,
                            sx,
                            cy * cx};
        for (int i = 0; i < 9; ++i)
            v[i] = t[i];
        break;
    }
    case ZYX:
    {
        const float t[9] = {cy * cz,
                            sx * sy * cz - cx * sz,
                            cx * sy * cz + sx * sz,
                            cy * sz,
                            cx * cz + sx * sy * sz,
                            cx * sy * sz - sx * cz,
                            -sy,
                            sx * cy,
                            cy * cx};
        for (int i = 0; i < 9; ++i)
            v[i] = t[i];
        break;
    }
    }
}
// 0x080b0f10
void Matrix3x3::transpose()
{
    swap(x.y, y.x);
    swap(x.z, z.x);
    swap(y.z, z.y);
}
// 0x080b0f40
void Matrix3x3::scale(float factor)
{
    x *= factor;
    y *= factor;
    z *= factor;
}
// 0x080b0f90
void Matrix3x3::invert()
{
    float* v = data();
    float m0 = v[0], m1 = v[1], m2 = v[2], m3 = v[3], m4 = v[4], m5 = v[5], m6 = v[6], m7 = v[7],
          m8 = v[8];
    float det =
        m6 * m1 * m5 + m3 * m7 * m2 + m4 * m0 * m8 - m4 * m6 * m2 - m7 * m0 * m5 - m3 * m1 * m8;
    float inv = 1.0f / det;
    v[0] = (m4 * m8 - m7 * m5) * inv;
    v[1] = -(m8 * m1 - m7 * m2) * inv;
    v[2] = (m1 * m5 - m4 * m2) * inv;
    v[3] = -(m3 * m8 - m6 * m5) * inv;
    v[4] = (m8 * m0 - m6 * m2) * inv;
    v[5] = -(m5 * m0 - m2 * m3) * inv;
    v[6] = (m3 * m7 - m4 * m6) * inv;
    v[7] = -(m7 * m0 - m6 * m1) * inv;
    v[8] = (m4 * m0 - m3 * m1) * inv;
}
// 0x080b1710
void Matrix3x3::lookAt(const Vec3& dir)
{
    z = dir;
    if (dir.x != 0.0f || dir.z != 0.0f)
    {
        x = cross(Vec3(0, 1, 0), z);
        y = cross(z, x);
    }
    else
    {
        y = cross(z, Vec3(0, 0, 1));
        x = cross(y, z);
    }
    x.normalize();
    y.normalize();
    z.normalize();
}
// 0x080b1890 Gram-Schmidt on the three columns.
void Matrix3x3::orthonormalize()
{
    x.normalize();
    y = y - x * dot(x, y);
    y.normalize();
    z = z - x * dot(x, z);
    z = z - y * dot(y, z);
    z.normalize();
}
// 0x080b1a90 rotate this matrix about one of its own axes.
void Matrix3x3::rotateAbout(int axis, float angle)
{
    Matrix3x3 r;
    r.makeRotation(column(axis), angle);
    *this = r * *this;
}
// 0x080b10a0
void Matrix3x3::dump()
{
    printf("%f %f %f\n", x.x, y.x, z.x);
    printf("%f %f %f\n", x.y, y.y, z.y);
    printf("%f %f %f\n", x.z, y.z, z.z);
}
// 0x080b0e50
Matrix3x3 Matrix3x3::createScaling(float sx, float sy, float sz)
{
    Matrix3x3 m;
    m.makeScaling(sx, sy, sz);
    return m;
}
// 0x080b15e0
Matrix3x3 Matrix3x3::createRotationX(float angle)
{
    float sinA = std::sin(angle), cosA = std::cos(angle);
    return Matrix3x3(Vec3(1, 0, 0), Vec3(0, cosA, sinA), Vec3(0, -sinA, cosA));
}
// 0x080b1580
Matrix3x3 Matrix3x3::createRotationY(float angle)
{
    float sinA = std::sin(angle), cosA = std::cos(angle);
    return Matrix3x3(Vec3(cosA, 0, -sinA), Vec3(0, 1, 0), Vec3(sinA, 0, cosA));
}
// 0x080b1520
Matrix3x3 Matrix3x3::createRotationZ(float angle)
{
    float sinA = std::sin(angle), cosA = std::cos(angle);
    return Matrix3x3(Vec3(cosA, sinA, 0), Vec3(-sinA, cosA, 0), Vec3(0, 0, 1));
}
// 0x080b1640
Matrix3x3 Matrix3x3::createRotation(float rx, float ry, float rz, Order order)
{
    Matrix3x3 m;
    m.makeRotation(rx, ry, rz, order);
    return m;
}
// 0x080b19a0
Matrix3x3 Matrix3x3::createRotation(const Vec3& axis, float angle)
{
    Matrix3x3 m;
    m.makeRotation(axis, angle);
    return m;
}

// ---- Matrix4x3 ----------------------------------------------------------------

// 0x080c6f70
void Matrix4x3::makeIdentity()
{
    Matrix3x3::makeIdentity();
    pos.set(0, 0, 0);
}
// 0x080c7030
void Matrix4x3::makeNull()
{
    Matrix3x3::makeNull();
    pos.set(0, 0, 0);
}
// 0x080c7060
void Matrix4x3::makeRotation(float rx, float ry, float rz, Matrix3x3::Order order)
{
    Matrix3x3::makeRotation(rx, ry, rz, order);
    pos.set(0, 0, 0);
}
// 0x080c70a0
void Matrix4x3::invertOrthonormal()
{
    Matrix3x3::transpose();
    Vec3 translation = pos;
    pos = -transformVector(translation);
}
// 0x080c7240: general affine inverse via the 4x4 cofactor expansion with the
// implicit (0,0,0,1) fourth row.
void Matrix4x3::invert()
{
    Matrix3x3 rotation(*this);
    rotation.invert();
    Vec3 translation = pos;
    Matrix3x3::operator=(rotation);
    pos = -rotation.transform(translation);
}
// 0x080c6fa0
void Matrix4x3::dump()
{
    printf("%f %f %f %f\n", x.x, y.x, z.x, pos.x);
    printf("%f %f %f %f\n", x.y, y.y, z.y, pos.y);
    printf("%f %f %f %f\n", x.z, y.z, z.z, pos.z);
}

// ---- Matrix4x4 ----------------------------------------------------------------

// 0x080b2690
Matrix4x4::Matrix4x4(float m00, float m01, float m02, float m03, float m10, float m11, float m12,
                     float m13, float m20, float m21, float m22, float m23, float m30, float m31,
                     float m32, float m33)
{
    m[0] = m00;
    m[1] = m10;
    m[2] = m20;
    m[3] = m30;
    m[4] = m01;
    m[5] = m11;
    m[6] = m21;
    m[7] = m31;
    m[8] = m02;
    m[9] = m12;
    m[10] = m22;
    m[11] = m32;
    m[12] = m03;
    m[13] = m13;
    m[14] = m23;
    m[15] = m33;
}
// 0x080b2700
Matrix4x4::Matrix4x4(const Matrix4x3& mat)
{
    m[0] = mat.x.x;
    m[1] = mat.x.y;
    m[2] = mat.x.z;
    m[3] = 0;
    m[4] = mat.y.x;
    m[5] = mat.y.y;
    m[6] = mat.y.z;
    m[7] = 0;
    m[8] = mat.z.x;
    m[9] = mat.z.y;
    m[10] = mat.z.z;
    m[11] = 0;
    m[12] = mat.pos.x;
    m[13] = mat.pos.y;
    m[14] = mat.pos.z;
    m[15] = 1;
}
// 0x080b2790
Matrix4x4& Matrix4x4::operator*=(float s)
{
    for (int i = 0; i < 16; ++i)
        m[i] *= s;
    return *this;
}
// 0x080b2820
Matrix4x4& Matrix4x4::operator/=(float s)
{
    for (int i = 0; i < 16; ++i)
        m[i] /= s;
    return *this;
}
Matrix4x4 Matrix4x4::operator*(const Matrix4x4& o) const
{
    Matrix4x4 r;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0;
            for (int k = 0; k < 4; ++k)
                sum += m[k * 4 + row] * o.m[col * 4 + k];
            r.m[col * 4 + row] = sum;
        }
    return r;
}
// 0x080b28b0
void Matrix4x4::transpose()
{
    swap(m[1], m[4]);
    swap(m[2], m[8]);
    swap(m[6], m[9]);
    swap(m[3], m[12]);
    swap(m[7], m[13]);
    swap(m[11], m[14]);
}
// 0x080b2990
void Matrix4x4::invert()
{
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
             m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
              m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
             m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[4] * inv[1] + m[8] * inv[2] + m[12] * inv[3];
    float invDet = 1.0f / det;
    for (int i = 0; i < 16; ++i)
        m[i] = inv[i] * invDet;
}
void Matrix4x4::dump()
{
    for (int row = 0; row < 4; ++row)
        printf("%f %f %f %f\n", m[row], m[4 + row], m[8 + row], m[12 + row]);
}

} // namespace core
