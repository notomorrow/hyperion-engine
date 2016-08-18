#include "ortho_camera.h"
#include "../../math/matrix_util.h"

namespace apex {
OrthoCamera::OrthoCamera(float left, float right, float bottom, float top, float near_clip, float far_clip)
    : Camera(512, 512, near_clip, near_clip), left(left), right(right), bottom(bottom), top(top)
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

void OrthoCamera::UpdateLogic(double dt)
{
}

void OrthoCamera::UpdateMatrices()
{
    Vector3 target = translation + direction;

    MatrixUtil::ToLookAt(view_mat, translation, target, up);
    MatrixUtil::ToOrtho(proj_mat,
        left, right,
        bottom, top,
        near_clip, far_clip);

    view_proj_mat = view_mat * proj_mat;
}
}