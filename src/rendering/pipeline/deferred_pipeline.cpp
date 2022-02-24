#include "deferred_pipeline.h"
#include "../postprocess/filters/deferred_rendering_filter.h"
#include "../../util/mesh_factory.h"
#include "../renderer.h"
#include "../camera/camera.h"

#include "../../gl_util.h"

namespace hyperion {

DeferredPipeline::DeferredPipeline()
    : m_pre_filters(new FilterStack()),
      m_post_filters(new FilterStack()),
      m_deferred_filter(new DeferredRenderingFilter()),
      m_blit_fbo(nullptr),
      m_gbuffer_initialized(false)
{
    m_post_filters->SetSavesLastToFbo(false); // render last in stack to backbuffer

    m_quad = MeshFactory::CreateQuad(true);
}

DeferredPipeline::~DeferredPipeline()
{
    if (m_blit_fbo != nullptr) {
        delete m_blit_fbo;
    }

    delete m_deferred_filter;
    delete m_post_filters;
    delete m_pre_filters;
}

void DeferredPipeline::RenderOpaqueBuckets(Renderer *renderer, Camera *cam, Framebuffer2D *fbo)
{
    fbo->Use();

    CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);
    renderer->RenderBucket(cam, Spatial::Bucket::RB_SKY, Octree::VisibilityState::CameraType::VIS_CAMERA_MAIN);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);

    renderer->RenderBucket(cam, Spatial::Bucket::RB_OPAQUE, Octree::VisibilityState::CameraType::VIS_CAMERA_MAIN);

    fbo->End();
}

void DeferredPipeline::InitializeBlitFbo(Framebuffer2D *read_fbo)
{
    ex_assert(m_blit_fbo == nullptr);

    m_blit_fbo = new Framebuffer2D(
        read_fbo->GetWidth(),
        read_fbo->GetHeight(),
        true,
        true,
        true,
        true,
        true,
        true
    );
}

void DeferredPipeline::InitializeGBuffer(Framebuffer2D *read_fbo)
{
    for (int i = 0; i < m_gbuffer.size(); i++) {
        Framebuffer::FramebufferAttachment attachment = Framebuffer::OrdinalToAttachment(i);

        if (Framebuffer::default_texture_attributes[i].is_volatile) {
            m_gbuffer[i] = Framebuffer2D::MakeTexture(
                attachment,
                read_fbo->GetWidth(),
                read_fbo->GetHeight(),
                nullptr
            );
        } else {
            // no need to copy
            m_gbuffer[i] = read_fbo->GetAttachment(attachment);
        }
    }
}

void DeferredPipeline::CopyFboTextures(Framebuffer2D *read_fbo)
{
    // Copy data from FBO into our textures
    CoreEngine::GetInstance()->BindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo->GetId());

    // TODO: maybe for first pass we can just use texture from fbo without copying...
    /// but is there a point if we have to copy anyway?
    for (int i = 0; i < m_gbuffer.size(); i++) {
        Framebuffer::FramebufferAttachment attachment = Framebuffer::OrdinalToAttachment(i);

        if (!Framebuffer::default_texture_attributes[i].is_volatile) {
            continue;
        }

        read_fbo->Store(attachment, m_gbuffer[i]);
    }

    CoreEngine::GetInstance()->BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void DeferredPipeline::Render(Renderer *renderer, Camera *cam, Framebuffer2D *fbo)
{
    /*
     * Render opaque objects in the scene into `fbo`.
     * All textures in the g-buffer will be written to.
     * Later, we will copy the depth buffer from this pass into
     * the blit framebuffer, where we'll render transparent objects
     * as an overlay, post-lighting.
    */
    RenderOpaqueBuckets(renderer, cam, fbo);

    if (m_blit_fbo == nullptr) {
        InitializeBlitFbo(fbo);
    }

    if (!m_gbuffer_initialized) {
        InitializeGBuffer(fbo);

        m_gbuffer_initialized = true;

        m_pre_filters->SetGBuffer(non_owning_ptr(&m_gbuffer));
        m_post_filters->SetGBuffer(non_owning_ptr(&m_gbuffer));
    }

    CopyFboTextures(fbo);

    /* We disable depth test and write as we are just rendering a quad to the entire screen */
    CoreEngine::GetInstance()->DepthMask(false);
    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::DEPTH_TEST);

    /* Render "pre" stage - effects such as SSAO write into a color attachment to be used later */
    m_pre_filters->Render(cam, fbo, m_blit_fbo);

    /* Render lighting into `m_blit_framebuffer` - transparent objects will be rendering on top of this */
    m_blit_fbo->Use();
    m_deferred_filter->Begin(cam, m_gbuffer);
    m_quad->Render(renderer, cam);
    // store from blit fbo to gbuffer
    m_deferred_filter->End(cam, m_blit_fbo, m_gbuffer, true);
    m_blit_fbo->End();

    /*
     * Render transparent objects on top of the deferred objects
     * Note that we have to blit the depth buffer from `fbo` (which has rendered the opaque objects)
     * into `m_blit_fbo`. This allows the rendering engine to perform depth testing on the objects in the scene,
     * even though all lighting calculations are done in screen-space.
     * 
     * We now enable depth writing as well as depth testing to render the transparent objects.
    */

    CoreEngine::GetInstance()->DepthMask(true);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::DEPTH_TEST);

    /* Blit the depth buffer contents from `fbo` (opaque objects) to `m_blit_fbo` */

    glDisable(GL_FRAMEBUFFER_SRGB); // have to disable SRGB for depth buffer blit; seems to be a bug on nvidia cards?
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo->GetId());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_blit_fbo->GetId());
    glBlitFramebuffer(
        0, 0, fbo->GetWidth(), fbo->GetHeight(), 0, 0, fbo->GetWidth(), fbo->GetHeight(), GL_DEPTH_BUFFER_BIT, GL_NEAREST
    );
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CatchGLErrors("Failed to blit depth buffer");
    glEnable(GL_FRAMEBUFFER_SRGB);

    /*
     * Render transparent objects into the framebuffer - they will be rendering as an overlay
     * of the deferred stuff, just with a modified depth buffer.
    */

    m_blit_fbo->Use();

    renderer->RenderBucket(cam, Spatial::Bucket::RB_TRANSPARENT, Octree::VisibilityState::CameraType::VIS_CAMERA_MAIN);
    renderer->RenderBucket(cam, Spatial::Bucket::RB_PARTICLE, Octree::VisibilityState::CameraType::VIS_CAMERA_MAIN);

    /* Write the 'final' color texture into the G-buffer. Now, we can proceed to the post-processing stage, where we manipulate this image. */
    m_blit_fbo->Store(Framebuffer::FRAMEBUFFER_ATTACHMENT_COLOR, m_gbuffer[Framebuffer::AttachmentToOrdinal(Framebuffer::FRAMEBUFFER_ATTACHMENT_COLOR)]);
    m_blit_fbo->End();

    /* We disable depth test and write as we are just rendering a quad to the entire screen */
    CoreEngine::GetInstance()->DepthMask(false);
    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::DEPTH_TEST);

    /* Render post processing effects on top of deferred shading + transparent objects */
    m_post_filters->Render(cam, fbo, m_blit_fbo);

    /* Revert changed renderer settings */

    CoreEngine::GetInstance()->DepthMask(true);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::DEPTH_TEST);
    
}

} // namespace hyperion