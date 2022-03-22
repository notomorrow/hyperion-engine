#include "render_container.h"
#include "../engine.h"

#include <rendering/backend/renderer_descriptor.h>

namespace hyperion::v2 {

RenderContainer::RenderContainer(std::unique_ptr<Shader> &&shader)
    : m_shader(std::move(shader))
{
}

RenderContainer::~RenderContainer()
{
    AssertThrowMsg(m_internal.material_uniform_buffer == nullptr, "Material uniform buffer should have been destroyed");
}

Material::ID RenderContainer::AddMaterial(Engine *engine, std::unique_ptr<Material> &&material)
{
    const Material::ID existing = m_materials.Find([&material](const std::unique_ptr<Material> &other) {
        return material->GetHashCode() == other->GetHashCode();
    });

    if (existing) {
        return existing;
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

void RenderContainer::Create(Engine *engine)
{
    //m_shader->Create(engine);

    CreateMaterialUniformBuffer(engine);

    UpdateDescriptorSet(engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT));
}

void RenderContainer::Destroy(Engine *engine)
{
    //m_shader->Destroy(engine);

    DestroyMaterialUniformBuffer(engine);
}

void RenderContainer::Render(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index)
{
    m_pipeline->BeginRenderPass(command_buffer, frame_index, VK_SUBPASS_CONTENTS_INLINE);
    m_pipeline->Bind(command_buffer);

    engine->GetInstance()->GetDescriptorPool().BindDescriptorSets(command_buffer, m_pipeline.get(), 0, 1);

    /* render meshes here */

    m_pipeline->EndRenderPass(command_buffer, frame_index);
}

} // namespace hyperion::v2