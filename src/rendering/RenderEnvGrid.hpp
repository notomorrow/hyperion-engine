/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_ENV_GRID_HPP
#define HYPERION_RENDER_ENV_GRID_HPP

#include <core/Handle.hpp>

#include <core/math/Matrix4.hpp>

#include <rendering/Renderer.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class EnvGrid;
class RenderView;
class RenderScene;

class RenderEnvGrid final : public RenderResourceBase
{
public:
    RenderEnvGrid(EnvGrid* envGrid);
    virtual ~RenderEnvGrid() override;

    HYP_FORCE_INLINE EnvGrid* GetEnvGrid() const
    {
        return m_envGrid;
    }

    void SetProbeIndices(Array<uint32>&& indices);

    void Render(FrameBase* frame, const RenderSetup& renderSetup);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GpuBufferHolderBase* GetGpuBufferHolder() const override;

private:
    EnvGrid* m_envGrid;
};

struct HYP_API EnvGridPassData : PassData
{
    virtual ~EnvGridPassData() override;

    ShaderRef shader;
    FramebufferRef framebuffer;

    ComputePipelineRef clearSh;
    ComputePipelineRef computeSh;
    ComputePipelineRef reduceSh;
    ComputePipelineRef finalizeSh;

    Array<DescriptorTableRef> computeShDescriptorTables;
    Array<GpuBufferRef> shTilesBuffers;

    ComputePipelineRef clearVoxels;
    ComputePipelineRef voxelizeProbe;
    ComputePipelineRef offsetVoxelGrid;
    ComputePipelineRef generateVoxelGridMipmaps;

    Array<ImageViewRef> voxelGridMips;
    Array<DescriptorTableRef> generateVoxelGridMipmapsDescriptorTables;

    Array<GpuBufferRef> uniformBuffers;

    ComputePipelineRef computeIrradiance;
    ComputePipelineRef computeFilteredDepth;
    ComputePipelineRef copyBorderTexels;

    uint32 currentProbeIndex;
    Queue<uint32> nextRenderIndices;
};

struct EnvGridPassDataExt : PassDataExt
{
    EnvGrid* envGrid = nullptr;

    EnvGridPassDataExt()
        : PassDataExt(TypeId::ForType<EnvGridPassDataExt>())
    {
    }

    virtual ~EnvGridPassDataExt() override = default;

    virtual PassDataExt* Clone() override
    {
        EnvGridPassDataExt* clone = new EnvGridPassDataExt;
        clone->envGrid = envGrid;

        return clone;
    }
};

class EnvGridRenderer : public RendererBase
{
public:
    EnvGridRenderer();
    virtual ~EnvGridRenderer() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& renderSetup) override final;

protected:
    void RenderProbe(FrameBase* frame, const RenderSetup& renderSetup, uint32 probeIndex);

    void ComputeEnvProbeIrradiance_SphericalHarmonics(FrameBase* frame, const RenderSetup& renderSetup, const Handle<EnvProbe>& probe);
    void ComputeEnvProbeIrradiance_LightField(FrameBase* frame, const RenderSetup& renderSetup, const Handle<EnvProbe>& probe);
    void OffsetVoxelGrid(FrameBase* frame, const RenderSetup& renderSetup, Vec3i offset);
    void VoxelizeProbe(FrameBase* frame, const RenderSetup& renderSetup, uint32 probeIndex);

    PassData* CreateViewPassData(View* view, PassDataExt& ext) override;
    void CreateVoxelGridData(EnvGrid* envGrid, EnvGridPassData& pd);
    void CreateSphericalHarmonicsData(EnvGrid* envGrid, EnvGridPassData& pd);
    void CreateLightFieldData(EnvGrid* envGrid, EnvGridPassData& pd);
};

} // namespace hyperion

#endif
