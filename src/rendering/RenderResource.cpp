#include <rendering/RenderResource.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderCommand.hpp>

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
    Assert(m_bufferIndex == ~0u);
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

void RenderResourceBase::AcquireBufferIndex()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    Assert(m_bufferIndex == ~0u);

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

    Assert(m_bufferIndex != ~0u);

    GpuBufferHolderBase* holder = GetGpuBufferHolder();
    Assert(holder != nullptr);

    holder->ReleaseIndex(m_bufferIndex);

    m_bufferIndex = ~0u;
    m_bufferAddress = nullptr;
}

#pragma endregion RenderResourceBase

} // namespace hyperion
