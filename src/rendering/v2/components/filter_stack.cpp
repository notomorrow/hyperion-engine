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
    // TMP
    m_quad = MeshFactory::CreateQuad();
    
    /* Add a renderpass per each filter */
    static const int num_filters = 2;
    static const std::array<std::string, 2> filter_shader_names = { "filter_pass", "deferred" };

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

        m_render_passes.push_back(engine->AddRenderPass(std::move(render_pass)));


        Filter filter{};

        filter.shader.reset(new Shader({
            SpirvObject{ SpirvObject::Type::VERTEX, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_vert.spv").Read()},
            SpirvObject{ SpirvObject::Type::FRAGMENT, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_frag.spv").Read() }
        }));

        filter.shader->Create(engine);

        m_filters.push_back(std::move(filter));
    }

    for (uint8_t i = 0; i < m_framebuffers.size(); i++) {
        m_framebuffers[i] = engine->AddFramebuffer(
            engine->GetInstance()->swapchain->extent.width,
            engine->GetInstance()->swapchain->extent.height,
            m_render_passes[0] // same layout, so just use first
        );
    }


    auto &filter_descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .AddDescriptorSet();
    
    // for rendering to/reading from filters (ping pong)
    filter_descriptor_set
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            0,
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[0].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[0].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            1,
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[1].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[1].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            2,
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[2].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[0])->GetWrappedObject()->GetAttachmentImageInfos()[2].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            3,
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[1])->GetWrappedObject()->GetAttachmentImageInfos()[0].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[1])->GetWrappedObject()->GetAttachmentImageInfos()[0].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            4,
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[1])->GetWrappedObject()->GetAttachmentImageInfos()[1].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[1])->GetWrappedObject()->GetAttachmentImageInfos()[1].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ))
        .AddDescriptor(std::make_unique<renderer::ImageSamplerDescriptor>(
            5,
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[1])->GetWrappedObject()->GetAttachmentImageInfos()[2].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_framebuffers[1])->GetWrappedObject()->GetAttachmentImageInfos()[2].sampler.get()),
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
            .Framebuffer(m_framebuffers[0])
            .Framebuffer(m_framebuffers[1]);

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

void FilterStack::Render(Engine *engine, Frame *frame)
{
    /* TODO: Our final pass HAS to end at framebuffer index 0,
     * so we calculate the start index based on the final index.
     */
    uint8_t fbo_dst_index = 1;

    for (int i = 0; i < m_filters.size(); i++) {
        const auto &filter = m_filters[i];

        uint8_t filter_src_index = (fbo_dst_index + 1) % m_framebuffers.size();

        filter.pipeline->push_constants.filter_framebuffer_src = filter_src_index;

        filter.pipeline->StartRenderPass(frame->command_buffer, fbo_dst_index);
        engine->GetInstance()->GetDescriptorPool().BindDescriptorSets(frame->command_buffer, filter.pipeline->layout);
        // TMP
        m_quad->RenderVk(frame, engine->GetInstance(), nullptr);
        filter.pipeline->EndRenderPass(frame->command_buffer, fbo_dst_index);

        fbo_dst_index = filter_src_index;
    }
}
} // namespace hyperion