/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <engine/EngineDriver.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/AsyncCompute.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderSwapchain.hpp>
#include <rendering/RenderConfig.hpp>

#include <asset/Assets.hpp>

#include <streaming/StreamingManager.hpp>

#include <core/profiling/ProfileScope.hpp>
#include <core/filesystem/FsUtil.hpp>

#include <util/MeshBuilder.hpp>

#include <scene/World.hpp>
#include <rendering/Texture.hpp>

#include <core/debug/StackDump.hpp>
#include <system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/net/NetRequestThread.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/object/HypClassUtils.hpp>

#include <system/AppContext.hpp>
#include <system/App.hpp>

#include <scripting/ScriptingService.hpp>

#include <engine/EngineGlobals.hpp>
#include <HyperionEngine.hpp>

#define HYP_LOG_FRAMES_PER_SECOND

namespace hyperion {

class RenderThread;

static RenderThread* g_renderThreadInstance = nullptr;

static struct GlobalDescriptorSetsDeclarations
{
    GlobalDescriptorSetsDeclarations()
    {
#include <rendering/inl/DescriptorSets.inl>
    }
} g_globalDescriptorSetsDeclarations;

void HandleSignal(int signum);

class RenderThread final : public Thread<Scheduler>
{
public:
    RenderThread(const Handle<AppContextBase>& appContext)
        : Thread(g_renderThread, ThreadPriorityValue::HIGHEST),
          m_appContext(appContext),
          m_isRunning(false)
    {
    }

    // Overrides Start() to not create a thread object. Runs the render loop on the main thread.
    bool Start()
    {
        Assert(m_isRunning.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

        // Must be current thread
        Threads::AssertOnThread(g_renderThread);

        g_renderThreadInstance = this;

        SetCurrentThreadObject(this);
        m_scheduler.SetOwnerThread(Id());

        signal(SIGINT, HandleSignal);
        signal(SIGSEGV, HandleSignal);

        (*this)();

        return true;
    }

    void Stop()
    {
        g_renderThreadInstance = nullptr;

        m_isRunning.Set(false, MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_isRunning.Get(MemoryOrder::ACQUIRE);
    }

private:
    virtual void operator()() override
    {
        Assert(m_appContext != nullptr);

        SystemEvent event;

        Queue<Scheduler::ScheduledTask> tasks;

        while (m_isRunning.Get(MemoryOrder::RELAXED))
        {
            // input manager stuff
            while (m_appContext->PollEvent(event))
            {
                m_appContext->GetMainWindow()->GetInputEventSink().Push(std::move(event));
            }

            RenderApi_BeginFrame_RenderThread();

            if (uint32 numEnqueued = m_scheduler.NumEnqueued())
            {
                m_scheduler.AcceptAll(tasks);

                while (tasks.Any())
                {
                    tasks.Pop().Execute();
                }
            }

            g_engineDriver->RenderNextFrame();

            RenderApi_EndFrame_RenderThread();
        }
    }

    Handle<AppContextBase> m_appContext;
    AtomicVar<bool> m_isRunning;
};

void HandleSignal(int signum)
{
    if (!g_renderThreadInstance)
    {
        return;
    }

    g_renderThreadInstance->Stop();
    g_renderThreadInstance->Join();

    exit(signum);
}

#pragma region Render commands

struct RENDER_COMMAND(RecreateSwapchain)
    : RenderCommand
{
    WeakHandle<EngineDriver> engineWeak;

    RENDER_COMMAND(RecreateSwapchain)(const Handle<EngineDriver>& engine)
        : engineWeak(engine)
    {
    }

    virtual ~RENDER_COMMAND(RecreateSwapchain)() override = default;

    virtual RendererResult operator()() override
    {
        Handle<EngineDriver> engine = engineWeak.Lock();

        if (!engine)
        {
            HYP_LOG(Rendering, Warning, "EngineDriver was destroyed before swapchain could be recreated");
            HYPERION_RETURN_OK;
        }

        engine->m_shouldRecreateSwapchain = true;

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region EngineDriver

const Handle<EngineDriver>& EngineDriver::GetInstance()
{
    return g_engineDriver;
}

EngineDriver::EngineDriver()
    : m_isShuttingDown(false),
      m_shouldRecreateSwapchain(false)
{
}

EngineDriver::~EngineDriver()
{
}

HYP_API void EngineDriver::Init()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_mainThread);

    // Set ready to false after render thread stops running.
    HYP_DEFER({ SetReady(false); });

    Assert(m_appContext != nullptr, "App context must be set before initializing the engine!");

    m_renderThread = MakeUnique<RenderThread>(m_appContext);

    Assert(m_appContext->GetMainWindow() != nullptr);

    // m_appContext->GetMainWindow()->OnWindowSizeChanged.Bind(
    //                                                        [this](Vec2i newWindowSize)
    //                                                        {
    //                                                            HYP_LOG(Engine, Info, "Resize window to {}", newWindowSize);

    //                                                            // m_finalPass->Resize(Vec2u(newWindowSize));
    //                                                        })
    //     .Detach();

    Assert(g_renderBackend != nullptr);

    g_renderBackend->GetOnSwapchainRecreatedDelegate()
        .Bind([this](SwapchainBase* swapchain)
            {
                m_finalPass = MakeUnique<FinalPass>(swapchain->HandleFromThis());
                m_finalPass->Create();
            })
        .Detach();

    // Update app configuration to reflect device, after instance is created (e.g RT is not supported)
    m_appContext->UpdateConfigurationOverrides();

#ifdef HYP_EDITOR
    // Create script compilation service
    m_scriptingService = MakeUnique<ScriptingService>(
        GetResourceDirectory() / "scripts" / "src",
        GetResourceDirectory() / "scripts" / "projects",
        GetExecutablePath()); // copy script binaries into executable path

    m_scriptingService->Start();
#endif

    RC<NetRequestThread> netRequestThread = MakeRefCountedPtr<NetRequestThread>();
    SetGlobalNetRequestThread(netRequestThread);
    netRequestThread->Start();

    // g_streamingManager->Start();

    // must start after net request thread
    if (GetCommandLineArguments()["Profile"])
    {
        StartProfilerConnectionThread(ProfilerConnectionParams {
            /* endpointUrl */ GetCommandLineArguments()["TraceURL"].ToString(),
            /* enabled */ true });
    }

    m_finalPass = MakeUnique<FinalPass>(g_renderBackend->GetSwapchain()->HandleFromThis());
    m_finalPass->Create();

    m_debugDrawer = MakeUnique<DebugDrawer>();
    m_debugDrawer->Initialize();

    m_world = CreateObject<World>();

    SetReady(true);
}

bool EngineDriver::IsRenderLoopActive() const
{
    return m_renderThread != nullptr
        && m_renderThread->IsRunning();
}

bool EngineDriver::StartRenderLoop()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_mainThread);

