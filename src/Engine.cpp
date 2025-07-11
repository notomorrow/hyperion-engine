/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/GPUBufferHolderMap.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/AsyncCompute.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <asset/Assets.hpp>

#include <streaming/StreamingManager.hpp>

#include <core/profiling/ProfileScope.hpp>
#include <core/filesystem/FsUtil.hpp>

#include <util/MeshBuilder.hpp>

#include <scene/World.hpp>
#include <scene/Texture.hpp>

#include <core/debug/StackDump.hpp>
#include <system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/net/NetRequestThread.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/object/HypClassUtils.hpp>

#include <system/AppContext.hpp>
#include <system/App.hpp>

#include <streaming/StreamingThread.hpp>

#include <scripting/ScriptingService.hpp>

#include <util/BlueNoise.hpp>

#include <HyperionEngine.hpp>

#define HYP_LOG_FRAMES_PER_SECOND
#define HYP_ENABLE_RENDER_STATS

namespace hyperion {

using renderer::FillMode;
using renderer::GPUBufferType;

Handle<Engine> g_engine = {};
Handle<AssetManager> g_asset_manager {};
// Handle<StreamingManager> g_streaming_manager {};
ShaderManager* g_shader_manager = nullptr;
MaterialCache* g_material_system = nullptr;
SafeDeleter* g_safe_deleter = nullptr;
IRenderingAPI* g_rendering_api = nullptr;

namespace renderer {
static struct GlobalDescriptorSetsDeclarations
{
    GlobalDescriptorSetsDeclarations()
    {
#include <rendering/inl/DescriptorSets.inl>
    }
} g_global_descriptor_sets_declarations;
} // namespace renderer

class RenderThread final : public Thread<Scheduler>
{
public:
    RenderThread(const Handle<AppContextBase>& app_context)
        : Thread(g_render_thread, ThreadPriorityValue::HIGHEST),
          m_app_context(app_context),
          m_is_running(false)
    {
    }

