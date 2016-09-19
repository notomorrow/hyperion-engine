#include "renderer.h"

namespace apex {
Renderer::Renderer()
{
    sky_bucket.reserve(5);
    opaque_bucket.reserve(30);
    transparent_bucket.reserve(20);
    particle_bucket.reserve(5);
}

void Renderer::ClearRenderables()
{
    sky_bucket.clear();
    opaque_bucket.clear();
    transparent_bucket.clear();
    particle_bucket.clear();
}

void Renderer::FindRenderables(Entity *top)
{
    if (top->GetRenderable() != nullptr) {
        switch (top->GetRenderable()->GetRenderBucket()) {
        case Renderable::RB_SKY:
            sky_bucket.push_back(std::make_pair(top->GetRenderable().get(), top->GetGlobalTransform()));
            break;
        case Renderable::RB_OPAQUE:
            opaque_bucket.push_back(std::make_pair(top->GetRenderable().get(), top->GetGlobalTransform()));
            break;
        case Renderable::RB_TRANSPARENT:
            transparent_bucket.push_back(std::make_pair(top->GetRenderable().get(), top->GetGlobalTransform()));
            break;
        case Renderable::RB_PARTICLE:
            particle_bucket.push_back(std::make_pair(top->GetRenderable().get(), top->GetGlobalTransform()));
            break;
        }
    }

    for (size_t i = 0; i < top->NumChildren(); i++) {
        Entity *child = top->GetChild(i).get();
        FindRenderables(child);
    }
}

void Renderer::RenderBucket(Camera *cam, std::vector<std::pair<Renderable*, Transform>> &bucket)
{
    for (auto it : bucket) {
        auto renderable = it.first;
        auto transform = it.second;
        if (renderable->m_shader != nullptr) {
            renderable->m_shader->ApplyMaterial(renderable->GetMaterial());
            renderable->m_shader->ApplyTransforms(transform.GetMatrix(), cam);
            renderable->m_shader->Use();
            renderable->Render();
            renderable->m_shader->End();
        }
    }
}

void Renderer::RenderAll(Camera *cam)
{
    CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());
    RenderBucket(cam, sky_bucket);
    RenderBucket(cam, opaque_bucket);
    RenderBucket(cam, transparent_bucket);
    RenderBucket(cam, particle_bucket);
}
} // namespace apex