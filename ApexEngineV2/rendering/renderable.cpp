#include "renderable.h"

namespace apex {
Renderable::RenderBucket Renderable::GetRenderBucket() const
{
    return bucket;
}

void Renderable::SetRenderBucket(RenderBucket rb)
{
    bucket = rb;
}

std::shared_ptr<Shader> Renderable::GetShader()
{
    return shader;
}

void Renderable::SetShader(std::shared_ptr<Shader> ptr)
{
    shader = ptr;
}

Material &Renderable::GetMaterial()
{
    return material;
}

void Renderable::SetMaterial(const Material &mat)
{
    material = mat;
}
}