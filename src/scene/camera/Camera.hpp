/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CAMERA_HPP
#define HYPERION_CAMERA_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/containers/Queue.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypObject.hpp>

#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>
#include <math/Frustum.hpp>
#include <math/Extent.hpp>

#include <input/InputHandler.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <GameCounter.hpp>

#include <atomic>
#include <mutex>

namespace hyperion {

class Engine;

enum class CameraType
{
    NONE,
    PERSPECTIVE,
    ORTHOGRAPHIC
};

struct CameraCommand
{
    enum
    {
        CAMERA_COMMAND_NONE,
        CAMERA_COMMAND_MAG,
        CAMERA_COMMAND_SCROLL,
        CAMERA_COMMAND_MOVEMENT
    } command;

    enum MovementType
    {
        CAMERA_MOVEMENT_NONE,
        CAMERA_MOVEMENT_LEFT,
        CAMERA_MOVEMENT_RIGHT,
        CAMERA_MOVEMENT_FORWARD,
        CAMERA_MOVEMENT_BACKWARD
    };

    union {
        struct {  // NOLINT(clang-diagnostic-microsoft-anon-tag)
            int mouse_x = 0,
                mouse_y = 0;
            float mx = 0.0f,
                  my = 0.0f; // in range -0.5f, 0.5f
        } mag_data;

        struct {  // NOLINT(clang-diagnostic-microsoft-anon-tag)
            int wheel_x = 0,
                wheel_y = 0;
        } scroll_data;

        struct {  // NOLINT(clang-diagnostic-microsoft-anon-tag)
            MovementType movement_type = CAMERA_MOVEMENT_NONE;
            float amount               = 1.0f;
        } movement_data;
    };
};

struct RENDER_COMMAND(UpdateCameraDrawProxy);

class Camera;

HYP_CLASS(Abstract)
class HYP_API CameraController : public EnableRefCountedPtrFromThis<CameraController>
{
    friend class Camera;

    HYP_OBJECT_BODY(CameraController);

public:
    CameraController(CameraType type);
    virtual ~CameraController() = default;

    InputHandler *GetInputHandler() const
        { return m_input_handler.Get(); }

    void SetInputHandler(UniquePtr<InputHandler> &&input_handler)
        { m_input_handler = std::move(input_handler); }

    Camera *GetCamera() const
        { return m_camera; }

    CameraType GetType() const
        { return m_type; }

    virtual bool IsMouseLocked() const
        { return false; }

    virtual void SetTranslation(const Vec3f &translation) { }
    virtual void SetNextTranslation(const Vec3f &translation) { }
    virtual void SetDirection(const Vec3f &direction) { }
    virtual void SetUpVector(const Vec3f &up) { }

    virtual void OnAdded(Camera *camera) = 0;

    virtual void UpdateLogic(double dt) = 0;
    virtual void UpdateViewMatrix() = 0;
    virtual void UpdateProjectionMatrix() = 0;

    void PushCommand(const CameraCommand &command);

protected:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) {}

    void UpdateCommandQueue(GameCounter::TickUnit dt);

    void SetMouseLocked(bool mouse_locked);

    Camera                  *m_camera;

    UniquePtr<InputHandler> m_input_handler;

    CameraType              m_type;

    std::mutex              m_command_queue_mutex;
    std::atomic_uint        m_command_queue_count;
    Queue<CameraCommand>    m_command_queue;
};

class PerspectiveCameraController;
class OrthoCameraController;
class FirstPersonCameraController;
class FollowCameraController;

struct CameraDrawProxy
{
    Matrix4     view;
    Matrix4     projection;
    Matrix4     previous_view;
    Vec3f       position;
    Vec3f       direction;
    Vec3f       up;
    Extent2D    dimensions;
    float       clip_near;
    float       clip_far;
    float       fov;
    Frustum     frustum;

    uint64      visibility_bitmask;
    uint16      visibility_nonce;
};

HYP_CLASS()
class HYP_API Camera : public BasicObject<Camera>
{
    HYP_OBJECT_BODY(Camera);

    HYP_PROPERTY(ID, &Camera::GetID)

public:
    friend class CameraController;
    friend class PerspectiveCameraController;
    friend class OrthoCameraController;
    friend class FirstPersonCameraController;
    friend class FollowCameraController;

