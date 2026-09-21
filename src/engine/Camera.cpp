// Reconstructed from Grimrock.bin.x86 Camera.cpp.
#include "engine/Camera.h"
#include "core/Window.h"
#include <cmath>

namespace engine
{

using namespace core;

// 0x080f7870
void makePerspectiveProjectionMatrix(Matrix4x4* m, float fov, float aspect, float nearZ, float farZ)
{
    float tanHalfFov = (float)std::tan(fov * 0.5f);
    float depthScale = farZ / (farZ - nearZ);
    m->makeNull();
    m->m[0] = 1.0f / tanHalfFov;
    m->m[5] = 1.0f / (tanHalfFov * aspect);
    m->m[10] = depthScale;
    m->m[11] = 1.0f;
    m->m[14] = -depthScale * nearZ;
}
// 0x080f7710
void makeOrthoProjectionMatrix(Matrix4x4* m, const Vec3& mn, const Vec3& mx)
{
    float sx = 2.0f / (mx.x - mn.x), sy = 2.0f / (mx.y - mn.y), sz = 1.0f / (mx.z - mn.z);
    m->makeNull();
    m->m[0] = sx;
    m->m[5] = sy;
    m->m[10] = sz;
    m->m[12] = -mn.x * sx - 1.0f;
    m->m[13] = -mn.y * sy - 1.0f;
    m->m[14] = -mn.z * sz;
    m->m[15] = 1.0f;
}

// 0x080f8ed0
Camera::Camera() : m_matricesDirty(true), m_near(0.0f), m_far(0.0f), m_planesDirty(true) {}
// 0x080f89f0
Camera::Camera(float fov, float aspect, float nearZ, float farZ)
    : m_matricesDirty(true), m_near(0.0f), m_far(0.0f), m_planesDirty(true)
{
    Matrix4x4 p;
    makePerspectiveProjectionMatrix(&p, fov, aspect, nearZ, farZ);
    setProjectionMatrix(p);
}
// 0x080f8500
Camera::Camera(const Matrix4x4& projection)
    : m_matricesDirty(true), m_near(0.0f), m_far(0.0f), m_planesDirty(true)
{
    setProjectionMatrix(projection);
}
// 0x080f8040
Camera::~Camera() {}
// 0x080f76f0
void Camera::notifyMoved()
{
    m_matricesDirty = true;
    m_planesDirty = true;
}
// 0x080f78f0
void Camera::setProjectionMatrix(const Matrix4x4& m)
{
    m_projection = m;
    m_invProjection = m;
    m_invProjection.invert();
    m_planesDirty = true;
    m_matricesDirty = true;
}
// 0x080f7a60: viewProjection = projection * worldToLocal
void Camera::validateMatrices() const
{
    Matrix4x4 view(getWorldToLocalMatrix());
    m_viewProjection = m_projection * view;
    m_invViewProjection = m_viewProjection;
    m_invViewProjection.invert();
    m_matricesDirty = false;
}
// 0x080f9100: Gribb-Hartmann planes from the view projection rows; near/far from the
// projection alone.
void Camera::validatePlanes() const
{
    if (m_matricesDirty)
        validateMatrices();
    const float* m = m_viewProjection.m;
    // row r of the matrix: (m[r], m[4+r], m[8+r], m[12+r])
    struct Row
    {
        float x, y, z, w;
    };
    Row r0 = {m[0], m[4], m[8], m[12]};
    Row r1 = {m[1], m[5], m[9], m[13]};
    Row r2 = {m[2], m[6], m[10], m[14]};
    Row r3 = {m[3], m[7], m[11], m[15]};
    Row rows[6] = {{r3.x + r0.x, r3.y + r0.y, r3.z + r0.z, r3.w + r0.w},
                   {r3.x - r0.x, r3.y - r0.y, r3.z - r0.z, r3.w - r0.w},
                   {r3.x + r1.x, r3.y + r1.y, r3.z + r1.z, r3.w + r1.w},
                   {r3.x - r1.x, r3.y - r1.y, r3.z - r1.z, r3.w - r1.w},
                   {r2.x, r2.y, r2.z, r2.w},
                   {r3.x - r2.x, r3.y - r2.y, r3.z - r2.z, r3.w - r2.w}};
    for (int i = 0; i < 6; ++i)
    {
        float inv =
            1.0f / std::sqrt(rows[i].x * rows[i].x + rows[i].y * rows[i].y + rows[i].z * rows[i].z);
        m_planes[i].normal.set(rows[i].x * inv, rows[i].y * inv, rows[i].z * inv);
        m_planes[i].d = -rows[i].w * inv;
    }
    const float* proj = m_projection.m;
    float nearLen = std::sqrt(proj[2] * proj[2] + proj[6] * proj[6] + proj[10] * proj[10]);
    m_near = -proj[14] / nearLen;
    float fx = proj[3] - proj[2], fy = proj[7] - proj[6], fz = proj[11] - proj[10];
    m_far = (proj[15] - proj[14]) / std::sqrt(fx * fx + fy * fy + fz * fz);
    m_planesDirty = false;
}
// 0x080f8010
const Matrix4x4& Camera::getViewProjectionMatrix() const
{
    if (m_matricesDirty)
        validateMatrices();
    return m_viewProjection;
}
// 0x080f7fe0
const Matrix4x4& Camera::getInverseViewProjectionMatrix() const
{
    if (m_matricesDirty)
        validateMatrices();
    return m_invViewProjection;
}
// 0x080f9590
const Plane& Camera::getPlane(int i) const
{
    if (m_planesDirty)
        validatePlanes();
    return m_planes[i];
}
// 0x080f9560
float Camera::getNear() const
{
    if (m_planesDirty)
        validatePlanes();
    return m_near;
}
// 0x080f9530
float Camera::getFar() const
{
    if (m_planesDirty)
        validatePlanes();
    return m_far;
}
// 0x080f8090
Vec3 Camera::projectWorldPoint(const Vec3& p) const
{
    if (m_matricesDirty)
        validateMatrices();
    Vec4 clip = m_viewProjection.transform(Vec4(p, 1.0f));
    if (clip.w <= 1e-6f)
        return Vec3(0.0f, 0.0f, -1.0f);
    float invW = 1.0f / clip.w;
    return Vec3(clip.x * invW * 0.5f + 0.5f, clip.y * invW * -0.5f + 0.5f, clip.z * invW);
}
// 0x080f8260: unproject the screen point on the near plane (z = 0) through the
// inverse projection; the ray starts at the camera origin.
Ray3 Camera::getViewRay(const Vec2& screen) const
{
    float x = screen.x * 2.0f - 1.0f;
    float y = -(screen.y * 2.0f - 1.0f);
    Vec4 c = m_invProjection.transform(Vec4(x, y, 0.0f, 1.0f));
    Vec3 dir(c.x / c.w, c.y / c.w, c.z / c.w);
    dir.normalize();
    return Ray3(Vec3(0, 0, 0), dir);
}
// 0x080f8350
Ray3 Camera::getWorldRay(const Vec2& screen) const
{
    const Matrix4x3& m = getLocalToWorldMatrix();
    Ray3 view = getViewRay(screen);
    return Ray3(m.pos, m.transformVector(view.dir));
}

// ---- CameraControls -------------------------------------------------------------

// 0x080f95c0
CameraControls::CameraControls(Window* window, Camera* camera, float scale)
    : m_pWindow(window), m_pCamera(camera), m_mouseX(10), m_mouseY(10), m_pitch(0), m_yaw(0),
      m_roll(0), m_velForward(0), m_velStrafe(0), m_velUp(0), m_forward(false), m_backward(false),
      m_left(false), m_right(false), m_up(false), m_down(false), m_scale(scale)
{
    m_mouseX.addSample(0.0f);
    m_mouseY.addSample(0.0f);
}
// 0x080f8200
CameraControls::~CameraControls() {}
// 0x080f77a0: W S A D E Q
bool CameraControls::handle(KeyEvent* e)
{
    switch (e->key)
    {
    case 'W':
        m_forward = e->pressed;
        return true;
    case 'S':
        m_backward = e->pressed;
        return true;
    case 'A':
        m_left = e->pressed;
        return true;
    case 'D':
        m_right = e->pressed;
        return true;
    case 'E':
        m_up = e->pressed;
        return true;
    case 'Q':
        m_down = e->pressed;
        return true;
    default:
        return false;
    }
}
// 0x080f7820
bool CameraControls::handle(MouseButtonEvent* e)
{
    return false;
}
// 0x080f7830: mouse look is not wired up in this build.
bool CameraControls::handle(MouseMotionEvent* e)
{
    return false;
}
static float damp(float v, float amount)
{
    if (v > 0.0f)
        return v - amount < 0.0f ? 0.0f : v - amount;
    return v + amount > 0.0f ? 0.0f : v + amount;
}
// 0x080f97a0: averaged mouse deltas rotate, keys accelerate in camera space (up stays
// vertical).
void CameraControls::update(float dt)
{
    if (!m_pCamera)
        return;
    constexpr float Acceleration = 32.0f;
    constexpr float MaxSpeed = 6.0f;
    constexpr float Damping = 16.0f;
    float mouseX = m_mouseX.get(), mouseY = m_mouseY.get();
    m_mouseX.addSample(0.0f);
    m_mouseY.addSample(0.0f);
    m_pitch -= mouseY + mouseY;
    m_yaw -= mouseX + mouseX;
    if (m_forward)
        m_velForward += Acceleration * dt;
    if (m_backward)
        m_velForward -= Acceleration * dt;
    if (m_left)
        m_velStrafe -= Acceleration * dt;
    if (m_right)
        m_velStrafe += Acceleration * dt;
    if (m_up)
        m_velUp += Acceleration * dt;
    if (m_down)
        m_velUp -= Acceleration * dt;
    float len =
        std::sqrt(m_velForward * m_velForward + m_velStrafe * m_velStrafe + m_velUp * m_velUp);
    if (len > MaxSpeed)
    {
        float scale = MaxSpeed / len;
        m_velForward *= scale;
        m_velStrafe *= scale;
        m_velUp *= scale;
    }
    const Matrix4x3& local = m_pCamera->getLocalMatrix();
    Vec3 move((local.x.x * m_velStrafe * dt + local.z.x * m_velForward * dt) * m_scale,
              (local.x.y * m_velStrafe * dt + local.z.y * m_velForward * dt + m_velUp * dt) *
                  m_scale,
              (local.x.z * m_velStrafe * dt + local.z.z * m_velForward * dt) * m_scale);
    m_pCamera->move(move);
    m_pCamera->setRotation(Matrix3x3::createRotation(m_pitch, m_yaw, m_roll, Matrix3x3::ZXY));
    m_velForward = damp(m_velForward, dt * Damping);
    m_velStrafe = damp(m_velStrafe, dt * Damping);
    m_velUp = damp(m_velUp, dt * Damping);
}

} // namespace engine
