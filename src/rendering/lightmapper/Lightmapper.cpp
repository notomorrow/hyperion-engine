/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/Lightmapper.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/Scene.hpp>
#include <rendering/Camera.hpp>
#include <rendering/EnvGrid.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/BVH.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/BLASComponent.hpp>
#include <scene/ecs/components/BVHComponent.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>
#include <core/utilities/Time.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer) : renderer::RenderCommand
{
    GPUBufferRef uniform_buffer;

    RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer)(GPUBufferRef uniform_buffer)
        : uniform_buffer(std::move(uniform_buffer))
    {
    }

    virtual ~RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms)));
        uniform_buffer->Memset(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms), 0x0);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(LightmapTraceRaysOnGPU) : renderer::RenderCommand
{
    LightmapJob         *job;
    Array<LightmapRay>  rays;

    RENDER_COMMAND(LightmapTraceRaysOnGPU)(
        LightmapJob *job,
        Array<LightmapRay> &&rays
    ) : job(job),
        rays(std::move(rays))
    {
    }

    virtual ~RENDER_COMMAND(LightmapTraceRaysOnGPU)() override
    {
        // SafeRelease(std::move(tlas));
    }

    virtual RendererResult operator()() override
    {
        Frame *frame = g_engine->GetGPUInstance()->GetFrameHandler()->GetCurrentFrame();

        const uint32 frame_index = frame->GetFrameIndex();
        const uint32 previous_frame_index = (frame_index + max_frames_in_flight - 1) % max_frames_in_flight;

        const RC<LightmapPathTracer> &path_tracer_radiance = job->GetParams().params.Get<LightmapJobGPUParams>().path_tracer_radiance;
        const RC<LightmapPathTracer> &path_tracer_irradiance = job->GetParams().params.Get<LightmapJobGPUParams>().path_tracer_irradiance;

        // Wait for path tracer to be ready to process rays
        AssertThrow(path_tracer_radiance != nullptr && path_tracer_radiance->GetPipeline()->IsCreated());
        AssertThrow(path_tracer_irradiance != nullptr && path_tracer_irradiance->GetPipeline()->IsCreated());
        
        uint32 ray_offset = 0;

        {
            // Read ray hits from last time this frame was rendered
            Array<LightmapRay> previous_rays;
            job->GetPreviousFrameRays(frame_index, previous_rays);
            
            // Read previous frame hits into CPU buffer
            if (previous_rays.Any()) {
                // @NOTE Use heap allocation to avoid stack overflow (max_ray_hits_gpu * sizeof(LightmapHit) > 1MB)
                UniquePtr<LightmapHitsBuffer> hits_buffer = MakeUnique<LightmapHitsBuffer>();

                path_tracer_radiance->ReadHitsBuffer(hits_buffer.Get(), frame_index);
                job->IntegrateRayHits(previous_rays.Data(), hits_buffer->hits.Data(), previous_rays.Size(), LIGHTMAP_SHADING_TYPE_RADIANCE);

                path_tracer_irradiance->ReadHitsBuffer(hits_buffer.Get(), frame_index);
                job->IntegrateRayHits(previous_rays.Data(), hits_buffer->hits.Data(), previous_rays.Size(), LIGHTMAP_SHADING_TYPE_IRRADIANCE);
            }

            ray_offset = job->GetTexelIndex() % MathUtil::Max(job->GetTexelIndices().Size(), 1u);

            job->SetPreviousFrameRays(frame_index, rays);
        }

        if (rays.Any()) {
            path_tracer_radiance->Trace(frame, rays, ray_offset);
            path_tracer_irradiance->Trace(frame, rays, ray_offset);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region LightmapAccelerationStructure

struct LightmapRayHitData
{
    Handle<Entity>  entity;
    Triangle        triangle;
    RayHit          hit;
};

using LightmapRayTestResults = FlatMap<float, LightmapRayHitData>;

class ILightmapAccelerationStructure
{
public:
    virtual ~ILightmapAccelerationStructure() = default;

    virtual LightmapRayTestResults TestRay(const Ray &ray) const = 0;
};

/// reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/bvh.html

class LightmapBottomLevelAccelerationStructure final : public ILightmapAccelerationStructure
{
public:
    LightmapBottomLevelAccelerationStructure(const Handle<Entity> &entity, BVHNode &bvh)
        : m_entity(entity),
          m_root(&bvh)
    {
    }

    virtual ~LightmapBottomLevelAccelerationStructure() override = default;

    virtual LightmapRayTestResults TestRay(const Ray &ray) const override
    {
        LightmapRayTestResults results;

        if (m_root != nullptr) {
            const RayTestResults triangle_ray_test_results = m_root->TestRay(ray);

            for (const RayHit &ray_hit : triangle_ray_test_results) {
                AssertThrow(ray_hit.user_data != nullptr);

                const BVHNode *bvh_node = static_cast<const BVHNode *>(ray_hit.user_data);

                results.Insert(
                    ray_hit.distance,
                    LightmapRayHitData {
                        m_entity,
                        bvh_node->GetTriangles()[ray_hit.id],
                        ray_hit
                    }
                );
            }
        }

        return results;
    }

    HYP_FORCE_INLINE BVHNode *GetRoot() const
        { return m_root; }

private:
    Handle<Entity>  m_entity;
    BVHNode         *m_root;
};

class LightmapTopLevelAccelerationStructure final : public ILightmapAccelerationStructure
{
public:
    virtual ~LightmapTopLevelAccelerationStructure() override = default;

    virtual LightmapRayTestResults TestRay(const Ray &ray) const override
    {
        LightmapRayTestResults results;

        for (const UniquePtr<LightmapBottomLevelAccelerationStructure> &acceleration_structure : m_acceleration_structures) {
            results.Merge(acceleration_structure->TestRay(ray));
        }

        return results;
    }

    void Add(UniquePtr<LightmapBottomLevelAccelerationStructure> &&acceleration_structure)
    {
        if (!acceleration_structure) {
            return;
        }

        m_acceleration_structures.PushBack(std::move(acceleration_structure));
    }

private:
    Array<UniquePtr<LightmapBottomLevelAccelerationStructure>>  m_acceleration_structures;
};

#pragma endregion LightmapAccelerationStructure

#pragma region LightmapPathTracer

LightmapPathTracer::LightmapPathTracer(const TLASRef &tlas, LightmapShadingType shading_type)
    : m_tlas(tlas),
      m_shading_type(shading_type),
      m_uniform_buffers({
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::CONSTANT_BUFFER),
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::CONSTANT_BUFFER)
      }),
      m_rays_buffers({
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER),
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER)
      }),
      m_hits_buffers({
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER),
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER)
      }),
      m_raytracing_pipeline(MakeRenderObject<RaytracingPipeline>())
{
    
}

