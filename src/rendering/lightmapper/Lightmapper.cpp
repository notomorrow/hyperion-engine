/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/Lightmapper.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/backend/RenderBackend.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <scene/BVH.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/Texture.hpp>
#include <scene/View.hpp>

#include <scene/lightmapper/LightmapVolume.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/LightmapVolumeComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/Time.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/math/Triangle.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <system/AppContext.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)
    : RenderCommand
{
    GpuBufferRef uniformBuffer;

    RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)(GpuBufferRef uniformBuffer)
        : uniformBuffer(std::move(uniformBuffer))
    {
    }

    virtual ~RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniformBuffer->Create());
        uniformBuffer->Memset(sizeof(RTRadianceUniforms), 0x0);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(LightmapRender)
    : RenderCommand
{
    LightmapJob* job;
    RenderView* view;
    Array<LightmapRay> rays;
    uint32 rayOffset;

    RENDER_COMMAND(LightmapRender)(LightmapJob* job, RenderView* view, Array<LightmapRay>&& rays, uint32 rayOffset)
        : job(job),
          view(view),
          rays(std::move(rays)),
          rayOffset(rayOffset)
    {
        if (view)
        {
            view->IncRef();
        }

        job->m_numConcurrentRenderingTasks.Increment(1, MemoryOrder::RELEASE);
    }

    virtual ~RENDER_COMMAND(LightmapRender)() override
    {
        if (view)
        {
            view->DecRef();
        }

        job->m_numConcurrentRenderingTasks.Decrement(1, MemoryOrder::RELEASE);
    }

    virtual RendererResult operator()() override
    {
        FrameBase* frame = g_renderBackend->GetCurrentFrame();

        RenderSetup renderSetup { &g_engine->GetWorld()->GetRenderResource(), view };

        RenderProxyList* rpl = nullptr;

        if (view)
        {
            rpl = &RenderApi_GetConsumerProxyList(view->GetView());
        }

        const uint32 frameIndex = frame->GetFrameIndex();
        const uint32 previousFrameIndex = (frameIndex + maxFramesInFlight - 1) % maxFramesInFlight;

        if (rpl)
        {
            if (const auto& skyProbes = rpl->envProbes.GetElements<SkyProbe>(); skyProbes.Any())
            {
                renderSetup.envProbe = skyProbes.Front();
            }
        }

        {
            // Read ray hits from last time this frame was rendered
            Array<LightmapRay> previousRays;
            job->GetPreviousFrameRays(previousRays);

            // Read previous frame hits into CPU buffer
            if (previousRays.Size() != 0)
            {
                Array<LightmapHit> hitsBuffer;
                hitsBuffer.Resize(previousRays.Size());

                for (ILightmapRenderer* lightmapRenderer : job->GetParams().renderers)
                {
                    Assert(lightmapRenderer != nullptr);

                    lightmapRenderer->ReadHitsBuffer(frame, hitsBuffer);

                    job->IntegrateRayHits(previousRays, hitsBuffer, lightmapRenderer->GetShadingType());
                }
            }

            job->SetPreviousFrameRays(rays);
        }

        if (rays.Any())
        {
            for (ILightmapRenderer* lightmapRenderer : job->GetParams().renderers)
            {
                Assert(lightmapRenderer != nullptr);

                lightmapRenderer->Render(frame, renderSetup, job, rays, rayOffset);
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region LightmapperConfig

void LightmapperConfig::PostLoadCallback()
{
    if (traceMode == LightmapTraceMode::GPU_PATH_TRACING)
    {
        if (!g_renderBackend->GetRenderConfig().IsRaytracingSupported())
        {
            traceMode = LightmapTraceMode::CPU_PATH_TRACING;

            HYP_LOG(Lightmap, Warning, "GPU path tracing is not supported on this device. Falling back to CPU path tracing.");
        }
    }
}

#pragma endregion LightmapperConfig

#pragma region LightmapAccelerationStructure

struct LightmapRayHit : RayHit
{
    Handle<Entity> entity;
    Triangle triangle;

    LightmapRayHit() = default;

    LightmapRayHit(const RayHit& rayHit, const Handle<Entity>& entity, const Triangle& triangle)
        : RayHit(rayHit),
          entity(entity),
          triangle(triangle)
    {
    }

    LightmapRayHit(const LightmapRayHit& other) = default;
    LightmapRayHit& operator=(const LightmapRayHit& other) = default;

    LightmapRayHit(LightmapRayHit&& other) noexcept
        : RayHit(static_cast<RayHit&&>(std::move(other))),
          entity(std::move(other.entity)),
          triangle(std::move(other.triangle))
    {
    }

    LightmapRayHit& operator=(LightmapRayHit&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        RayHit::operator=(static_cast<RayHit&&>(std::move(other)));
        entity = std::move(other.entity);
        triangle = std::move(other.triangle);

        return *this;
    }

    virtual ~LightmapRayHit() = default;

    bool operator==(const LightmapRayHit& other) const
    {
        return static_cast<const RayHit&>(*this) == static_cast<const RayHit&>(other)
            && entity == other.entity
            && triangle == other.triangle;
    }

    bool operator!=(const LightmapRayHit& other) const
    {
        return static_cast<const RayHit&>(*this) != static_cast<const RayHit&>(other)
            || entity != other.entity
            || triangle != other.triangle;
    }

    bool operator<(const LightmapRayHit& other) const
    {
        if (static_cast<const RayHit&>(*this) < static_cast<const RayHit&>(other))
        {
            return true;
        }

        if (entity < other.entity)
        {
            return true;
        }

        if (entity == other.entity && triangle.GetPosition() < other.triangle.GetPosition())
        {
            return true;
        }

        return false;
    }
};

using LightmapRayTestResults = FlatSet<LightmapRayHit>;

class ILightmapAccelerationStructure
{
public:
    virtual ~ILightmapAccelerationStructure() = default;

    virtual const Transform& GetTransform() const = 0;

    virtual LightmapRayTestResults TestRay(const Ray& ray) const = 0;
};

class LightmapBottomLevelAccelerationStructure final : public ILightmapAccelerationStructure
{
public:
    LightmapBottomLevelAccelerationStructure(const LightmapSubElement* subElement, const BVHNode* bvh)
        : m_subElement(subElement),
          m_root(bvh)
    {
        Assert(m_subElement != nullptr);
        Assert(m_root != nullptr);
    }

    LightmapBottomLevelAccelerationStructure(const LightmapBottomLevelAccelerationStructure& other) = delete;
    LightmapBottomLevelAccelerationStructure& operator=(const LightmapBottomLevelAccelerationStructure& other) = delete;

    LightmapBottomLevelAccelerationStructure(LightmapBottomLevelAccelerationStructure&& other) noexcept
        : m_subElement(other.m_subElement),
          m_root(other.m_root)
    {
        other.m_subElement = nullptr;
        other.m_root = nullptr;
    }

    LightmapBottomLevelAccelerationStructure& operator=(LightmapBottomLevelAccelerationStructure&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_subElement = other.m_subElement;
        m_root = std::move(other.m_root);

        other.m_subElement = nullptr;
        other.m_root = nullptr;

        return *this;
    }

    virtual ~LightmapBottomLevelAccelerationStructure() override = default;

    HYP_FORCE_INLINE const Handle<Entity>& GetEntity() const
    {
        return m_subElement->entity;
    }

    virtual const Transform& GetTransform() const override
    {
        return m_subElement->transform;
    }

    virtual LightmapRayTestResults TestRay(const Ray& ray) const override
    {
        Assert(m_root != nullptr);

        LightmapRayTestResults results;

        const Matrix4& modelMatrix = m_subElement->transform.GetMatrix();

        const Ray localSpaceRay = modelMatrix.Inverted() * ray;

        RayTestResults localBvhResults = m_root->TestRay(localSpaceRay);

        if (localBvhResults.Any())
        {
            const Matrix4 normalMatrix = modelMatrix.Transposed().Inverted();

            RayTestResults bvhResults;

            for (RayHit hit : localBvhResults)
            {
                Vec4f transformedNormal = normalMatrix * Vec4f(hit.normal, 0.0f);
                hit.normal = transformedNormal.GetXYZ().Normalized();

                Vec4f transformedPosition = modelMatrix * Vec4f(hit.hitpoint, 1.0f);
                transformedPosition /= transformedPosition.w;

                hit.hitpoint = transformedPosition.GetXYZ();

                hit.distance = (hit.hitpoint - ray.position).Length();

                bvhResults.AddHit(hit);
            }

            for (const RayHit& rayHit : bvhResults)
            {
                Assert(rayHit.userData != nullptr);

                const BVHNode* bvhNode = static_cast<const BVHNode*>(rayHit.userData);

                const Triangle& triangle = bvhNode->triangles[rayHit.id];

                results.Emplace(rayHit, m_subElement->entity, triangle);
            }
        }

        return results;
    }

    HYP_FORCE_INLINE const BVHNode* GetRoot() const
    {
        return m_root;
    }

private:
    const LightmapSubElement* m_subElement;
    const BVHNode* m_root;
};

class LightmapTopLevelAccelerationStructure final : public ILightmapAccelerationStructure
{
public:
    virtual ~LightmapTopLevelAccelerationStructure() override = default;

    virtual const Transform& GetTransform() const override
    {
        return Transform::identity;
    }

    virtual LightmapRayTestResults TestRay(const Ray& ray) const override
    {
        LightmapRayTestResults results;

        for (const LightmapBottomLevelAccelerationStructure& accelerationStructure : m_accelerationStructures)
        {
            if (!ray.TestAABB(accelerationStructure.GetTransform() * accelerationStructure.GetRoot()->aabb))
            {
                continue;
            }

            results.Merge(accelerationStructure.TestRay(ray));
        }

        return results;
    }

    void Add(const LightmapSubElement* subElement, const BVHNode* bvh)
    {
        m_accelerationStructures.EmplaceBack(subElement, bvh);
    }

    void RemoveAll()
    {
        m_accelerationStructures.Clear();
    }

private:
    Array<LightmapBottomLevelAccelerationStructure> m_accelerationStructures;
};

#pragma endregion LightmapAccelerationStructure

#pragma region LightmapperWorkerThread

class LightmapperWorkerThread : public TaskThread
{
public:
    LightmapperWorkerThread(ThreadId id)
        : TaskThread(id)
    {
    }

    virtual ~LightmapperWorkerThread() override = default;
};

#pragma endregion LightmapperWorkerThread

#pragma region LightmapThreadPool

class LightmapThreadPool : public TaskThreadPool
{
public:
    LightmapThreadPool()
        : TaskThreadPool(TypeWrapper<LightmapperWorkerThread>(), "LightmapperWorker", NumThreadsToCreate())
    {
        HYP_LOG(Lightmap, Info, "Tracing lightmap rays using {} threads", m_threads.Size());
    }

    virtual ~LightmapThreadPool() override = default;

private:
    static uint32 NumThreadsToCreate()
    {
        uint32 numThreads = g_engine->GetAppContext()->GetConfiguration().Get("lightmapper.num_threads_per_job").ToUInt32(4);
        return MathUtil::Clamp(numThreads, 1u, 128u);
    }
};

#pragma endregion LightmapThreadPool

#pragma region LightmapGPUPathTracer

class HYP_API LightmapGPUPathTracer : public ILightmapRenderer
{
public:
    LightmapGPUPathTracer(const Handle<Scene>& scene, LightmapShadingType shadingType);
    LightmapGPUPathTracer(const LightmapGPUPathTracer& other) = delete;
    LightmapGPUPathTracer& operator=(const LightmapGPUPathTracer& other) = delete;
    LightmapGPUPathTracer(LightmapGPUPathTracer&& other) noexcept = delete;
    LightmapGPUPathTracer& operator=(LightmapGPUPathTracer&& other) noexcept = delete;
    virtual ~LightmapGPUPathTracer() override;

    HYP_FORCE_INLINE const RaytracingPipelineRef& GetPipeline() const
    {
        return m_raytracingPipeline;
    }

    virtual uint32 MaxRaysPerFrame() const override
    {
        return uint32(-1);
    }

    virtual LightmapShadingType GetShadingType() const override
    {
        return m_shadingType;
    }

    virtual void Create() override;
    virtual void UpdateRays(Span<const LightmapRay> rays) override;
    virtual void ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits) override;
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset) override;

private:
    void CreateUniformBuffer();
    void UpdateUniforms(FrameBase* frame, uint32 rayOffset);

    Handle<Scene> m_scene;
    LightmapShadingType m_shadingType;

    FixedArray<GpuBufferRef, maxFramesInFlight> m_uniformBuffers;
    FixedArray<GpuBufferRef, maxFramesInFlight> m_raysBuffers;

    GpuBufferRef m_hitsBufferGpu;

    RaytracingPipelineRef m_raytracingPipeline;
};

LightmapGPUPathTracer::LightmapGPUPathTracer(const Handle<Scene>& scene, LightmapShadingType shadingType)
    : m_scene(scene),
      m_shadingType(shadingType),
      m_uniformBuffers({ g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms)),
          g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms)) }),
      m_raysBuffers({ g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(Vec4f) * 2 * (512 * 512)),
          g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(Vec4f) * 2 * (512 * 512)) }),
      m_hitsBufferGpu(g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(LightmapHit) * (512 * 512)))
{
}

