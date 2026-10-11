/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerChannel.hpp>

#include <Core/Containers/Map.hpp>

namespace Hyperion {

static Mutex s_channelsMutex;
static Map<const World*, SharedPtr<GlimmerChannel>> s_channels;

SharedPtr<GlimmerChannel> GlimmerChannel::Get(const World* world)
{
    Mutex::Guard guard(s_channelsMutex);

    if (const auto* it = s_channels.TryGet(world))
    {
        return it->second;
    }

    return SharedPtr<GlimmerChannel>();
}

void GlimmerChannel::Register(const World* world, const SharedPtr<GlimmerChannel>& channel)
{
    Mutex::Guard guard(s_channelsMutex);

    s_channels.Set(world, channel);
}

void GlimmerChannel::Unregister(const World* world)
{
    Mutex::Guard guard(s_channelsMutex);

    s_channels.Erase(world);
}

void GlimmerChannel::Publish(const GlimmerChannelState& state, Array<GlimmerGroundUpload, GlimmerAllocator>&& groundUploads)
{
    Mutex::Guard guard(m_mutex);

    if (state.groundGeneration != m_state.groundGeneration)
    {
        // uploads for the old generation would be written over the new window
        m_pendingGroundUploads.Clear();
    }

    m_state = state;

    for (GlimmerGroundUpload& upload : groundUploads)
    {
        m_pendingGroundUploads.PushBack(std::move(upload));
    }
}

void GlimmerChannel::Consume(GlimmerChannelState& outState, Array<GlimmerGroundUpload, GlimmerAllocator>& outGroundUploads)
{
    Mutex::Guard guard(m_mutex);

    outState = m_state;
    outGroundUploads = std::move(m_pendingGroundUploads);
    m_pendingGroundUploads = Array<GlimmerGroundUpload, GlimmerAllocator>();
}

void GlimmerChannel::PublishProbeDebug(Array<GlimmerProbeDebugRecord, GlimmerAllocator>&& records)
{
    Mutex::Guard guard(m_mutex);

    m_probeDebugRecords = std::move(records);
    m_hasProbeDebugRecords = true;
}

bool GlimmerChannel::ConsumeProbeDebug(Array<GlimmerProbeDebugRecord, GlimmerAllocator>& outRecords)
{
    Mutex::Guard guard(m_mutex);

    if (!m_hasProbeDebugRecords)
    {
        return false;
    }

    outRecords = std::move(m_probeDebugRecords);
    m_probeDebugRecords = Array<GlimmerProbeDebugRecord, GlimmerAllocator>();
    m_hasProbeDebugRecords = false;

    return true;
}

} // namespace Hyperion