LightmapPathTracer::~LightmapPathTracer()
{
    SafeRelease(std::move(m_uniform_buffers));
    SafeRelease(std::move(m_rays_buffers));
    SafeRelease(std::move(m_hits_buffers));
    SafeRelease(std::move(m_raytracing_pipeline));
}

void LightmapPathTracer::CreateUniformBuffer()
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_uniform_buffers[frame_index] = MakeRenderObject<GPUBuffer>(UniformBuffer());

        PUSH_RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer, m_uniform_buffers[frame_index]);
    }
}

void LightmapPathTracer::Create()
{
    CreateUniformBuffer();

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        DeferCreate(
            m_hits_buffers[frame_index],
            g_engine->GetGPUDevice(),
            sizeof(LightmapHitsBuffer)
        );

        DeferCreate(
            m_rays_buffers[frame_index],
            g_engine->GetGPUDevice(),
            sizeof(Vec4f) * 2
        );
    }

    ShaderProperties shader_properties;

    switch (m_shading_type) {
    case LIGHTMAP_SHADING_TYPE_RADIANCE:
        shader_properties.Set("MODE_RADIANCE");
        break;
    case LIGHTMAP_SHADING_TYPE_IRRADIANCE:
        shader_properties.Set("MODE_IRRADIANCE");
        break;
    }

    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("LightmapPathTracer"), shader_properties);
    AssertThrow(shader.IsValid());

    renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("TLAS"), m_tlas);
        descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), m_tlas->GetMeshDescriptionsBuffer());
        descriptor_set->SetElement(NAME("HitsBuffer"), m_hits_buffers[frame_index]);
        descriptor_set->SetElement(NAME("RaysBuffer"), m_rays_buffers[frame_index]);

        descriptor_set->SetElement(NAME("LightsBuffer"), g_engine->GetRenderData()->lights->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("MaterialsBuffer"), g_engine->GetRenderData()->materials->GetBuffer(frame_index));

        descriptor_set->SetElement(NAME("RTRadianceUniforms"), m_uniform_buffers[frame_index]);
    }

    DeferCreate(
        descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_raytracing_pipeline = MakeRenderObject<RaytracingPipeline>(
        shader,
        descriptor_table
    );

    DeferCreate(
        m_raytracing_pipeline,
        g_engine->GetGPUDevice()
    );
}

void LightmapPathTracer::UpdateUniforms(Frame *frame, uint32 ray_offset)
{
    RTRadianceUniforms uniforms { };
    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    uniforms.ray_offset = ray_offset;

    const uint32 max_bound_lights = MathUtil::Min(g_engine->GetRenderState()->NumBoundLights(), ArraySize(uniforms.light_indices));
    uint32 num_bound_lights = 0;

    for (uint32 light_type = 0; light_type < uint32(LightType::MAX); light_type++) {
        if (num_bound_lights >= max_bound_lights) {
            break;
        }

        for (const auto &it : g_engine->GetRenderState()->bound_lights[light_type]) {
            if (num_bound_lights >= max_bound_lights) {
                break;
            }

            uniforms.light_indices[num_bound_lights++] = it->GetBufferIndex();
        }
    }

    uniforms.num_bound_lights = num_bound_lights;

    m_uniform_buffers[frame->GetFrameIndex()]->Copy(g_engine->GetGPUDevice(), sizeof(uniforms), &uniforms);
}

void LightmapPathTracer::ReadHitsBuffer(LightmapHitsBuffer *ptr, uint32 frame_index)
{
    m_hits_buffers[frame_index]->Read(
        g_engine->GetGPUDevice(),
        sizeof(LightmapHitsBuffer),
        ptr
    );

    //Memory::MemCpy(ptr, &m_previous_hits_buffers[frame_index], sizeof(LightmapHitsBuffer));
}

