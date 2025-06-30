/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_MATERIAL_HPP
#define HYPERION_RENDER_MATERIAL_HPP

#include <rendering/Shader.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderResource.hpp>

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

struct MaterialShaderData
{
    Vec4f albedo;

    // 4 vec4s of 0.0..1.0 values stuffed into uint32s
    Vec4u packedParams;

    Vec2f uvScale;
    float parallaxHeight;
    float _pad0;

    uint32 textureIndex[16];

    uint32 textureUsage;
    uint32 _pad1;
    uint32 _pad2;
    uint32 _pad3;

    Vec4f _pad4[4];
    Vec4f _pad5[4];
};

static_assert(sizeof(MaterialShaderData) == 256);

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

    /*! \note Only call this method from the render thread or task initiated by the render thread */
    HYP_FORCE_INLINE const FixedArray<DescriptorSetRef, maxFramesInFlight>& GetDescriptorSets() const
    {
        return m_descriptorSets;
    }

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
    FixedArray<DescriptorSetRef, maxFramesInFlight> m_descriptorSets;
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

    HYP_FORCE_INLINE const DescriptorSetRef& GetInvalidMaterialDescriptorSet(uint32 frameIndex) const
    {
        return m_invalidMaterialDescriptorSets[frameIndex];
    }

    /*! \brief Get the descriptor set for the given material and frame index. Only
     *   to be used from the render thread. If the descriptor set is not found, nullptr
     *   is returned.
     *   \param material The material to get the descriptor set for
     *   \param frameIndex The frame index to get the descriptor set for
     *   \returns The descriptor set for the given material and frame index or nullptr if not found
     */
    const DescriptorSetRef& GetDescriptorSet(const RenderMaterial* material, uint32 frameIndex) const;

    /*! \brief Add a material to the manager. This will create a descriptor set for
     *  the material and add it to the manager. Usable from any thread.
     *  \note If this function is called form the render thread, the descriptor set will
     *  be created immediately. If called from any other thread, the descriptor set will
     *  be created on the next call to Update.
     *  \param material The material to add
     */
    FixedArray<DescriptorSetRef, maxFramesInFlight> AddMaterial(RenderMaterial* material);

    /*! \brief Add a material to the manager. This will create a descriptor set for
     *  the material and add it to the manager. Usable from any thread.
     *  \note If this function is called form the render thread, the descriptor set will
     *  be created immediately. If called from any other thread, the descriptor set will
     *  be created on the next call to Update.
     *  \param material The material to add
     *  \param textures The textures to add to the material
     */
    FixedArray<DescriptorSetRef, maxFramesInFlight> AddMaterial(RenderMaterial* material, FixedArray<Handle<Texture>, maxBoundTextures>&& textures);

    /*! \brief Remove a material from the manager. Only to be used from the render thread.
     *  \param material The Id of the material to remove
     */
    void RemoveMaterial(const RenderMaterial* material);

    /*! \brief Initialize the MaterialDescriptorSetManager - Only to be used by the owning Engine instance. */
    void Initialize();

    /*! \brief Process any pending additions or removals. Usable from the render thread. */
    void UpdatePendingDescriptorSets(FrameBase* frame);

    /*! \brief Update the descriptor sets for the given frame. Usable from the render thread. */
    void Update(FrameBase* frame);

private:
    void CreateInvalidMaterialDescriptorSet();

    FixedArray<DescriptorSetRef, maxFramesInFlight> m_invalidMaterialDescriptorSets;

    HashMap<RenderMaterial*, FixedArray<DescriptorSetRef, maxFramesInFlight>> m_materialDescriptorSets;

    Array<Pair<RenderMaterial*, FixedArray<DescriptorSetRef, maxFramesInFlight>>> m_pendingAddition;
    Array<RenderMaterial*> m_pendingRemoval;
    Mutex m_pendingMutex;
    AtomicVar<bool> m_pendingAdditionFlag;

    FixedArray<Array<RenderMaterial*>, maxFramesInFlight> m_descriptorSetsToUpdate;
    Mutex m_descriptorSetsToUpdateMutex;
    AtomicVar<uint32> m_descriptorSetsToUpdateFlag;
};

} // namespace hyperion

#endif
