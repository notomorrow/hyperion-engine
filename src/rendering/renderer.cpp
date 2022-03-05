#include "renderer.h"
#include "shader_manager.h"
#include "environment.h"
#include "postprocess/filters/deferred_rendering_filter.h"

#include "../util.h"

namespace hyperion {
Renderer::Renderer()
    : m_fbo(nullptr),
      m_environment(Environment::GetInstance()),
      m_deferred_pipeline(new DeferredPipeline())
{
    // TODO: re-introduce frustum culling
    m_octree_callback_id = SceneManager::GetInstance()->GetOctree()->AddCallback([this](OctreeChangeEvent evt, const Octree *oct, int node_id, const Spatial *spatial, OctreeRawData_t raw_data) {
        if (evt == OCTREE_INSERT_NODE) {
            AssertExit(spatial != nullptr);

            for (int i = 0; i < Octree::VisibilityState::CameraType::VIS_CAMERA_MAX; i++) {
                m_all_items.GetBucket(spatial->GetBucket()).AddItem(BucketItem(node_id, *spatial));
            }
        } else if (evt == OCTREE_REMOVE_NODE) {
            AssertExit(spatial != nullptr);

            m_all_items.GetBucket(spatial->GetBucket()).RemoveItem(node_id);
        } else if (evt == OCTREE_VISIBILITY_STATE) {
            Octree::VisibilityState::CameraType camera_type = Octree::VisibilityState::CameraType(raw_data);
            
            for (const auto &node : oct->GetNodes()) {
                if (auto *ptr = m_all_items.GetBucket(node.m_spatial.GetBucket()).GetItemPtr(node.m_id)) {
                    ptr->m_flags |= 1 << camera_type; // temp
                }
            }
        }
    });
}

Renderer::~Renderer()
{
    SceneManager::GetInstance()->GetOctree()->RemoveCallback(m_octree_callback_id);

    delete m_deferred_pipeline;
    delete m_fbo;
}

void Renderer::Render(SystemWindow *window, Camera *cam, Octree::VisibilityState::CameraType camera_type)
{
    int width, height;
    window->GetSize(&width, &height);
    if (m_fbo == nullptr) {
        m_fbo = new Framebuffer2D(
            width,
            height,
            true, // color
            true, // depth
            true, // normals
            true, // positions
            true, // data
            true, // ao
            true, // tangents
            true  // bitangents
        );
    }

    if (!GetBucket(Spatial::Bucket::RB_BUFFER).IsEmpty()) {
        RenderBucket(cam, Spatial::Bucket::RB_BUFFER, camera_type); // PRE
    }

    m_deferred_pipeline->Render(this, cam, m_fbo);

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, Spatial::Bucket::RB_DEBUG, camera_type);
    RenderBucket(cam, Spatial::Bucket::RB_SCREEN, camera_type);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
}

void Renderer::RenderBucket(Camera *cam, Spatial::Bucket spatial_bucket, Octree::VisibilityState::CameraType camera_type, Shader *override_shader)
{
    Bucket &bucket = m_all_items.GetBucket(spatial_bucket);

    bool enable_frustum_culling = bucket.enable_culling;

    // proceed even if no shader is set if the render bucket is BUFFER.
    bool render_if_no_shader = &bucket == &GetBucket(Spatial::Bucket::RB_BUFFER);

    Shader *shader = nullptr;
    Shader *last_shader = nullptr;

    for (BucketItem &it : bucket.GetItems()) {
        if (!it.alive) {
            continue;
        }

        if (!(it.m_flags & (1 << camera_type))) {
            continue;
        }

        it.m_flags &= ~(1 << camera_type);

        shader = (override_shader ? override_shader : it.GetSpatial().GetRenderable()->m_shader.get());

        if (!shader && !render_if_no_shader) {
            continue;
        }

        if (shader) {
            shader->ApplyMaterial(it.GetSpatial().GetMaterial());
            shader->ApplyTransforms(it.GetSpatial().GetTransform(), cam);

#if RENDERER_SHADER_GROUPING
            if (shader != last_shader) {
                if (last_shader != nullptr) {
                    last_shader->End();
                }

                last_shader = shader;

                shader->Use(); // this will call ApplyUniforms() as well
            } else {
                shader->ApplyUniforms(); // re-using shader; have to apply uniforms manually
            }
#else
            shader->Use();
#endif

            it.GetSpatial().GetRenderable()->Render(this, cam);

#if !RENDERER_SHADER_GROUPING
            shader->End();
#endif

        } else if (render_if_no_shader) {
            it.GetSpatial().GetRenderable()->Render(this, cam);
        }

        SetRendererDefaults();
    }

#if RENDERER_SHADER_GROUPING
    if (last_shader != nullptr) {
        last_shader->End();
    }
#endif
}

void Renderer::SetRendererDefaults()
{
    CoreEngine::GetInstance()->BlendFunc(CoreEngine::GLEnums::ONE, CoreEngine::GLEnums::ZERO);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::DEPTH_TEST);
    CoreEngine::GetInstance()->DepthMask(true);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
    CoreEngine::GetInstance()->CullFace(CoreEngine::GLEnums::BACK);
    CoreEngine::GetInstance()->BindTexture(CoreEngine::GLEnums::TEXTURE_2D, 0);
    CoreEngine::GetInstance()->BindTexture(CoreEngine::GLEnums::TEXTURE_CUBE_MAP, 0);
}

void Renderer::RenderAll(Camera *cam, Octree::VisibilityState::CameraType camera_type, Framebuffer2D *fbo)
{
    if (!GetBucket(Spatial::Bucket::RB_BUFFER).IsEmpty()) {
        RenderBucket(cam, Spatial::Bucket::RB_BUFFER, camera_type); // PRE
    }

    if (fbo) {
        fbo->Use();
    } else {
        CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());
    }

    CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, Spatial::Bucket::RB_SKY, camera_type);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, Spatial::Bucket::RB_OPAQUE, camera_type);
    RenderBucket(cam, Spatial::Bucket::RB_TRANSPARENT, camera_type);
    RenderBucket(cam, Spatial::Bucket::RB_PARTICLE, camera_type);

    if (fbo) {
        fbo->End();
    }
}
} // namespace hyperion