    // Overrides Start() to not create a thread object. Runs the render loop on the main thread.
    bool Start()
    {
        AssertThrow(m_is_running.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

        // Must be current thread
        Threads::AssertOnThread(g_render_thread);

        SetCurrentThreadObject(this);
        m_scheduler.SetOwnerThread(GetID());

        (*this)();

        return true;
    }

    void Stop()
    {
        m_is_running.Set(false, MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_is_running.Get(MemoryOrder::ACQUIRE);
    }

private:
    virtual void operator()() override
    {
        AssertThrow(m_app_context != nullptr);

        SystemEvent event;

        Queue<Scheduler::ScheduledTask> tasks;

        while (m_is_running.Get(MemoryOrder::RELAXED))
        {
            // input manager stuff
            while (m_app_context->PollEvent(event))
            {
                m_app_context->GetMainWindow()->GetInputEventSink().Push(std::move(event));
            }

            if (uint32 num_enqueued = m_scheduler.NumEnqueued())
            {
                m_scheduler.AcceptAll(tasks);

                while (tasks.Any())
                {
                    tasks.Pop().Execute();
                }
            }

            g_engine->RenderNextFrame();
        }
    }

    Handle<AppContextBase> m_app_context;
    AtomicVar<bool> m_is_running;
};

#pragma region Render commands

struct RENDER_COMMAND(RecreateSwapchain)
    : renderer::RenderCommand
{
    WeakHandle<Engine> engine_weak;

    RENDER_COMMAND(RecreateSwapchain)(const Handle<Engine>& engine)
        : engine_weak(engine)
    {
    }

    virtual ~RENDER_COMMAND(RecreateSwapchain)() override = default;

    virtual RendererResult operator()() override
    {
        Handle<Engine> engine = engine_weak.Lock();

        if (!engine)
        {
            HYP_LOG(Rendering, Warning, "Engine was destroyed before swapchain could be recreated");
            HYPERION_RETURN_OK;
        }

        engine->m_should_recreate_swapchain = true;

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region Engine

const Handle<Engine>& Engine::GetInstance()
{
    return g_engine;
}

Engine::Engine()
    : m_is_shutting_down(false),
      m_should_recreate_swapchain(false)
{
}

Engine::~Engine()
{
}

HYP_API void Engine::Init()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_main_thread);

    // Set ready to false after render thread stops running.
    HYP_DEFER({ SetReady(false); });

    AssertThrowMsg(m_app_context != nullptr, "App context must be set before initializing the engine!");

    m_render_thread = MakeUnique<RenderThread>(m_app_context);

    AssertThrow(m_app_context->GetMainWindow() != nullptr);

    // m_app_context->GetMainWindow()->OnWindowSizeChanged.Bind(
    //                                                        [this](Vec2i new_window_size)
    //                                                        {
    //                                                            HYP_LOG(Engine, Info, "Resize window to {}", new_window_size);

    //                                                            // m_final_pass->Resize(Vec2u(new_window_size));
    //                                                        })
    //     .Detach();

    RenderObjectDeleter<renderer::Platform::current>::Initialize();

    TaskSystem::GetInstance().Start();

    AssertThrow(g_rendering_api != nullptr);

    g_rendering_api->GetOnSwapchainRecreatedDelegate()
        .Bind([this](SwapchainBase* swapchain)
            {
                m_final_pass = MakeUnique<FinalPass>(swapchain->HandleFromThis());
                m_final_pass->Create();
            })
        .Detach();

    m_global_descriptor_table = g_rendering_api->MakeDescriptorTable(&renderer::GetStaticDescriptorTableDeclaration());

    // Update app configuration to reflect device, after instance is created (e.g RT is not supported)
    m_app_context->UpdateConfigurationOverrides();

    m_configuration.SetToDefaultConfiguration();
    m_configuration.LoadFromDefinitionsFile();

    if (!m_shader_compiler.LoadShaderDefinitions())
    {
        HYP_FAIL("Failed to load shaders from definitions file!");
    }

    m_gpu_buffer_holder_map = MakeUnique<GPUBufferHolderMap>();

    m_render_data = MakeUnique<ShaderGlobals>();
    m_render_data->Create();

    m_placeholder_data = MakeUnique<PlaceholderData>();
    m_placeholder_data->Create();

    m_render_state = CreateObject<RenderState>();
    InitObject(m_render_state);

#ifdef HYP_EDITOR
    // Create script compilation service
    m_scripting_service = MakeUnique<ScriptingService>(
        GetResourceDirectory() / "scripts" / "src",
        GetResourceDirectory() / "scripts" / "projects",
        GetResourceDirectory() / "scripts" / "bin");

    m_scripting_service->Start();
#endif

    RC<NetRequestThread> net_request_thread = MakeRefCountedPtr<NetRequestThread>();
    SetGlobalNetRequestThread(net_request_thread);
    net_request_thread->Start();

    RC<StreamingThread> streaming_thread = MakeRefCountedPtr<StreamingThread>();
    SetGlobalStreamingThread(streaming_thread);
    streaming_thread->Start();

    // g_streaming_manager->Start();

    // must start after net request thread
    if (GetCommandLineArguments()["Profile"])
    {
        StartProfilerConnectionThread(ProfilerConnectionParams {
            /* endpoint_url */ GetCommandLineArguments()["TraceURL"].ToString(),
            /* enabled */ true });
    }

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        // Global
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("WorldsBuffer"), GetRenderData()->worlds->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightsBuffer"), GetRenderData()->lights->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CurrentLight"), GetRenderData()->lights->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ObjectsBuffer"), GetRenderData()->objects->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CamerasBuffer"), GetRenderData()->cameras->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvGridsBuffer"), GetRenderData()->env_grids->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbesBuffer"), GetRenderData()->env_probes->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CurrentEnvProbe"), GetRenderData()->env_probes->GetBuffer(frame_index));

        for (uint32 i = 0; i < max_bound_reflection_probes; i++)
        {
            m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), i, GetPlaceholderData()->DefaultTexture2D->GetRenderResource().GetImageView());
        }

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("VoxelGridTexture"), GetPlaceholderData()->GetImageView3D1x1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightFieldColorTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightFieldDepthTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("BlueNoiseBuffer"), GPUBufferRef::Null());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SphereSamplesBuffer"), GPUBufferRef::Null());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsTextureArray"), GetPlaceholderData()->GetImageView2D1x1R8Array());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PointLightShadowMapsTextureArray"), GetPlaceholderData()->GetImageViewCube1x1R8Array());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsBuffer"), GetRenderData()->shadow_map_data->GetBuffer(frame_index));

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightmapVolumesBuffer"), GetRenderData()->lightmap_volumes->GetBuffer(frame_index));

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIUniforms"), GetPlaceholderData()->GetOrCreateBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(DDGIUniforms), true /* exact size */));
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIIrradianceTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIDepthTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("RTRadianceResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SamplerNearest"), GetPlaceholderData()->GetSamplerNearest());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SamplerLinear"), GetPlaceholderData()->GetSamplerLinearMipmap());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("UITexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("FinalOutputTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        // Object
        m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("CurrentObject"), GetRenderData()->objects->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("MaterialsBuffer"), GetRenderData()->materials->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("SkeletonsBuffer"), GetRenderData()->skeletons->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("LightmapVolumeIrradianceTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("LightmapVolumeRadianceTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        // Material
        if (g_rendering_api->GetRenderConfig().IsBindlessSupported())
        {
            for (uint32 texture_index = 0; texture_index < max_bindless_resources; texture_index++)
            {
                m_global_descriptor_table->GetDescriptorSet(NAME("Material"), frame_index)
                    ->SetElement(NAME("Textures"), texture_index, GetPlaceholderData()->GetImageView2D1x1R8());
            }
        }
    }

    CreateBlueNoiseBuffer();
    CreateSphereSamplesBuffer();

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Create());

    m_material_descriptor_set_manager = MakeUnique<MaterialDescriptorSetManager>();
    m_material_descriptor_set_manager->Initialize();

    m_graphics_pipeline_cache = MakeUnique<GraphicsPipelineCache>();
    m_graphics_pipeline_cache->Initialize();

    m_final_pass = MakeUnique<FinalPass>(g_rendering_api->GetSwapchain()->HandleFromThis());
    m_final_pass->Create();

    m_debug_drawer = MakeUnique<DebugDrawer>();

    m_view = AllocateResource<RenderView>(nullptr);
    m_view->SetViewport(Viewport {
        .extent = m_app_context->GetMainWindow()->GetDimensions(),
        .position = Vec2i::Zero() });
    m_view->IncRef();

    m_world = CreateObject<World>();

    SetReady(true);
}

