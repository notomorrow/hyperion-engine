/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerLocalLights.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static constexpr size_t MaxChangesPerGeneration = 32;
static constexpr size_t ChangeJournalGenerations = 8;
static constexpr float BounceReach = 2.0f;

#pragma region GlimmerLocalLights

void GlimmerLocalLights::Update(const GlimmerChannelState& state)
{
    HYP_SCOPE;

    GlimmerSceneChanges changes;

    const auto addSphere = [&changes](const Vec4f& positionRadius)
    {
        GlimmerSceneChange change;
        change.bounds.min = positionRadius.GetXYZ() - Vec3f(positionRadius.w * BounceReach);
        change.bounds.max = positionRadius.GetXYZ() + Vec3f(positionRadius.w * BounceReach);

        changes.Add(change);
    };

    for (uint32 lightIndex = 0; lightIndex < state.numLocalLights; lightIndex++)
    {
        const GlimmerLocalLightState& light = state.localLights[lightIndex];

        const auto previous = m_lights.FindIf([&light](const GlimmerLocalLightState& other)
            {
                return other.key == light.key;
            });

        if (previous == m_lights.End())
        {
            addSphere(light.positionRadius);

            continue;
        }

        if (previous->positionRadius != light.positionRadius
            || previous->color != light.color
            || previous->spotDirection != light.spotDirection
            || previous->spotAngles != light.spotAngles)
        {
            addSphere(previous->positionRadius);
            addSphere(light.positionRadius);
        }
    }

    for (const GlimmerLocalLightState& previous : m_lights)
    {
        bool isKept = false;

        for (uint32 lightIndex = 0; lightIndex < state.numLocalLights && !isKept; lightIndex++)
        {
            isKept = state.localLights[lightIndex].key == previous.key;
        }

        if (!isKept)
        {
            addSphere(previous.positionRadius);
        }
    }

    m_lights.Clear();

    m_shaderData = GlimmerLocalLightsShaderData {};
    m_shaderData.info.x = state.numLocalLights;

    for (uint32 lightIndex = 0; lightIndex < state.numLocalLights; lightIndex++)
    {
        const GlimmerLocalLightState& light = state.localLights[lightIndex];

        m_lights.PushBack(light);

        m_shaderData.lights[lightIndex] = GlimmerLocalLightShaderData {
            light.positionRadius,
            light.color,
            light.spotDirection,
            light.spotAngles
        };
    }

    if (changes.Size() == 0)
    {
        return;
    }

    changes.Quantize(MaxChangesPerGeneration);

    m_generation++;

    if (m_changeJournal.Size() >= ChangeJournalGenerations)
    {
        m_changeJournal.PopFront();
    }

    m_changeJournal.PushBack(ChangeSet { m_generation, std::move(changes) });
}

bool GlimmerLocalLights::GetChangesSince(uint32 seenGeneration, GlimmerSceneChanges& outChanges) const
{
    if (seenGeneration == m_generation)
    {
        return true;
    }

    if (m_changeJournal.Empty() || seenGeneration > m_generation || m_changeJournal.Front().generation > seenGeneration + 1)
    {
        return false;
    }

    for (const ChangeSet& changeSet : m_changeJournal)
    {
        if (changeSet.generation > seenGeneration)
        {
            outChanges.changes.Concat(changeSet.changes.changes.ToSpan());
        }
    }

    return true;
}

#pragma endregion GlimmerLocalLights

} // namespace Hyperion
