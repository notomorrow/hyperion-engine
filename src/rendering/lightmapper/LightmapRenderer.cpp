#include <rendering/lightmapper/LightmapRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

#pragma region Render commands

struct RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer) : renderer::RenderCommand
{
    GPUBufferRef uniform_buffer;

    RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer)(const GPUBufferRef &uniform_buffer)
        : uniform_buffer(uniform_buffer)
    {
    }

    virtual ~RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer)() override = default;

    virtual Result operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms)));
        uniform_buffer->Memset(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms), 0x0);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

// LightmapPathTracer

LightmapPathTracer::LightmapPathTracer(Handle<TLAS> tlas)
    : m_tlas(std::move(tlas)),
      m_uniform_buffer(MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::CONSTANT_BUFFER)),
      m_rays_buffer(MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER)),
      m_hits_buffer(MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER)),
      m_hits_staging_buffer(MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER)),
      m_raytracing_pipeline(MakeRenderObject<renderer::RaytracingPipeline>())
{
    
}

LightmapPathTracer::~LightmapPathTracer()
{
    SafeRelease(std::move(m_uniform_buffer));
    SafeRelease(std::move(m_rays_buffer));
    SafeRelease(std::move(m_hits_buffer));
    SafeRelease(std::move(m_hits_staging_buffer));
    SafeRelease(std::move(m_raytracing_pipeline));
}

void LightmapPathTracer::CreateUniformBuffer()
{
    m_uniform_buffer = MakeRenderObject<GPUBuffer>(UniformBuffer());

    PUSH_RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer, m_uniform_buffer);
}

void LightmapPathTracer::Create()
{
    CreateUniformBuffer();

    DeferCreate(
        m_hits_buffer,
        g_engine->GetGPUDevice(),
        sizeof(LightmapHitsBuffer)
    );

    DeferCreate(
        m_hits_staging_buffer,
        g_engine->GetGPUDevice(),
        sizeof(LightmapHitsBuffer)
    );

    DeferCreate(
        m_rays_buffer,
        g_engine->GetGPUDevice(),
        sizeof(Vec4f) * 2
    );

    Handle<Shader> shader = g_shader_manager->GetOrCreate(HYP_NAME(LightmapPathTracer));

    if (!InitObject(shader)) {
        return;
    }

    renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader().GetDefinition().GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSet2Ref &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(RTRadianceDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(HYP_NAME(TLAS), m_tlas->GetInternalTLAS());
        descriptor_set->SetElement(HYP_NAME(MeshDescriptionsBuffer), m_tlas->GetInternalTLAS()->GetMeshDescriptionsBuffer());
        descriptor_set->SetElement(HYP_NAME(HitsBuffer), m_hits_buffer);
        descriptor_set->SetElement(HYP_NAME(RaysBuffer), m_rays_buffer);

        descriptor_set->SetElement(HYP_NAME(LightsBuffer), g_engine->GetRenderData()->lights.GetBuffer());
        descriptor_set->SetElement(HYP_NAME(MaterialsBuffer), g_engine->GetRenderData()->materials.GetBuffer());

        descriptor_set->SetElement(HYP_NAME(RTRadianceUniforms), m_uniform_buffer);
    }

    DeferCreate(
        descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_raytracing_pipeline = MakeRenderObject<RaytracingPipeline>(
        shader->GetShaderProgram(),
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

    const uint32 num_bound_lights = MathUtil::Min(uint32(g_engine->GetRenderState().lights.Size()), 16);

    for (uint32 index = 0; index < num_bound_lights; index++) {
        uniforms.light_indices[index] = (g_engine->GetRenderState().lights.Data() + index)->first.ToIndex();
    }

    uniforms.num_bound_lights = num_bound_lights;

    m_uniform_buffer->Copy(g_engine->GetGPUDevice(), sizeof(uniforms), &uniforms);
}

void LightmapPathTracer::ReadHitsBuffer(LightmapHitsBuffer *ptr)
{
    m_hits_buffer->Read(
        g_engine->GetGPUDevice(),
        sizeof(LightmapHitsBuffer),
        ptr
    );
}

void LightmapPathTracer::Trace(Frame *frame, const Array<LightmapRay> &rays, uint32 ray_offset)
{
    UpdateUniforms(frame, ray_offset);

    { // rays buffer
        Array<float> ray_float_data;
        ray_float_data.Resize(rays.Size() * 8);

        for (uint i = 0; i < rays.Size(); i++) {
            ray_float_data[i * 8 + 0] = rays[i].ray.position.x;
            ray_float_data[i * 8 + 1] = rays[i].ray.position.y;
            ray_float_data[i * 8 + 2] = rays[i].ray.position.z;

            ray_float_data[i * 8 + 4] = rays[i].ray.direction.x;
            ray_float_data[i * 8 + 5] = rays[i].ray.direction.y;
            ray_float_data[i * 8 + 6] = rays[i].ray.direction.z;
        }
        
        bool rays_buffer_resized = false;

        HYPERION_ASSERT_RESULT(m_rays_buffer->EnsureCapacity(g_engine->GetGPUDevice(), ray_float_data.Size() * sizeof(float), &rays_buffer_resized));
        m_rays_buffer->Copy(g_engine->GetGPUDevice(), ray_float_data.Size() * sizeof(float), ray_float_data.Data());

        if (rays_buffer_resized) {
            for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                m_raytracing_pipeline->GetDescriptorTable().Get()->GetDescriptorSet(HYP_NAME(RTRadianceDescriptorSet), frame_index)
                    ->SetElement(HYP_NAME(RaysBuffer), m_rays_buffer);

                HYPERION_ASSERT_RESULT(m_raytracing_pipeline->GetDescriptorTable().Get()->Update(g_engine->GetGPUDevice(), frame_index));
            }
        }
    }

    { // hits buffer (zero it out)
        m_hits_buffer->Memset(g_engine->GetGPUDevice(), sizeof(LightmapHitsBuffer), 0x0);
    }
    
    m_raytracing_pipeline->Bind(frame->GetCommandBuffer());

    m_raytracing_pipeline->GetDescriptorTable().Get()->Bind(
        frame,
        m_raytracing_pipeline,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_hits_buffer->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::UNORDERED_ACCESS
    );

    m_raytracing_pipeline->TraceRays(
        g_engine->GetGPUDevice(),
        frame->GetCommandBuffer(),
        Extent3D { uint32(rays.Size()), 1, 1 }
    );

    m_hits_buffer->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::UNORDERED_ACCESS
    );

    m_previous_frame_rays = rays;
}