void Engine::CreateBlueNoiseBuffer()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    static_assert(sizeof(BlueNoiseBuffer::sobol_256spp_256d) == sizeof(BlueNoise::sobol_256spp_256d));
    static_assert(sizeof(BlueNoiseBuffer::scrambling_tile) == sizeof(BlueNoise::scrambling_tile));
    static_assert(sizeof(BlueNoiseBuffer::ranking_tile) == sizeof(BlueNoise::ranking_tile));

    constexpr SizeType blue_noise_buffer_size = sizeof(BlueNoiseBuffer);

    constexpr SizeType sobol_256spp_256d_offset = offsetof(BlueNoiseBuffer, sobol_256spp_256d);
    constexpr SizeType sobol_256spp_256d_size = sizeof(BlueNoise::sobol_256spp_256d);
    constexpr SizeType scrambling_tile_offset = offsetof(BlueNoiseBuffer, scrambling_tile);
    constexpr SizeType scrambling_tile_size = sizeof(BlueNoise::scrambling_tile);
    constexpr SizeType ranking_tile_offset = offsetof(BlueNoiseBuffer, ranking_tile);
    constexpr SizeType ranking_tile_size = sizeof(BlueNoise::ranking_tile);

    static_assert(blue_noise_buffer_size == (sobol_256spp_256d_offset + sobol_256spp_256d_size) + ((scrambling_tile_offset - (sobol_256spp_256d_offset + sobol_256spp_256d_size)) + scrambling_tile_size) + ((ranking_tile_offset - (scrambling_tile_offset + scrambling_tile_size)) + ranking_tile_size));

    GPUBufferRef blue_noise_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STORAGE_BUFFER, sizeof(BlueNoiseBuffer));
    HYPERION_ASSERT_RESULT(blue_noise_buffer->Create());
    blue_noise_buffer->Copy(sobol_256spp_256d_offset, sobol_256spp_256d_size, &BlueNoise::sobol_256spp_256d[0]);
    blue_noise_buffer->Copy(scrambling_tile_offset, scrambling_tile_size, &BlueNoise::scrambling_tile[0]);
    blue_noise_buffer->Copy(ranking_tile_offset, ranking_tile_size, &BlueNoise::ranking_tile[0]);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("BlueNoiseBuffer"), blue_noise_buffer);
    }
}

void Engine::CreateSphereSamplesBuffer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    GPUBufferRef sphere_samples_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(Vec4f) * 4096);
    HYPERION_ASSERT_RESULT(sphere_samples_buffer->Create());

    Vec4f* sphere_samples = new Vec4f[4096];

    uint32 seed = 0;

    for (uint32 i = 0; i < 4096; i++)
    {
        Vec3f sample = MathUtil::RandomInSphere(Vec3f {
            MathUtil::RandomFloat(seed),
            MathUtil::RandomFloat(seed),
            MathUtil::RandomFloat(seed) });

        sphere_samples[i] = Vec4f(sample, 0.0f);
    }

    sphere_samples_buffer->Copy(sizeof(Vec4f) * 4096, sphere_samples);

    delete[] sphere_samples;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("SphereSamplesBuffer"), sphere_samples_buffer);
    }
}

