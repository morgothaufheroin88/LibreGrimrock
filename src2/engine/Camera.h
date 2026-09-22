// Camera node and free-look controls, from Camera.cpp (0x080f76f0-0x080f9d00).
#pragma once
#include "core/Array.h"
#include "core/EventHandler.h"
#include "core/Prim.h"
#include "engine/Node.h"

namespace core
{
class Window;
template <class T> class Average
{
  public:
    explicit Average(int maxSamples = 10) : m_maxSamples(maxSamples) {}
    void addSample(T v)
    {
        if (m_samples.size() < m_maxSamples)
            m_samples.push_back(v);
        else
        {
            m_samples.erase(0);
            m_samples.push_back(v);
        }
    }
    T get() const
    {
        T sum = 0;
        for (int i = 0; i < m_samples.size(); ++i)
            sum += m_samples[i];
        return m_samples.size() ? sum / (T)m_samples.size() : sum;
    }
    T& back()
    {
        return m_samples.back();
    }
    int size() const
    {
        return m_samples.size();
    }

  private:
    Array<T> m_samples;
    int m_maxSamples;
};
} // namespace core

namespace engine
{

// D3D style clip space: z in [0, 1], w = z. (0x080f7870 / 0x080f7710)
void makePerspectiveProjectionMatrix(core::Matrix4x4* m, float fov, float aspect, float nearZ,
                                     float farZ);
void makeOrthoProjectionMatrix(core::Matrix4x4* m, const core::Vec3& min, const core::Vec3& max);

class Camera : public Node
{
  public:
    enum PlaneIndex
    {
        LeftPlane = 0,
        RightPlane,
        BottomPlane,
        TopPlane,
        NearPlane,
        FarPlane
    };

    Camera();
    Camera(float fov, float aspect, float nearZ, float farZ);
    explicit Camera(const core::Matrix4x4& projection);
    ~Camera();
    void notifyMoved();

    void setProjectionMatrix(const core::Matrix4x4& m);
    const core::Matrix4x4& getProjectionMatrix() const
    {
        return m_projection;
    }
    const core::Matrix4x4& getInverseProjectionMatrix() const
    {
        return m_invProjection;
    }
    const core::Matrix4x4& getViewProjectionMatrix() const;
    const core::Matrix4x4& getInverseViewProjectionMatrix() const;
    const core::Plane& getPlane(int i) const;
    float getNear() const;
    float getFar() const;
    // Grimrock 2: up to six extra world space clip planes for the culling (a bit of the
    // mask per plane), the mirrored winding of reflection cameras and the lod factor
    // scaling the distances of the skinning/shadow/dissolve tests.
    static constexpr int NumUserClipPlanes = 6;
    void setUserClipPlane(int i, const core::Plane& plane)
    {
        m_userClipPlanes[i] = plane;
    }
    const core::Plane& getUserClipPlane(int i) const
    {
        return m_userClipPlanes[i];
    }
    unsigned int getUserClipPlaneMask() const
    {
        return m_userClipPlaneMask;
    }
    void setUserClipPlaneMask(unsigned int mask)
    {
        m_userClipPlaneMask = mask;
    }
    bool getInverseCulling() const
    {
        return m_inverseCulling;
    }
    void setInverseCulling(bool b)
    {
        m_inverseCulling = b;
    }
    float getLodFactor() const
    {
        return m_lodFactor;
    }
    void setLodFactor(float f)
    {
        m_lodFactor = f;
    }
    // Normalized device coordinates in [0,1] with y down; z is the clip space depth.
    core::Vec3 projectWorldPoint(const core::Vec3& p) const;
    // Rays from a screen position in [0,1] x [0,1].
    core::Ray3 getViewRay(const core::Vec2& screen) const;
    core::Ray3 getWorldRay(const core::Vec2& screen) const;

  private:
    void validateMatrices() const;
    void validatePlanes() const;

    core::Matrix4x4 m_projection;
    core::Matrix4x4 m_invProjection;
    mutable core::Matrix4x4 m_viewProjection;
    mutable core::Matrix4x4 m_invViewProjection;
    mutable bool m_matricesDirty;
    mutable core::Plane m_planes[6];
    mutable float m_near;
    mutable float m_far;
    mutable bool m_planesDirty;
    core::Plane m_userClipPlanes[NumUserClipPlanes];
    unsigned int m_userClipPlaneMask;
    bool m_inverseCulling;
    float m_lodFactor;
};

// WASD/QE fly camera driven by window events (0x004b5800-0x004b5ea0).
class CameraControls : public core::EventHandler
{
  public:
    CameraControls(core::Window* window, Camera* camera, float scale);
    ~CameraControls();
    bool handle(core::KeyEvent* e);
    bool handle(core::MouseButtonEvent* e);
    bool handle(core::MouseMotionEvent* e);
    void update(float dt);
    Camera* getCamera() const
    {
        return m_pCamera;
    }
    void setCamera(Camera* camera)
    {
        m_pCamera = camera;
    }
    float getScale() const
    {
        return m_scale;
    }
    void setScale(float s)
    {
        m_scale = s;
    }
    // 0x004b56f0: disabling also releases the movement keys
    void setEnableControls(bool enable)
    {
        m_enableControls = enable;
        if (!enable)
            m_forward = m_backward = m_left = m_right = m_up = m_down = false;
    }
    bool getEnableControls() const
    {
        return m_enableControls;
    }

  private:
    [[maybe_unused]] core::Window* m_pWindow; // kept for the original layout
    Camera* m_pCamera;
    core::Average<float> m_mouseX;
    core::Average<float> m_mouseY;
    float m_pitch, m_yaw, m_roll;
    float m_velForward, m_velStrafe, m_velUp;
    bool m_forward, m_backward, m_left, m_right, m_up, m_down;
    float m_scale;
    bool m_enableControls;
};

} // namespace engine
