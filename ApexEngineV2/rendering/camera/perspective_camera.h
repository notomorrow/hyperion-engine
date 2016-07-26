#ifndef PERSPECTIVE_CAMERA_H
#define PERSPECTIVE_CAMERA_H

#include "camera.h"

namespace apex {
class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(float fov, int width, int height, float n, float f);

    virtual void UpdateLogic(double dt);
    void UpdateMatrices();

protected:
    float fov, near_clip, far_clip;
    Vector3 target;
};
}

#endif