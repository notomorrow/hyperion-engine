#ifndef CAMERA_H
#define CAMERA_H

#include "../../math/vector3.h"
#include "../../math/matrix4.h"

namespace apex {
class Camera {
public:
    Camera(int width, int height, float near_clip, float far_clip);
    virtual ~Camera() = default;

    int GetWidth() const;
    void SetWidth(int w);
    int GetHeight() const;
    void SetHeight(int h);

    float GetNear() const;
    void SetNear(float n);
    float GetFar() const;
    void SetFar(float f);

    const Vector3 &GetTranslation() const;
    virtual void SetTranslation(const Vector3 &vec);
    const Vector3 &GetDirection() const;
    virtual void SetDirection(const Vector3 &vec);
    const Vector3 &GetUpVector() const;
    virtual void SetUpVector(const Vector3 &vec);

    Matrix4 &GetViewMatrix();
    void SetViewMatrix(const Matrix4 &mat);
    Matrix4 &GetProjectionMatrix();
    void SetProjectionMatrix(const Matrix4 &mat);
    Matrix4 &GetViewProjectionMatrix();
    void SetViewProjectionMatrix(const Matrix4 &mat);

    void Rotate(const Vector3 &axis, float radians);
    void Update(double dt);

    virtual void UpdateLogic(double dt) = 0;
    virtual void UpdateMatrices() = 0;

protected:
    Vector3 translation, direction, up;
    Matrix4 view_mat, proj_mat, view_proj_mat;

    int width, height;
    float near_clip, far_clip;
};
}

#endif