// LightmapJob

LightmapJob::LightmapJob(Handle<Scene> scene, LightmapTraceMode trace_mode)
    : m_scene(std::move(scene)),
      m_trace_mode(trace_mode),
      m_is_ready { false },
      m_texel_index(0)
{
    if (m_scene.IsValid()) {
        m_scene->GetEntityManager()->PushCommand([this](EntityManager &mgr, GameCounter::TickUnit)
        {
            for (auto [entity, mesh_component, transform_component] : mgr.GetEntitySet<MeshComponent, TransformComponent>()) {
                if (!mesh_component.mesh.IsValid()) {
                    continue;
                }

                if (!mesh_component.material.IsValid()) {
                    continue;
                }

                // Only process opaque and translucent materials
                if (mesh_component.material->GetBucket() != BUCKET_OPAQUE && mesh_component.material->GetBucket() != BUCKET_TRANSLUCENT) {
                    continue;
                }

                const RC<StreamedMeshData> &streamed_mesh_data = mesh_component.mesh->GetStreamedMeshData();

                if (!streamed_mesh_data) {
                    continue;
                }

                const Matrix4 &transform_matrix = transform_component.transform.GetMatrix();

                m_entities.PushBack(LightmapEntity {
                    entity,
                    mesh_component.mesh,
                    mesh_component.material,
                    transform_matrix
                });

                // Set triangle data in m_triangle_cache
                // On CPU, we need to cache the triangles for ray tracing
                if (m_trace_mode == LIGHTMAP_TRACE_MODE_CPU) {
                    auto ref = streamed_mesh_data->AcquireRef();
                    const MeshData &mesh_data = ref->GetMeshData();

                    const Matrix4 normal_matrix = transform_matrix.Inverted().Transpose();

                    for (uint i = 0; i < mesh_data.indices.Size(); i += 3) {
                        Triangle triangle {
                            mesh_data.vertices[mesh_data.indices[i + 0]],
                            mesh_data.vertices[mesh_data.indices[i + 1]],
                            mesh_data.vertices[mesh_data.indices[i + 2]]
                        };

                        triangle[0].position = transform_matrix * triangle[0].position;
                        triangle[1].position = transform_matrix * triangle[1].position;
                        triangle[2].position = transform_matrix * triangle[2].position;

                        triangle[0].normal = (normal_matrix * Vec4f(triangle[0].normal, 0.0f)).GetXYZ();
                        triangle[1].normal = (normal_matrix * Vec4f(triangle[1].normal, 0.0f)).GetXYZ();
                        triangle[2].normal = (normal_matrix * Vec4f(triangle[2].normal, 0.0f)).GetXYZ();

                        triangle[0].tangent = (normal_matrix * Vec4f(triangle[0].tangent, 0.0f)).GetXYZ();
                        triangle[1].tangent = (normal_matrix * Vec4f(triangle[1].tangent, 0.0f)).GetXYZ();
                        triangle[2].tangent = (normal_matrix * Vec4f(triangle[2].tangent, 0.0f)).GetXYZ();

                        triangle[0].bitangent = (normal_matrix * Vec4f(triangle[0].bitangent, 0.0f)).GetXYZ();
                        triangle[1].bitangent = (normal_matrix * Vec4f(triangle[1].bitangent, 0.0f)).GetXYZ();
                        triangle[2].bitangent = (normal_matrix * Vec4f(triangle[2].bitangent, 0.0f)).GetXYZ();

                        m_triangle_cache[mesh_component.mesh.GetID()].PushBack(triangle);
                    }
                }
            }

            BuildUVMap();

            // Flatten texel indices, grouped by mesh IDs
            const LightmapUVMap &uv_map = GetUVMap();
            m_texel_indices.Reserve(uv_map.uvs.Size());

            for (const auto &it : uv_map.mesh_to_uv_indices) {
                for (uint i = 0; i < it.second.Size(); i++) {
                     m_texel_indices.PushBack(it.second[i]);
                }
            }

            m_is_ready.Set(true, MemoryOrder::RELAXED);
        });
    }
}

