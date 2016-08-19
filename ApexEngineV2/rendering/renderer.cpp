#include "renderer.h"

namespace apex {
Renderer::Renderer()
{
    opaque_bucket.reserve(30);
    transparent_bucket.reserve(30);
    sky_bucket.reserve(30);
}

void Renderer::ClearRenderables()
{
    opaque_bucket.clear();
    transparent_bucket.clear();
    sky_bucket.clear();
}

void Renderer::FindRenderables(Entity *top)
{
    if (top->GetRenderable() != nullptr) {
        switch (top->GetRenderable()->GetRenderBucket()) {
        case Renderable::RB_OPAQUE:
            opaque_bucket.push_back(std::make_pair(top->GetRenderable().get(), top->GetGlobalTransform()));
            break;
        case Renderable::RB_TRANSPARENT:
            transparent_bucket.push_back(std::make_pair(top->GetRenderable().get(), top->GetGlobalTransform()));
            break;
        case Renderable::RB_SKY:
            sky_bucket.push_back(std::make_pair(top->GetRenderable().get(), top->GetGlobalTransform()));
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
        if (renderable->shader != nullptr) {
            renderable->shader->ApplyMaterial(renderable->GetMaterial());
            renderable->shader->ApplyTransforms(transform.GetMatrix(), cam);
            renderable->shader->Use();
            renderable->Render();
            renderable->shader->End();
        }
    }
}

void Renderer::RenderAll(Camera *cam)
{
    CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());
    RenderBucket(cam, sky_bucket);
    RenderBucket(cam, opaque_bucket);
    RenderBucket(cam, transparent_bucket);
}
}