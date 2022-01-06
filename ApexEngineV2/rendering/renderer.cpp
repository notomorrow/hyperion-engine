#include "renderer.h"
#include "framebuffer_2d.h"

namespace apex {
Renderer::Renderer(const RenderWindow &render_window)
    : m_render_window(render_window),
      m_fbo(nullptr)
{
    m_post_processing = new PostProcessing();

    sky_bucket.reserve(5);
    opaque_bucket.reserve(30);
    transparent_bucket.reserve(20);
    particle_bucket.reserve(5);
}

Renderer::~Renderer()
{
    delete m_post_processing;
    delete m_fbo;
}

void Renderer::Begin(Camera *cam, Entity *top)
{
    FindRenderables(cam, top);
}

void Renderer::Render(Camera *cam)
{
    if (m_fbo == nullptr) {
        m_fbo = new Framebuffer2D(m_render_window.GetScaledWidth(), m_render_window.GetScaledHeight());
    }

    RenderAll(cam, m_fbo);
    RenderPost(cam, m_fbo);
}

void Renderer::End(Camera *cam)
{
    ClearRenderables();
    RenderPost(cam, m_fbo);
}

void Renderer::ClearRenderables()
{
    // TODO: proper fragmentention - change detection to not require clearing and processing again each time
    sky_bucket.clear();
    opaque_bucket.clear();
    transparent_bucket.clear();
    particle_bucket.clear();
}

void Renderer::FindRenderables(Camera *cam, Entity *top)
{
    if (top->GetRenderable() != nullptr) {
        if (top->GetRenderable()->GetRenderBucket() != Renderable::RB_SKY) {
            const Frustum &frustum = cam->GetFrustum();

            if (!frustum.BoundingBoxInFrustum(top->GetAABB())) {
                return;
            }
        }

        BucketItem bucket_item;
        bucket_item.renderable = top->GetRenderable().get();
        bucket_item.material = &top->GetMaterial();
        bucket_item.transform = top->GetGlobalTransform();

        switch (top->GetRenderable()->GetRenderBucket()) {
            case Renderable::RB_SKY:
                sky_bucket.push_back(bucket_item);
                break;
            case Renderable::RB_OPAQUE:
                opaque_bucket.push_back(bucket_item);
                break;
            case Renderable::RB_TRANSPARENT:
                transparent_bucket.push_back(bucket_item);
                break;
            case Renderable::RB_PARTICLE:
                particle_bucket.push_back(bucket_item);
                break;
        }
    }

    for (size_t i = 0; i < top->NumChildren(); i++) {
        Entity *child = top->GetChild(i).get();
        FindRenderables(cam, child);
    }
}

void Renderer::RenderBucket(Camera *cam, Bucket_t &bucket, Shader *override_shader)
{
    Shader *shader = nullptr;

    // TODO: group by same shader
    for (BucketItem &it : bucket) {
        if ((shader = override_shader ? override_shader : it.renderable->m_shader.get())) {
            shader->ApplyMaterial(*it.material);
            shader->ApplyTransforms(it.transform.GetMatrix(), cam);
            shader->Use();
            it.renderable->Render();
            shader->End();
        }
    }
}

void Renderer::RenderAll(Camera *cam, Framebuffer *fbo)
{
    if (fbo) {
        fbo->Use();

        // todo: test
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else {
        CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());
    }

    glDisable(GL_CULL_FACE);
    RenderBucket(cam, sky_bucket);
    glEnable(GL_CULL_FACE);
    RenderBucket(cam, opaque_bucket);
    RenderBucket(cam, transparent_bucket);
    RenderBucket(cam, particle_bucket);

    if (fbo) {
        fbo->End();
    }
}

void Renderer::RenderPost(Camera *cam, Framebuffer *fbo)
{
    if (m_post_processing->GetFilters().empty()) {
        return;
    }

    //glDisable(GL_CULL_FACE);

    m_post_processing->Render(cam, fbo);

    //glEnable(GL_CULL_FACE);
}
} // namespace apex