    friend struct RENDER_COMMAND(UpdateCameraDrawProxy);

    Camera();
    Camera(int width, int height);
    Camera(float fov, int width, int height, float _near, float _far);
    Camera(int width, int height, float left, float right, float bottom, float top, float _near, float _far);
    ~Camera();

    HYP_FORCE_INLINE const FramebufferRef &GetFramebuffer() const
        { return m_framebuffer; }

    void SetFramebuffer(const FramebufferRef &framebuffer);

    HYP_METHOD(Property="Controller", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const RC<CameraController> &GetCameraController() const
        { return m_camera_controller; }

    HYP_METHOD(Property="Controller", Serialize=true, Editor=true)
    void SetCameraController(const RC<CameraController> &camera_controller)
    {
        m_camera_controller = camera_controller;

        if (m_camera_controller) {
            m_camera_controller->OnAdded(this);
        }

        UpdateViewMatrix();
        UpdateProjectionMatrix();
        UpdateViewProjectionMatrix();
    }

    void SetToPerspectiveProjection(
        float fov, float _near, float _far
    )
    {
        m_fov  = fov;
        m_near = _near;
        m_far  = _far;

        m_proj_mat = Matrix4::Perspective(
            m_fov,
            m_width, m_height,
            m_near,  m_far
        );
    }

    void SetToOrthographicProjection(
        float left, float right,
        float bottom, float top,
        float _near, float _far
    )
    {
        m_left = left;     m_right = right;
        m_bottom = bottom; m_top = top;
        m_near = _near;    m_far = _far;

        m_proj_mat = Matrix4::Orthographic(
            m_left,   m_right,
            m_bottom, m_top,
            m_near,   m_far
        );
    }

    HYP_METHOD(Property="Width", Serialize=true, Editor=true)
    HYP_FORCE_INLINE int GetWidth() const
        { return m_width; }


    HYP_METHOD(Property="Width", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetWidth(int width)
        { m_width = width; }

    HYP_METHOD(Property="Height", Serialize=true, Editor=true)
    HYP_FORCE_INLINE int GetHeight() const
        { return m_height; }

    HYP_METHOD(Property="Height", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetHeight(int height)
        { m_height = height; }

    HYP_METHOD(Property="Near", Serialize=true, Editor=true)
    HYP_FORCE_INLINE float GetNear() const
        { return m_near; }

    HYP_METHOD(Property="Near", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetNear(float _near)
        { m_near = _near; }

    HYP_METHOD(Property="Far", Serialize=true, Editor=true)
    HYP_FORCE_INLINE float GetFar() const
        { return m_far; }

    HYP_METHOD(Property="Far", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetFar(float _far)
        { m_far = _far; }

    // perspective only
    HYP_METHOD(Property="FOV", Serialize=true, Editor=true)
    HYP_FORCE_INLINE float GetFOV() const
        { return m_fov; }

    // ortho only
    HYP_METHOD(Property="Left", Serialize=true, Editor=true)
    HYP_FORCE_INLINE float GetLeft() const
        { return m_left; }

    HYP_METHOD(Property="Left", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetLeft(float left)
        { m_left = left; }

    HYP_METHOD(Property="Right", Serialize=true, Editor=true)
    HYP_FORCE_INLINE float GetRight() const
        { return m_right; }

    HYP_METHOD(Property="Right", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetRight(float right)
        { m_right = right; }

    HYP_METHOD(Property="Bottom", Serialize=true, Editor=true)
    HYP_FORCE_INLINE float GetBottom() const
        { return m_bottom; }

    HYP_METHOD(Property="Bottom", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetBottom(float bottom)
        { m_bottom = bottom; }

    HYP_METHOD(Property="Top", Serialize=true, Editor=true)
    HYP_FORCE_INLINE float GetTop() const
        { return m_top; }
        
    HYP_METHOD(Property="Top", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetTop(float top)
        { m_top = top; }

    HYP_METHOD(Property="Translation", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const Vec3f &GetTranslation() const
        { return m_translation; }

    HYP_METHOD(Property="Translation", Serialize=true, Editor=true)
    void SetTranslation(const Vec3f &translation);

    void SetNextTranslation(const Vec3f &translation);

    HYP_METHOD(Property="Direction", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const Vec3f &GetDirection() const
        { return m_direction; }

    HYP_METHOD(Property="Direction", Serialize=true, Editor=true)
    void SetDirection(const Vec3f &direction);

    HYP_METHOD(Property="Up", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const Vec3f &GetUpVector() const
        { return m_up; }

    HYP_METHOD(Property="Up", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetUpVector(const Vec3f &up);

    HYP_METHOD()
    HYP_FORCE_INLINE Vec3f GetSideVector() const
        { return m_up.Cross(m_direction); }

    HYP_METHOD()
    HYP_FORCE_INLINE Vec3f GetTarget() const
        { return m_translation + m_direction; }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetTarget(const Vec3f &target)
        { SetDirection(target - m_translation); }

    HYP_METHOD()
    void Rotate(const Vec3f &axis, float radians);

    HYP_METHOD(Property="Frustum", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const Frustum &GetFrustum() const
        { return m_frustum; }

    HYP_METHOD(Property="Frustum", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetFrustum(const Frustum &frustum)
        { m_frustum = frustum; }

    HYP_METHOD(Property="ViewMatrix", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const Matrix4 &GetViewMatrix() const
        { return m_view_mat; }

    HYP_METHOD(Property="ViewMatrix", Serialize=true, Editor=true)
    void SetViewMatrix(const Matrix4 &view_mat);

    HYP_METHOD(Property="ViewMatrix", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const Matrix4 &GetProjectionMatrix() const
        { return m_proj_mat; }

    HYP_METHOD(Property="ViewMatrix", Serialize=true, Editor=true)
    void SetProjectionMatrix(const Matrix4 &proj_mat);

    HYP_METHOD()
    HYP_FORCE_INLINE const Matrix4 &GetViewProjectionMatrix() const
        { return m_view_proj_mat; }

    HYP_METHOD()
    void SetViewProjectionMatrix(const Matrix4 &view_mat, const Matrix4 &proj_mat);

    HYP_METHOD()
    HYP_FORCE_INLINE const Matrix4 &GetPreviousViewMatrix() const
        { return m_previous_view_matrix; }

    /*! \brief Transform a 2D vector of x,y ranging from [0, 1] into ndc coordinates */
    HYP_METHOD()
    Vec3f TransformScreenToNDC(const Vec2f &screen) const;

    /*! \brief Transform a 3D vector in NDC space into world coordinates */
    HYP_METHOD()
    Vec4f TransformNDCToWorld(const Vec3f &ndc) const;

    /*! \brief Transform a 3D vector in world space into NDC space */
    HYP_METHOD()
    Vec3f TransformWorldToNDC(const Vec3f &world) const;

    /*! \brief Transform a 3D vector in world space into screen space */
    HYP_METHOD()
    Vec2f TransformWorldToScreen(const Vec3f &world) const;

    /*! \brief Transform a 3D vector in NDC into screen space */
    HYP_METHOD()
    Vec2f TransformNDCToScreen(const Vec3f &ndc) const;

    /*! \brief Transform a 2D vector of x,y ranging from [0, 1] into world coordinates */
    HYP_METHOD()
    Vec4f TransformScreenToWorld(const Vec2f &screen) const;

    HYP_METHOD()
    Vec2f GetPixelSize() const;

    HYP_FORCE_INLINE const CameraDrawProxy &GetProxy() const
        { return m_proxy; }

    void Update(GameCounter::TickUnit dt);
    void UpdateMatrices();

    void Init();

protected:
    void UpdateViewMatrix();
    void UpdateProjectionMatrix();
    void UpdateViewProjectionMatrix();

    RC<CameraController>    m_camera_controller;

    FramebufferRef          m_framebuffer;

    Vec3f                   m_translation, m_next_translation, m_direction, m_up;
    Matrix4                 m_view_mat, m_proj_mat;
    Frustum                 m_frustum;

    int                     m_width, m_height;
    float                   m_near, m_far;

    // only for perspective
    float                   m_fov;

    // only for ortho
    float                   m_left, m_right, m_bottom, m_top;

private:
    Matrix4                 m_view_proj_mat;
    Matrix4                 m_previous_view_matrix;
    
    CameraDrawProxy         m_proxy;
};

} // namespace hyperion

#endif