LightmapGPUPathTracer::~LightmapGPUPathTracer()
{
    SafeRelease(std::move(m_uniformBuffers));
    SafeRelease(std::move(m_raysBuffers));
    SafeRelease(std::move(m_hitsBufferGpu));
    SafeRelease(std::move(m_raytracingPipeline));
}

void LightmapGPUPathTracer::CreateUniformBuffer()
{
    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        m_uniformBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms));

        PUSH_RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer, m_uniformBuffers[frameIndex]);
    }
}

void LightmapGPUPathTracer::Create()
{
    Assert(m_scene.IsValid());

    Assert(m_scene->GetWorld() != nullptr);
    Assert(m_scene->GetWorld()->IsReady());

    CreateUniformBuffer();

    DeferCreate(m_hitsBufferGpu);

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        DeferCreate(m_raysBuffers[frameIndex]);
    }

    ShaderProperties shaderProperties;

    switch (m_shadingType)
    {
    case LightmapShadingType::RADIANCE:
        shaderProperties.Set("MODE_RADIANCE");
        break;
    case LightmapShadingType::IRRADIANCE:
        shaderProperties.Set("MODE_IRRADIANCE");
        break;
    default:
        HYP_UNREACHABLE();
    }

    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("LightmapGPUPathTracer"), shaderProperties);
    Assert(shader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        const TLASRef& tlas = m_scene->GetWorld()->GetRenderResource().GetEnvironment()->GetTopLevelAccelerationStructures()[frameIndex];
        Assert(tlas != nullptr);

        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("TLAS"), tlas);
        descriptorSet->SetElement(NAME("MeshDescriptionsBuffer"), tlas->GetMeshDescriptionsBuffer());
        descriptorSet->SetElement(NAME("HitsBuffer"), m_hitsBufferGpu);
        descriptorSet->SetElement(NAME("RaysBuffer"), m_raysBuffers[frameIndex]);

        descriptorSet->SetElement(NAME("LightsBuffer"), g_renderGlobalState->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
        descriptorSet->SetElement(NAME("MaterialsBuffer"), g_renderGlobalState->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));

        descriptorSet->SetElement(NAME("RTRadianceUniforms"), m_uniformBuffers[frameIndex]);
    }

    DeferCreate(descriptorTable);

    m_raytracingPipeline = g_renderBackend->MakeRaytracingPipeline(
        shader,
        descriptorTable);

    DeferCreate(m_raytracingPipeline);
}

