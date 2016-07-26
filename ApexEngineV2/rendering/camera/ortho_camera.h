#ifndef ORTHO_CAMERA_H
#define ORTHO_CAMERA_H

#include "camera.h"

namespace apex {
class OrthoCamera : public Camera {
public:
    OrthoCamera(float left, float right, float bottom, float top, float n, float f);

    float GetLeft() const;
    float GetRight() const;
    float GetBottom() const;
    float GetTop() const;
    float GetNear() const;
    float GetFar() const;

    void SetLeft(float);
    void SetRight(float);
    void SetBottom(float);
    void SetTop(float);
    void SetNear(float);
    void SetFar(float);

    virtual void UpdateLogic(double dt);
    void UpdateMatrices();

private:
    float left, right, bottom, top, near_clip, far_clip;
};
}

#endif