void LightmapPathTracer::Trace(Frame *frame, const Array<LightmapRay> &rays, uint32 ray_offset)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();
    const uint32 previous_frame_index = (frame->GetFrameIndex() + max_frames_in_flight - 1) % max_frames_in_flight;

    const SceneRenderResources *scene_render_resources = g_engine->GetRenderState()->GetActiveScene();
    const CameraRenderResources *camera_render_resources = &g_engine->GetRenderState()->GetActiveCamera();
    
    /*m_hits_buffers[previous_frame_index]->Read(
        g_engine->GetGPUDevice(),
        sizeof(LightmapHitsBuffer),
        &m_previous_hits_buffers[previous_frame_index]
    );*/

    UpdateUniforms(frame, ray_offset);

    { // rays buffer
        Array<float> ray_float_data;
        ray_float_data.Resize(rays.Size() * 8);

        for (uint32 i = 0; i < rays.Size(); i++) {
            ray_float_data[i * 8 + 0] = rays[i].ray.position.x;
            ray_float_data[i * 8 + 1] = rays[i].ray.position.y;
            ray_float_data[i * 8 + 2] = rays[i].ray.position.z;
            ray_float_data[i * 8 + 3] = 1.0f;
            ray_float_data[i * 8 + 4] = rays[i].ray.direction.x;
            ray_float_data[i * 8 + 5] = rays[i].ray.direction.y;
            ray_float_data[i * 8 + 6] = rays[i].ray.direction.z;
            ray_float_data[i * 8 + 7] = 0.0f;
        }
        
        bool rays_buffer_resized = false;

        HYPERION_ASSERT_RESULT(m_rays_buffers[frame->GetFrameIndex()]->EnsureCapacity(g_engine->GetGPUDevice(), ray_float_data.ByteSize(), &rays_buffer_resized));
        m_rays_buffers[frame->GetFrameIndex()]->Copy(g_engine->GetGPUDevice(), ray_float_data.ByteSize(), ray_float_data.Data());

        if (rays_buffer_resized) {
            m_raytracing_pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex())
                ->SetElement(NAME("RaysBuffer"), m_rays_buffers[frame->GetFrameIndex()]);

            HYPERION_ASSERT_RESULT(m_raytracing_pipeline->GetDescriptorTable()->Update(g_engine->GetGPUDevice(), frame->GetFrameIndex()));
        }
    }
    
    m_raytracing_pipeline->Bind(frame->GetCommandBuffer());

    m_raytracing_pipeline->GetDescriptorTable()->Bind(
        frame,
        m_raytracing_pipeline,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resources) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resources) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(g_engine->GetRenderState()->bound_env_grid.ToIndex()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(g_engine->GetRenderState()->GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_hits_buffers[frame->GetFrameIndex()]->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::UNORDERED_ACCESS
    );

    m_raytracing_pipeline->TraceRays(
        g_engine->GetGPUDevice(),
        frame->GetCommandBuffer(),
        Vec3u { uint32(rays.Size()), 1, 1 }
    );

    m_hits_buffers[frame->GetFrameIndex()]->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::UNORDERED_ACCESS
    );
}

#pragma endregion LightmapPathTracer

#pragma region LightmapJobThread

class LightmapTaskThread : public TaskThread
{
public:
    LightmapTaskThread(ThreadID id)
        : TaskThread(id)
    {
    }

    virtual ~LightmapTaskThread() override = default;
};

#pragma endregion LightmapJobThread

#pragma region LightmapTaskThreadPool

class LightmapTaskThreadPool : public TaskThreadPool
{
public:
    LightmapTaskThreadPool()
        : TaskThreadPool(TypeWrapper<LightmapTaskThread>(), NAME("LightmapperTracingThread"), NumThreadsToCreate())
    {
        HYP_LOG(Lightmap, Info, "Tracing lightmap rays using {} threads", m_threads.Size());
    }

    virtual ~LightmapTaskThreadPool() override = default;

private:
    static uint32 NumThreadsToCreate()
    {
        uint32 num_threads = g_engine->GetAppContext()->GetConfiguration().Get("lightmapper.threads").ToUInt32(4);
        return MathUtil::Clamp(num_threads, 1u, 128u);
    }
};

#pragma endregion LightmapTaskThreadPool

#pragma region LightmapJob

LightmapJob::LightmapJob(LightmapJobParams &&params)
    : m_params(std::move(params)),
      m_texel_index(0),
      m_task_thread_pool(MakeUnique<LightmapTaskThreadPool>())
{
}

LightmapJob::~LightmapJob()
{
    for (Task<void> &task : m_current_tasks) {
        if (!task.Cancel()) {
            task.Await();
        }
    }

    if (m_params.trace_mode == LightmapTraceMode::LIGHTMAP_TRACE_MODE_GPU) {
        // Prevent dangling pointers to this from being used
        HYP_SYNC_RENDER();
    }
}

void LightmapJob::Start()
{
    m_running_semaphore.Produce(1, [this](bool)
    {
        if (!m_uv_map.HasValue()) {
            // No elements to process
            if (!m_params.elements_view) {
                m_uv_map = LightmapUVMap { };

                return;
            }

            HYP_LOG(Lightmap, Info, "Lightmap job {}: Enqueue task to build UV map", m_uuid);

            m_build_uv_map_task = TaskSystem::GetInstance().Enqueue([this]() -> Result<LightmapUVMap>
            {
                return BuildUVMap();
            }, TaskThreadPoolName::THREAD_POOL_BACKGROUND);
        }
    });
}

void LightmapJob::Stop()
{
    m_running_semaphore.Release(1, [this](bool)
    {
        if (m_task_thread_pool->IsRunning()) {
            m_task_thread_pool->Stop();
        }
    });
}

bool LightmapJob::IsCompleted() const
{
    return !m_running_semaphore.IsInSignalState();
}

