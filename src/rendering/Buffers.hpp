/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_BUFFERS_HPP
#define HYPERION_BUFFERS_HPP

#include <rendering/DrawProxy.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/Platform.hpp>

#include <math/Rect.hpp>
#include <math/Matrix4.hpp>

#include <core/Defines.hpp>

#include <core/lib/HeapArray.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Range.hpp>
#include <core/lib/FixedArray.hpp>

#include <Constants.hpp>
#include <Types.hpp>
#include <Threads.hpp>

#include <memory>
#include <atomic>
#include <mutex>
#include <climits>


#define HYP_RENDER_OBJECT_OFFSET(cls, index) \
    (uint32((index) * sizeof(cls ## ShaderData)))

namespace hyperion::renderer {

namespace platform {
template <PlatformType PLATFORM>
class Device;
} // namespace platform

using Device = platform::Device<Platform::CURRENT>;

} // namespace hyperion::renderer

namespace hyperion {

using renderer::Device;
using renderer::ShaderVec2;
using renderer::ShaderVec3;
using renderer::ShaderVec4;
using renderer::ShaderMat4;
using renderer::GPUBuffer;
using renderer::GPUBufferType;

static constexpr SizeType max_entities_per_instance_batch = 60;
static constexpr SizeType max_probes_in_sh_grid_buffer = max_bound_ambient_probes;

enum EnvGridType : uint32
{
    ENV_GRID_TYPE_INVALID   = uint32(-1),
    ENV_GRID_TYPE_SH        = 0,
    ENV_GRID_TYPE_MAX
};

struct alignas(256) EntityInstanceBatch
{
    uint32 num_entities;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;
    uint32 indices[max_entities_per_instance_batch];
};

static_assert(sizeof(EntityInstanceBatch) == 256);

struct alignas(16) ParticleShaderData
{
    Vec4f   position;   //   4 x 4 = 16
    Vec4f   velocity;   // + 4 x 4 = 32
    Vec4f   color;      // + 4 x 4 = 48
    Vec4f   attributes; // + 4 x 4 = 64
};

static_assert(sizeof(ParticleShaderData) == 64);

struct alignas(16) GaussianSplattingInstanceShaderData {
    Vec4f   position;   //   4 x 4 = 16
    Vec4f   rotation;   // + 4 x 4 = 32
    Vec4f   scale;      // + 4 x 4 = 48
    Vec4f   color;      // + 4 x 4 = 64
};

static_assert(sizeof(GaussianSplattingInstanceShaderData) == 64);

struct alignas(16) GaussianSplattingSceneShaderData {
    Matrix4 model_matrix;
};

static_assert(sizeof(GaussianSplattingSceneShaderData) == 64);

struct alignas(256) CubemapUniforms
{
    Matrix4 projection_matrices[6];
    Matrix4 view_matrices[6];
};

static_assert(sizeof(CubemapUniforms) % 256 == 0);

struct alignas(256) SkeletonShaderData
{
    static constexpr SizeType max_bones = 256;

    Matrix4 bones[max_bones];
};

static_assert(sizeof(SkeletonShaderData) % 256 == 0);

enum EntityGPUDataFlags : uint32
{
    ENTITY_GPU_FLAG_NONE            = 0x0,
    ENTITY_GPU_FLAG_HAS_SKELETON    = 0x1
};

struct alignas(256) EntityShaderData
{
    Matrix4 model_matrix;
    Matrix4 previous_model_matrix;

    Vec4f   _pad0;
    Vec4f   _pad1;
    Vec4f   world_aabb_max;
    Vec4f   world_aabb_min;

    uint32  entity_index;
    uint32  _unused;
    uint32  material_index;
    uint32  skeleton_index;

    uint32  bucket;
    uint32  flags;
    uint32  _pad3;
    uint32  _pad4;

    Vec4u   user_data;
    Vec4f   _pad6;
};

static_assert(sizeof(EntityShaderData) == 256);

struct MaterialShaderData
{
    Vec4f               albedo;
    
    // 4 vec4s of 0.0..1.0 values stuffed into uint32s
    Vec4u               packed_params;
    
    Vec2f               uv_scale;
    float               parallax_height;
    float               _pad0;
    
    uint32              texture_index[16];
    
    uint32              texture_usage;
    uint32              _pad1;
    uint32              _pad2;
    uint32              _pad3;
};

static_assert(sizeof(MaterialShaderData) == 128);

struct SceneShaderData
{
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4f   fog_params;

    float   global_timer;
    uint32  frame_counter;
    uint32  enabled_render_components_mask;
    uint32  enabled_environment_maps_mask;

    HYP_PAD_STRUCT_HERE(uint8, 64 + 128);
};

static_assert(sizeof(SceneShaderData) == 256);

struct alignas(256) CameraShaderData
{
    Matrix4     view;
    Matrix4     projection;
    Matrix4     previous_view;

    Vec4u       dimensions;
    Vec4f       camera_position;
    Vec4f       camera_direction;
    Vec4f       jitter;
    
    float       camera_near;
    float       camera_far;
    float       camera_fov;
    float       _pad0;
};

static_assert(sizeof(CameraShaderData) == 512);

struct alignas(256) EnvGridShaderData
{
    uint32  probe_indices[max_bound_ambient_probes];

    Vec4f   center;
    Vec4f   extent;
    Vec4f   aabb_max;
    Vec4f   aabb_min;

    Vec4u   density;
    Vec4u   enabled_indices_mask;

    Vec4f   voxel_grid_aabb_max;
    Vec4f   voxel_grid_aabb_min;
};

static_assert(sizeof(EnvGridShaderData) == 4352);

struct alignas(256) ShadowShaderData
{
    Matrix4 projection;
    Matrix4 view;
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec2u   dimensions;
    uint32  flags;
};

static_assert(sizeof(ShadowShaderData) == 256);

struct alignas(256) EnvProbeShaderData
{
    Matrix4 face_view_matrices[6];

    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4f   world_position;

    uint32  texture_index;
    uint32  flags;
    float   camera_near;
    float   camera_far;

    Vec2u   dimensions;
    Vec2u   _pad2;

    Vec4i   position_in_grid;
    Vec4i   position_offset;
    Vec4u   _pad5;
};

static_assert(sizeof(EnvProbeShaderData) == 512);

struct alignas(16) ImmediateDrawShaderData
{
    Matrix4 transform;
    uint32  color_packed;
    uint32  probe_type;
    uint32  probe_id;
};

static_assert(sizeof(ImmediateDrawShaderData) == 80);

struct alignas(16) ObjectInstance
{
    uint32 entity_id;
    uint32 draw_command_index;
    uint32 instance_index;
    uint32 batch_index;
};

static_assert(sizeof(ObjectInstance) == 16);

struct alignas(128) LightShaderData
{
    uint32  light_id;
    uint32  light_type;
    uint32  color_packed;
    float   radius;
    // 16

    float   falloff;
    uint32  shadow_map_index;
    Vec2f   area_size;
    // 32

    Vec4f   position_intensity;
    Vec4f   normal;
    // 64

    Vec2f   spot_angles;
    uint32  material_id;
    uint32  _pad2;

    Vec4u   pad3;
    Vec4u   pad4;
    Vec4u   pad5;
};

static_assert(sizeof(LightShaderData) == 128);

struct alignas(256) SH9Buffer
{
    Vec4f   values[16];
};

static_assert(sizeof(SH9Buffer) == 256);

struct alignas(256) SHGridBuffer
{
    Vec4f   color_values[max_probes_in_sh_grid_buffer * 9];
};

// static_assert(sizeof(SHGridBuffer) == 9216);

struct alignas(16) EnvGridProbeDataBuffer
{
    static constexpr Vec2u probe_storage_resolution = { 64, 64 };

    Vec2u   data[probe_storage_resolution.x * probe_storage_resolution.y][max_bound_ambient_probes];
};

struct alignas(16) SHTile
{
    Vec4f   coeffs_weights[9];
};

static_assert(sizeof(SHTile) == 144);

struct alignas(16) VoxelUniforms
{
    Vec4f   extent;
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4u   dimensions; // num mipmaps stored in w component
};

static_assert(sizeof(VoxelUniforms) == 64);

struct alignas(256) BlueNoiseBuffer
{
    Vec4i   sobol_256spp_256d[256 * 256 / 4];
    Vec4i   scrambling_tile[128 * 128 * 8 / 4];
    Vec4i   ranking_tile[128 * 128 * 8 / 4];
};

static_assert(sizeof(BlueNoiseBuffer) == 1310720);
struct alignas(16) PostProcessingUniforms
{
    Vec2u   effect_counts; // pre, post
    Vec2u   last_enabled_indices; // pre, post
    Vec2u   masks; // pre, post
};

static_assert(sizeof(PostProcessingUniforms) == 32);

struct alignas(256) DDGIUniforms
{
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4u   probe_border;
    Vec4u   probe_counts;
    Vec4u   grid_dimensions;
    Vec4u   image_dimensions;
    Vec4u   params; // x = probe distance, y = num rays per probe, z = flags, w = num bound lights
    uint32  shadow_map_index;
    uint32  _pad0, _pad1, _pad2;
    uint32  light_indices[16];
    //HYP_PAD_STRUCT_HERE(uint32, 4);
};

static_assert(sizeof(DDGIUniforms) == 256);


struct alignas(16) RTRadianceUniforms
{
    uint32 num_bound_lights;
    uint32 ray_offset; // for lightmapper
    uint32 _pad1, _pad2;
    uint32 light_indices[16];
};

static_assert(sizeof(RTRadianceUniforms) == 80);

/* max number of skeletons, based on size in mb */
static const SizeType max_skeletons = (8ull * 1024ull * 1024ull) / sizeof(SkeletonShaderData);
static const SizeType max_skeletons_bytes = max_skeletons * sizeof(SkeletonShaderData);
/* max number of materials, based on size in mb */
static const SizeType max_materials = (8ull * 1024ull * 1024ull) / sizeof(MaterialShaderData);
static const SizeType max_materials_bytes = max_materials * sizeof(MaterialShaderData);
/* max number of entities, based on size in mb */
static const SizeType max_entities = (32ull * 1024ull * 1024ull) / sizeof(EntityShaderData);
static const SizeType max_entities_bytes = max_entities * sizeof(EntityShaderData);
/* max number of scenes, based on size in kb */
static const SizeType max_scenes = (32ull * 1024ull) / sizeof(SceneShaderData);
static const SizeType max_scenes_bytes = max_scenes * sizeof(SceneShaderData);
/* max number of cameras, based on size in kb */
static const SizeType max_cameras = (16ull * 1024ull) / sizeof(CameraShaderData);
static const SizeType max_cameras_bytes = max_cameras * sizeof(CameraShaderData);
/* max number of lights, based on size in kb */
static const SizeType max_lights = (64ull * 1024ull) / sizeof(LightShaderData);
static const SizeType max_lights_bytes = max_lights * sizeof(LightShaderData);
/* max number of shadow maps, based on size in kb */
static const SizeType max_shadow_maps = (4ull * 1024ull) / sizeof(ShadowShaderData);
static const SizeType max_shadow_maps_bytes = max_shadow_maps * sizeof(ShadowShaderData);
/* max number of env probes, based on size in mb */
static const SizeType max_env_probes = (8ull * 1024ull * 1024ull) / sizeof(EnvProbeShaderData);
static const SizeType max_env_probes_bytes = max_env_probes * sizeof(EnvProbeShaderData);
/* max number of env grids, based on size in mb */
static const SizeType max_env_grids = (1ull * 1024ull * 1024ull) / sizeof(EnvGridShaderData);
static const SizeType max_env_grids_bytes = max_env_grids * sizeof(EnvGridShaderData);
/* max number of immediate drawn objects, based on size in mb */
static const SizeType max_immediate_draws = (1ull * 1024ull * 1024ull) / sizeof(ImmediateDrawShaderData);
static const SizeType max_immediate_draws_bytes = max_immediate_draws * sizeof(ImmediateDrawShaderData);
/* max number of instance batches, based on size in mb */
static const SizeType max_entity_instance_batches = (8ull * 1024ull * 1024ull) / sizeof(EntityInstanceBatch);
static const SizeType max_entity_instance_batches_bytes = max_entity_instance_batches * sizeof(EntityInstanceBatch);

template <class T>
using BufferTicket = uint;

template <class StructType, GPUBufferType BufferType, SizeType Size>
class ShaderData
{
public:
    ShaderData()
    {
        m_buffer = MakeRenderObject<GPUBuffer>(BufferType);

        m_staging_objects_pool.m_cpu_buffer.dirty.SetStart(0);
        m_staging_objects_pool.m_cpu_buffer.dirty.SetEnd(Size);
    }

    ShaderData(const ShaderData &other) = delete;
    ShaderData &operator=(const ShaderData &other) = delete;
    ~ShaderData() = default;

    const GPUBufferRef &GetBuffer() const
        { return m_buffer; }
    
    void Create(Device *device)
    {
        HYPERION_ASSERT_RESULT(m_buffer->Create(device, sizeof(StructType) * Size));
        m_buffer->Memset(device, sizeof(StructType) * Size, 0x00); // fill with zeros
    }

    void Destroy(Device *device)
    {
        SafeRelease(std::move(m_buffer));
    }

    void UpdateBuffer(Device *device, SizeType buffer_index)
    {
        m_staging_objects_pool.m_cpu_buffer.PerformUpdate(
            device,
            m_buffer
        );
    }

    void Set(SizeType index, const StructType &value)
    {
        m_staging_objects_pool.Set(index, value);
    }

    /*! \brief Get a reference to an object in the _current_ staging buffer,
     * use when it is preferable to fetch the object, update the struct, and then
     * call Set. This is usually when the object would have a large stack size
     */
    StructType &Get(SizeType index)
    {
        return m_staging_objects_pool.m_cpu_buffer.objects[index];
    }
    
    void MarkDirty(SizeType index)
    {
        m_staging_objects_pool.m_cpu_buffer.MarkDirty(index);
    }


    // @TODO: Optimize to use double buffering rather than needing mutex lock.

    BufferTicket<StructType>        current_index = 1; // reserve first index (0)
    Queue<BufferTicket<StructType>> free_indices;
    std::mutex                      m_mutex;

    BufferTicket<StructType> AcquireTicket()
    {
        std::lock_guard guard(m_mutex);
        // Threads::AssertOnThread(THREAD_RENDER);

        if (free_indices.Any()) {
            return free_indices.Pop();
        }

        return current_index++;
    }

    void ReleaseTicket(BufferTicket<StructType> batch_index)
    {
        // Threads::AssertOnThread(THREAD_RENDER);

        if (batch_index == 0) {
            return;
        }

        ResetBatch(batch_index);

        std::lock_guard guard(m_mutex);

        free_indices.Push(batch_index);
    }

    void ResetBatch(BufferTicket<StructType> batch_index)
    {
        // Threads::AssertOnThread(THREAD_RENDER | THREAD_TASK);

        if (batch_index == 0) {
            return;
        }

        AssertThrow(batch_index < Size);

        auto &batch = Get(batch_index);
        Memory::MemSet(&batch, 0, sizeof(StructType));

        MarkDirty(batch_index);
    }
    
private:
    struct StagingObjectsPool
    {
        static constexpr uint32 num_staging_buffers = 1;

        StagingObjectsPool() = default;
        StagingObjectsPool(const StagingObjectsPool &other) = delete;
        StagingObjectsPool &operator=(const StagingObjectsPool &other) = delete;
        StagingObjectsPool(StagingObjectsPool &other) = delete;
        StagingObjectsPool &operator=(StagingObjectsPool &&other) = delete;
        ~StagingObjectsPool() = default;

        struct StagingObjects
        {
            HeapArray<StructType, Size> objects;
            Range<SizeType>             dirty;

            StagingObjects()
            {
                for (auto &object : objects) {
                    object = { };
                }
            }

            StagingObjects(const StagingObjects &other) = delete;
            StagingObjects &operator=(const StagingObjects &other) = delete;
            StagingObjects(StagingObjects &other) = delete;
            StagingObjects &operator=(StagingObjects &&other) = delete;
            ~StagingObjects() = default;

            void PerformUpdate(Device *device, const GPUBufferRef &buffer)
            {
                const SizeType dirty_end = dirty.GetEnd(),
                    dirty_start = dirty.GetStart();

                if (dirty_end <= dirty_start) {
                    return;
                }

                const SizeType dirty_distance = dirty_end - dirty_start;

                buffer->Copy(
                    device,
                    dirty_start * sizeof(StructType),
                    dirty_distance * sizeof(StructType),
                    &objects.Data()[dirty_start]
                );

                dirty.Reset();
            }

            void MarkDirty(SizeType index)
            {
                dirty |= Range<SizeType> { index, index + 1 };
            }

        } m_cpu_buffer;

        void Set(SizeType index, const StructType &value)
        {
            AssertThrowMsg(index < m_cpu_buffer.objects.Size(), "Cannot set shader data at %llu in buffer: out of bounds", index);

            m_cpu_buffer.objects[index] = value;
            m_cpu_buffer.MarkDirty(index);
        }

        const StructType &Get(SizeType index)
        {
            AssertThrowMsg(index < m_cpu_buffer.objects.Size(), "Cannot get shader data at %llu in buffer: out of bounds", index);

            return m_cpu_buffer.objects[index];
        }

        void MarkDirty(SizeType index)
            { m_cpu_buffer.MarkDirty(index); }
    };

    GPUBufferRef        m_buffer;
    StagingObjectsPool  m_staging_objects_pool;
};

} // namespace hyperion

#endif
