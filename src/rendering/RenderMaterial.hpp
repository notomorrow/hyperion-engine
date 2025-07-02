/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_MATERIAL_HPP
#define HYPERION_RENDER_MATERIAL_HPP

#include <rendering/ShaderManager.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderResource.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>

#include <Types.hpp>

namespace hyperion {

class Material;
class Texture;
class RenderTexture;

enum class MaterialTextureKey : uint64;

class RenderMaterial final : public RenderResourceBase
{
public:
    RenderMaterial(Material* material);
    RenderMaterial(RenderMaterial&& other) noexcept;
    virtual ~RenderMaterial() override;

    HYP_FORCE_INLINE Material* GetMaterial() const
    {
        return m_material;
    }

    // /*! \note Only call this method from the render thread or task initiated by the render thread */
    // HYP_FORCE_INLINE const FixedArray<DescriptorSetRef, maxFramesInFlight>& GetDescriptorSets() const
    // {
    //     return m_descriptorSets;
    // }

    void SetTexture(MaterialTextureKey textureKey, const Handle<Texture>& texture);
    void SetTextures(FlatMap<MaterialTextureKey, Handle<Texture>>&& textures);

    void SetBoundTextureIDs(const Array<ObjId<Texture>>& boundTextureIds);

    void SetBufferData(const MaterialShaderData& bufferData);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GpuBufferHolderBase* GetGpuBufferHolder() const override;

private:
    void CreateDescriptorSets();
    void DestroyDescriptorSets();

    void UpdateBufferData();

    Material* m_material;
    FlatMap<MaterialTextureKey, Handle<Texture>> m_textures;
    HashMap<ObjId<Texture>, TResourceHandle<RenderTexture>> m_renderTextures;
    Array<ObjId<Texture>> m_boundTextureIds;
    MaterialShaderData m_bufferData;
    // FixedArray<DescriptorSetRef, maxFramesInFlight> m_descriptorSets;
};

class HYP_API MaterialDescriptorSetManager
{
public:
    MaterialDescriptorSetManager();
    MaterialDescriptorSetManager(const MaterialDescriptorSetManager& other) = delete;
    MaterialDescriptorSetManager& operator=(const MaterialDescriptorSetManager& other) = delete;
    MaterialDescriptorSetManager(MaterialDescriptorSetManager&& other) noexcept = delete;
    MaterialDescriptorSetManager& operator=(MaterialDescriptorSetManager&& other) noexcept = delete;
    ~MaterialDescriptorSetManager();

    /*! \brief Retrieve the descriptor set for the material and the given frame index. The material must be bound in this frame
     *  \detail Only call from the render thread or a render task */
    const DescriptorSetRef& ForBoundMaterial(const Material* material, uint32 frameIndex);

    FixedArray<DescriptorSetRef, maxFramesInFlight> Allocate(uint32 boundIndex);
    FixedArray<DescriptorSetRef, maxFramesInFlight> Allocate(
        uint32 boundIndex,
        Span<const uint32> textureIndirectIndices,
        Span<const Handle<Texture>> textures);
    void Remove(uint32 boundIndex);

    void CreateFallbackMaterialDescriptorSet();

private:
    FixedArray<DescriptorSetRef, maxFramesInFlight> m_fallbackMaterialDescriptorSets;

    // bound index => descriptor sets
    HashMap<uint32, FixedArray<DescriptorSetRef, maxFramesInFlight>> m_materialDescriptorSets;
};

} // namespace hyperion

#endif