Result<LightmapUVMap> LightmapJob::BuildUVMap()
{
    return LightmapUVBuilder { { m_params.elements_view } }.Build();
}

void LightmapJob::Process()
{
    AssertThrow(IsRunning());

    HYP_LOG(Lightmap, Info, "Processing lightmap job {}", m_uuid);

    if (!m_uv_map.HasValue()) {
        // wait for uv map to finish building
        
        // If uv map is not valid, it must have a task that is building it
        AssertThrow(m_build_uv_map_task.IsValid());

        if (!m_build_uv_map_task.IsCompleted()) {
            HYP_LOG(Lightmap, Info, "Lightmap job {}: Waiting on UV map to finish building", m_uuid);

            return;
        } else {
            if (Result<LightmapUVMap> &uv_map_result = m_build_uv_map_task.Await()) {
                m_uv_map = std::move(*uv_map_result);
            }

            if (m_uv_map.HasValue()) {
                // Flatten texel indices, grouped by mesh IDs
                m_texel_indices.Reserve(m_uv_map->uvs.Size());

                for (const auto &it : m_uv_map->mesh_to_uv_indices) {
                    for (uint32 i = 0; i < it.second.Size(); i++) {
                        m_texel_indices.PushBack(it.second[i]);
                    }
                }

                // start our thread pool for tracing rays
                m_task_thread_pool->Start();
            } else {
                HYP_LOG(Lightmap, Error, "Failed to build UV map for lightmap job {}", m_uuid);

                // Mark as ready to stop further processing
                Stop();
                
                return;
            }
        }

        return;
    }

    HYP_LOG(Lightmap, Info, "Processing lightmap job {} ({} current tasks, {} current rays)",
        m_uuid, m_current_tasks.Size(), m_current_rays.Size());

    if (m_current_tasks.Any()) {
        for (Task<void> &task : m_current_tasks) {
            if (!task.IsCompleted()) {
                // Wait for next call
                return;
            }
        }

        for (Task<void> &task : m_current_tasks) {
            task.Await();
        }

        m_current_tasks.Clear();
    }
    
    m_current_rays.Clear();

    if (m_texel_index >= m_texel_indices.Size() * num_multisamples) {
        Stop();

        return;
    }

    switch (m_params.trace_mode) {
    case LightmapTraceMode::LIGHTMAP_TRACE_MODE_CPU:
        GatherRays(max_ray_hits_cpu, m_current_rays);

        HYP_LOG(Lightmap, Info, "Lightmap job {}: Found {} rays", m_uuid, m_current_rays.Size());

        if (m_current_rays.Empty()) {
            Stop();

            return;
        }

        TraceRaysOnCPU(m_current_rays, LIGHTMAP_SHADING_TYPE_IRRADIANCE);
        TraceRaysOnCPU(m_current_rays, LIGHTMAP_SHADING_TYPE_RADIANCE);

        HYP_LOG(Lightmap, Info, "Lightmap job {}: Found {} more tasks", m_uuid, m_current_tasks.Size());

        if (m_current_tasks.Empty()) {
            Stop();

            return;
        }

        break;
    case LightmapTraceMode::LIGHTMAP_TRACE_MODE_GPU:
    {
        { // Check if we can stop
            Mutex::Guard guard(m_previous_frame_rays_mutex);

            if (m_previous_frame_rays.Empty()) {
                Stop();

                return;
            }
        }

        GatherRays(max_ray_hits_gpu, m_current_rays);

        PUSH_RENDER_COMMAND(LightmapTraceRaysOnGPU, this, std::move(m_current_rays));

        break;
    }
    }
}