    if (m_renderThread == nullptr)
    {
        HYP_LOG(Engine, Error, "Render thread is not initialized!");
        return false;
    }

    if (m_renderThread->IsRunning())
    {
        HYP_LOG(Engine, Warning, "Render thread is already running!");
        return true;
    }

    m_renderThread->Start();

    RenderApi_Shutdown();

    return true;
}

void EngineDriver::RequestStop()
{
    if (m_renderThread != nullptr)
    {
        if (m_renderThread->IsRunning())
        {
            m_renderThread->Stop();
        }
    }
}

void EngineDriver::FinalizeStop()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_mainThread);

    m_isShuttingDown.Set(true, MemoryOrder::SEQUENTIAL);

    HYP_LOG(Engine, Info, "Stopping all engine processes");

    m_delegates.OnShutdown();

    if (m_scriptingService)
    {
        m_scriptingService->Stop();
        m_scriptingService.Reset();
    }

    // must stop before net request thread
    StopProfilerConnectionThread();

    if (RC<NetRequestThread> netRequestThread = GetGlobalNetRequestThread())
    {
        if (netRequestThread->IsRunning())
        {
            netRequestThread->Stop();
        }

        if (netRequestThread->CanJoin())
        {
            netRequestThread->Join();
        }

        SetGlobalNetRequestThread(nullptr);
    }

    m_world.Reset();

    m_debugDrawer.Reset();

    m_finalPass.Reset();

    g_safeDeleter->ForceDeleteAll();

    m_renderThread->Join();
    m_renderThread.Reset();
}

HYP_API void EngineDriver::RenderNextFrame()
{
    HYP_PROFILE_BEGIN;

    FrameBase* frame = g_renderBackend->PrepareNextFrame();

    PreFrameUpdate(frame);

    if (m_world->IsReady())
    {
        g_renderGlobalState->gpuBuffers[GRB_WORLDS]->WriteBufferData(0, RenderApi_GetWorldBufferData(), sizeof(WorldShaderData));

        RenderSetup rs { m_world, nullptr };
        g_renderGlobalState->mainRenderer->RenderFrame(frame, rs);

        m_finalPass->Render(frame, rs);
    }

    g_renderGlobalState->UpdateBuffers(frame);

    g_renderBackend->PresentFrame(frame);
}

void EngineDriver::PreFrameUpdate(FrameBase* frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    RenderObjectDeleter::Iterate();

    g_safeDeleter->PerformEnqueuedDeletions();
}

#pragma endregion EngineDriver

} // namespace hyperion
