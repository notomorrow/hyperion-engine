#ifndef HYPERION_V2_RENDER_CONTAINER_H
#define HYPERION_V2_RENDER_CONTAINER_H

#include "material.h"
#include "shader.h"
#include "texture.h"
#include "util.h"

#include <rendering/backend/renderer_graphics_pipeline.h>

#include <memory>
#include <vector>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::DescriptorSet;
using renderer::GPUBuffer;
using renderer::UniformBuffer;

class Engine;

class RenderContainer {
public:
    static constexpr size_t max_materials = 16;

    RenderContainer(std::unique_ptr<Shader> &&shader);
    RenderContainer(const RenderContainer &other) = delete;
    RenderContainer &operator=(const RenderContainer &other) = delete;
    ~RenderContainer();

    Material::ID AddMaterial(Engine *engine, std::unique_ptr<Material> &&material);
    
    template <class ...Args>
    Texture::ID AddTexture(Engine *engine, std::unique_ptr<Texture> &&texture, Args &&... args)
        { return m_textures.Add(engine, std::move(texture), std::move(args)...); }

    inline Texture *GetTexture(Texture::ID id)
        { return m_textures.Get(id); }

    inline const Texture *GetTexture(Texture::ID id) const
        { return const_cast<RenderContainer *>(this)->GetTexture(id); }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void Render(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index);

private:
    void CreateMaterialUniformBuffer(Engine *engine);
    void DestroyMaterialUniformBuffer(Engine *engine);
    void UpdateDescriptorSet(DescriptorSet *);

    std::unique_ptr<Shader> m_shader;
    ObjectHolder<Material> m_materials;
    ObjectHolder<Texture> m_textures;

    struct InternalData {
        struct alignas(16) MaterialData {
            Vector4 albedo;
            float metalness;
            float roughness;
        };

        std::array<MaterialData, max_materials> material_parameters;
        UniformBuffer *material_uniform_buffer = nullptr;
    } m_internal;

    std::unique_ptr<renderer::GraphicsPipeline> m_pipeline;
};

} // namespace hyperion::v2

#endif