void LightmapJob::GatherRays(uint32 max_ray_hits, Array<LightmapRay> &out_rays)
{
    HYP_LOG(Lightmap, Info, "Gathering rays for lightmap job {}", m_uuid);

    Optional<Pair<ID<Mesh>, StreamedDataRef<StreamedMeshData>>> streamed_mesh_data_refs { };

    uint32 ray_index = 0;

    while (ray_index < max_ray_hits) {
        if (m_texel_index >= m_texel_indices.Size() * num_multisamples) {
            HYP_LOG(Lightmap, Info, "Lightmap job {}: stopping gathering, texel index = {}, texel_indices count = {}", m_uuid, m_texel_index, m_texel_indices.Size());

            break;
        }

        const uint32 uv_index = m_texel_indices[m_texel_index % m_texel_indices.Size()];
        
        AssertThrowMsg(
            uv_index < m_uv_map->uvs.Size(),
            "UV index (%llu) out of range of UV map (size: %llu)",
            uv_index,
            m_uv_map->uvs.Size()
        );

        const LightmapUV &uv = m_uv_map->uvs[uv_index];

        const Handle<Mesh> &mesh = uv.mesh;

        if (!mesh.IsValid()) {
            HYP_LOG(Lightmap, Warning, "Lightmap job {}: Mesh at texel index {} is not valid, skipping", m_uuid, m_texel_index);

            ++m_texel_index;

            continue;
        }

        if (!mesh->GetStreamedMeshData()) {
            HYP_LOG(Lightmap, Warning, "Lightmap job {}: Mesh {} does not have streamed mesh data set, skipping", m_uuid, mesh->GetName());

            ++m_texel_index;

            continue;
        }

        if (!streamed_mesh_data_refs.HasValue() || streamed_mesh_data_refs->first != mesh.GetID()) {
            streamed_mesh_data_refs.Set({ mesh.GetID(), mesh->GetStreamedMeshData()->AcquireRef() });
        }

        // Convert UV to world space
        AssertThrow(streamed_mesh_data_refs->second->IsInMemory());
        const MeshData &mesh_data = streamed_mesh_data_refs->second->GetMeshData();

        AssertThrowMsg(
            uv.triangle_index * 3 + 2 < mesh_data.indices.Size(),
            "Triangle index (%u) out of range of mesh indices. Num Indices: %u",
            uv.triangle_index,
            mesh->NumIndices()
        );

        const Matrix4 normal_matrix = uv.transform.Inverted().Transpose();

        const uint32 triangle_indices[3] = {
            mesh_data.indices[uv.triangle_index * 3 + 0],
            mesh_data.indices[uv.triangle_index * 3 + 1],
            mesh_data.indices[uv.triangle_index * 3 + 2]
        };

        AssertThrow(triangle_indices[0] < mesh_data.vertices.Size());
        AssertThrow(triangle_indices[1] < mesh_data.vertices.Size());
        AssertThrow(triangle_indices[2] < mesh_data.vertices.Size());

        const Vec3f vertex_positions[3] = {
            uv.transform * mesh_data.vertices[triangle_indices[0]].position,
            uv.transform * mesh_data.vertices[triangle_indices[1]].position,
            uv.transform * mesh_data.vertices[triangle_indices[2]].position
        };

        const Vec3f vertex_normals[3] = {
            (Vec4f(mesh_data.vertices[triangle_indices[0]].normal, 0.0f)).GetXYZ(),
            (Vec4f(mesh_data.vertices[triangle_indices[1]].normal, 0.0f)).GetXYZ(),
            (Vec4f(mesh_data.vertices[triangle_indices[2]].normal, 0.0f)).GetXYZ()
        };

        const Vec3f position = vertex_positions[0] * uv.barycentric_coords.x
            + vertex_positions[1] * uv.barycentric_coords.y
            + vertex_positions[2] * uv.barycentric_coords.z;
        
        const Vec3f normal = (uv.transform * Vec4f((vertex_normals[0] * uv.barycentric_coords.x
            + vertex_normals[1] * uv.barycentric_coords.y
            + vertex_normals[2] * uv.barycentric_coords.z), 0.0f)).GetXYZ().Normalize();

        out_rays.PushBack(LightmapRay {
            Ray { position, normal },
            mesh.GetID(),
            uv.triangle_index,
            uv_index
        });
        
        ++m_texel_index;
        ++ray_index;
    }

    HYP_LOG(Lightmap, Info, "Lightmap job {}: Gathered {} rays", m_uuid, ray_index);
}

void LightmapJob::IntegrateRayHits(const LightmapRay *rays, const LightmapHit *hits, uint32 num_hits, LightmapShadingType shading_type)
{
    for (uint32 i = 0; i < num_hits; i++) {
        const LightmapRay &ray = rays[i];
        const LightmapHit &hit = hits[i];

        LightmapUVMap &uv_map = GetUVMap();

        AssertThrowMsg(
            ray.texel_index < uv_map.uvs.Size(),
            "Texel index (%llu) out of range of UV map (size: %llu)",
            ray.texel_index,
            uv_map.uvs.Size()
        );

        LightmapUV &uv = uv_map.uvs[ray.texel_index];

        switch (shading_type) {
        case LIGHTMAP_SHADING_TYPE_RADIANCE:
            uv.radiance = (uv.radiance * (Vec4f(1.0f) - Vec4f(hit.color.w))) + Vec4f(hit.color * hit.color.w);
            break;
        case LIGHTMAP_SHADING_TYPE_IRRADIANCE:
            uv.irradiance = (uv.irradiance * (Vec4f(1.0f) - Vec4f(hit.color.w))) + Vec4f(hit.color * hit.color.w);
            break;
        }
    }
}

void LightmapJob::TraceSingleRayOnCPU(const LightmapRay &ray, LightmapRayHitPayload &out_payload)
{
    out_payload.throughput = Vec4f(0.0f);
    out_payload.emissive = Vec4f(0.0f);
    out_payload.radiance = Vec4f(0.0f);
    out_payload.normal = Vec3f(0.0f);
    out_payload.distance = -1.0f;
    out_payload.barycentric_coords = Vec3f(0.0f);
    out_payload.mesh_id = ID<Mesh>::invalid;
    out_payload.triangle_index = 0;

    LightmapTopLevelAccelerationStructure *acceleration_structure = GetAccelerationStructure();

    if (!acceleration_structure) {
        HYP_LOG(Lightmap, Warning, "No CPU acceleration structure set while tracing on CPU, cannot perform trace");

        return;
    }

    LightmapRayTestResults results = acceleration_structure->TestRay(ray.ray);
    
    if (!results.Any()) {
        return;
    }
    
    for (const Pair<float, LightmapRayHitData> &hit_data : results) {
        if (hit_data.first < 0.0f) {
            continue;
        }

        if (!hit_data.second.entity.IsValid()) {
            continue;
        }

        auto element_it = m_params.all_elements_map->Find(hit_data.second.entity);

        if (element_it == m_params.all_elements_map->End() || element_it->second == nullptr) {
            continue;
        }

        const LightmapElement &element = *element_it->second;

        const ID<Mesh> mesh_id = element.mesh.GetID();

        const Vec3f barycentric_coords = hit_data.second.hit.barycentric_coords;

        const Triangle &triangle = hit_data.second.triangle;

        const Vec2f uv = triangle.GetPoint(0).GetTexCoord0() * barycentric_coords.x
            + triangle.GetPoint(1).GetTexCoord0() * barycentric_coords.y
            + triangle.GetPoint(2).GetTexCoord0() * barycentric_coords.z;

        const Vec4f color = Vec4f(element.material->GetParameter(Material::MATERIAL_KEY_ALBEDO));

        // @TODO sample textures

        out_payload.emissive = Vec4f(0.0f);
        out_payload.throughput = color;
        out_payload.barycentric_coords = barycentric_coords;
        out_payload.mesh_id = mesh_id;
        out_payload.triangle_index = hit_data.second.hit.id;
        out_payload.normal = hit_data.second.hit.normal;
        out_payload.distance = hit_data.first;

        return;
    }
}

