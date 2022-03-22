#ifndef HYPERION_V2_RENDER_CONTAINER_H
#define HYPERION_V2_RENDER_CONTAINER_H

#include "material.h"
#include "shader.h"
#include "texture.h"
#include "framebuffer.h"
#include "render_pass.h"
#include "util.h"

/* TMP */
#include <rendering/mesh.h>
#include <math/transform.h>

#include <rendering/backend/renderer_graphics_pipeline.h>

#include <memory>
#include <vector>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::DescriptorSet;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::MeshInputAttributeSet;

class Engine;

class RenderContainer : public EngineComponent<renderer::GraphicsPipeline> {
public:
    static constexpr size_t max_materials = 16;

    struct Spatial {
        std::shared_ptr<Mesh> mesh; /* TMP */
        MeshInputAttributeSet attributes;
        Transform transform;
        Material::ID material_id;
    };

    RenderContainer(Shader::ID shader_id, RenderPass::ID render_pass_id);
    RenderContainer(const RenderContainer &other) = delete;
    RenderContainer &operator=(const RenderContainer &other) = delete;
    ~RenderContainer();

    inline VkPrimitiveTopology GetTopology() const
        { return m_topology; }
    inline void SetTopology(VkPrimitiveTopology topology)
        { m_topology = topology; }

    void AddSpatial(Spatial &&spatial);

    /* Materials (owned) */
    Material::ID AddMaterial(Engine *engine, std::unique_ptr<Material> &&material);
    inline constexpr const Material *GetMaterial(Material::ID id) const
        { return m_materials.Get(id); }

    /* Non-owned objects - owned by `engine`, used by the pipeline */

    inline void AddFramebuffer(Framebuffer::ID id) { m_fbo_ids.Add(id); }
    inline void RemoveFramebuffer(Framebuffer::ID id) { m_fbo_ids.Remove(id); }
    
    void PrepareDescriptors(Engine *engine);
    /* Build pipeline */
    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void Render(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index);

private:
    void CreateMaterialUniformBuffer(Engine *engine);
    void DestroyMaterialUniformBuffer(Engine *engine);
    void UpdateDescriptorSet(DescriptorSet *);

    Shader::ID m_shader_id;
    RenderPass::ID m_render_pass_id;
    MeshInputAttributeSet m_vertex_attributes;
    VkPrimitiveTopology m_topology;
    ObjectHolder<Material> m_materials;

    ObjectIdHolder<Texture> m_texture_ids;
    ObjectIdHolder<Framebuffer> m_fbo_ids;

    std::vector<Spatial> m_spatials;

    struct InternalData {
        struct alignas(16) MaterialData {
            Vector4 albedo;
            float metalness;
            float roughness;
        };

        std::array<MaterialData, max_materials> material_parameters;
        UniformBuffer *material_uniform_buffer = nullptr;
    } m_internal;
};

} // namespace hyperion::v2

#endif