// Geometric primitives and tests, reconstructed from Prim.cpp (0x080ae190-0x080afd30).
#pragma once
#include "core/Matrix.h"

namespace core
{

class Plane
{
  public:
    Vec3 normal;
    float d;
    Plane() : normal(0, 1, 0), d(0) {}
    Plane(const Vec3& n, float d_) : normal(n), d(d_) {}
    Plane(const Vec3& n, const Vec3& point) : normal(n), d(dot(n, point)) {}
    float distance(const Vec3& p) const
    {
        return dot(normal, p) - d;
    }
};

class Ray3
{
  public:
    Vec3 origin;
    Vec3 dir;
    Ray3() {}
    Ray3(const Vec3& o, const Vec3& d) : origin(o), dir(d) {}
};

class Sphere2
{
  public:
    Vec2 center;
    float radius;
    Sphere2() : radius(0) {}
    Sphere2(const Vec2& c, float r) : center(c), radius(r) {}
};

class Sphere3
{
  public:
    Vec3 center;
    float radius;
    Sphere3() : radius(0) {}
    Sphere3(const Vec3& c, float r) : center(c), radius(r) {}
};

class AABox2
{
  public:
    Vec2 min, max;
    AABox2() {}
    AABox2(const Vec2& mn, const Vec2& mx) : min(mn), max(mx) {}
    Vec2 center() const
    {
        return (min + max) * 0.5f;
    }
    Vec2 size() const
    {
        return max - min;
    }
    bool contains(const Vec2& p) const
    {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
    }
};

class AABox3
{
  public:
    Vec3 min, max;
    AABox3() {}
    AABox3(const Vec3& mn, const Vec3& mx) : min(mn), max(mx) {}
    Vec3 center() const
    {
        return (min + max) * 0.5f;
    }
    Vec3 size() const
    {
        return max - min;
    }
    void makeEmpty()
    {
        min.set(1e30f, 1e30f, 1e30f);
        max.set(-1e30f, -1e30f, -1e30f);
    }
    bool isEmpty() const
    {
        return min.x > max.x || min.y > max.y || min.z > max.z;
    }
    void addPoint(const Vec3& p)
    {
        min = minVec(min, p);
        max = maxVec(max, p);
    }
    void addBox(const AABox3& b)
    {
        min = minVec(min, b.min);
        max = maxVec(max, b.max);
    }
    bool contains(const Vec3& p) const
    {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z &&
               p.z <= max.z;
    }
};

// 0x080ae190 / 0x080ae210
float sqrDistPointBox(const Vec2& p, const AABox2& box);
float sqrDistPointBox(const Vec3& p, const AABox3& box);
// 0x080ae2c0 / 0x080ae2f0
bool testSphereSphere(const Sphere2& a, const Sphere2& b);
bool testSphereSphere(const Sphere3& a, const Sphere3& b);
// 0x080ae330 / 0x080ae380
bool testBoxBox(const AABox2& a, const AABox2& b);
bool testBoxBox(const AABox3& a, const AABox3& b);
// 0x080ae3e0 / 0x080af140
bool testBoxPlane(const AABox3& box, const Plane& plane);
bool testBoxPlane(const AABox3& box, const Plane* planes, int numPlanes);
// 0x080af810 / 0x080af8a0
bool testSphereBox(const Sphere2& sphere, const AABox2& box);
bool testSphereBox(const Sphere3& sphere, const AABox3& box);
// 0x080ae460 / 0x080aebe0
bool intersectRayBox(const Ray3& ray, const AABox3& box, float& t, float tmin = 0.0f);
bool testRayBox(const Ray3& ray, const AABox3& box);
// 0x080aeb10
bool intersectRaySphere(const Ray3& ray, const Sphere3& sphere, float& t, float tmin = 0.0f);
// 0x080ae660
bool intersectRayPlane(const Ray3& ray, const Plane& plane, float& t);
// 0x080ae6d0 / 0x080afd30
bool intersectRayTriangle(const Ray3& ray, const Vec3& a, const Vec3& b, const Vec3& c, float& t,
                          float& u, float& v, float tmin = 0.0f);
bool intersectRayTriangle(const Ray3& ray, const Vec3& a, const Vec3& b, const Vec3& c, float& t,
                          float tmin = 0.0f);
// 0x080ae8a0 / 0x080aea70
void barycentric(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& p, float& u, float& v,
                 float& w);
bool testPointTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& p);
// 0x080af210
AABox3 transformBox(const AABox3& box, const Matrix4x3& m);
// 0x080aedd0 / 0x080af960: 8 corners / 6 planes of a symmetric perspective frustum.
void computeFrustumPoints(Vec3* points, float fov, float nearZ, float farZ, const Matrix4x3& m);
void computeFrustumPlanes(Plane* planes, float fov, float nearZ, float farZ, const Matrix4x3& m);

} // namespace core
