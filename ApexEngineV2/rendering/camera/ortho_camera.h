#ifndef ORTHO_CAMERA_H
#define ORTHO_CAMERA_H

#include "camera.h"

namespace apex {
class OrthoCamera : public Camera {
public:
    OrthoCamera(float left, float right, float bottom, float top, float near_clip, float far_clip);

    float GetLeft() const;
    float GetRight() const;
    float GetBottom() const;
    float GetTop() const;

    void SetLeft(float);
    void SetRight(float);
    void SetBottom(float);
    void SetTop(float);

    virtual void UpdateLogic(double dt);
    void UpdateMatrices();

private:
    float left, right, bottom, top;
};
}

#endif