bool LightmapJob::IsCompleted() const
{
    if (!IsReady()) {
        return false;
    }

    if (!m_scene.IsValid()) {
        return true;
    }

    if (m_texel_index >= m_texel_indices.Size() * num_multisamples) {
        return true;
    }

    return false;
}


void LightmapJob::BuildUVMap()
{
    LightmapUVBuilder uv_builder { { m_entities } };

    auto uv_builder_result = uv_builder.Build();

    // @TODO Handle bad result

    m_uv_map = std::move(uv_builder_result.uv_map);
}

void LightmapJob::GatherRays(uint max_ray_hits, Array<LightmapRay> &out_rays)
{
    if (!IsReady()) {
        return;
    }

    if (IsCompleted()) {
        return;
    }

    Optional<Pair<ID<Mesh>, StreamedDataRef<StreamedMeshData>>> streamed_mesh_data { };

    uint ray_index = 0;

    while (ray_index < max_ray_hits) {
        if (m_texel_index >= m_texel_indices.Size() * num_multisamples) {
            break;
        }

        const LightmapUV &uv = m_uv_map.uvs[m_texel_indices[m_texel_index % m_texel_indices.Size()]];

        Handle<Mesh> mesh = Handle<Mesh>(uv.mesh_id);

        if (!mesh.IsValid()) {
            ++m_texel_index;
            continue;
        }

        if (!mesh->GetStreamedMeshData()) {
            ++m_texel_index;
            continue;
        }

        if (!streamed_mesh_data.HasValue() || streamed_mesh_data.Get().first != mesh.GetID()) {
            streamed_mesh_data.Set({
                mesh.GetID(),
                mesh->GetStreamedMeshData()->AcquireRef()
            });
        }

        // Convert UV to world space
        const MeshData &mesh_data = streamed_mesh_data.Get().second->GetMeshData();

        AssertThrowMsg(
            uv.triangle_index * 3 + 2 < mesh_data.indices.Size(),
            "Triangle index (%u) out of range of mesh indices",
            uv.triangle_index
        );

        const Matrix4 normal_matrix = uv.transform.Inverted().Transpose();

        const Vec3f vertex_positions[3] = {
            uv.transform * mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 0]].position,
            uv.transform * mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 1]].position,
            uv.transform * mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 2]].position
        };

        const Vec3f vertex_normals[3] = {
            (normal_matrix * Vec4f(mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 0]].normal, 0.0f)).GetXYZ(),
            (normal_matrix * Vec4f(mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 1]].normal, 0.0f)).GetXYZ(),
            (normal_matrix * Vec4f(mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 2]].normal, 0.0f)).GetXYZ()
        };

        const Vec3f position = vertex_positions[0] * uv.barycentric_coords.x
            + vertex_positions[1] * uv.barycentric_coords.y
            + vertex_positions[2] * uv.barycentric_coords.z;
        
        const Vec3f normal = (vertex_normals[0] * uv.barycentric_coords.x
            + vertex_normals[1] * uv.barycentric_coords.y
            + vertex_normals[2] * uv.barycentric_coords.z).Normalize();

        out_rays.PushBack(LightmapRay {
            Ray {
                position,
                normal
            },
            mesh.GetID(),
            uv.triangle_index,
            m_texel_indices[m_texel_index % m_texel_indices.Size()]
        });
        
        ++m_texel_index;
        ++ray_index;
    }
}