void LightmapJob::TraceRaysOnCPU(const Array<LightmapRay> &rays, LightmapShadingType shading_type)
{
    m_current_tasks.Concat(TaskSystem::GetInstance().ParallelForEach_Async(*m_task_thread_pool, rays, [this, shading_type](const LightmapRay &first_ray, uint32 index, uint32 batch_index)
    {
        uint32 seed = (uint32)rand();//index * m_texel_index;

        FixedArray<LightmapRay, max_bounces_cpu + 1> recursive_rays;
        
        FixedArray<LightmapRayHitPayload, max_bounces_cpu + 1> bounces;
        int num_bounces = 0;

        Vec3f direction = first_ray.ray.direction;

        if (shading_type == LightmapShadingType::LIGHTMAP_SHADING_TYPE_IRRADIANCE) {
            direction = MathUtil::RandomInHemisphere(
                Vec3f(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed)),
                first_ray.ray.direction
            );
        }

        Vec3f origin = first_ray.ray.position + first_ray.ray.direction * 0.05f;

        for (int bounce_index = 0; bounce_index < max_bounces_cpu; bounce_index++) {
            LightmapRay bounce_ray = first_ray;

            if (bounce_index != 0) {
                bounce_ray.mesh_id = bounces[bounce_index - 1].mesh_id;
                bounce_ray.triangle_index = bounces[bounce_index - 1].triangle_index;
            }

            bounce_ray.ray = Ray {
                origin,
                direction
            };

            recursive_rays[bounce_index] = bounce_ray;

            LightmapRayHitPayload &payload = bounces[bounce_index];
            payload = { };
            payload.throughput = Vec4f(1.0f);

            TraceSingleRayOnCPU(bounce_ray, payload);

            if (payload.distance < 0.0f) {
                // @TODO Sample environment map
                const Vec3f normal = bounce_index == 0 ? first_ray.ray.direction : bounces[bounce_index - 1].normal;

                // testing!! @FIXME
                const Vec3f L = Vec3f(-0.4f, 0.65f, 0.1f).Normalize();
                payload.emissive += Vec4f(1.0f) * MathUtil::Max(0.0f, normal.Dot(L));

                // payload.emissive += Vec4f(1.0f);

                ++num_bounces;

                break;
            }

            Vec3f hit_position = origin + direction * payload.distance;
            origin = hit_position + payload.normal * 0.05f;

            if (shading_type == LightmapShadingType::LIGHTMAP_SHADING_TYPE_IRRADIANCE) {
                direction = MathUtil::RandomInHemisphere(
                    Vec3f(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed)),
                    payload.normal
                );
            } else {
                // @TODO
            }

            ++num_bounces;
        }

        for (int bounce_index = int(num_bounces - 1); bounce_index >= 0; bounce_index--) {
            Vec4f radiance = bounces[bounce_index].emissive;

            if (bounce_index != num_bounces - 1) {
                radiance += bounces[bounce_index + 1].radiance * bounces[bounce_index].throughput;
            }

            float p = MathUtil::Max(radiance.x, MathUtil::Max(radiance.y, MathUtil::Max(radiance.z, radiance.w)));

            if (MathUtil::RandomFloat(seed) > p) {
                break;
            }

            radiance /= MathUtil::Max(p, 0.0001f);

            bounces[bounce_index].radiance = radiance;
        }

        if (num_bounces != 0) {
            LightmapHit hit;
            hit.color = bounces[0].radiance;

            if (MathUtil::IsNaN(hit.color) || !MathUtil::IsFinite(hit.color)) {
                HYP_LOG_ONCE(Lightmap, Warning, "NaN or infinite color detected while tracing rays");

                hit.color = Vec4f(0.0f);
            }

            hit.color.w = 1.0f;

            IntegrateRayHits(&recursive_rays[0], &hit, 1, shading_type);
        }
    }));
}

#pragma endregion LightmapJob

#pragma region Lightmapper

Lightmapper::Lightmapper(LightmapTraceMode trace_mode, const Handle<Scene> &scene)
    : m_trace_mode(trace_mode),
      m_scene(scene),
      m_num_jobs { 0 }
{
    if (m_trace_mode == LightmapTraceMode::LIGHTMAP_TRACE_MODE_GPU) {
        if (!m_path_tracer_radiance) {
            m_path_tracer_radiance.Emplace(scene->GetTLAS(), LIGHTMAP_SHADING_TYPE_RADIANCE);
            m_path_tracer_radiance->Create();
        }

        if (!m_path_tracer_irradiance) {
            m_path_tracer_irradiance.Emplace(scene->GetTLAS(), LIGHTMAP_SHADING_TYPE_IRRADIANCE);
            m_path_tracer_irradiance->Create();
        }
    }
}