void LightmapGPUPathTracer::UpdateUniforms(FrameBase* frame, uint32 rayOffset)
{
    RTRadianceUniforms uniforms {};
    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    uniforms.rayOffset = rayOffset;

    // const uint32 maxBoundLights = MathUtil::Min(g_engine->GetRenderState()->NumBoundLights(), ArraySize(uniforms.lightIndices));
    // uint32 numBoundLights = 0;

    // for (uint32 lightType = 0; lightType < uint32(LT_MAX); lightType++) {
    //     if (numBoundLights >= maxBoundLights) {
    //         break;
    //     }

    //     for (const auto &it : g_engine->GetRenderState()->boundLights[lightType]) {
    //         if (numBoundLights >= maxBoundLights) {
    //             break;
    //         }

    //         uniforms.lightIndices[numBoundLights++] = it->GetBufferIndex();
    //     }
    // }

    // uniforms.numBoundLights = numBoundLights;

    // FIXME: Lights are now stored per-view.
    // We don't have a View for Lightmapper since it is for the entire RenderWorld it is indirectly attached to.
    // We'll need to find a way to get the lights for the current view.
    // Ideas:
    // a) create a View for the Lightmapper and use that to get the lights. It will need to collect the lights on the Game thread so we'll need to add some kind of System to do that.
    // b) add a function to the RenderScene to get all the lights in the scene and use that to get the lights for the current view. This has a drawback that we will always have some RenderLight active when it could be inactive if it is not in any view.
    // OR: We can just use the lights in the current view and ignore the rest. This is a bit of a hack but it will work for now.
    HYP_NOT_IMPLEMENTED();

    uniforms.numBoundLights = 0;

    m_uniformBuffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);
}

void LightmapGPUPathTracer::UpdateRays(Span<const LightmapRay> rays)
{
}

void LightmapGPUPathTracer::ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits)
{
    // @TODO Some kind of function like WaitForFrameToComplete to ensure that the hits buffer is not being written to in the current frame.

    const GpuBufferRef& hitsBuffer = m_hitsBufferGpu;

    GpuBufferRef stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, outHits.Size() * sizeof(LightmapHit));
    HYPERION_ASSERT_RESULT(stagingBuffer->Create());
    stagingBuffer->Memset(outHits.Size() * sizeof(LightmapHit), 0);

    SingleTimeCommands commands;

    commands.Push([&](CmdList& cmd)
        {
            const ResourceState previousResourceState = hitsBuffer->GetResourceState();

            // put src image in state for copying from
            cmd.Add<InsertBarrier>(hitsBuffer, RS_COPY_SRC);

            // put dst buffer in state for copying to
            cmd.Add<InsertBarrier>(stagingBuffer, RS_COPY_DST);

            cmd.Add<CopyBuffer>(stagingBuffer, hitsBuffer, outHits.Size() * sizeof(LightmapHit));

            cmd.Add<InsertBarrier>(stagingBuffer, RS_COPY_SRC);

            cmd.Add<InsertBarrier>(hitsBuffer, previousResourceState);
        });

    HYPERION_ASSERT_RESULT(commands.Execute());

    stagingBuffer->Read(sizeof(LightmapHit) * outHits.Size(), outHits.Data());

    HYPERION_ASSERT_RESULT(stagingBuffer->Destroy());
}

void LightmapGPUPathTracer::Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());

    const uint32 frameIndex = frame->GetFrameIndex();
    const uint32 previousFrameIndex = (frame->GetFrameIndex() + maxFramesInFlight - 1) % maxFramesInFlight;

    UpdateUniforms(frame, rayOffset);

    { // rays buffer
        Array<Vec4f> rayData;
        rayData.Resize(rays.Size() * 2);

        for (SizeType i = 0; i < rays.Size(); i++)
        {
            rayData[i * 2] = Vec4f(rays[i].ray.position, 1.0f);
            rayData[i * 2 + 1] = Vec4f(rays[i].ray.direction, 0.0f);
        }

        bool raysBufferResized = false;

        HYPERION_ASSERT_RESULT(m_raysBuffers[frame->GetFrameIndex()]->EnsureCapacity(rayData.ByteSize(), &raysBufferResized));
        m_raysBuffers[frame->GetFrameIndex()]->Copy(rayData.ByteSize(), rayData.Data());

        if (raysBufferResized)
        {
            m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex())->SetElement(NAME("RaysBuffer"), m_raysBuffers[frame->GetFrameIndex()]);
        }

        bool hitsBufferResized = false;

        /*HYPERION_ASSERT_RESULT(m_hitsBuffers[frame->GetFrameIndex()]->EnsureCapacity(rays.Size() * sizeof(LightmapHit), &hitsBufferResized));
        m_hitsBuffers[frame->GetFrameIndex()]->Memset(rays.Size() * sizeof(LightmapHit), 0);

        if (hitsBufferResized) {
            m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex())
                ->SetElement(NAME("HitsBuffer"), m_hitsBuffers[frame->GetFrameIndex()]);
        }*/

        if (raysBufferResized || hitsBufferResized)
        {
            m_raytracingPipeline->GetDescriptorTable()->Update(frame->GetFrameIndex());
        }
    }

    frame->GetCommandList().Add<BindRaytracingPipeline>(m_raytracingPipeline);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_raytracingPipeline->GetDescriptorTable(),
        m_raytracingPipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*renderSetup.world) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<InsertBarrier>(m_hitsBufferGpu, RS_UNORDERED_ACCESS);

    frame->GetCommandList().Add<TraceRays>(
        m_raytracingPipeline,
        Vec3u { uint32(rays.Size()), 1, 1 });

    frame->GetCommandList().Add<InsertBarrier>(m_hitsBufferGpu, RS_UNORDERED_ACCESS);
}

