#ifndef PERSPECTIVE_CAMERA_H
#define PERSPECTIVE_CAMERA_H

#include "camera.h"

namespace apex {
class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(float fov, int width, int height, float near, float far);
    virtual ~PerspectiveCamera() = default;

    virtual void UpdateLogic(double dt);
    void UpdateMatrices();
};
} // namespace apex

#endif