Lightmapper::~Lightmapper()
{
    m_path_tracer_radiance.Reset();
    m_path_tracer_irradiance.Reset();

    m_queue.Clear();

    m_num_jobs.Set(0, MemoryOrder::RELEASE);
}

bool Lightmapper::IsComplete() const
{
    return m_num_jobs.Get(MemoryOrder::ACQUIRE) == 0;
}

LightmapJobParams Lightmapper::CreateLightmapJobParams(
    SizeType start_index,
    SizeType end_index,
    UniquePtr<LightmapTopLevelAccelerationStructure> &&acceleration_structure
)
{
    LightmapJobParams job_params {
        m_trace_mode,
        m_scene,
        m_lightmap_elements.ToSpan().Slice(start_index, end_index),
        &m_all_elements_map
    };

    switch (m_trace_mode) {
    case LightmapTraceMode::LIGHTMAP_TRACE_MODE_CPU:
        job_params.params = LightmapJobCPUParams { std::move(acceleration_structure) };

        break;
    case LightmapTraceMode::LIGHTMAP_TRACE_MODE_GPU:
        job_params.params = LightmapJobGPUParams { m_path_tracer_radiance, m_path_tracer_irradiance };

        break;
    }

    return job_params;
}

void Lightmapper::PerformLightmapping()
{
    const uint32 ideal_triangles_per_job = g_engine->GetAppContext()->GetConfiguration().Get("lightmapper.ideal_triangles_per_job").ToUInt32(8192);

    AssertThrowMsg(m_num_jobs.Get(MemoryOrder::ACQUIRE) == 0, "Cannot initialize lightmap renderer -- jobs currently running!");

    // Build jobs
    HYP_LOG(Lightmap, Info, "Building graph for lightmapper");

    EntityManager &mgr = *m_scene->GetEntityManager();

    m_lightmap_elements.Clear();
    m_all_elements_map.Clear();

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ)) {
        if (!mesh_component.mesh.IsValid()) {
            HYP_LOG(Lightmap, Info, "Skip entity with invalid mesh on MeshComponent");

            continue;
        }

        if (!mesh_component.material.IsValid()) {
            HYP_LOG(Lightmap, Info, "Skip entity with invalid material on MeshComponent");

            continue;
        }

        // Only process opaque and translucent materials
        if (mesh_component.material->GetBucket() != BUCKET_OPAQUE && mesh_component.material->GetBucket() != BUCKET_TRANSLUCENT) {
            HYP_LOG(Lightmap, Info, "Skip entity with bucket that is not opaque or translucent");

            continue;
        }

        // GPU lightmap trace mode requires a raytracing BLAS to be attached
        if (m_trace_mode == LightmapTraceMode::LIGHTMAP_TRACE_MODE_GPU) {
            BLASComponent *blas_component = mgr.TryGetComponent<BLASComponent>(entity_id);

            if (!blas_component || !blas_component->blas) {
                HYP_LOG(Lightmap, Info, "Skipping entity #{} because it has no bottom level acceleration structure attached", entity_id.Value());

                continue;
            }
        }

        Handle<Entity> entity { entity_id };
        AssertThrow(entity.IsValid());

        m_lightmap_elements.PushBack(LightmapElement {
            entity,
            mesh_component.mesh,
            mesh_component.material,
            transform_component.transform,
            bounding_box_component.world_aabb
        });
    }

    UniquePtr<LightmapTopLevelAccelerationStructure> acceleration_structure;

    uint32 num_triangles = 0;

    SizeType start_index;
    SizeType end_index;

    for (start_index = 0, end_index = 0; end_index < m_lightmap_elements.Size(); end_index++) {
        LightmapElement &element = m_lightmap_elements[end_index];

        m_all_elements_map.Set(element.entity, &element);

        if (ideal_triangles_per_job != 0 && num_triangles != 0 && num_triangles + element.mesh->NumIndices() / 3 > ideal_triangles_per_job) {
            if (end_index - start_index != 0) {
                HYP_LOG(Lightmap, Info, "Adding lightmap job for {} entities", end_index - start_index);

                UniquePtr<LightmapJob> job = MakeUnique<LightmapJob>(CreateLightmapJobParams(start_index, end_index, std::move(acceleration_structure)));

                start_index = end_index;

                AddJob(std::move(job));
            }

            num_triangles = 0;
        }

        if (m_trace_mode == LIGHTMAP_TRACE_MODE_CPU) {
            if (!acceleration_structure) {
                acceleration_structure = MakeUnique<LightmapTopLevelAccelerationStructure>();
            }

            BVHComponent *bvh_component = mgr.TryGetComponent<BVHComponent>(element.entity);

            if (!bvh_component) {
                // BVHUpdaterSystem will calculate BVH when added
                mgr.AddComponent<BVHComponent>(element.entity, BVHComponent { });

                bvh_component = &mgr.GetComponent<BVHComponent>(element.entity);
            }

            acceleration_structure->Add(MakeUnique<LightmapBottomLevelAccelerationStructure>(
                element.entity,
                bvh_component->bvh
            ));
        }

        HYP_LOG(Lightmap, Info, "Add Entity (#{}) to be processed for lightmap", element.entity.GetID().Value());

        num_triangles += element.mesh->NumIndices() / 3;
    }

    if (end_index - start_index != 0) {
        HYP_LOG(Lightmap, Info, "Adding final lightmap job for {} entities", end_index - start_index);

        UniquePtr<LightmapJob> job = MakeUnique<LightmapJob>(CreateLightmapJobParams(start_index, end_index, std::move(acceleration_structure)));

        AddJob(std::move(job));
    } else {
        HYP_LOG(Lightmap, Info, "Skipping adding lightmap job, no entities to process");
    }
}

