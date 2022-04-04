#include "render_list.h"

#include "../engine.h"

namespace hyperion::v2 {

RenderList::RenderList()
{
    for (size_t i = 0; i < m_buckets.size(); i++) {
        m_buckets[i] = {
            .bucket = GraphicsPipeline::Bucket(i),
            .pipelines = {.defer_create = true},
            .render_pass_id = {}
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
        for (auto &framebuffer_id : framebuffer_ids) {
            pipeline->AddFramebuffer(framebuffer_id);
        }
    }

    pipelines.CreateAll(engine);
}

void RenderList::Bucket::CreateRenderPass(Engine *engine)
{
    AssertThrow(render_pass_id.value == 0);

    auto mode = renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == GraphicsPipeline::BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE;
    }
    
    auto render_pass = std::make_unique<RenderPass>(renderer::RenderPass::Stage::RENDER_PASS_STAGE_SHADER, mode);

    /* For our color attachment */
    render_pass->Get().AddAttachment({
        .format = engine->GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_COLOR)
    });
    /* For our normals attachment */
    render_pass->Get().AddAttachment({
        .format = engine->GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_GBUFFER)
    });
    /* For our positions attachment */
    render_pass->Get().AddAttachment({
        .format = engine->GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_GBUFFER)
    });

    if (bucket == GraphicsPipeline::BUCKET_TRANSLUCENT) {
        /* Add depth attachment for reading the forward renderer's depth buffer before we render transparent objects */
        render_pass->Get().AddDepthAttachment(
            std::make_unique<renderer::AttachmentBase>(
                Image::ToVkFormat(engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_DEPTH)),
                VK_ATTACHMENT_LOAD_OP_LOAD,
                VK_ATTACHMENT_STORE_OP_STORE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                3,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            ));
    } else {
        render_pass->Get().AddAttachment({
            .format = engine->GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_DEPTH)
        });
    }

    render_pass_id = engine->AddRenderPass(std::move(render_pass));
}

void RenderList::Bucket::CreateFramebuffers(Engine *engine)
{
    AssertThrow(render_pass_id.value != 0);
    AssertThrow(framebuffer_ids.empty());

    auto *render_pass = engine->GetRenderPass(render_pass_id);
    AssertThrow(render_pass != nullptr);

    const uint32_t num_frames = engine->GetInstance()->GetFrameHandler()->NumFrames();

    for (uint32_t i = 0; i < 1/*num_frames*/; i++) {
        auto framebuffer = std::make_unique<Framebuffer>(
            engine->GetInstance()->swapchain->extent.width,
            engine->GetInstance()->swapchain->extent.height
        );

        /* Add all attachments from the renderpass */
        for (auto &it : render_pass->Get().GetAttachments()) {
            framebuffer->Get().AddAttachment(it.second.format);
        }

        if (bucket == GraphicsPipeline::BUCKET_TRANSLUCENT) {
            /* Reference depth image from the opaque layer */
            auto *forward_fbo = engine->GetFramebuffer(engine->GetRenderList()[GraphicsPipeline::Bucket::BUCKET_OPAQUE].framebuffer_ids[0]);
            AssertThrow(forward_fbo != nullptr);

            auto image_view = std::make_unique<ImageView>();
            HYPERION_ASSERT_RESULT(image_view->Create(engine->GetInstance()->GetDevice(), forward_fbo->Get().GetAttachmentImageInfos()[3].image.get()));

            framebuffer->Get().AddAttachment({
                .image_view = std::move(image_view)
            });
        }
        
        framebuffer_ids.push_back(engine->AddFramebuffer(
            std::move(framebuffer),
            render_pass_id
        ));
    }
}

void RenderList::Bucket::Destroy(Engine *engine)
{
    AssertThrow(render_pass_id.value != 0);

    for (const auto &framebuffer_id : framebuffer_ids) {
        engine->RemoveFramebuffer(framebuffer_id);
    }

    framebuffer_ids.clear();

    engine->RemoveRenderPass(render_pass_id);

    pipelines.RemoveAll(engine);
}

void RenderList::Bucket::BeginRenderPass(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index)
{
    auto &render_pass = engine->GetRenderPass(render_pass_id)->Get();

    if (!pipelines.objects.empty()) {
        AssertThrowMsg(pipelines.objects[0]->Get().GetConstructionInfo().render_pass == &render_pass, "Render pass for pipeline does not match render bucket renderpass");
    }

    auto &framebuffer = engine->GetFramebuffer(framebuffer_ids[frame_index])->Get();

    render_pass.Begin(
        command_buffer,
        framebuffer.GetFramebuffer(),
        VkExtent2D{ uint32_t(framebuffer.GetWidth()), uint32_t(framebuffer.GetHeight()) }
    );
}

void RenderList::Bucket::EndRenderPass(Engine *engine, CommandBuffer *command_buffer)
{
    auto &render_pass = engine->GetRenderPass(render_pass_id)->Get();

    if (!pipelines.objects.empty()) {
        AssertThrowMsg(pipelines.objects[0]->Get().GetConstructionInfo().render_pass == &render_pass, "Render pass for pipeline does not match render bucket renderpass");
    }

    render_pass.End(command_buffer);
}


} // namespace hyperion::v2