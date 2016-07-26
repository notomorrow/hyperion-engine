#include "renderer.h"

namespace apex {
void Renderer::ClearRenderables()
{
    opaque_bucket.clear();
    transparent_bucket.clear();
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
        if (it.first->shader != nullptr) {
            it.first->shader->ApplyTransforms(it.second.GetMatrix(),
                cam->GetViewMatrix(), cam->GetProjectionMatrix());
            it.first->shader->Use();
            it.first->Render();
            it.first->shader->End();
        }
    }
}

void Renderer::RenderAll(Camera *cam)
{
    CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());
    RenderBucket(cam, opaque_bucket);
    RenderBucket(cam, transparent_bucket);
}
}