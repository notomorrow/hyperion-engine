#ifndef HYPERION_V2_ENV_GRID_HPP
#define HYPERION_V2_ENV_GRID_HPP

#include <core/Base.hpp>
#include <core/lib/AtomicVar.hpp>

#include <rendering/EntityDrawCollection.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <core/Containers.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

class Entity;

using EnvGridFlags = uint32;

enum EnvGridFlagBits : EnvGridFlags
{
    ENV_GRID_FLAGS_NONE                     = 0x0,
    ENV_GRID_FLAGS_RESET_REQUESTED          = 0x1,
    ENV_GRID_FLAGS_NEEDS_VOXEL_GRID_OFFSET  = 0x2
};

struct RenderCommand_UpdateEnvProbeAABBsInGrid;

struct EnvProbeCollection
{
    uint                                                    num_probes = 0;
    FixedArray<uint, max_bound_ambient_probes * 2>          indirect_indices = { 0 };
    FixedArray<Handle<EnvProbe>, max_bound_ambient_probes>  probes = { };

    // Must be called on init, before render thread starts using probes too
    // returns the index
    uint AddProbe(Handle<EnvProbe> probe)
    {
        AssertThrow(num_probes < max_bound_ambient_probes);

        const uint index = num_probes++;

        probes[index] = std::move(probe);
        indirect_indices[index] = index;
        indirect_indices[max_bound_ambient_probes + index] = index;

        return index;
    }

    void AddProbe(uint index, Handle<EnvProbe> probe)
    {
        AssertThrow(index < max_bound_ambient_probes);

        num_probes = MathUtil::Max(num_probes, index + 1);

        probes[index] = std::move(probe);
        indirect_indices[index] = index;
        indirect_indices[max_bound_ambient_probes + index] = index;
    }

    void SetProbeIndexOnGameThread(uint index, uint new_index)
    {
        AssertThrow(index < max_bound_ambient_probes);
        AssertThrow(new_index < max_bound_ambient_probes);

        indirect_indices[index] = new_index;
    }

    uint GetEnvProbeIndexOnGameThread(uint index) const
        { return indirect_indices[index]; }

    Handle<EnvProbe> &GetEnvProbeDirect(uint index)
        { return probes[index]; }

    const Handle<EnvProbe> &GetEnvProbeDirect(uint index) const
        { return probes[index]; }

    Handle<EnvProbe> &GetEnvProbeOnGameThread(uint index)
        { return probes[indirect_indices[index]]; }

    const Handle<EnvProbe> &GetEnvProbeOnGameThread(uint index) const
        { return probes[indirect_indices[index]]; }

    void SetProbeIndexOnRenderThread(uint index, uint new_index)
    {
        AssertThrow(index < max_bound_ambient_probes);
        AssertThrow(new_index < max_bound_ambient_probes);

        indirect_indices[max_bound_ambient_probes + index] = new_index;
    }

    uint GetEnvProbeIndexOnRenderThread(uint index) const
        { return indirect_indices[max_bound_ambient_probes + index]; }
    
    Handle<EnvProbe> &GetEnvProbeOnRenderThread(uint index)
        { return probes[indirect_indices[max_bound_ambient_probes + index]]; }
    
    const Handle<EnvProbe> &GetEnvProbeOnRenderThread(uint index) const
        { return probes[indirect_indices[max_bound_ambient_probes + index]]; }
};

class EnvGrid : public RenderComponent<EnvGrid>
{
public:
    friend struct RenderCommand_UpdateEnvProbeAABBsInGrid;

    EnvGrid(Name name, EnvGridType type, const BoundingBox &aabb, const Extent3D &density);
    EnvGrid(const EnvGrid &other) = delete;
    EnvGrid &operator=(const EnvGrid &other) = delete;
    virtual ~EnvGrid();

    HYP_FORCE_INLINE EnvGridType GetEnvGridType() const
        { return m_type; }

    void SetCameraData(const Vec3f &camera_position);

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    Vec3f SizeOfProbe() const
        { return m_aabb.GetExtent() / Vec3f(m_density); }

    EnvProbeType GetEnvProbeType() const
    {
        switch (GetEnvGridType()) {
        case ENV_GRID_TYPE_SH:
            return ENV_PROBE_TYPE_AMBIENT;
        default:
            return ENV_PROBE_TYPE_INVALID;
        }
    }

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    void CreateShader();
    void CreateFramebuffer();

    void CreateVoxelGridData();

    void CreateSHData();

    void RenderEnvProbe(
        Frame *frame,
        uint32 probe_index
    );

    void ComputeSH(
        Frame *frame,
        const ImageRef &image,
        const ImageViewRef &image_view,
        uint32 probe_index
    );

    void OffsetVoxelGrid(
        Frame *frame,
        Vec3i offset
    );

    void VoxelizeProbe(
        Frame *frame,
        uint32 probe_index
    );

    EnvGridType m_type;

    BoundingBox m_aabb;
    BoundingBox m_voxel_grid_aabb;
    Vec3f m_offset;
    Extent3D m_density;
    
    Handle<Camera> m_camera;
    RenderList m_render_list;

    Handle<Shader> m_ambient_shader;
    Handle<Framebuffer> m_framebuffer;
    
    // Array<Handle<EnvProbe>> m_ambient_probes;
    // Array<const EnvProbeDrawProxy *> m_env_probe_draw_proxies;

    EnvProbeCollection m_env_probe_collection;

    EnvGridShaderData m_shader_data;
    uint m_current_probe_index;

    AtomicVar<EnvGridFlags> m_flags;

    ComputePipelineRef m_clear_sh;
    ComputePipelineRef m_compute_sh;
    ComputePipelineRef m_finalize_sh;
    DescriptorTableRef m_compute_sh_descriptor_table;
    GPUBufferRef m_sh_tiles_buffer;

    ComputePipelineRef m_clear_voxels;
    ComputePipelineRef m_voxelize_probe;
    ComputePipelineRef m_offset_voxel_grid;
    ComputePipelineRef m_generate_voxel_grid_mipmaps;
    
    Handle<Texture> m_voxel_grid_texture;

    Array<ImageViewRef> m_voxel_grid_mips;
    Array<DescriptorTableRef> m_generate_voxel_grid_mipmaps_descriptor_tables;

    Queue<uint> m_next_render_indices;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP