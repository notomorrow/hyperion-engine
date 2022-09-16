#include <rendering/DeferredSystem.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

const FixedArray<TextureFormatDefault, num_gbuffer_textures> DeferredSystem::gbuffer_textures = {
    TEXTURE_FORMAT_DEFAULT_COLOR,   // color
    TEXTURE_FORMAT_DEFAULT_NORMALS, // normal
    TEXTURE_FORMAT_DEFAULT_GBUFFER_8BIT, // material
    TEXTURE_FORMAT_DEFAULT_GBUFFER_8BIT, // tangent
    TEXTURE_FORMAT_DEFAULT_DEPTH    // depth
};

DeferredSystem::DeferredSystem()
{
    for (size_t i = 0; i < m_buckets.Size(); i++) {
        m_buckets[i].SetBucket(Bucket(i));
    }
}

void DeferredSystem::AddFramebuffersToPipelines(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.AddFramebuffersToPipelines();
    }
}

void DeferredSystem::AddPendingRendererInstances(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.AddPendingRendererInstances(engine);
    }
}

void DeferredSystem::Create(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.CreateRenderPass(engine);
        bucket.CreateFramebuffers(engine);
    }
}

void DeferredSystem::Destroy(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.Destroy(engine);
    }
}

DeferredSystem::RendererInstanceHolder::RendererInstanceHolder()
{
}

DeferredSystem::RendererInstanceHolder::~RendererInstanceHolder()
{
}

void DeferredSystem::RendererInstanceHolder::AddRendererInstance(Handle<RendererInstance> &&renderer_instance)
{
    AddFramebuffersToPipeline(renderer_instance);

    std::lock_guard guard(renderer_instances_mutex);

    renderer_instances_pending_addition.PushBack(std::move(renderer_instance));
    renderer_instances_changed.Set(true);
}

void DeferredSystem::RendererInstanceHolder::AddPendingRendererInstances(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!renderer_instances_changed.Get()) {
        return;
    }

    DebugLog(LogType::Debug, "Adding %llu pending RendererInstances\n", renderer_instances_pending_addition.Size());

    std::lock_guard guard(renderer_instances_mutex);
    DebugLog(LogType::Debug, "Adding pending RendererInstances, locked mutex.\n");

    for (auto it = renderer_instances_pending_addition.Begin(); it != renderer_instances_pending_addition.End(); ++it) {
        AssertThrow(*it != nullptr);

        engine->InitObject(*it);

        renderer_instances.PushBack(std::move(*it));
    }
    
    renderer_instances_pending_addition = {};
    renderer_instances_changed.Set(false);
}

void DeferredSystem::RendererInstanceHolder::AddFramebuffersToPipelines()
{
    for (auto &pipeline : renderer_instances) {
        AddFramebuffersToPipeline(pipeline);
    }
}

void DeferredSystem::RendererInstanceHolder::AddFramebuffersToPipeline(Handle<RendererInstance> &pipeline)
{
    for (auto &framebuffer : framebuffers) {
        pipeline->AddFramebuffer(Handle<Framebuffer>(framebuffer));
    }
}

void DeferredSystem::RendererInstanceHolder::CreateRenderPass(Engine *engine)
{
    AssertThrow(render_pass == nullptr);

    auto mode = renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE;
    }
    
    render_pass = engine->CreateHandle<RenderPass>(
        RenderPassStage::SHADER,
        mode
    );

    if (BucketIsRenderable(bucket)) { // add gbuffer attachments
        AttachmentRef *attachment_ref;

        for (UInt i = 0; i < gbuffer_textures.Size() - 1 /* because depth already accounted for */; i++) {
            auto framebuffer_image = std::make_unique<renderer::FramebufferImage2D>(
                engine->GetInstance()->swapchain->extent,
                engine->GetDefaultFormat(gbuffer_textures[i]),
                nullptr
            );

            attachments.PushBack(std::make_unique<renderer::Attachment>(
                std::move(framebuffer_image),
                RenderPassStage::SHADER
            ));

            HYPERION_ASSERT_RESULT(attachments.Back()->AddAttachmentRef(
                engine->GetInstance()->GetDevice(),
                renderer::LoadOperation::CLEAR,
                renderer::StoreOperation::STORE,
                &attachment_ref
            ));

            render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);
        }

        constexpr SizeType depth_texture_index = gbuffer_textures.Size() - 1;

        /* Add depth attachment */
        if (bucket == BUCKET_TRANSLUCENT) { // translucent reuses the opaque bucket's depth buffer.
            auto &forward_fbo = engine->GetDeferredSystem()[BUCKET_OPAQUE].framebuffers[0];
            AssertThrow(forward_fbo != nullptr);

            renderer::AttachmentRef *depth_attachment;

            // reuse the depth attachment.
            HYPERION_ASSERT_RESULT(forward_fbo->GetFramebuffer().GetAttachmentRefs().at(depth_texture_index)->AddAttachmentRef(
                engine->GetInstance()->GetDevice(),
                renderer::StoreOperation::STORE,
                &depth_attachment
            ));

            depth_attachment->SetBinding(depth_texture_index);

            render_pass->GetRenderPass().AddAttachmentRef(depth_attachment);
        } else {
            // create new depth attachment.
            attachments.PushBack(std::make_unique<renderer::Attachment>(
                std::make_unique<renderer::FramebufferImage2D>(
                    engine->GetInstance()->swapchain->extent,
                    engine->GetDefaultFormat(gbuffer_textures[depth_texture_index]),
                    nullptr
                ),
                RenderPassStage::SHADER
            ));

            HYPERION_ASSERT_RESULT(attachments.Back()->AddAttachmentRef(
                engine->GetInstance()->GetDevice(),
                renderer::LoadOperation::CLEAR,
                renderer::StoreOperation::STORE,
                &attachment_ref
            ));

            render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);
        }
    }

    for (auto &attachment : attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }
    
    engine->InitObject(render_pass);
}

void DeferredSystem::RendererInstanceHolder::CreateFramebuffers(Engine *engine)
{
    AssertThrow(framebuffers.Empty());
    
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto framebuffer = engine->CreateHandle<Framebuffer>(
            engine->GetInstance()->swapchain->extent,
            Handle<RenderPass>(render_pass)
        );

        for (auto *attachment_ref : render_pass->GetRenderPass().GetAttachmentRefs()) {
            framebuffer->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }

        engine->InitObject(framebuffer);
        framebuffers.PushBack(framebuffer);
    }
}

void DeferredSystem::RendererInstanceHolder::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;

    renderer_instances.Clear();

    renderer_instances_mutex.lock();
    renderer_instances_pending_addition.Clear();
    renderer_instances_mutex.unlock();

    framebuffers.Clear();

    for (const auto &attachment : attachments) {
        HYPERION_PASS_ERRORS(
            attachment->Destroy(engine->GetInstance()->GetDevice()),
            result
        );
    }

    HYPERION_ASSERT_RESULT(result);
}

} // namespace hyperion::v2
