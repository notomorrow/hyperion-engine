#ifndef PERSPECTIVE_CAMERA_H
#define PERSPECTIVE_CAMERA_H

#include "camera.h"

namespace apex {
class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(float fov, int width, int height, float near_clip, float far_clip);

    virtual void UpdateLogic(double dt);
    void UpdateMatrices();

protected:
    float fov;
};
}

#endif