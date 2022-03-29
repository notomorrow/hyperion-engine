#include "deferred.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

namespace hyperion::v2 {

DeferredRendering::DeferredRendering()
    : PostEffect(Shader::ID{})
{
    
}

DeferredRendering::~DeferredRendering() = default;

void DeferredRendering::CreateShader(Engine *engine)
{
    m_shader_id = engine->AddShader(std::make_unique<Shader>(std::vector<SubShader>{
        SubShader{ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/deferred_vert.spv").Read()}},
        SubShader{ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/deferred_frag.spv").Read()}}
    }));
}

void DeferredRendering::CreateRenderPass(Engine *engine)
{
    /* Add the filters' renderpass */
    auto render_pass = std::make_unique<RenderPass>(renderer::RenderPass::RENDER_PASS_STAGE_SHADER, renderer::RenderPass::RENDER_PASS_SECONDARY_COMMAND_BUFFER);

    /* For our color attachment */
    render_pass->Get().AddAttachment({
        .format = engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_COLOR)
    });

    /* Add depth attachment for reading the forward renderer's depth buffer before we render transparent objects */
    render_pass->Get().AddDepthAttachment(
        std::make_unique<renderer::AttachmentBase>(
        
            renderer::Image::ToVkFormat(engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_DEPTH)),
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            1,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ));

    m_render_pass_id = engine->AddRenderPass(std::move(render_pass));
}

void DeferredRendering::CreateFrameData(Engine *engine)
{
    const uint32_t num_frames = engine->GetInstance()->GetFrameHandler()->NumFrames();

    m_frame_data = std::make_unique<PerFrameData<CommandBuffer>>(num_frames);

    auto fbo = std::make_unique<Framebuffer>(engine->GetInstance()->swapchain->extent.width, engine->GetInstance()->swapchain->extent.height);
    fbo->Get().AddAttachment(engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_COLOR));
    

    /* Add depth attachment */
    auto forward_fbo = engine->GetFramebuffer(Framebuffer::ID{1});
    auto image_view = std::make_unique<ImageView>();
    HYPERION_ASSERT_RESULT(image_view->Create(engine->GetInstance()->GetDevice(), forward_fbo->Get().GetAttachmentImageInfos()[3].image.get()));
    fbo->Get().AddAttachment({
        .image_view = std::move(image_view)
    });

    m_framebuffer_id = engine->AddFramebuffer(std::move(fbo), m_render_pass_id);

    for (uint32_t i = 0; i < num_frames; i++) {
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        m_frame_data->At(i).Set<CommandBuffer>(std::move(command_buffer));
    }
}

void DeferredRendering::Destroy(Engine *engine)
{
    PostEffect::Destroy(engine);
}

void DeferredRendering::Render(Engine *engine, CommandBuffer *primary_command_buffer, uint32_t frame_index)
{
    PostEffect::Render(engine, primary_command_buffer, frame_index);
}

} // namespace hyperion::v2