#pragma endregion LightmapGPUPathTracer

#pragma region LightmapCPUPathTracer

class HYP_API LightmapCPUPathTracer : public ILightmapRenderer
{
public:
    LightmapCPUPathTracer(const Handle<Scene>& scene, LightmapShadingType shadingType);
    LightmapCPUPathTracer(const LightmapCPUPathTracer& other) = delete;
    LightmapCPUPathTracer& operator=(const LightmapCPUPathTracer& other) = delete;
    LightmapCPUPathTracer(LightmapCPUPathTracer&& other) noexcept = delete;
    LightmapCPUPathTracer& operator=(LightmapCPUPathTracer&& other) noexcept = delete;
    virtual ~LightmapCPUPathTracer() override;

    virtual uint32 MaxRaysPerFrame() const override
    {
        return uint32(-1);
    }

    virtual LightmapShadingType GetShadingType() const override
    {
        return m_shadingType;
    }

    virtual void Create() override;
    virtual void UpdateRays(Span<const LightmapRay> rays) override;
    virtual void ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits) override;
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset) override;

private:
    void TraceSingleRayOnCPU(LightmapJob* job, const LightmapRay& ray, LightmapRayHitPayload& outPayload);

    Handle<Scene> m_scene;
    LightmapShadingType m_shadingType;

    Array<LightmapHit, DynamicAllocator> m_hitsBuffer;

    Array<LightmapRay, DynamicAllocator> m_currentRays;

    LightmapThreadPool m_threadPool;

    AtomicVar<uint32> m_numTracingTasks;
};

LightmapCPUPathTracer::LightmapCPUPathTracer(const Handle<Scene>& scene, LightmapShadingType shadingType)
    : m_scene(scene),
      m_shadingType(shadingType),
      m_numTracingTasks(0)
{
}

LightmapCPUPathTracer::~LightmapCPUPathTracer()
{
    if (m_threadPool.IsRunning())
    {
        m_threadPool.Stop();
    }
}

void LightmapCPUPathTracer::Create()
{
    m_threadPool.Start();
}

void LightmapCPUPathTracer::UpdateRays(Span<const LightmapRay> rays)
{
}

void LightmapCPUPathTracer::ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits)
{
    Threads::AssertOnThread(g_renderThread);

    Assert(m_numTracingTasks.Get(MemoryOrder::ACQUIRE) == 0,
        "Cannot read hits buffer while tracing is in progress");

    Assert(outHits.Size() == m_hitsBuffer.Size());

    Memory::MemCpy(outHits.Data(), m_hitsBuffer.Data(), m_hitsBuffer.ByteSize());
}

void LightmapCPUPathTracer::Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset)
{
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());

    Assert(m_numTracingTasks.Get(MemoryOrder::ACQUIRE) == 0,
        "Trace is already in progress");

    Handle<Texture> envProbeTexture;

    if (renderSetup.envProbe)
    {
        // prepare env probe texture to be sampled on the CPU in the tasks
        envProbeTexture = renderSetup.envProbe->GetPrefilteredEnvMap();
        // envProbeTexture->Readback();

        HYP_NOT_IMPLEMENTED();
    }

    m_hitsBuffer.Resize(rays.Size());

    m_currentRays.Resize(rays.Size());
    Memory::MemCpy(m_currentRays.Data(), rays.Data(), m_currentRays.ByteSize());

    m_numTracingTasks.Increment(rays.Size(), MemoryOrder::RELEASE);

    TaskBatch* taskBatch = new TaskBatch();
    taskBatch->pool = &m_threadPool;

    const uint32 numItems = uint32(m_currentRays.Size());
    const uint32 numBatches = m_threadPool.GetProcessorAffinity();
    const uint32 itemsPerBatch = (numItems + numBatches - 1) / numBatches;

    for (uint32 batchIndex = 0; batchIndex < numBatches; batchIndex++)
    {
        taskBatch->AddTask([this, job, batchIndex, itemsPerBatch, numItems, envProbeTexture = envProbeTexture](...)
            {
                const uint32 offsetIndex = batchIndex * itemsPerBatch;
                const uint32 maxIndex = MathUtil::Min(offsetIndex + itemsPerBatch, numItems);

                for (uint32 index = offsetIndex; index < maxIndex; index++)
                {
                    HYP_DEFER({ m_numTracingTasks.Decrement(1, MemoryOrder::RELEASE); });

                    const LightmapRay& firstRay = m_currentRays[index];

                    uint32 seed = (uint32)rand(); // index * m_texelIndex;

                    FixedArray<LightmapRay, maxBouncesCpu + 1> recursiveRays;
                    FixedArray<LightmapRayHitPayload, maxBouncesCpu + 1> bounces;

                    int numBounces = 0;

                    Vec3f direction = firstRay.ray.direction.Normalized();

                    if (m_shadingType == LightmapShadingType::IRRADIANCE)
                    {
                        direction = MathUtil::RandomInHemisphere(
                            Vec3f(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed)),
                            firstRay.ray.direction)
                                        .Normalize();
                    }

                    Vec3f origin = firstRay.ray.position + direction * 0.001f;

                    // Vec4f skyColor;

                    // if (envProbeTexture.IsValid()) {
                    //     Vec4f envProbeSample = envProbeTexture->SampleCube(direction);

                    //     skyColor = envProbeSample;
                    // }

                    for (int bounceIndex = 0; bounceIndex < maxBouncesCpu; bounceIndex++)
                    {
                        LightmapRay bounceRay = firstRay;

                        if (bounceIndex != 0)
                        {
                            bounceRay.meshId = bounces[bounceIndex - 1].meshId;
                            bounceRay.triangleIndex = bounces[bounceIndex - 1].triangleIndex;
                        }

                        bounceRay.ray = Ray {
                            origin,
                            direction
                        };

                        recursiveRays[bounceIndex] = bounceRay;

                        LightmapRayHitPayload& payload = bounces[bounceIndex];
                        payload = {};

                        TraceSingleRayOnCPU(job, bounceRay, payload);

                        if (payload.distance < 0.0f)
                        {
                            payload.throughput = Vec4f(0.0f);

                            Assert(bounceIndex < bounces.Size());

                            // @TODO Sample environment map
                            const Vec3f normal = bounceRay.ray.direction;

                            // if (envProbeTexture.IsValid()) {
                            //     Vec4f envProbeSample = envProbeTexture->SampleCube(direction);

                            //     payload.emissive += envProbeSample;
                            // }

                            // testing!! @FIXME
                            const Vec3f L = Vec3f(-0.4f, 0.65f, 0.1f).Normalize();
                            payload.emissive += Vec4f(1.0f) * MathUtil::Max(0.0f, normal.Dot(L));

                            ++numBounces;

                            break;
                        }

                        Vec3f hitPosition = origin + direction * payload.distance;

                        if (m_shadingType == LightmapShadingType::IRRADIANCE)
                        {
                            const Vec3f rnd = Vec3f(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed));

                            direction = MathUtil::RandomInHemisphere(rnd, payload.normal).Normalize();
                        }
                        else
                        {
                            ++numBounces;
                            break;
                        }

                        origin = hitPosition + direction * 0.001f;

                        ++numBounces;
                    }

                    for (int bounceIndex = int(numBounces - 1); bounceIndex >= 0; bounceIndex--)
                    {
                        Vec4f radiance = bounces[bounceIndex].emissive;

                        if (bounceIndex != numBounces - 1)
                        {
                            radiance += bounces[bounceIndex + 1].radiance * bounces[bounceIndex].throughput;
                        }

                        float p = MathUtil::Max(radiance.x, MathUtil::Max(radiance.y, MathUtil::Max(radiance.z, radiance.w)));

                        if (MathUtil::RandomFloat(seed) > p)
                        {
                            break;
                        }

                        radiance /= MathUtil::Max(p, 0.0001f);

                        bounces[bounceIndex].radiance = radiance;
                    }

                    LightmapHit& hit = m_hitsBuffer[index];

                    if (numBounces != 0)
                    {
                        hit.color = bounces[0].radiance;

                        if (MathUtil::IsNaN(hit.color) || !MathUtil::IsFinite(hit.color))
                        {
                            HYP_LOG_ONCE(Lightmap, Warning, "NaN or infinite color detected while tracing rays");

                            hit.color = Vec4f(0.0f);
                        }

                        hit.color.w = 1.0f;
                    }
                }
            });
    }

    TaskSystem::GetInstance().EnqueueBatch(taskBatch);

    job->AddTask(taskBatch);
}

