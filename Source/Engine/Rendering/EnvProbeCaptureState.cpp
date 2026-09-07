/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/EnvProbeCaptureState.hpp>

#include <Scene/EnvProbe.hpp>

#include <Scene/Layer.hpp>

#include <Scene/Systems/LayerOverrideSystem.hpp>

#include <Scene/World.hpp>

#include <Rendering/Texture.hpp>

#include <Asset/AssetRegistry.hpp>

namespace Hyperion {

EnvProbeCaptureState::EnvProbeCaptureState(EnvProbe* envProbe, Name layerName)
    : layerName(layerName),
      m_envProbe(envProbe)
{
}

EnvProbeCaptureState::~EnvProbeCaptureState()
{
    if (!m_envProbe)
    {
        return;
    }

    if (m_envProbe->GetCaptureState() == this)
    {
        m_envProbe->m_captureState = nullptr;
    }
}

void EnvProbeCaptureState::Begin()
{
    Assert(m_envProbe != nullptr);

    if (!m_envProbe)
    {
        return;
    }

    m_envProbe->SetDimensions(EnvProbe::GetDefaultDimensions(m_envProbe->GetEnvProbeType()));

    m_envProbe->m_captureState = this;

    const int32 numReadbacks = (m_envProbe->ShouldComputeSphericalHarmonics() ? 1 : 0)
        + ((m_envProbe->GetEnvProbeFlags() & EPF_VISIBILITY) ? 1 : 0)
        + ((m_envProbe->GetEnvProbeFlags() & EPF_HIT_MASK) ? 1 : 0);

    m_envProbe->m_pendingCaptureReadbacks.Set(numReadbacks, MemoryOrder::RELEASE);

    m_envProbe->InitCaptureData(this);

    m_envProbe->needsRender.Store(true);
}

void EnvProbeCaptureState::End(bool commitResult)
{
    Assert(m_envProbe != nullptr);

    if (!m_envProbe)
    {
        return;
    }

    if (m_envProbe->GetCaptureState() == this)
    {
        m_envProbe->m_captureState = nullptr;
    }

    if (!commitResult)
    {
        // abandon ship
        return;
    }

    if (texture.IsValid())
    {
        texture->SetIsTransient(false);
        texture->SetName(EnvProbe::BuildBakedTextureName(m_envProbe->GetName(), layerName));

        GetCurrentAssetRegistry()->PutAssetUnique(texture);

        m_envProbe->SetBakedTextureForLayer(texture, layerName);
    }

    if (visibilityTexture.IsValid())
    {
        visibilityTexture->SetIsTransient(false);
        visibilityTexture->SetName(EnvProbe::BuildVisibilityTextureName(m_envProbe->GetName(), layerName));

        GetCurrentAssetRegistry()->PutAssetUnique(visibilityTexture);

        m_envProbe->SetVisibilityTextureForLayer(visibilityTexture, layerName);
    }

    if (m_envProbe->ShouldComputeSphericalHarmonics())
    {
        m_envProbe->SetSphericalHarmonicsDataForLayer(sphericalHarmonics, layerName);
    }

    m_envProbe->DestroyCaptureData();

    // Set the overrides!
    if (World* world = m_envProbe->GetWorld(); world && world->GetActiveLayerName() == layerName)
    {
        if (LayerOverrideSystem* layerOverrideSystem = world->GetSystem<LayerOverrideSystem>())
        {
            layerOverrideSystem->ApplyOverrides(m_envProbe, layerName);
        }
    }
}

} // namespace Hyperion
