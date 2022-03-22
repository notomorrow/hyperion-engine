#include "render_container.h"
#include "../engine.h"

#include <rendering/backend/renderer_descriptor.h>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;

RenderContainer::RenderContainer(Shader::ID shader_id, RenderPass::ID render_pass_id)
    : m_shader_id(shader_id),
      m_render_pass_id(render_pass_id),
      m_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
      m_vertex_attributes(MeshInputAttributeSet(
          MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT))
{
    /* TMP */
}

RenderContainer::~RenderContainer()
{
    AssertThrowMsg(m_internal.material_uniform_buffer == nullptr, "Material uniform buffer should have been destroyed");
}

void RenderContainer::AddSpatial(Spatial &&spatial)
{
    /* append any attributes not yet added */
    m_vertex_attributes.Merge(spatial.attributes);

    m_spatials.push_back(std::move(spatial));
}

Material::ID RenderContainer::AddMaterial(Engine *engine, std::unique_ptr<Material> &&material)
{
    const Material::ID existing_id = m_materials.Find([&material](const std::unique_ptr<Material> &other) {
        return material->GetHashCode() == other->GetHashCode();
    });

    if (existing_id) {
        return existing_id;
    }

    AssertThrowMsg(m_materials.Size() + 1 <= max_materials, "Maximum number of materials reached");

    return m_materials.Add(engine, std::move(material));
}

void RenderContainer::CreateMaterialUniformBuffer(Engine *engine)
{
    AssertThrow(m_internal.material_uniform_buffer == nullptr);

    for (size_t i = 0; i < m_materials.Size(); i++) {
        m_internal.material_parameters[i] = InternalData::MaterialData{
            .albedo = m_materials.objects[i]->GetParameter<Vector4>(Material::MATERIAL_KEY_ALBEDO),
            .metalness = m_materials.objects[i]->GetParameter<float>(Material::MATERIAL_KEY_METALNESS),
            .roughness = m_materials.objects[i]->GetParameter<float>(Material::MATERIAL_KEY_ROUGHNESS)
        };
    }

    m_internal.material_uniform_buffer = new UniformBuffer();
    m_internal.material_uniform_buffer->Create(
        engine->GetInstance()->GetDevice(),
        m_internal.material_parameters.size() * sizeof(InternalData::MaterialData)
    );
    m_internal.material_uniform_buffer->Copy(
        engine->GetInstance()->GetDevice(),
        m_internal.material_parameters.size() * sizeof(InternalData::MaterialData),
        m_internal.material_parameters.data()
    );
}

void RenderContainer::DestroyMaterialUniformBuffer(Engine *engine)
{
    AssertThrow(m_internal.material_uniform_buffer != nullptr);

    m_internal.material_uniform_buffer->Destroy(engine->GetInstance()->GetDevice());

    delete m_internal.material_uniform_buffer;
    m_internal.material_uniform_buffer = nullptr;
}

void RenderContainer::UpdateDescriptorSet(DescriptorSet *descriptor_set)
{
    /* TODO: make this dynamically set */
    descriptor_set->AddDescriptor(std::make_unique<renderer::BufferDescriptor>(
        0,
        m_internal.material_uniform_buffer,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_FRAGMENT_BIT
    ));
}

void RenderContainer::PrepareDescriptors(Engine *engine)
{
    CreateMaterialUniformBuffer(engine);

    UpdateDescriptorSet(engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT));
}


void RenderContainer::Create(Engine *engine)
{
    renderer::GraphicsPipeline::Builder builder;

    builder.VertexAttributes(m_vertex_attributes);
    builder.Topology(m_topology);

    auto *shader = engine->GetShader(m_shader_id);
    AssertThrow(shader != nullptr);

    builder.m_construction_info.shader = non_owning_ptr(&shader->Get());

    auto *render_pass = engine->GetRenderPass(m_render_pass_id);
    AssertThrow(render_pass != nullptr);
    // TODO: Assert that render_pass matches the layout of what the fbo was set up with

    builder.m_construction_info.render_pass = non_owning_ptr(&render_pass->Get());

    for (const auto &fbo_id : m_fbo_ids.ids) {
        if (auto *fbo = engine->GetFramebuffer(fbo_id)) {
            AssertThrow(fbo != nullptr);
            builder.m_construction_info.fbos.push_back(non_owning_ptr(&fbo->Get()));
        }
    }

    EngineComponent::Create(engine, std::move(builder.m_construction_info), &engine->GetInstance()->GetDescriptorPool());
}

void RenderContainer::Destroy(Engine *engine)
{
    EngineComponent::Destroy(engine);

    DestroyMaterialUniformBuffer(engine);
}

void RenderContainer::Render(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index)
{
    auto *instance = engine->GetInstance();

    m_wrapped.BeginRenderPass(command_buffer, frame_index, VK_SUBPASS_CONTENTS_INLINE);
    m_wrapped.Bind(command_buffer);

    instance->GetDescriptorPool().BindDescriptorSets(command_buffer, &m_wrapped);

    /* TMP */
    for (auto &spatial : m_spatials) {
        m_wrapped.push_constants.material_index = spatial.material_id.value;
        m_wrapped.SubmitPushConstants(command_buffer);
        spatial.mesh->RenderVk(command_buffer, instance, nullptr);
    }

    m_wrapped.EndRenderPass(command_buffer, frame_index);
}

} // namespace hyperion::v2