void LightmapCPUPathTracer::TraceSingleRayOnCPU(LightmapJob* job, const LightmapRay& ray, LightmapRayHitPayload& outPayload)
{
    outPayload.throughput = Vec4f(0.0f);
    outPayload.emissive = Vec4f(0.0f);
    outPayload.radiance = Vec4f(0.0f);
    outPayload.normal = Vec3f(0.0f);
    outPayload.distance = -1.0f;
    outPayload.barycentricCoords = Vec3f(0.0f);
    outPayload.meshId = ObjId<Mesh>::invalid;
    outPayload.triangleIndex = ~0u;

    ILightmapAccelerationStructure* accelerationStructure = job->GetParams().accelerationStructure;

    if (!accelerationStructure)
    {
        HYP_LOG(Lightmap, Warning, "No acceleration structure set while tracing on CPU, cannot perform trace");

        return;
    }

    LightmapRayTestResults results = accelerationStructure->TestRay(ray.ray);

    if (!results.Any())
    {
        return;
    }

    for (const LightmapRayHit& hit : results)
    {
        if (hit.distance + 0.0001f <= 0.0f)
        {
            continue;
        }

        Assert(hit.entity.IsValid());

        auto it = job->GetParams().subElementsByEntity->Find(hit.entity);

        Assert(it != job->GetParams().subElementsByEntity->End());
        Assert(it->second != nullptr);

        const LightmapSubElement& subElement = *it->second;

        const ObjId<Mesh> meshId = subElement.mesh->Id();

        const Vec3f barycentricCoords = hit.barycentricCoords;

        const Triangle& triangle = hit.triangle;

        const Vec2f uv = triangle.GetPoint(0).GetTexCoord0() * barycentricCoords.x
            + triangle.GetPoint(1).GetTexCoord0() * barycentricCoords.y
            + triangle.GetPoint(2).GetTexCoord0() * barycentricCoords.z;

        Vec4f albedo = Vec4f(subElement.material->GetParameter(Material::MATERIAL_KEY_ALBEDO));

        // sample albedo texture, if present
        if (const Handle<Texture>& albedoTexture = subElement.material->GetTexture(MaterialTextureKey::ALBEDO_MAP))
        {
            Vec4f albedoTextureColor = albedoTexture->Sample2D(uv);

            albedo *= albedoTextureColor;
        }

        outPayload.emissive = Vec4f(0.0f);
        outPayload.throughput = albedo;
        outPayload.barycentricCoords = barycentricCoords;
        outPayload.meshId = meshId;
        outPayload.triangleIndex = hit.id;
        outPayload.normal = hit.normal;
        outPayload.distance = hit.distance;

        return;
    }
}

#pragma endregion LightmapCPUPathTracer

#pragma region LightmapJob

static constexpr uint32 g_maxConcurrentRenderingTasksPerJob = 1;

LightmapJob::LightmapJob(LightmapJobParams&& params)
    : m_params(std::move(params)),
      m_elementIndex(~0u),
      m_texelIndex(0),
      m_lastLoggedPercentage(0),
      m_numConcurrentRenderingTasks(0)
{

    Handle<Camera> camera = CreateObject<Camera>();
    camera->SetName(NAME_FMT("LightmapJob_{}_Camera", m_uuid));
    camera->AddCameraController(CreateObject<OrthoCameraController>());
    InitObject(camera);

    // dummy output target
    ViewOutputTargetDesc outputTargetDesc {
        .extent = Vec2u::One(),
        .attachments = { { TF_RGBA8 } }
    };

    m_view = CreateObject<View>(ViewDesc {
        .flags = ViewFlags::COLLECT_STATIC_ENTITIES
            | ViewFlags::SKIP_FRUSTUM_CULLING
            | ViewFlags::SKIP_ENV_GRIDS
            | ViewFlags::SKIP_LIGHTMAP_VOLUMES,
        .viewport = Viewport { .extent = Vec2u::One(), .position = Vec2i::Zero() },
        .outputTargetDesc = outputTargetDesc,
        .scenes = { m_params.scene },
        .camera = camera });

    InitObject(m_view);
}

LightmapJob::~LightmapJob()
{
    for (TaskBatch* taskBatch : m_currentTasks)
    {
        taskBatch->AwaitCompletion();

        delete taskBatch;
    }

    m_resourceCache.Clear();
}

void LightmapJob::Start()
{
    m_runningSemaphore.Produce(1, [this](bool)
        {
            if (!m_uvMap.HasValue())
            {
                // No elements to process
                if (!m_params.subElementsView)
                {
                    m_uvMap = LightmapUVMap {};

                    return;
                }

                if (m_params.config->traceMode == LightmapTraceMode::CPU_PATH_TRACING)
                {
                    HYP_LOG(Lightmap, Info, "Lightmap job {}: Preloading sub-element cached resources", m_uuid);

                    for (const LightmapSubElement& subElement : m_params.subElementsView)
                    {
                        if (subElement.mesh.IsValid() && subElement.mesh->GetStreamedMeshData())
                        {
                            m_resourceCache.EmplaceBack(*subElement.mesh->GetStreamedMeshData());
                        }

                        if (subElement.material.IsValid())
                        {
                            for (const auto& it : subElement.material->GetTextures())
                            {
                                if (it.second.IsValid() && it.second->GetStreamedTextureData())
                                {
                                    m_resourceCache.EmplaceBack(*it.second->GetStreamedTextureData());
                                }
                            }
                        }
                    }
                }

                HYP_LOG(Lightmap, Info, "Lightmap job {}: Enqueue task to build UV map", m_uuid);

                m_uvBuilder = LightmapUVBuilder { { m_params.subElementsView } };

                m_buildUvMapTask = TaskSystem::GetInstance().Enqueue([this]() -> TResult<LightmapUVMap>
                    {
                        return m_uvBuilder.Build();
                    },
                    TaskThreadPoolName::THREAD_POOL_BACKGROUND);
            }
        });
}