bool Engine::IsRenderLoopActive() const
{
    return m_render_thread != nullptr
        && m_render_thread->IsRunning();
}

bool Engine::StartRenderLoop()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_main_thread);

    if (m_render_thread == nullptr)
    {
        HYP_LOG(Engine, Error, "Render thread is not initialized!");
        return false;
    }

    if (m_render_thread->IsRunning())
    {
        HYP_LOG(Engine, Warning, "Render thread is already running!");
        return true;
    }

    m_render_thread->Start();

    return true;
}

void Engine::RequestStop()
{
    if (m_render_thread != nullptr)
    {
        if (m_render_thread->IsRunning())
        {
            m_render_thread->Stop();
        }
    }
}

void Engine::FinalizeStop()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_main_thread);

    m_is_shutting_down.Set(true, MemoryOrder::SEQUENTIAL);

    HYP_LOG(Engine, Info, "Stopping all engine processes");

    m_delegates.OnShutdown();

    if (m_scripting_service)
    {
        m_scripting_service->Stop();
        m_scripting_service.Reset();
    }

    // must stop before net request thread
    StopProfilerConnectionThread();

    // g_streaming_manager->Stop();

    if (RC<StreamingThread> streaming_thread = GetGlobalStreamingThread())
    {
        if (streaming_thread->IsRunning())
        {
            streaming_thread->Stop();
        }

        if (streaming_thread->CanJoin())
        {
            streaming_thread->Join();
        }

        SetGlobalStreamingThread(nullptr);
    }

    if (RC<NetRequestThread> net_request_thread = GetGlobalNetRequestThread())
    {
        if (net_request_thread->IsRunning())
        {
            net_request_thread->Stop();
        }

        if (net_request_thread->CanJoin())
        {
            net_request_thread->Join();
        }

        SetGlobalNetRequestThread(nullptr);
    }

    m_world.Reset();

    if (TaskSystem::GetInstance().IsRunning())
    { // Stop task system
        HYP_LOG(Tasks, Info, "Stopping task system");

        TaskSystem::GetInstance().Stop();

        HYP_LOG(Tasks, Info, "Task system stopped");
    }

    m_graphics_pipeline_cache->Destroy();
    m_graphics_pipeline_cache.Reset();

    m_material_descriptor_set_manager.Reset();

    m_view->DecRef();
    FreeResource(m_view);
    m_view = nullptr;

    m_debug_drawer.Reset();

    m_gpu_buffer_holder_map.Reset();

    m_render_state.Reset();

    m_render_data->Destroy();

    m_final_pass.Reset();

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Destroy());

    m_placeholder_data->Destroy();

    g_safe_deleter->ForceDeleteAll();
    RenderObjectDeleter<renderer::Platform::current>::RemoveAllNow(/* force */ true);

    m_render_thread->Join();
    m_render_thread.Reset();
}

HYP_API void Engine::RenderNextFrame()
{
    HYP_PROFILE_BEGIN;

#ifdef HYP_ENABLE_RENDER_STATS
    // @TODO Refactor to use double buffering
    m_render_stats_calculator.Advance(m_render_stats);
    OnRenderStatsUpdated(m_render_stats);
#endif

    FrameBase* frame = g_rendering_api->PrepareNextFrame();

    PreFrameUpdate(frame);

    // temp
    if (m_world->IsReady())
    {
        m_world->GetRenderResource().Render(frame);

        m_final_pass->Render(frame, &m_world->GetRenderResource());

        for (auto& it : m_gpu_buffer_holder_map->GetItems())
        {
            it.second->UpdateBufferSize(frame->GetFrameIndex());
            it.second->UpdateBufferData(frame->GetFrameIndex());
        }

        m_world->GetRenderResource().PostRender(frame);
    }

    g_rendering_api->PresentFrame(frame);
}

void Engine::PreFrameUpdate(FrameBase* frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    // Add/remove pending descriptor sets before flushing render commands or updating buffers and descriptor sets.
    // otherwise we'll have an issue when render commands expect the descriptor sets to be there.
    m_material_descriptor_set_manager->UpdatePendingDescriptorSets(frame);
    m_material_descriptor_set_manager->Update(frame);

    HYPERION_ASSERT_RESULT(renderer::RenderCommands::Flush());

    if (m_world->IsReady())
        m_world->GetRenderResource().PreRender(frame);

    RenderObjectDeleter<renderer::Platform::current>::Iterate();

    g_safe_deleter->PerformEnqueuedDeletions();

    m_render_state->ResetStates(RENDER_STATE_ACTIVE_ENV_PROBE | RENDER_STATE_ACTIVE_LIGHT);
}

#pragma endregion Engine

} // namespace hyperion