Optional<LightmapHit> LightmapJob::TraceSingleRayOnCPU(const LightmapRay &ray)
{
    RayTestResults octree_results;

    if (m_scene->GetOctree().TestRay(ray.ray, octree_results)) {
        // distance, hit
        FlatMap<float, LightmapHit> results;

        for (const auto &hit : octree_results) {
            // now ray test each result as triangle mesh to find exact hit point
            if (ID<Entity> entity_id = ID<Entity>(hit.id)) {
                MeshComponent *mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(entity_id);
                TransformComponent *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(entity_id);

                if (mesh_component != nullptr && mesh_component->mesh.IsValid() && mesh_component->material.IsValid() && mesh_component->mesh->NumIndices() != 0 && transform_component != nullptr) {
                    const ID<Mesh> mesh_id = mesh_component->mesh.GetID();

                    auto triangle_cache_it = m_triangle_cache.Find(mesh_id);
                    AssertThrow(triangle_cache_it != m_triangle_cache.End());

                    const Optional<RayHit> hit = ray.ray.TestTriangleList(
                        triangle_cache_it->second,
                        transform_component->transform
                    );

                    if (hit.HasValue()) {
                        // Sample albedo and return as LightmapHit
                        const uint triangle_index = hit->id;
                        const Vec3f barycentric_coords = hit->barycentric_coords;

                        const Triangle &triangle = triangle_cache_it->second[triangle_index];

                        const Vec2f uv = triangle.GetPoint(0).texcoord0 * barycentric_coords.x
                            + triangle.GetPoint(1).texcoord0 * barycentric_coords.y
                            + triangle.GetPoint(2).texcoord0 * barycentric_coords.z;

                        const Vec4f color = Vec4f(mesh_component->material->GetParameter(Material::MATERIAL_KEY_ALBEDO));

                        // @TODO Sample albedo from texture

                        auto insert_result = results.Insert({
                            hit->distance,
                            {
                                color
                            }
                        });

                        // @TODO Recursion
                    }
                }
            }
        }

        if (!results.Empty()) {
            return results.Front().second;
        }
    }

    return { };
}

void LightmapJob::TraceRaysOnCPU(const Array<LightmapRay> &rays)
{
    rays.ParallelForEach(TaskSystem::GetInstance(), [this](const LightmapRay &ray, uint index, uint batch_index)
    {
        Optional<LightmapHit> hit = TraceSingleRayOnCPU(ray);

        if (hit.HasValue()) {
            LightmapUVMap &uv_map = GetUVMap();
            LightmapUV &uv = uv_map.uvs[ray.texel_index];

            //uv.color += Vec4f(hit.color * (1.0f / float(LightmapJob::num_multisamples)));
            uv.color = (uv.color * (Vec4f(1.0f) - Vec4f(hit->color.w))) + Vec4f(hit->color * hit->color.w);
        }
    });
}

// LightmapRenderer

LightmapRenderer::LightmapRenderer(Name name)
    : RenderComponent(name),
      m_trace_mode(LIGHTMAP_TRACE_MODE_CPU),
      m_num_jobs { 0u }
{
}

void LightmapRenderer::Init()
{
    if (g_engine->GetConfig().Get(CONFIG_RT_ENABLED)) {
        // trace on GPU if the card supports ray tracing
        m_trace_mode = LIGHTMAP_TRACE_MODE_GPU;
    }
}

void LightmapRenderer::InitGame()
{
}

void LightmapRenderer::OnRemoved()
{
    m_path_tracer.Reset();

    Mutex::Guard guard(m_queue_mutex);

    m_queue.Clear();

    m_num_jobs.Set(0u, MemoryOrder::RELAXED);
}

void LightmapRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    if (m_trace_mode != LIGHTMAP_TRACE_MODE_CPU) {
        return;
    }

    if (!m_num_jobs.Get(MemoryOrder::RELAXED)) {
        return;
    }

    DebugLog(
        LogType::Debug,
        "Processing %u lightmap jobs...\n",
        m_num_jobs.Get(MemoryOrder::RELAXED)
    );

    // Trace lightmap on CPU

    Mutex::Guard guard(m_queue_mutex);

    LightmapJob *job = m_queue.Front().Get();

    if (job->IsCompleted()) {
        HandleCompletedJob(job);

        return;
    }

    Array<LightmapRay> rays;

    job->GatherRays(max_ray_hits_cpu, rays);

    if (rays.Any()) {
        job->TraceRaysOnCPU(rays);
    }
}

void LightmapRenderer::OnRender(Frame *frame)
{
    if (m_trace_mode != LIGHTMAP_TRACE_MODE_GPU) {
        return;
    }

    if (!m_num_jobs.Get(MemoryOrder::RELAXED)) {
        return;
    }

    if (!m_path_tracer) {
        m_path_tracer.Reset(new LightmapPathTracer(m_parent->GetScene()->GetTLAS()));
        m_path_tracer->Create();
    }

    // Wait for path tracer to be ready to process rays
    if (!m_path_tracer->GetPipeline()->IsCreated()) {
        return;
    }

    DebugLog(
        LogType::Debug,
        "Processing %u lightmap jobs...\n",
        m_num_jobs.Get(MemoryOrder::RELAXED)
    );
    
    Array<LightmapRay> current_frame_rays;
    uint32 ray_offset = 0;

    // Read ray hits from previous frame
    const Array<LightmapRay> previous_frame_rays = std::move(m_path_tracer->GetPreviousFrameRays());

    LightmapHitsBuffer hits_buffer { };
    m_path_tracer->ReadHitsBuffer(&hits_buffer);
    
    {
        Mutex::Guard guard(m_queue_mutex);

        LightmapJob *job = m_queue.Front().Get();

        if (previous_frame_rays.Any()) {
            for (uint i = 0; i < previous_frame_rays.Size(); i++) {
                const LightmapRay &ray = previous_frame_rays[i];
                const LightmapHit &hit = hits_buffer.hits[i];

                LightmapUVMap &uv_map = job->GetUVMap();
                LightmapUV &uv = uv_map.uvs[ray.texel_index];

                //uv.color += Vec4f(hit.color * (1.0f / float(LightmapJob::num_multisamples)));
                uv.color = (uv.color * (Vec4f(1.0f) - Vec4f(hit.color.w))) + Vec4f(hit.color * hit.color.w);
            }
        }

        if (job->IsCompleted()) {
            HandleCompletedJob(job);

            return;
        }

        ray_offset = job->GetTexelIndex() % MathUtil::Max(job->GetTexelIndices().Size(), 1u);

        job->GatherRays(max_ray_hits_gpu, current_frame_rays);
    }

    if (current_frame_rays.Any()) {
        // Enqueue trace command
        m_path_tracer->Trace(frame, current_frame_rays, ray_offset);
    }
}

void LightmapRenderer::HandleCompletedJob(LightmapJob *job)
{
    DebugLog(LogType::Debug, "Lightmap tracing completed. Writing bitmap...\n");

    const LightmapUVMap &uv_map = job->GetUVMap();
    const Array<float> float_array = uv_map.ToFloatArray();

    ByteBuffer bitmap_bytebuffer(float_array.Size() * sizeof(float), float_array.Data());
    UniquePtr<StreamedData> streamed_data(new MemoryStreamedData(std::move(bitmap_bytebuffer)));
    (void)streamed_data->Load();

    Handle<Texture> lightmap_texture = CreateObject<Texture>(
        Extent3D { uv_map.width, uv_map.height, 1 },
        InternalFormat::RGBA32F,
        ImageType::TEXTURE_TYPE_2D,
        FilterMode::TEXTURE_FILTER_LINEAR,
        WrapMode::TEXTURE_WRAP_REPEAT,
        std::move(streamed_data)
    );

    InitObject(lightmap_texture);

    for (const auto &it : job->GetEntities()) {
        if (!it.material) {
            // @TODO: Set to default material
            continue;
        }

        it.material->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_LIGHT_MAP, lightmap_texture);
    }

    m_queue.Pop();
    m_num_jobs.Decrement(1u, MemoryOrder::RELAXED);
}

} // namespace hyperion::v2