void LightmapJob::Stop()
{
    m_runningSemaphore.Release(1);
}

void LightmapJob::Stop(const Error& error)
{
    HYP_LOG(Lightmap, Error, "Lightmap job {} stopped with error: {}", m_uuid, error.GetMessage());

    m_result = error;

    Stop();
}

bool LightmapJob::IsCompleted() const
{
    return !m_runningSemaphore.IsInSignalState();
}

void LightmapJob::AddTask(TaskBatch* taskBatch)
{
    Mutex::Guard guard(m_currentTasksMutex);

    m_currentTasks.PushBack(taskBatch);
}

void LightmapJob::Process()
{
    Assert(IsRunning());
    Assert(!m_result.HasError(), "Unhandled error in lightmap job: %s", *m_result.GetError().GetMessage());

    if (m_numConcurrentRenderingTasks.Get(MemoryOrder::ACQUIRE) >= g_maxConcurrentRenderingTasksPerJob)
    {
        // Wait for current rendering tasks to complete before enqueueing new ones.

        return;
    }

    if (!m_uvMap.HasValue())
    {
        // wait for uv map to finish building

        // If uv map is not valid, it must have a task that is building it
        Assert(m_buildUvMapTask.IsValid());

        if (!m_buildUvMapTask.IsCompleted())
        {
            // return early so we don't block - we need to wait for build task to complete before processing
            return;
        }

        if (TResult<LightmapUVMap>& uvMapResult = m_buildUvMapTask.Await(); uvMapResult.HasValue())
        {
            m_uvMap = std::move(*uvMapResult);
        }
        else
        {
            Stop(uvMapResult.GetError());

            return;
        }

        if (m_uvMap.HasValue())
        {
            LightmapElement element;

            if (!m_params.volume->AddElement(*m_uvMap, element, /* shrinkToFit */ true, /* downscaleLimit */ 0.1f))
            {
                Stop(HYP_MAKE_ERROR(Error, "Failed to add LightmapElement to LightmapVolume for lightmap job {}! Dimensions: {}, UV map size: {}",
                    m_uuid, m_params.volume->GetAtlas().atlasDimensions, Vec2u(m_uvMap->width, m_uvMap->height)));

                return;
            }

            m_elementIndex = element.index;
            Assert(m_elementIndex != ~0u);

            // Flatten texel indices, grouped by mesh IDs to prevent unnecessary loading/unloading
            m_texelIndices.Reserve(m_uvMap->uvs.Size());

            for (const auto& it : m_uvMap->meshToUvIndices)
            {
                m_texelIndices.Concat(it.second);
            }

            // Free up memory
            m_uvMap->meshToUvIndices.Clear();
        }
        else
        {
            // Mark as ready to stop further processing
            Stop(HYP_MAKE_ERROR(Error, "Failed to build UV map for lightmap job {}", m_uuid));
        }

        return;
    }

    if (m_view.IsValid())
    {
        m_view->UpdateVisibility();
        m_view->Update(0.0f);
    }

    {
        Mutex::Guard guard(m_currentTasksMutex);

        if (m_currentTasks.Any())
        {
            for (TaskBatch* taskBatch : m_currentTasks)
            {
                if (!taskBatch->IsCompleted())
                {
                    // Skip this call

                    return;
                }
            }

            for (TaskBatch* taskBatch : m_currentTasks)
            {
                taskBatch->AwaitCompletion();

                delete taskBatch;
            }

            m_currentTasks.Clear();
        }
    }

    bool hasRemainingRays = false;

    {
        Mutex::Guard guard(m_previousFrameRaysMutex);

        if (m_previousFrameRays.Any())
        {
            hasRemainingRays = true;
        }
    }

    if (!hasRemainingRays
        && m_texelIndex >= m_texelIndices.Size() * m_params.config->numSamples
        && m_numConcurrentRenderingTasks.Get(MemoryOrder::ACQUIRE) == 0)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap job {}: All texels processed ({} / {}), stopping", m_uuid, m_texelIndex, m_texelIndices.Size() * m_params.config->numSamples);

        Stop();

        return;
    }

    const SizeType maxRays = MathUtil::Min(m_params.renderers[0]->MaxRaysPerFrame(), m_params.config->maxRaysPerFrame);

    Array<LightmapRay> rays;
    rays.Reserve(maxRays);

    GatherRays(maxRays, rays);

    const uint32 rayOffset = uint32(m_texelIndex % (m_texelIndices.Size() * m_params.config->numSamples));

    for (ILightmapRenderer* lightmapRenderer : m_params.renderers)
    {
        Assert(lightmapRenderer != nullptr);

        lightmapRenderer->UpdateRays(rays);
    }

    const double percentage = double(m_texelIndex) / double(m_texelIndices.Size() * m_params.config->numSamples) * 100.0;

    if (MathUtil::Abs(MathUtil::Floor(percentage) - MathUtil::Floor(m_lastLoggedPercentage)) >= 1)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap job {}: Texel {} / {} ({}%)",
            m_uuid.ToString(), m_texelIndex, m_texelIndices.Size() * m_params.config->numSamples, percentage);

        m_lastLoggedPercentage = percentage;
    }

    PUSH_RENDER_COMMAND(LightmapRender, this, &m_view->GetRenderResource(), std::move(rays), rayOffset);
}

void LightmapJob::GatherRays(uint32 maxRayHits, Array<LightmapRay>& outRays)
{
    for (uint32 rayIndex = 0; rayIndex < maxRayHits && HasRemainingTexels(); ++rayIndex)
    {
        const uint32 texelIndex = NextTexel();

        LightmapRay ray = m_uvMap->uvs[texelIndex].ray;
        ray.texelIndex = texelIndex;

        outRays.PushBack(ray);
    }
}

