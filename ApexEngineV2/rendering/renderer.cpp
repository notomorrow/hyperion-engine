#include "renderer.h"
#include "framebuffer_2d.h"
#include "shader_manager.h"
#include "postprocess/filters/deferred_rendering_filter.h"

#include <cassert>

namespace apex {
Renderer::Renderer(const RenderWindow &render_window)
    : m_render_window(render_window),
      m_fbo(nullptr),
      m_is_deferred(false)
{
    m_post_processing = new PostProcessing();

    m_buckets[Renderable::RB_SKY].enable_culling = false;
    m_buckets[Renderable::RB_PARTICLE].enable_culling = false; // TODO
    m_buckets[Renderable::RB_SCREEN].enable_culling = false;
    m_buckets[Renderable::RB_DEBUG].enable_culling = false;
}

Renderer::~Renderer()
{
    delete m_post_processing;
    delete m_fbo;
}

void Renderer::Begin(Camera *cam, Entity *top)
{
    FindRenderables(cam, top, false, true);
}

void Renderer::Render(Camera *cam)
{
    if (m_fbo == nullptr) {
        m_fbo = new Framebuffer2D(
            m_render_window.GetScaledWidth(),
            m_render_window.GetScaledHeight(),
            true, // color
            true, // depth
            true, // normals
            true, // positions
            true  // data
        );
    }

    RenderAll(cam, m_fbo);
}

void Renderer::End(Camera *cam, Entity *top)
{
    RenderPost(cam, m_fbo);

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, m_buckets[Renderable::RB_SCREEN]);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
}

void Renderer::ClearRenderables()
{
   for (int i = 0; i < sizeof(m_buckets) / sizeof(Bucket); i++) {
       m_buckets[i].ClearAll();
   }
}

void Renderer::FindRenderables(Camera *cam, Entity *top, bool frustum_culled, bool is_root)
{
    const size_t entity_hash = top->GetHashCode().Value();
    size_t previous_hash = 0;
    const auto it = m_hash_cache.find(top);
    bool recalc = true;

    Bucket *new_bucket = nullptr;

    if (it != m_hash_cache.end()) {
        previous_hash = it->second;

        if (previous_hash == entity_hash) {
            recalc = false;
        }
    }

    if (recalc) {
        if (top->PendingRemoval()) {
            m_hash_cache.erase(top);
        } else {
            m_hash_cache[top] = entity_hash;
        }
    }

    if (top->GetRenderable() != nullptr) { // NOTE: this currently doesnt handle renderable being set to null.
        const Renderable::RenderBucket new_bucket_index = top->GetRenderable()->GetRenderBucket();

        assert(new_bucket_index < sizeof(m_buckets) / sizeof(Bucket));

        new_bucket = &m_buckets[new_bucket_index];
    
        if (recalc) {
            bool push_new = true;

            if (previous_hash != 0) {
                auto item_in_existing_bucket = m_hash_to_bucket.find(previous_hash);

                Bucket *previous_bucket = nullptr;

                // IF bucket has changed, we have to remove the entry from that bucket first.
                if (item_in_existing_bucket != m_hash_to_bucket.end()) {
                    const Renderable::RenderBucket previous_bucket_index = item_in_existing_bucket->second;

                    assert(previous_bucket_index < sizeof(m_buckets) / sizeof(Bucket));

                    // remove from prev bucket
                    previous_bucket = &m_buckets[previous_bucket_index];

                    m_hash_to_bucket.erase(item_in_existing_bucket);
                }

                if (previous_bucket != nullptr) {
                    if (top->PendingRemoval() || new_bucket != previous_bucket) {
                        previous_bucket->RemoveItem(previous_hash);
                    } else {
                        BucketItem bucket_item;
                        bucket_item.renderable = top->GetRenderable().get();
                        bucket_item.material = &top->GetMaterial();
                        bucket_item.aabb = top->GetAABB();
                        bucket_item.transform = top->GetGlobalTransform();
                        bucket_item.hash_code = entity_hash;

                        previous_bucket->SetItem(previous_hash, bucket_item);

                        push_new = false;
                    }
                }
            }

            if (!top->PendingRemoval()) {
                m_hash_to_bucket[entity_hash] = new_bucket_index;

                if (push_new) {
                    BucketItem bucket_item;
                    bucket_item.renderable = top->GetRenderable().get();
                    bucket_item.material = &top->GetMaterial();
                    bucket_item.aabb = top->GetAABB();
                    bucket_item.transform = top->GetGlobalTransform();
                    bucket_item.hash_code = entity_hash;

                    new_bucket->AddItem(bucket_item);
                }
            }
        }
    }

    if (!frustum_culled && !is_root) {
        frustum_culled = !MemoizedFrustumCheck(cam, top->GetAABB());
    }

    if (new_bucket != nullptr) {
        if (BucketItem *bucket_item_ptr = new_bucket->GetItemPtr(entity_hash)) {
            bucket_item_ptr->frustum_culled = frustum_culled;
        }
    }

    for (size_t i = 0; i < top->NumChildren(); i++) {
        Entity *child = top->GetChild(i).get();
        FindRenderables(cam, child, frustum_culled);
    }

    for (size_t i = 0; i < top->NumChildrenPendingRemoval(); i++) {
        Entity *child = top->GetChildPendingRemoval(i).get();
        FindRenderables(cam, child, frustum_culled);
    }
}

bool Renderer::MemoizedFrustumCheck(Camera *cam, const BoundingBox &aabb)
{
    return cam->GetFrustum().BoundingBoxInFrustum(aabb);
}

void Renderer::RenderBucket(Camera *cam, Bucket &bucket, Shader *override_shader, bool enable_frustum_culling)
{
    enable_frustum_culling = enable_frustum_culling && bucket.enable_culling;

    Shader *shader = nullptr;

    for (const BucketItem &it : bucket.GetItems()) {
        if (!it.alive) {
            continue;
        }

        if (enable_frustum_culling && it.frustum_culled) {
            continue;
        }

        // TODO: group by same shader
        shader = (override_shader ? override_shader : it.renderable->m_shader.get());

        if (shader) {
            shader->ApplyMaterial(*it.material);
            shader->ApplyTransforms(it.transform, cam);
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

        CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);
    } else {
        CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());
    }

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, m_buckets[Renderable::RB_SKY]);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, m_buckets[Renderable::RB_OPAQUE]);
    RenderBucket(cam, m_buckets[Renderable::RB_TRANSPARENT]);
    RenderBucket(cam, m_buckets[Renderable::RB_PARTICLE]);
    RenderBucket(cam, m_buckets[Renderable::RB_DEBUG]);

    if (fbo) {
        fbo->End();
    }
}

void Renderer::RenderPost(Camera *cam, Framebuffer *fbo)
{
    if (m_post_processing->GetFilters().empty()) {
        return;
    }

    m_post_processing->Render(cam, fbo);
}

void Renderer::SetDeferred(bool deferred)
{
    if (m_is_deferred == deferred) {
        return;
    }

    if (deferred) {
        m_post_processing->AddFilter<DeferredRenderingFilter>("deferred", 10);
    } else {
        m_post_processing->RemoveFilter("deferred");
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("DEFERRED", deferred)
    );

    m_is_deferred = deferred;
}
} // namespace apex
