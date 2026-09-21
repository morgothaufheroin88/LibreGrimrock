// core::Vector2/3/4<T> and core::Quaternion<T>. Header-only templates in the original
// (only their instantiations show up in the binary).
#pragma once
#include <cmath>

namespace core
{

template <class T> class Vector2
{
  public:
    T x, y;
    Vector2() : x(0), y(0) {}
    Vector2(T x_, T y_) : x(x_), y(y_) {}
    void set(T x_, T y_)
    {
        x = x_;
        y = y_;
    }
    Vector2 operator+(const Vector2& o) const
    {
        return Vector2(x + o.x, y + o.y);
    }
    Vector2 operator-(const Vector2& o) const
    {
        return Vector2(x - o.x, y - o.y);
    }
    Vector2 operator-() const
    {
        return Vector2(-x, -y);
    }
    Vector2 operator*(T s) const
    {
        return Vector2(x * s, y * s);
    }
    Vector2 operator/(T s) const
    {
        return Vector2(x / s, y / s);
    }
    Vector2& operator+=(const Vector2& o)
    {
        x += o.x;
        y += o.y;
        return *this;
    }
    Vector2& operator-=(const Vector2& o)
    {
        x -= o.x;
        y -= o.y;
        return *this;
    }
    Vector2& operator*=(T s)
    {
        x *= s;
        y *= s;
        return *this;
    }
    bool operator==(const Vector2& o) const
    {
        return x == o.x && y == o.y;
    }
    bool operator!=(const Vector2& o) const
    {
        return !(*this == o);
    }
    T length() const
    {
        return (T)std::sqrt(x * x + y * y);
    }
    T sqrLength() const
    {
        return x * x + y * y;
    }
    void normalize()
    {
        T l = length();
        if (l != 0)
        {
            x /= l;
            y /= l;
        }
    }
    T& operator[](int i)
    {
        return (&x)[i];
    }
    T operator[](int i) const
    {
        return (&x)[i];
    }
};

template <class T> class Vector3
{
  public:
    T x, y, z;
    Vector3() : x(0), y(0), z(0) {}
    Vector3(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}
    void set(T x_, T y_, T z_)
    {
        x = x_;
        y = y_;
        z = z_;
    }
    Vector3 operator+(const Vector3& o) const
    {
        return Vector3(x + o.x, y + o.y, z + o.z);
    }
    Vector3 operator-(const Vector3& o) const
    {
        return Vector3(x - o.x, y - o.y, z - o.z);
    }
    Vector3 operator-() const
    {
        return Vector3(-x, -y, -z);
    }
    Vector3 operator*(T s) const
    {
        return Vector3(x * s, y * s, z * s);
    }
    Vector3 operator/(T s) const
    {
        return Vector3(x / s, y / s, z / s);
    }
    Vector3& operator+=(const Vector3& o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    Vector3& operator-=(const Vector3& o)
    {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        return *this;
    }
    Vector3& operator*=(T s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }
    Vector3& operator/=(T s)
    {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }
    bool operator==(const Vector3& o) const
    {
        return x == o.x && y == o.y && z == o.z;
    }
    bool operator!=(const Vector3& o) const
    {
        return !(*this == o);
    }
    T length() const
    {
        return (T)std::sqrt(x * x + y * y + z * z);
    }
    T sqrLength() const
    {
        return x * x + y * y + z * z;
    }
    void normalize()
    {
        T l = length();
        if (l != 0)
        {
            T inv = 1 / l;
            x *= inv;
            y *= inv;
            z *= inv;
        }
    }
    T& operator[](int i)
    {
        return (&x)[i];
    }
    T operator[](int i) const
    {
        return (&x)[i];
    }
};

template <class T> class Vector4
{
  public:
    T x, y, z, w;
    Vector4() : x(0), y(0), z(0), w(0) {}
    Vector4(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vector4(const Vector3<T>& v, T w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    void set(T x_, T y_, T z_, T w_)
    {
        x = x_;
        y = y_;
        z = z_;
        w = w_;
    }
    Vector3<T> xyz() const
    {
        return Vector3<T>(x, y, z);
    }
    Vector4 operator+(const Vector4& o) const
    {
        return Vector4(x + o.x, y + o.y, z + o.z, w + o.w);
    }
    Vector4 operator-(const Vector4& o) const
    {
        return Vector4(x - o.x, y - o.y, z - o.z, w - o.w);
    }
    Vector4 operator*(T s) const
    {
        return Vector4(x * s, y * s, z * s, w * s);
    }
    Vector4& operator*=(T s)
    {
        x *= s;
        y *= s;
        z *= s;
        w *= s;
        return *this;
    }
    bool operator==(const Vector4& o) const
    {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
    T& operator[](int i)
    {
        return (&x)[i];
    }
    T operator[](int i) const
    {
        return (&x)[i];
    }
};

template <class T> class Quaternion
{
  public:
    T x, y, z, w;
    Quaternion() : x(0), y(0), z(0), w(1) {}
    Quaternion(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_) {}
    void makeIdentity()
    {
        x = y = z = 0;
        w = 1;
    }
    T length() const
    {
        return (T)std::sqrt(x * x + y * y + z * z + w * w);
    }
    void normalize()
    {
        T l = length();
        if (l != 0)
        {
            T inv = 1 / l;
            x *= inv;
            y *= inv;
            z *= inv;
            w *= inv;
        }
    }
    Quaternion operator*(const Quaternion& q) const
    {
        return Quaternion(
            w * q.x + x * q.w + y * q.z - z * q.y, w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w, w * q.w - x * q.x - y * q.y - z * q.z);
    }
};

template <class T> inline T dot(const Vector2<T>& a, const Vector2<T>& b)
{
    return a.x * b.x + a.y * b.y;
}
template <class T> inline T dot(const Vector3<T>& a, const Vector3<T>& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
template <class T> inline T dot(const Vector4<T>& a, const Vector4<T>& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
template <class T> inline T dot(const Quaternion<T>& a, const Quaternion<T>& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
template <class T> inline Vector3<T> cross(const Vector3<T>& a, const Vector3<T>& b)
{
    return Vector3<T>(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
template <class T> inline T length(const Vector2<T>& v)
{
    return v.length();
}
template <class T> inline T length(const Vector3<T>& v)
{
    return v.length();
}
template <class T> inline Vector2<T> normalize(const Vector2<T>& v)
{
    Vector2<T> r(v);
    r.normalize();
    return r;
}
template <class T> inline Vector3<T> normalize(const Vector3<T>& v)
{
    Vector3<T> r(v);
    r.normalize();
    return r;
}
template <class T> inline Vector3<T> operator*(T s, const Vector3<T>& v)
{
    return v * s;
}
template <class T> inline Vector2<T> operator*(T s, const Vector2<T>& v)
{
    return v * s;
}
template <class T> inline Vector3<T> lerp(const Vector3<T>& a, const Vector3<T>& b, T t)
{
    return a + (b - a) * t;
}
template <class T> inline Vector3<T> minVec(const Vector3<T>& a, const Vector3<T>& b)
{
    return Vector3<T>(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z);
}
template <class T> inline Vector3<T> maxVec(const Vector3<T>& a, const Vector3<T>& b)
{
    return Vector3<T>(a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z);
}

// Spherical linear interpolation, as used by the animation code.
template <class T> inline Quaternion<T> slerp(const Quaternion<T>& a, const Quaternion<T>& b, T t)
{
    T cosom = dot(a, b);
    Quaternion<T> bb = b;
    if (cosom < 0)
    {
        cosom = -cosom;
        bb = Quaternion<T>(-b.x, -b.y, -b.z, -b.w);
    }
    T s0, s1;
    if (1 - cosom > (T)1e-6)
    {
        T omega = (T)std::acos(cosom);
        T sinom = (T)std::sin(omega);
        s0 = (T)std::sin((1 - t) * omega) / sinom;
        s1 = (T)std::sin(t * omega) / sinom;
    }
    else
    {
        s0 = 1 - t;
        s1 = t;
    }
    return Quaternion<T>(s0 * a.x + s1 * bb.x, s0 * a.y + s1 * bb.y, s0 * a.z + s1 * bb.z,
                         s0 * a.w + s1 * bb.w);
}

typedef Vector2<float> Vec2;
typedef Vector3<float> Vec3;
typedef Vector4<float> Vec4;
typedef Vector2<int> Vec2i;
typedef Vector3<int> Vec3i;
typedef Quaternion<float> Quat;

} // namespace core