void LightmapJob::IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shadingType)
{
    Assert(rays.Size() == hits.Size());

    LightmapUVMap& uvMap = GetUVMap();

    for (SizeType i = 0; i < hits.Size(); i++)
    {
        const LightmapRay& ray = rays[i];
        const LightmapHit& hit = hits[i];

        LightmapUV& uv = uvMap.uvs[ray.texelIndex];

        switch (shadingType)
        {
        case LightmapShadingType::RADIANCE:
            uv.radiance += Vec4f(hit.color.GetXYZ(), 1.0f); //= Vec4f(MathUtil::Lerp(uv.radiance.GetXYZ() * uv.radiance.w, hit.color.GetXYZ(), hit.color.w), 1.0f);
            break;
        case LightmapShadingType::IRRADIANCE:
            uv.irradiance += Vec4f(hit.color.GetXYZ(), 1.0f); //= Vec4f(MathUtil::Lerp(uv.irradiance.GetXYZ() * uv.irradiance.w, hit.color.GetXYZ(), hit.color.w), 1.0f); //
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
}

#pragma endregion LightmapJob

#pragma region Lightmapper

Lightmapper::Lightmapper(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb)
    : m_config(std::move(config)),
      m_scene(scene),
      m_aabb(aabb),
      m_numJobs { 0 }
{
    HYP_LOG(Lightmap, Info, "Initializing lightmapper: {}", m_config.ToString());

    for (uint32 i = 0; i < uint32(LightmapShadingType::MAX); i++)
    {
        switch (LightmapShadingType(i))
        {
        case LightmapShadingType::RADIANCE:
            if (!m_config.radiance)
            {
                continue;
            }

            break;
        case LightmapShadingType::IRRADIANCE:
            if (!m_config.irradiance)
            {
                continue;
            }

            break;
        default:
            HYP_UNREACHABLE();
        }

        UniquePtr<ILightmapRenderer>& lightmapRenderer = m_lightmapRenderers.EmplaceBack();

        switch (m_config.traceMode)
        {
        case LightmapTraceMode::GPU_PATH_TRACING:
            lightmapRenderer.EmplaceAs<LightmapGPUPathTracer>(m_scene, LightmapShadingType(i));
            break;
        case LightmapTraceMode::CPU_PATH_TRACING:
            lightmapRenderer.EmplaceAs<LightmapCPUPathTracer>(m_scene, LightmapShadingType(i));
            break;
        default:
            HYP_UNREACHABLE();
        }

        lightmapRenderer->Create();
    }

    Assert(m_lightmapRenderers.Any());

    m_volume = CreateObject<LightmapVolume>(m_aabb);
    InitObject(m_volume);

    Handle<Entity> lightmapVolumeEntity = m_scene->GetEntityManager()->AddEntity();
    m_scene->GetEntityManager()->AddComponent<LightmapVolumeComponent>(lightmapVolumeEntity, LightmapVolumeComponent { m_volume });
    m_scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(lightmapVolumeEntity, BoundingBoxComponent { m_aabb, m_aabb });

    Handle<Node> lightmapVolumeNode = m_scene->GetRoot()->AddChild();
    lightmapVolumeNode->SetName(Name::Unique("LightmapVolume"));
    lightmapVolumeNode->SetEntity(lightmapVolumeEntity);
}

Lightmapper::~Lightmapper()
{
    m_lightmapRenderers = {};

    m_queue.Clear();

    m_accelerationStructure.Reset();
}

bool Lightmapper::IsComplete() const
{
    return m_numJobs.Get(MemoryOrder::ACQUIRE) == 0;
}

LightmapJobParams Lightmapper::CreateLightmapJobParams(
    SizeType startIndex,
    SizeType endIndex,
    LightmapTopLevelAccelerationStructure* accelerationStructure)
{
    LightmapJobParams jobParams {
        &m_config,
        m_scene,
        m_volume,
        m_subElements.ToSpan().Slice(startIndex, endIndex - startIndex),
        &m_subElementsByEntity,
        accelerationStructure
    };

    jobParams.renderers.Resize(m_lightmapRenderers.Size());

    for (SizeType i = 0; i < m_lightmapRenderers.Size(); i++)
    {
        jobParams.renderers[i] = m_lightmapRenderers[i].Get();
    }

    return jobParams;
}

void Lightmapper::PerformLightmapping()
{
    HYP_SCOPE;
    const uint32 idealTrianglesPerJob = m_config.idealTrianglesPerJob;

    Assert(m_numJobs.Get(MemoryOrder::ACQUIRE) == 0, "Cannot initialize lightmap renderer -- jobs currently running!");

    // Build jobs
    HYP_LOG(Lightmap, Info, "Building graph for lightmapper");

    EntityManager& mgr = *m_scene->GetEntityManager();

    m_subElements.Clear();
    m_subElementsByEntity.Clear();

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (!meshComponent.mesh.IsValid())
        {
            HYP_LOG(Lightmap, Info, "Skip entity with invalid mesh on MeshComponent");

            continue;
        }

        if (!meshComponent.material.IsValid())
        {
            HYP_LOG(Lightmap, Info, "Skip entity with invalid material on MeshComponent");

            continue;
        }

        // Only process opaque and translucent materials
        if (meshComponent.material->GetBucket() != RB_OPAQUE && meshComponent.material->GetBucket() != RB_TRANSLUCENT)
        {
            HYP_LOG(Lightmap, Info, "Skip entity with bucket that is not opaque or translucent");

            continue;
        }

        if (m_config.traceMode == LightmapTraceMode::GPU_PATH_TRACING)
        {
            if (!meshComponent.raytracingData)
            {
                HYP_LOG(Lightmap, Info, "Skipping Entity {} because it has no raytracing data set", entity->Id());

                continue;
            }
        }

        m_subElements.PushBack(LightmapSubElement {
            entity->HandleFromThis(),
            meshComponent.mesh,
            meshComponent.material,
            transformComponent.transform,
            boundingBoxComponent.worldAabb });
    }

    Assert(m_accelerationStructure == nullptr);
    m_accelerationStructure = MakeUnique<LightmapTopLevelAccelerationStructure>();

    if (m_subElements.Empty())
    {
        return;
    }

    for (LightmapSubElement& subElement : m_subElements)
    {
        if (!subElement.mesh->BuildBVH())
        {
            HYP_LOG(Lightmap, Error, "Failed to build BVH for mesh on entity {} in lightmapper", subElement.entity.Id());

            continue;
        }

        m_accelerationStructure->Add(&subElement, &subElement.mesh->GetBVH());
    }

    uint32 numTriangles = 0;
    SizeType startIndex = 0;

    for (SizeType index = 0; index < m_subElements.Size(); index++)
    {
        LightmapSubElement& subElement = m_subElements[index];

        m_subElementsByEntity.Set(subElement.entity, &subElement);

        if (idealTrianglesPerJob != 0 && numTriangles != 0 && numTriangles + subElement.mesh->NumIndices() / 3 > idealTrianglesPerJob)
        {
            UniquePtr<LightmapJob> job = MakeUnique<LightmapJob>(CreateLightmapJobParams(startIndex, index + 1, m_accelerationStructure.Get()));

            startIndex = index + 1;

            AddJob(std::move(job));

            numTriangles = 0;
        }

        numTriangles += subElement.mesh->NumIndices() / 3;
    }

    if (startIndex < m_subElements.Size() - 1)
    {
        UniquePtr<LightmapJob> job = MakeUnique<LightmapJob>(CreateLightmapJobParams(startIndex, m_subElements.Size(), m_accelerationStructure.Get()));

        AddJob(std::move(job));
    }
}

void Lightmapper::Update(float delta)
{
    HYP_SCOPE;
    uint32 numJobs = m_numJobs.Get(MemoryOrder::ACQUIRE);

    Mutex::Guard guard(m_queueMutex);

    Assert(!m_queue.Empty());
    LightmapJob* job = m_queue.Front().Get();

    // Start job if not started
    if (!job->IsRunning())
    {
        job->Start();
    }

    job->Process();

    if (job->IsCompleted())
    {
        HandleCompletedJob(job);
    }
}

void Lightmapper::HandleCompletedJob(LightmapJob* job)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    m_scene->GetWorld()->RemoveView(job->GetView());

    if (job->GetResult().HasError())
    {
        HYP_LOG(Lightmap, Error, "Lightmap job {} failed with error: {}", job->GetUUID(), job->GetResult().GetError().GetMessage());

        m_queue.Pop();
        m_numJobs.Decrement(1, MemoryOrder::RELEASE);

        return;
    }

    HYP_LOG(Lightmap, Debug, "Tracing completed for lightmapping job {} ({} subelements)", job->GetUUID(), job->GetSubElements().Size());

    const LightmapUVMap& uvMap = job->GetUVMap();
    const uint32 elementIndex = job->GetElementIndex();

    if (!m_volume->BuildElementTextures(uvMap, elementIndex))
    {
        HYP_LOG(Lightmap, Error, "Failed to build LightmapElement textures for LightmapVolume, element index: {}", job->GetElementIndex());

        return;
    }

    const LightmapElement* element = m_volume->GetElement(elementIndex);
    Assert(element != nullptr);

    HYP_LOG(Lightmap, Debug, "Lightmap job {}: Building element with index {}, UV offset: {}, Scale: {}", job->GetUUID(), elementIndex,
        element->offsetUv, element->scale);

    for (SizeType subElementIndex = 0; subElementIndex < job->GetSubElements().Size(); subElementIndex++)
    {
        LightmapSubElement& subElement = job->GetSubElements()[subElementIndex];

        auto updateMeshData = [&]()
        {
            const Handle<Mesh>& mesh = subElement.mesh;
            Assert(mesh.IsValid());

            Assert(subElementIndex < job->GetUVBuilder().GetMeshData().Size());

            const LightmapMeshData& lightmapMeshData = job->GetUVBuilder().GetMeshData()[subElementIndex];
            Assert(lightmapMeshData.mesh == mesh);

            MeshData newMeshData;
            newMeshData.vertices = lightmapMeshData.vertices;
            newMeshData.indices = lightmapMeshData.indices;

            for (uint32 i = 0; i < newMeshData.vertices.Size(); i++)
            {
                Vec2f& lightmapUv = newMeshData.vertices[i].texcoord1;
                lightmapUv.y = 1.0f - lightmapUv.y; // Invert Y coordinate for lightmaps
                lightmapUv *= element->scale;
                lightmapUv += Vec2f(element->offsetUv.x, element->offsetUv.y);
            }

            ResourceHandle resourceHandle;
            mesh->SetStreamedMeshData(MakeRefCountedPtr<StreamedMeshData>(std::move(newMeshData), resourceHandle));
        };

        updateMeshData();

        bool isNewMaterial = false;

        subElement.material = subElement.material.IsValid() ? subElement.material->Clone() : CreateObject<Material>();
        isNewMaterial = true;

        subElement.material->SetBucket(RB_LIGHTMAP);

#if 0
        // temp ; testing. - will instead be set in the lightmap volume
        Bitmap<4, float> radianceBitmap = uvMap.ToBitmapRadiance();
        Bitmap<4, float> irradianceBitmap = uvMap.ToBitmapIrradiance();

        Handle<Texture> irradianceTexture = CreateObject<Texture>(TextureData {
            TextureDesc {
                TT_TEX2D,
                TF_RGBA32F,
                Vec3u { uvMap.width, uvMap.height, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_REPEAT },
            ByteBuffer(irradianceBitmap.GetUnpackedFloats().ToByteView()) });
        InitObject(irradianceTexture);

        Handle<Texture> radianceTexture = CreateObject<Texture>(TextureData {
            TextureDesc {
                TT_TEX2D,
                TF_RGBA32F,
                Vec3u { uvMap.width, uvMap.height, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_REPEAT },
            ByteBuffer(radianceBitmap.GetUnpackedFloats().ToByteView()) });
        InitObject(radianceTexture);

        subElement.material->SetTexture(MaterialTextureKey::IRRADIANCE_MAP, irradianceTexture);
        subElement.material->SetTexture(MaterialTextureKey::RADIANCE_MAP, radianceTexture);
#else
        // @TEMP
        subElement.material->SetTexture(MaterialTextureKey::IRRADIANCE_MAP, m_volume->GetAtlasTextures().At(LTT_IRRADIANCE));
        subElement.material->SetTexture(MaterialTextureKey::RADIANCE_MAP, m_volume->GetAtlasTextures().At(LTT_RADIANCE));
#endif

        auto updateMeshComponent = [entityManagerWeak = m_scene->GetEntityManager()->WeakHandleFromThis(), elementIndex = job->GetElementIndex(), volume = m_volume, subElement = subElement, newMaterial = (isNewMaterial ? subElement.material : Handle<Material>::empty)]()
        {
            Handle<EntityManager> entityManager = entityManagerWeak.Lock();

            if (!entityManager)
            {
                HYP_LOG(Lightmap, Error, "Failed to lock EntityManager while updating lightmap element");

                return;
            }

            const Handle<Entity>& entity = subElement.entity;

            if (entityManager->HasComponent<MeshComponent>(entity))
            {
                MeshComponent& meshComponent = entityManager->GetComponent<MeshComponent>(entity);

                if (newMaterial.IsValid())
                {
                    InitObject(newMaterial);

                    meshComponent.material = std::move(newMaterial);
                }

                meshComponent.lightmapVolume = volume.ToWeak();
                meshComponent.lightmapElementIndex = elementIndex;
                meshComponent.lightmapVolumeUuid = volume->GetUUID();
            }
            else
            {
                Assert(newMaterial.IsValid());
                InitObject(newMaterial);

                MeshComponent meshComponent {};
                meshComponent.mesh = subElement.mesh;
                meshComponent.material = newMaterial;
                meshComponent.lightmapVolume = volume.ToWeak();
                meshComponent.lightmapElementIndex = elementIndex;
                meshComponent.lightmapVolumeUuid = volume->GetUUID();

                entityManager->AddComponent<MeshComponent>(entity, std::move(meshComponent));
            }

            entityManager->AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
        };

        if (Threads::IsOnThread(m_scene->GetEntityManager()->GetOwnerThreadId()))
        {
            // If we are on the same thread, we can update the mesh component immediately
            updateMeshComponent();
        }
        else
        {
            // Enqueue the update to be performed on the owner thread
            ThreadBase* thread = Threads::GetThread(m_scene->GetEntityManager()->GetOwnerThreadId());
            Assert(thread != nullptr);

            thread->GetScheduler().Enqueue(std::move(updateMeshComponent), TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }

    m_queue.Pop();
    m_numJobs.Decrement(1, MemoryOrder::RELEASE);
}

#pragma endregion Lightmapper

} // namespace hyperion