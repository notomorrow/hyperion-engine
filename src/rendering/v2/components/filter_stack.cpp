#include "filter_stack.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>
#include <util/mesh_factory.h>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;

FilterStack::FilterStack()
    : m_framebuffers{-1},
      m_filters{}
{
}

FilterStack::~FilterStack() = default;

void FilterStack::Create(Engine *engine)
{
    VkSemaphoreCreateInfo semaphore_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    if (vkCreateSemaphore(engine->GetInstance()->GetDevice()->GetDevice(), &semaphore_info, nullptr, &wait_sp) != VK_SUCCESS) {
        throw "could not create wait semaphore";
    }

    VkFenceCreateInfo fence_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(engine->GetInstance()->GetDevice()->GetDevice(), &fence_info, nullptr, &this->m_fc_submit) != VK_SUCCESS) {
        throw "Failed to create fence";
    }



    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = engine->GetInstance()->command_pool;
    alloc_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(engine->GetInstance()->GetDevice()->GetDevice(), &alloc_info, &final_cmd_buffer) != VK_SUCCESS) {
        throw "Failed to allocate command buffers";
    }


    // TMP
    m_quad = MeshFactory::CreateQuad();
    
    /* Add a renderpass per each filter */
    static const int num_filters = 2;
    static const std::array<std::string, 2> filter_shader_names = {  "deferred", "filter_pass" };

    for (int i = 0; i < num_filters; i++) {
        /* Add the filters' renderpass */
        auto render_pass = std::make_unique<v2::RenderPass>(v2::RenderPass::RENDER_PASS_STAGE_SHADER);

        /* For our color attachment */
        render_pass->AddAttachment({
            .format = engine->GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_COLOR)
        });
        /* For our normals attachment */
        render_pass->AddAttachment({
            .format = engine->GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_GBUFFER)
        });
        /* For our positions attachment */
        render_pass->AddAttachment({
            .format = engine->GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_GBUFFER)
        });

        render_pass->AddAttachment({
            .format = engine->GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_DEPTH)
        });



        Filter filter{};

        filter.shader.reset(new Shader({
            SpirvObject{ SpirvObject::Type::VERTEX, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_vert.spv").Read()},
            SpirvObject{ SpirvObject::Type::FRAGMENT, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_frag.spv").Read() }
        }));

        filter.shader->Create(engine);

        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = engine->GetInstance()->command_pool;
        alloc_info.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(engine->GetInstance()->GetDevice()->GetDevice(), &alloc_info, &filter.cmd) != VK_SUCCESS) {
            throw "Failed to allocate command buffers";
        }


        VkSemaphoreCreateInfo semaphore_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        if (vkCreateSemaphore(engine->GetInstance()->GetDevice()->GetDevice(), &semaphore_info, nullptr, &filter.sp) != VK_SUCCESS) {
            throw "failed to create render semaphore";
        }

        m_render_passes.push_back(engine->AddRenderPass(std::move(render_pass)));
        filter.framebuffer = engine->AddFramebuffer(
            engine->GetInstance()->swapchain->extent.width,
            engine->GetInstance()->swapchain->extent.height,
            m_render_passes.back()
        );

        m_filters.push_back(std::move(filter));
    }


    auto &filter_descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .AddDescriptorSet();
    
    // for rendering to/reading from filters (ping pong)
    filter_descriptor_set
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            0,
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[0].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[0].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            1,
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[1].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[1].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            2,
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[2].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[2].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            3,
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[0].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[0].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            4,
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[1].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[1].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            5,
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[2].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].framebuffer)->GetWrappedObject()->GetAttachmentImageInfos()[2].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ));


    //auto descriptor_set_result = filter_descriptor_set.Create(engine->GetInstance()->GetDevice(), &engine->GetInstance()->GetDescriptorPool());
    //AssertThrowMsg(descriptor_set_result, "%s", descriptor_set_result.message);
    //engine->GetInstance()->GetDescriptorPool().m_descriptor_sets_view[2] = filter_descriptor_set.m_set; // TMP!

    /* Finalize descriptor pool */
    auto descriptor_pool_result = engine->GetInstance()->GetDescriptorPool().Create(engine->GetInstance()->GetDevice());
    AssertThrowMsg(descriptor_pool_result, "%s", descriptor_pool_result.message);

    // prob gonna have to add a pipeline for each filter..?

    /* TODO: pull attrs from quad mesh when new mesh class exists */
    const auto vertex_attributes = MeshInputAttributeSet(
        MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT);

    for (int i = 0; i < num_filters; i++) {
        Pipeline::Builder builder;

        builder
            .Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) /* full screen quad is a triangle fan */
            .Shader(m_filters[i].shader->GetWrappedObject())
            .VertexAttributes(vertex_attributes)
            .RenderPass(m_render_passes[i])
            .Framebuffer(m_filters[i].framebuffer);

        engine->AddPipeline(std::move(builder), &m_filters[i].pipeline);
    }

    RecordFilters(engine);
}

