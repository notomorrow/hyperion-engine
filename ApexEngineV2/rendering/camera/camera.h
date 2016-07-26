#ifndef CAMERA_H
#define CAMERA_H

#include "../../math/vector3.h"
#include "../../math/matrix4.h"

namespace apex {
class Camera {
public:
    Camera(int width, int height);

    int GetWidth() const;
    void SetWidth(int w);
    int GetHeight() const;
    void SetHeight(int h);

    const Vector3 &GetTranslation() const;
    void SetTranslation(const Vector3 &vec);
    const Vector3 &GetDirection() const;
    void SetDirection(const Vector3 &vec);
    const Vector3 &GetUpVector() const;
    void SetUpVector(const Vector3 &vec);

    Matrix4 &GetViewMatrix();
    Matrix4 &GetProjectionMatrix();
    Matrix4 &GetViewProjectionMatrix();

    void Rotate(const Vector3 &axis, float radians);
    void Update(double dt);

    virtual void UpdateLogic(double dt) = 0;
    virtual void UpdateMatrices() = 0;

protected:
    Vector3 translation, direction, up;
    Matrix4 view_mat, proj_mat, view_proj_mat;

    int width, height;
};
}

#endif