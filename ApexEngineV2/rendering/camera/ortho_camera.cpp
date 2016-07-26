#include "ortho_camera.h"
#include "../../math/matrix_util.h"

namespace apex {
OrthoCamera::OrthoCamera(float left, float right, float bottom, float top, float n, float f)
    : Camera(512, 512), left(left), right(right), bottom(bottom), top(top), near_clip(n), far_clip(f)
{
}

float OrthoCamera::GetLeft() const
{
    return left;
}

float OrthoCamera::GetRight() const
{
    return right;
}

float OrthoCamera::GetBottom() const
{
    return bottom;
}

float OrthoCamera::GetTop() const
{
    return top;
}

float OrthoCamera::GetNear() const
{
    return near_clip;
}

float OrthoCamera::GetFar() const
{
    return far_clip;
}

void OrthoCamera::SetLeft(float f)
{
    left = f;
}

void OrthoCamera::SetRight(float f)
{
    right = f;
}

void OrthoCamera::SetBottom(float f)
{
    bottom = f;
}

void OrthoCamera::SetTop(float f)
{
    top = f;
}

void OrthoCamera::SetNear(float f)
{
    near_clip = f;
}

void OrthoCamera::SetFar(float f)
{
    far_clip = f;
}

void OrthoCamera::UpdateLogic(double dt)
{
}

void OrthoCamera::UpdateMatrices()
{
    MatrixUtil::ToOrtho(proj_mat,
        left, right,
        bottom, top,
        near_clip, far_clip);

    view_proj_mat = view_mat * proj_mat;
}
}