void FilterStack::Destroy(Engine *engine)
{
    m_quad.reset();

    /* Render passes + framebuffer cleanup are handled by the engine */
    for (const auto &filter : m_filters) {
        filter.shader->Destroy(engine);
    }

    /* TODO:should be handling our own data. we cna't rely on the engine to cleanup at the end, what if we're just rebuilding? */
}

void FilterStack::RecordFilters(Engine *engine)
{
    /* TODO: Our final pass HAS to end at framebuffer index 0,
     * so we calculate the start index based on the final index.
     */
    uint8_t fbo_dst_index = 1;

    for (int i = 0; i < m_filters.size(); i++) {
        const auto &filter = m_filters[i];

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        if (vkBeginCommandBuffer(filter.cmd, &begin_info) != VK_SUCCESS) {
            throw "failed to begin cmd buffer";
        }

        uint8_t filter_src_index = (fbo_dst_index + 1) % m_framebuffers.size();

        filter.pipeline->push_constants.filter_framebuffer_src = filter_src_index;

        filter.pipeline->StartRenderPass(filter.cmd, 0);
        engine->GetInstance()->GetDescriptorPool().BindDescriptorSets(filter.cmd, filter.pipeline->layout, 0, 2);
        // TMP
        renderer::Frame frame;
        frame.command_buffer = filter.cmd;
        m_quad->RenderVk(&frame, engine->GetInstance(), nullptr);

        filter.pipeline->EndRenderPass(filter.cmd, 0);

        fbo_dst_index = filter_src_index;


        if (vkEndCommandBuffer(filter.cmd) != VK_SUCCESS) {
            throw "failed to end cmd buffer";
        }
    }
}

void FilterStack::Render(Engine *engine, Frame *frame)
{
    vkWaitForFences(engine->GetInstance()->GetDevice()->GetDevice(), 1, &this->m_fc_submit, true, UINT64_MAX);
    vkResetFences(engine->GetInstance()->GetDevice()->GetDevice(), 1, &this->m_fc_submit);

    VkCommandBuffer cmd_buffers[2] = { m_filters[0].cmd, m_filters[1].cmd };
    VkSemaphore wait_semaphores[2] = { m_filters[0].sp, m_filters[1].sp };


    VkSubmitInfo submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkPipelineStageFlags wait_stages[2] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &frame->sp_swap_release;
    submit_info.signalSemaphoreCount = 2;
    submit_info.pSignalSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 2;
    submit_info.pCommandBuffers = cmd_buffers;

    auto result = vkQueueSubmit(engine->GetInstance()->queue_graphics, 1, &submit_info, m_fc_submit);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to submit draw command buffer!\n");

   /* VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(filter.cmd, &begin_info) != VK_SUCCESS) {
        throw "failed to begin cmd buffer";
    }
    if (vkEndCommandBuffer(filter.cmd) != VK_SUCCESS) {
        throw "failed to begin cmd buffer";
    }
    VkPipelineStageFlags wait_stages2[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 2;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &this->wait_sp;
    submit_info.pWaitDstStageMask = wait_stages2;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame->command_buffer;

    result = vkQueueSubmit(engine->GetInstance()->queue_graphics, 1, &submit_info, nullptr);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to submit draw command buffer!\n");*/
}
} // namespace hyperion