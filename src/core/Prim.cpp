// Reconstructed from Grimrock.bin.x86 Prim.cpp.
#include "core/Prim.h"
#include "core/Math.h"
#include <cfloat>
#include <cmath>

namespace core
{

static inline float axisDist(float v, float lo, float hi)
{
    if (v < lo)
        return lo - v;
    if (v > hi)
        return v - hi;
    return 0.0f;
}

// 0x080ae190
float sqrDistPointBox(const Vec2& p, const AABox2& box)
{
    float x = axisDist(p.x, box.min.x, box.max.x);
    float y = axisDist(p.y, box.min.y, box.max.y);
    return x * x + y * y;
}
// 0x080ae210
float sqrDistPointBox(const Vec3& p, const AABox3& box)
{
    float x = axisDist(p.x, box.min.x, box.max.x);
    float y = axisDist(p.y, box.min.y, box.max.y);
    float z = axisDist(p.z, box.min.z, box.max.z);
    return x * x + y * y + z * z;
}
// 0x080ae2c0
bool testSphereSphere(const Sphere2& a, const Sphere2& b)
{
    Vec2 delta = b.center - a.center;
    float radiusSum = a.radius + b.radius;
    return dot(delta, delta) <= radiusSum * radiusSum;
}
// 0x080ae2f0
bool testSphereSphere(const Sphere3& a, const Sphere3& b)
{
    Vec3 delta = b.center - a.center;
    float radiusSum = a.radius + b.radius;
    return dot(delta, delta) <= radiusSum * radiusSum;
}
// 0x080ae330
bool testBoxBox(const AABox2& a, const AABox2& b)
{
    return b.min.x <= a.max.x && a.min.x <= b.max.x && b.min.y <= a.max.y && a.min.y <= b.max.y;
}
// 0x080ae380
bool testBoxBox(const AABox3& a, const AABox3& b)
{
    return b.min.x <= a.max.x && a.min.x <= b.max.x && b.min.y <= a.max.y && a.min.y <= b.max.y &&
           b.min.z <= a.max.z && a.min.z <= b.max.z;
}
// 0x080ae3e0: box is on the positive side of (or straddles) the plane.
bool testBoxPlane(const AABox3& box, const Plane& plane)
{
    Vec3 center = (box.min + box.max) * 0.5f;
    Vec3 extent = (box.max - box.min) * 0.5f;
    float projectedRadius = extent.x * std::fabs(plane.normal.x) +
                            extent.y * std::fabs(plane.normal.y) +
                            extent.z * std::fabs(plane.normal.z);
    return dot(center, plane.normal) - plane.d >= -projectedRadius;
}
// 0x080af140
bool testBoxPlane(const AABox3& box, const Plane* planes, int numPlanes)
{
    for (int i = 0; i < numPlanes; ++i)
        if (!testBoxPlane(box, planes[i]))
            return false;
    return true;
}
// 0x080af810
bool testSphereBox(const Sphere2& s, const AABox2& box)
{
    return sqrDistPointBox(s.center, box) <= s.radius * s.radius;
}
// 0x080af8a0
bool testSphereBox(const Sphere3& s, const AABox3& box)
{
    return sqrDistPointBox(s.center, box) <= s.radius * s.radius;
}
// 0x080ae460: slab test.
bool intersectRayBox(const Ray3& ray, const AABox3& box, float& t, float tmin)
{
    float tnear = tmin, tfar = FLT_MAX;
    for (int axis = 0; axis < 3; ++axis)
    {
        float origin = ray.origin[axis], dir = ray.dir[axis];
        if (std::fabs(dir) < 1e-6f)
        {
            if (origin < box.min[axis] || origin > box.max[axis])
                return false;
        }
        else
        {
            float t1 = (box.min[axis] - origin) / dir, t2 = (box.max[axis] - origin) / dir;
            if (t2 < t1)
                swap(t1, t2);
            if (t1 > tnear)
                tnear = t1;
            if (t2 < tfar)
                tfar = t2;
            if (tfar < tnear)
                return false;
        }
    }
    t = tnear;
    return true;
}
// 0x080aebe0
bool testRayBox(const Ray3& ray, const AABox3& box)
{
    float distance;
    return intersectRayBox(ray, box, distance, 0.0f);
}
// 0x080aeb10
bool intersectRaySphere(const Ray3& ray, const Sphere3& sphere, float& t, float tmin)
{
    // quadratic in t with a = 1 (unit direction): t^2 + 2bt + c = 0
    Vec3 toOrigin = ray.origin - sphere.center;
    float b = dot(ray.dir, toOrigin);
    float c = dot(toOrigin, toOrigin) - sphere.radius * sphere.radius;
    if (c > 0.0f && b > 0.0f)
        return false;
    float disc = b * b - c;
    if (disc < 0.0f)
        return false;
    t = -b - std::sqrt(disc);
    return t >= tmin;
}
// 0x080ae660
bool intersectRayPlane(const Ray3& ray, const Plane& plane, float& t)
{
    float denom = dot(ray.dir, plane.normal);
    if (denom == 0.0f)
        return false;
    t = -(dot(ray.origin, plane.normal) - plane.d) / denom;
    return true;
}
// 0x080ae6d0: Moller-Trumbore.
bool intersectRayTriangle(const Ray3& ray, const Vec3& a, const Vec3& b, const Vec3& c, float& t,
                          float& u, float& v, float tmin)
{
    Vec3 edge1 = b - a, edge2 = c - a;
    Vec3 pvec = cross(ray.dir, edge2);
    float det = dot(edge1, pvec);
    if (det > -1e-5f && det < 1e-5f)
        return false;
    float invDet = 1.0f / det;
    Vec3 tvec = ray.origin - a;
    u = dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f)
        return false;
    Vec3 qvec = cross(tvec, edge1);
    v = dot(ray.dir, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f)
        return false;
    t = dot(edge2, qvec) * invDet;
    return t >= tmin;
}
// 0x080afd30
bool intersectRayTriangle(const Ray3& ray, const Vec3& a, const Vec3& b, const Vec3& c, float& t,
                          float tmin)
{
    float u, v;
    return intersectRayTriangle(ray, a, b, c, t, u, v, tmin);
}
// 0x080ae8a0: barycentric coordinates via projection on the dominant axis.
void barycentric(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& p, float& u, float& v,
                 float& w)
{
    Vec3 n = cross(b - a, c - a);
    float ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);
    if (ax >= ay && ax >= az)
    {
        u = ((b.y - c.y) * (p.z - c.z) + (c.z - b.z) * (p.y - c.y)) / n.x;
        v = ((c.y - a.y) * (p.z - c.z) + (a.z - c.z) * (p.y - c.y)) / n.x;
    }
    else if (ay >= az)
    {
        float area = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
        u = ((b.z - c.z) * (p.x - c.x) + (c.x - b.x) * (p.z - c.z)) / area;
        v = ((c.z - a.z) * (p.x - c.x) + (a.x - c.x) * (p.z - c.z)) / area;
    }
    else
    {
        u = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / n.z;
        v = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / n.z;
    }
    w = 1.0f - u - v;
}
// 0x080aea70
bool testPointTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& p)
{
    float u, v, w;
    barycentric(a, b, c, p, u, v, w);
    return v >= 0.0f && w >= 0.0f && v + w <= 1.0f;
}
// 0x080af210
AABox3 transformBox(const AABox3& box, const Matrix4x3& m)
{
    AABox3 r;
    r.makeEmpty();
    for (int i = 0; i < 8; ++i)
    {
        Vec3 corner(i & 1 ? box.max.x : box.min.x, i & 2 ? box.max.y : box.min.y,
                    i & 4 ? box.max.z : box.min.z);
        r.addPoint(m.transformPoint(corner));
    }
    return r;
}
// 0x080aedd0
void computeFrustumPoints(Vec3* points, float fov, float nearZ, float farZ, const Matrix4x3& m)
{
    float tanHalfFov = std::tan(fov * 0.5f);
    float nearHalf = tanHalfFov * nearZ, farHalf = tanHalfFov * farZ;
    points[0].set(-nearHalf, nearHalf, nearZ);
    points[1].set(nearHalf, nearHalf, nearZ);
    points[2].set(nearHalf, -nearHalf, nearZ);
    points[3].set(-nearHalf, -nearHalf, nearZ);
    points[4].set(-farHalf, farHalf, farZ);
    points[5].set(farHalf, farHalf, farZ);
    points[6].set(farHalf, -farHalf, farZ);
    points[7].set(-farHalf, -farHalf, farZ);
    for (int i = 0; i < 8; ++i)
        points[i] = m.transformPoint(points[i]);
}
// 0x080af960: four side planes from the far corners, then near and far planes.
void computeFrustumPlanes(Plane* planes, float fov, float nearZ, float farZ, const Matrix4x3& m)
{
    float farHalf = std::tan(fov * 0.5f) * farZ;
    Vec3 farCorners[4];
    farCorners[0] = m.transformPoint(Vec3(-farHalf, farHalf, farZ));
    farCorners[1] = m.transformPoint(Vec3(farHalf, farHalf, farZ));
    farCorners[2] = m.transformPoint(Vec3(farHalf, -farHalf, farZ));
    farCorners[3] = m.transformPoint(Vec3(-farHalf, -farHalf, farZ));
    for (int i = 0; i < 4; ++i)
    {
        Vec3 n = cross(farCorners[(i + 1) % 4] - m.pos, farCorners[i] - m.pos);
        n.normalize();
        planes[i].normal = n;
        planes[i].d = dot(m.pos, n);
    }
    planes[4].normal = m.z;
    planes[4].d = dot(m.pos + m.z * nearZ, m.z);
    planes[5].normal = -m.z;
    planes[5].d = dot(m.pos + m.z * farZ, -m.z);
}

} // namespace core
