#include "renderer.h"
#include "framebuffer_2d.h"

#include <cassert>

namespace apex {
Renderer::Renderer(const RenderWindow &render_window)
    : m_render_window(render_window),
      m_fbo(nullptr)
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
        m_fbo = new Framebuffer2D(m_render_window.GetScaledWidth(), m_render_window.GetScaledHeight());
    }

    RenderAll(cam, m_fbo);
}

void Renderer::End(Camera *cam, Entity *top)
{
    RenderPost(cam, m_fbo);

    glDisable(GL_CULL_FACE);
    RenderBucket(cam, m_buckets[Renderable::RB_SCREEN]);
    glEnable(GL_CULL_FACE);
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
        /*const float far_squared = cam->GetFar() * cam->GetFar();
        const float radius = top->GetAABB().GetDimensions().LengthSquared();
        const float distance = top->GetAABB().GetCenter().DistanceSquared(cam->GetTranslation());

        if (distance - radius >= far_squared) {
            frustum_culled = true;
        } else {*/
            frustum_culled = !MemoizedFrustumCheck(cam, top->GetAABB());
        //}
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

void Renderer::RenderBucket(Camera *cam, Bucket &bucket, Shader *override_shader)
{
    Shader *shader = nullptr;

    for (const BucketItem &it : bucket.GetItems()) {
        if (!it.alive) {
            continue;
        }

        if (bucket.enable_culling && it.frustum_culled) {
            continue;
        }

        // TODO: group by same shader
        if ((shader = override_shader ? override_shader : it.renderable->m_shader.get())) {
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

        // todo: test
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else {
        CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());
    }

    glDisable(GL_CULL_FACE);
    RenderBucket(cam, m_buckets[Renderable::RB_SKY]);
    glEnable(GL_CULL_FACE);
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

    //glDisable(GL_CULL_FACE);

    m_post_processing->Render(cam, fbo);

    //glEnable(GL_CULL_FACE);
}
} // namespace apex