void Lightmapper::Update(GameCounter::TickUnit delta)
{
    uint32 num_jobs = m_num_jobs.Get(MemoryOrder::ACQUIRE);

    if (num_jobs == 0) {
        return;
    }

    HYP_LOG(Lightmap, Info, "Processing {} lightmap jobs...", num_jobs);

    Mutex::Guard guard(m_queue_mutex);

    AssertThrow(!m_queue.Empty());
    LightmapJob *job = m_queue.Front().Get();

    // Start job if not started
    if (!job->IsRunning()) {
        job->Start();
    }

    job->Process();

    if (job->IsCompleted()) {
        HandleCompletedJob(job);
    }
}

void Lightmapper::HandleCompletedJob(LightmapJob *job)
{
    HYP_LOG(Lightmap, Debug, "Tracing completed for lightmapping job {}...", job->GetUUID());

    const LightmapUVMap &uv_map = job->GetUVMap();

    FixedArray<Bitmap<4, float>, 2> bitmaps = {
        uv_map.ToBitmapRadiance(),
        uv_map.ToBitmapIrradiance()
    };

    // for (auto &bitmap : bitmaps) {
    //     Bitmap<4, float> bitmap_dilated = bitmap;

    //     // Dilate lightmap
    //     for (uint32 x = 0; x < bitmap.GetWidth(); x++) {
    //         for (uint32 y = 0; y < bitmap.GetHeight(); y++) {
    //             Vec3f color = bitmap.GetPixel(x, y);
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x - 1, y - 1));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x, y - 1));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x + 1, y - 1));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x - 1, y));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x + 1, y));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x - 1, y + 1));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x, y + 1));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x + 1, y + 1));

    //             bitmap_dilated.GetPixel(x, y) = color;
    //         }
    //     }

    //     bitmap = bitmap_dilated;

    //     // Apply bilateral blur
    //     Bitmap<4, float> bitmap_blurred = bitmap;

    //     for (uint32 x = 0; x < bitmap.GetWidth(); x++) {
    //         for (uint32 y = 0; y < bitmap.GetHeight(); y++) {
    //             Vec3f color = Vec3f(0.0f);

    //             float total_weight = 0.0f;

    //             for (int dx = -1; dx <= 1; dx++) {
    //                 for (int dy = -1; dy <= 1; dy++) {
    //                     const uint32 nx = x + dx;
    //                     const uint32 ny = y + dy;

    //                     if (nx >= bitmap.GetWidth() || ny >= bitmap.GetHeight()) {
    //                         continue;
    //                     }

    //                     const Vec3f neighbor_color = bitmap.GetPixel(nx, ny);

    //                     const float spatial_weight = MathUtil::Exp(-float(dx * dx + dy * dy) / (2.0f * 1.0f));
    //                     const float color_weight = MathUtil::Exp(-(neighbor_color - bitmap.GetPixel(x, y)).LengthSquared() / (2.0f * 0.1f));

    //                     const float weight = spatial_weight * color_weight;

    //                     color += neighbor_color * weight;
    //                     total_weight += weight;
    //                 }
    //             }

    //             bitmap_blurred.GetPixel(x, y) = color / MathUtil::Max(total_weight, MathUtil::epsilon_f);
    //         }
    //     }

    //     bitmap = bitmap_blurred;
    // }
    
    // Temp; write to rgb8 bitmap
    uint32 num = rand() % 150;
    bitmaps[0].Write("lightmap_" + String::ToString(num) + "_radiance.bmp");
    bitmaps[1].Write("lightmap_" + String::ToString(num) + "_irradiance.bmp");

    FixedArray<Handle<Texture>, 2> textures;

    for (uint32 i = 0; i < 2; i++) {
        RC<StreamedTextureData> streamed_data = MakeRefCountedPtr<StreamedTextureData>(TextureData {
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA32F,
                Vec3u { uv_map.width, uv_map.height, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_REPEAT
            },
            bitmaps[i].ToByteBuffer()
        });

        Handle<Texture> texture = CreateObject<Texture>(std::move(streamed_data));
        InitObject(texture);

        textures[i] = std::move(texture);
    }

    for (LightmapElement &element : job->GetElements()) {
        bool is_new_material = false;

        if (!element.material) {
            // @TODO: Set to default material
            continue;
        }

        if (!element.material->IsDynamic()) {
            element.material = element.material->Clone();

            is_new_material = true;
        }
        
        element.material->SetTexture(MaterialTextureKey::RADIANCE_MAP, textures[0]);
        element.material->SetTexture(MaterialTextureKey::IRRADIANCE_MAP, textures[1]);

        if (is_new_material) {
            InitObject(element.material);

            m_scene->GetEntityManager()->PushCommand([entity = element.entity, mesh = element.mesh, new_material = element.material](EntityManager &mgr, GameCounter::TickUnit)
            {
                if (MeshComponent *mesh_component = mgr.TryGetComponent<MeshComponent>(entity)) {
                    mesh_component->material = std::move(new_material);
                    mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
                } else {
                    mgr.AddComponent<MeshComponent>(entity, MeshComponent {
                        mesh,
                        new_material
                    });
                }
            });
        }        
    }

    m_queue.Pop();
    m_num_jobs.Decrement(1, MemoryOrder::RELEASE);
}

#pragma endregion Lightmapper

} // namespace hyperion