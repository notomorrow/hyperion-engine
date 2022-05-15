#ifndef CAMERA_H
#define CAMERA_H

#include <math/vector3.h>
#include <math/matrix4.h>
#include <math/frustum.h>

namespace hyperion {

enum class CameraType {
    PERSPECTIVE,
    ORTHOGRAPHIC,
    OTHER
};

class Camera {
public:
    Camera(CameraType camera_type, int width, int height, float _near, float _far);
    virtual ~Camera() = default;

    CameraType GetCameraType() const { return m_camera_type; }

    int GetWidth() const        { return m_width; }
    void SetWidth(int width)    { m_width = width; }
    int GetHeight() const       { return m_height; }
    void SetHeight(int height)  { m_height = height; }
    float GetNear() const       { return m_near; }
    void SetNear(float _near)   { m_near = _near; }
    float GetFar() const        { return m_far; }
    void SetFar(float _far)     { m_far = _far; }
    float GetFov() const        { return m_fov; }

    const Vector3 &GetTranslation() const             { return m_translation; }
    virtual void SetTranslation(const Vector3 &translation);

    const Vector3 &GetDirection() const               { return m_direction; }
    void SetDirection(const Vector3 &direction)       { m_direction = direction; }

    const Vector3 &GetUpVector() const                { return m_up; }
    void SetUpVector(const Vector3 &up)               { m_up = up; }

    Vector3 GetSideVector() const                     { return m_up.Cross(m_direction); }

    Vector3 GetTarget() const                         { return m_translation + m_direction; }
    void SetTarget(const Vector3 &target)             { m_direction = target - m_translation; }

    const Frustum &GetFrustum() const                 { return m_frustum; }

    const Matrix4 &GetViewMatrix() const              { return m_view_mat; }
    void SetViewMatrix(const Matrix4 &view_mat)       { m_view_mat = view_mat; }

    const Matrix4 &GetProjectionMatrix() const        { return m_proj_mat; }
    void SetProjectionMatrix(const Matrix4 &proj_mat) { m_proj_mat = proj_mat; }

    const Matrix4 &GetViewProjectionMatrix() const    { return m_view_proj_mat; }

    void Rotate(const Vector3 &axis, float radians);
    void Update(double dt);

    virtual void UpdateLogic(double dt) = 0;
    virtual void UpdateViewMatrix() = 0;
    virtual void UpdateProjectionMatrix() = 0;
    void UpdateMatrices();

protected:
    CameraType m_camera_type;

    Vector3 m_translation, m_direction, m_up;
    Matrix4 m_view_mat, m_proj_mat;
    Frustum m_frustum;

    int m_width, m_height;
    float m_near, m_far;
    float m_fov; // only for perspective

private:
    Matrix4 m_view_proj_mat;

    void UpdateFrustum();
};
}

#endif
