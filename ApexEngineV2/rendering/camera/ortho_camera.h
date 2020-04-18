#ifndef ORTHO_CAMERA_H
#define ORTHO_CAMERA_H

#include "camera.h"

namespace apex {
class OrthoCamera : public Camera {
public:
    OrthoCamera(float left, float right, float bottom, float top, float near, float far);
    virtual ~OrthoCamera() = default;

    inline float GetLeft() const        { return m_left; }
    inline void SetLeft(float left)     { m_left = left; }
    inline float GetRight() const       { return m_right; }
    inline void SetRight(float right)   { m_right = right; }
    inline float GetBottom() const      { return m_bottom; }
    inline void SetBottom(float bottom) { m_bottom = bottom; }
    inline float GetTop() const         { return m_top; }
    inline void SetTop(float top)       { m_top = top; }

    virtual void UpdateLogic(double dt);
    void UpdateMatrices();

private:
    float m_left, m_right, m_bottom, m_top;
};
}

#endif