#include "render_list.h"

#include "../engine.h"

namespace hyperion::v2 {

RenderList::RenderList()
{
    for (size_t i = 0; i < m_buckets.size(); i++) {
        m_buckets[i] = {
            .bucket = GraphicsPipeline::Bucket(i),
            .pipelines = {.defer_create = true}
        };
    }
}

void RenderList::CreatePipelines(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.CreatePipelines(engine);
    }
}


void RenderList::Create(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.CreateRenderPass(engine);
        bucket.CreateFramebuffers(engine);
    }
}

void RenderList::Destroy(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.Destroy(engine);
    }
}

void RenderList::Bucket::CreatePipelines(Engine *engine)
{
    for (auto &pipeline : pipelines.objects) {
        for (auto &framebuffer : framebuffers) {
            pipeline->AddFramebuffer(framebuffer.Acquire());
        }
    }

    pipelines.CreateAll(engine);
}

void RenderList::Bucket::CreateRenderPass(Engine *engine)
{
    AssertThrow(render_pass == nullptr);

    auto mode = renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == GraphicsPipeline::BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE;
    }
    
    auto render_pass = std::make_unique<RenderPass>(
        renderer::RenderPassStage::SHADER,
        mode
    );


    renderer::AttachmentRef *attachment_ref;

    m_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            engine->GetInstance()->swapchain->extent,
            engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_COLOR),
            nullptr
        ),
        renderer::RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        engine->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    render_pass->Get().AddRenderPassAttachmentRef(attachment_ref);

    for (int i = 0; i < 2; i++) {
        m_attachments.push_back(std::make_unique<renderer::Attachment>(
            std::make_unique<renderer::FramebufferImage2D>(
                engine->GetInstance()->swapchain->extent,
                engine->GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_GBUFFER),
                nullptr
            ),
            renderer::RenderPassStage::SHADER
        ));

        HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
            engine->GetInstance()->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_ref
        ));

        render_pass->Get().AddRenderPassAttachmentRef(attachment_ref);
    }

    if (bucket == GraphicsPipeline::BUCKET_TRANSLUCENT) {
        auto &forward_fbo = engine->GetRenderList()[GraphicsPipeline::Bucket::BUCKET_OPAQUE].framebuffers[0];
        AssertThrow(forward_fbo != nullptr);

        renderer::AttachmentRef *depth_attachment;

        HYPERION_ASSERT_RESULT(forward_fbo->Get().GetRenderPassAttachmentRefs().at(3)->AddAttachmentRef(
            engine->GetInstance()->GetDevice(),
            renderer::StoreOperation::STORE,
            &depth_attachment
        ));

        depth_attachment->SetBinding(3);

        render_pass->Get().AddRenderPassAttachmentRef(depth_attachment);
    } else {
        m_attachments.push_back(std::make_unique<renderer::Attachment>(
            std::make_unique<renderer::FramebufferImage2D>(
                engine->GetInstance()->swapchain->extent,
                engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_DEPTH),
                nullptr
            ),
            renderer::RenderPassStage::SHADER
        ));

        HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
            engine->GetInstance()->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_ref
        ));

        render_pass->Get().AddRenderPassAttachmentRef(attachment_ref);
    }

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }

    this->render_pass = engine->resources.render_passes.Add(std::move(render_pass));
    this->render_pass.Init();
}

void RenderList::Bucket::CreateFramebuffers(Engine *engine)
{
    AssertThrow(framebuffers.empty());

    const uint32_t num_frames = engine->GetInstance()->GetFrameHandler()->NumFrames();

    for (uint32_t i = 0; i < 1/*num_frames*/; i++) {
        auto framebuffer = std::make_unique<Framebuffer>(engine->GetInstance()->swapchain->extent, render_pass.Acquire());

        for (auto *attachment_ref : render_pass->Get().GetRenderPassAttachmentRefs()) {
            auto vk_desc = attachment_ref->GetAttachmentDescription();
            auto vk_ref = attachment_ref->GetAttachmentReference();

            framebuffer->Get().AddRenderPassAttachmentRef(attachment_ref);
        }

        framebuffers.push_back(engine->resources.framebuffers.Add(
            std::move(framebuffer)
        ));

        framebuffers.back().Init();
    }
}

void RenderList::Bucket::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;
    
    framebuffers.clear();

    for (auto &attachment : m_attachments) {
        HYPERION_PASS_ERRORS(attachment->Destroy(engine->GetInstance()->GetDevice()), result);
    }

    pipelines.RemoveAll(engine);

    HYPERION_ASSERT_RESULT(result);
}

void RenderList::Bucket::Begin(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index)
{
    if (!pipelines.objects.empty()) {
        AssertThrowMsg(pipelines.objects[0]->Get().GetConstructionInfo().render_pass == &render_pass->Get(), "Render pass for pipeline does not match render bucket renderpass");
    }

    framebuffers[frame_index]->BeginCapture(command_buffer);
}

void RenderList::Bucket::End(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index)
{
    if (!pipelines.objects.empty()) {
        AssertThrowMsg(pipelines.objects[0]->Get().GetConstructionInfo().render_pass == &render_pass->Get(), "Render pass for pipeline does not match render bucket renderpass");
    }
    
    framebuffers[frame_index]->EndCapture(command_buffer);
}


} // namespace hyperion::v2