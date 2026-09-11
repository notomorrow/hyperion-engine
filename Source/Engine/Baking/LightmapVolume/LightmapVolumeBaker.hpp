/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/Baker.hpp>
#include <Baking/BakerMemory.hpp>

#include <Baking/LightmapVolume/LightmapVolumeBakeData.hpp>

namespace Hyperion {

class LightmapVolume;
enum class LightmapElementId : uint32;

namespace Baking {

template <>
class Baker<LightmapVolume> final : public BakerBase
{
public:
    Baker(BakerConfig&& config, BakeLayer& bakeLayer, const Handle<LightmapVolume>& volume);

    Baker(const Baker& other) = delete;
    Baker& operator=(const Baker& other) = delete;

    Baker(Baker&& other) noexcept = delete;
    Baker& operator=(Baker&& other) noexcept = delete;

    virtual ~Baker() override
    {
        if (m_atlasBuildTask.IsValid() && !m_atlasBuildTask.IsCompleted())
        {
            m_atlasBuildTask.Await();
        }
    }

    virtual bool ShouldSplitIntoJobs() const override
    {
        return true;
    }

    virtual uint32 GetShadingTypesMask() const override
    {
        if (m_shadingTypesMaskOverride != 0)
        {
            return m_shadingTypesMaskOverride;
        }

        return (1u << int(PathTraceType::Lightmap));
    }

    virtual uint32 NumTexelSamples() const override
    {
        if (m_shadingTypesMaskOverride == (1u << int(PathTraceType::BentNormals)))
        {
            return m_config.bentNormalSamples;
        }

        return BakerBase::NumTexelSamples();
    }

    virtual const TypeInfo& GetInnerType() const
    {
        return TypeOf<LightmapVolume>();
    }

    Name GetBakeLayerName() const override;
    
    /// If true, we should skip UV1 generation for meshes
    bool ShouldReuseExistingPacking() const
    {
        return m_reuseExistingPacking;
    }

protected:
    bool ComputeShouldReuseExistingPacking();

    virtual BakeDataBase& GetBakeData() override
    {
        return m_bakeData;
    }

    virtual UniquePtr<BakeJobBase> CreateJob(BakeJobParams&& params) override;

    virtual void CreateLightmapRenderers() override;

    virtual void Initialize_Internal() override;
    virtual void OnCompleted_Internal() override;
    virtual void Build() override;

    virtual bool IsBuildAsync() const override
    {
        return true;
    }

    virtual bool PollBuildReady() override
    {
        return m_atlasBuildTask.IsValid() && m_atlasBuildTask.IsCompleted();
    }

    virtual void OnBuildReady() override;

    Handle<LightmapVolume> m_volume;
    BakeData<LightmapVolume> m_bakeData;
    Array<LightmapElementId, BakerAllocator> m_lightmapElementIds;

    // Decided once per bake in Build(); see ComputeShouldReuseExistingPacking
    bool m_reuseExistingPacking = false;

    // The packing-shape hash captured when that decision was made; written to the volume when a
    // new packing is generated in OnBuildReady()
    uint64 m_packingEntryHash = 0;

    Task<BakeData<LightmapVolume>> m_atlasBuildTask;
};

} // namespace Baking

} // namespace Hyperion
