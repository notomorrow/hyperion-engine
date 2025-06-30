#include <rendering/RenderResource.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Mutex.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Resource);

#pragma region RenderResourceBase

RenderResourceBase::RenderResourceBase()
    : m_bufferIndex(~0u),
      m_bufferAddress(nullptr)
{
}

RenderResourceBase::RenderResourceBase(RenderResourceBase&& other) noexcept
    : ResourceBase(static_cast<ResourceBase&&>(other)),
      m_bufferIndex(other.m_bufferIndex),
      m_bufferAddress(other.m_bufferAddress)
{
    other.m_bufferIndex = ~0u;
    other.m_bufferAddress = nullptr;
}

RenderResourceBase::~RenderResourceBase()
{
}

void RenderResourceBase::Initialize()
{
    AssertThrow(m_bufferIndex == ~0u);
    AcquireBufferIndex();

    Initialize_Internal();
}

void RenderResourceBase::Destroy()
{
    if (m_bufferIndex != ~0u)
    {
        ReleaseBufferIndex();
    }

    Destroy_Internal();
}

void RenderResourceBase::Update()
{
    Update_Internal();
}

ThreadBase* RenderResourceBase::GetOwnerThread() const
{
    static ThreadBase* const renderThread = Threads::GetThread(g_renderThread);

    return renderThread;
}

bool RenderResourceBase::CanExecuteInline() const
{
    return Threads::IsOnThread(g_renderThread);
}

void RenderResourceBase::FlushScheduledTasks() const
{
    HYP_FAIL("Cannot flush scheduled tasks in RenderResourceBase!");
    // HYPERION_ASSERT_RESULT(RenderCommands::Flush());
}

void RenderResourceBase::EnqueueOp(Proc<void()>&& proc)
{
    struct RENDER_COMMAND(RenderResourceOperation)
        : RenderCommand
    {
        Proc<void()> proc;

        RENDER_COMMAND(RenderResourceOperation)(Proc<void()>&& proc)
            : proc(std::move(proc))
        {
        }

        virtual ~RENDER_COMMAND(RenderResourceOperation)() override = default;

        virtual RendererResult operator()() override
        {
            proc();

            return {};
        }
    };

    PUSH_RENDER_COMMAND(RenderResourceOperation, std::move(proc));
}

void RenderResourceBase::AcquireBufferIndex()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    AssertThrow(m_bufferIndex == ~0u);

    GpuBufferHolderBase* holder = GetGpuBufferHolder();

    if (holder == nullptr)
    {
        return;
    }

    m_bufferIndex = holder->AcquireIndex(&m_bufferAddress);
}

void RenderResourceBase::ReleaseBufferIndex()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    AssertThrow(m_bufferIndex != ~0u);

    GpuBufferHolderBase* holder = GetGpuBufferHolder();
    AssertThrow(holder != nullptr);

    holder->ReleaseIndex(m_bufferIndex);

    m_bufferIndex = ~0u;
    m_bufferAddress = nullptr;
}

#pragma endregion RenderResourceBase

} // namespace hyperion