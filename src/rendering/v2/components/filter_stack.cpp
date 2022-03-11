#include "filter_stack.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>
#include <util/mesh_factory.h>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;

FilterStack::FilterStack()
    : m_filters{}
{
}

FilterStack::~FilterStack() = default;

void FilterStack::Create(Engine *engine)
{

    // TMP
    m_quad = MeshFactory::CreateQuad();
    
    /* Add a renderpass per each filter */
    static const int num_filters = 2;
    static const std::array<std::string, 2> filter_shader_names = {  "deferred", "filter_pass" };

    for (int i = 0; i < num_filters; i++) {
        /* Add the filters' renderpass */
        auto render_pass = std::make_unique<v2::RenderPass>(v2::RenderPass::RENDER_PASS_STAGE_SHADER, v2::RenderPass::RENDER_PASS_SECONDARY_COMMAND_BUFFER);

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

        m_render_passes.push_back(engine->AddRenderPass(std::move(render_pass)));

        filter.frame_data.framebuffers.resize(engine->GetInstance()->GetNumImages());
        filter.frame_data.command_buffers.resize(engine->GetInstance()->GetNumImages());

        for (size_t j = 0; j < engine->GetInstance()->GetNumImages(); j++) {
            filter.frame_data.framebuffers[j] = engine->AddFramebuffer(
                engine->GetInstance()->swapchain->extent.width,
                engine->GetInstance()->swapchain->extent.height,
                m_render_passes.back()
            );


            auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

            auto command_buffer_result = command_buffer->Create(
                engine->GetInstance()->GetDevice(),
                engine->GetInstance()->command_pool
            );

            AssertThrowMsg(command_buffer_result, "%s", command_buffer_result.message);
            
            filter.frame_data.command_buffers[j] = std::move(command_buffer);
        }

        m_filters.push_back(std::move(filter));
    }


    auto &filter_descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .AddDescriptorSet();
    
    // for rendering to/reading from filters (ping pong)
    filter_descriptor_set
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            0,
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[0].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[0].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            1,
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[1].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[1].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            2,
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[2].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[0].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[2].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            3,
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[0].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[0].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            4,
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[1].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[1].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            5,
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[2].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_filters[1].frame_data.framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[2].sampler.get()),
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
            .RenderPass(m_render_passes[i]);

        for (int j = 0; j < m_filters[i].frame_data.framebuffers.size(); j++) {
            builder.Framebuffer(m_filters[i].frame_data.framebuffers[j]);
        }

        engine->AddPipeline(std::move(builder), &m_filters[i].pipeline);
    }
    
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
    for (int i = 0; i < m_filters.size(); i++) {
        auto &filter = m_filters[i];
        
    }
}

void FilterStack::Render(Engine *engine, Frame *frame, uint32_t frame_index)
{
    //vkWaitForFences(engine->GetInstance()->GetDevice()->GetDevice(), 1, &this->m_frame_fences[frame_index], true, UINT64_MAX);
    //vkResetFences(engine->GetInstance()->GetDevice()->GetDevice(), 1, &this->m_frame_fences[frame_index]);

    /*auto result = CommandBuffer::SubmitSecondary(
        frame->command_buffer,
        m_filter_command_buffers
    );*/

    for (size_t i = 0; i < m_filters.size(); i++) {
        auto &filter = m_filters[i];
        filter.pipeline->BeginRenderPass(frame->command_buffer, frame_index, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

        /* TMP: we can pre-record this */
        auto &command_buffer = filter.frame_data.command_buffers[frame_index];

        command_buffer->Reset(engine->GetInstance()->GetDevice());

        command_buffer->Record(
            engine->GetInstance()->GetDevice(),
            filter.pipeline->GetConstructionInfo().render_pass.get(),
            [this, engine, &filter](VkCommandBuffer cmd) {
                renderer::Result result = renderer::Result::OK;
                
                filter.pipeline->Bind(cmd);

                HYPERION_PASS_ERRORS(
                    engine->GetInstance()->GetDescriptorPool().BindDescriptorSets(cmd, filter.pipeline->layout, 0, 2),
                    result
                );

                // TMP
                renderer::Frame tmp_frame;
                tmp_frame.command_buffer = cmd;

                m_quad->RenderVk(&tmp_frame, engine->GetInstance(), nullptr);
                

                return result;
            });

        auto result = command_buffer->SubmitSecondary(frame->command_buffer);
        AssertThrowMsg(result, "%s", result.message);
        filter.pipeline->EndRenderPass(frame->command_buffer, frame_index);
    }

}
} // namespace hyperion