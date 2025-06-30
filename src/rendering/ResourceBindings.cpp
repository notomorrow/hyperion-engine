#include <rendering/RenderProxy.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/lightmapper/RenderLightmapVolume.hpp>

#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/Texture.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Defines.hpp>

#include <Engine.hpp>

namespace hyperion {

void OnBindingChanged_ReflectionProbe(EnvProbe* envProbe, uint32 prev, uint32 next)
{
    AssertDebug(envProbe != nullptr);
    AssertDebug(envProbe->IsReady());

    AssertThrow(envProbe->IsA<SkyProbe>() || envProbe->IsA<ReflectionProbe>(),
        "EnvProbe must be a SkyProbe or ReflectionProbe, but is: %s", envProbe->InstanceClass()->GetName());

    if (!envProbe->GetPrefilteredEnvMap().IsValid())
    {
        HYP_LOG(Rendering, Error, "EnvProbe {} (class: {}) has no prefiltered env map set!\n", envProbe->Id(),
            envProbe->InstanceClass()->GetName());

        return;
    }

    DebugLog(LogType::Debug, "EnvProbe %u (class: %s) binding changed from %u to %u\n", envProbe->Id().Value(),
        *envProbe->InstanceClass()->GetName(),
        prev, next);

    if (prev != ~0u)
    {
        HYP_LOG(Rendering, Debug, "UN setting env probe texture at index: {}", prev);
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            g_renderGlobalState->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("EnvProbeTextures"), prev, g_renderGlobalState->PlaceholderData->DefaultTexture2D->GetRenderResource().GetImageView());
        }
    }
    else
    {
        envProbe->GetRenderResource().IncRef();
        envProbe->GetPrefilteredEnvMap()->GetRenderResource().IncRef();
    }

    IRenderProxy* proxy = RenderApi_GetRenderProxy(envProbe->Id());
    AssertThrow(proxy != nullptr);

    RenderProxyEnvProbe* proxyCasted = static_cast<RenderProxyEnvProbe*>(proxy);

    // temp shit
    AssertDebug(envProbe->GetRenderResource().GetBufferIndex() != ~0u);
    RenderApi_AssignResourceBinding(envProbe, envProbe->GetRenderResource().GetBufferIndex());

    HYP_LOG(Rendering, Debug, "Setting env probe texture at index: {} to tex with id: {}  proxy: {}  at frame {}", next, envProbe->GetPrefilteredEnvMap().Id(), (void*)proxy,
        RenderApi_GetFrameIndex_RenderThread());

    // proxyCasted->bufferData.textureIndex = next;

    if (next != ~0u)
    {
        AssertDebug(envProbe->GetPrefilteredEnvMap().IsValid());
        AssertDebug(envProbe->GetPrefilteredEnvMap()->IsReady());

        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            g_renderGlobalState->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)
                ->SetElement(NAME("EnvProbeTextures"), next, envProbe->GetPrefilteredEnvMap()->GetRenderResource().GetImageView());
        }
    }
    else
    {
        envProbe->GetRenderResource().DecRef();
        envProbe->GetPrefilteredEnvMap()->GetRenderResource().DecRef();
    }
}

void WriteBufferData_EnvProbe(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, void* bufferData, SizeType size)
{
    AssertDebug(gpuBufferHolder != nullptr);
    AssertDebug(idx != ~0u);

    EnvProbeShaderData* bufferDataCasted = reinterpret_cast<EnvProbeShaderData*>(bufferData);
    bufferDataCasted->textureIndex = idx;

    gpuBufferHolder->WriteBufferData(idx, bufferDataCasted, size);
}

void OnBindingChanged_AmbientProbe(EnvProbe* envProbe, uint32 prev, uint32 next)
{
    AssertDebug(envProbe != nullptr);
    AssertDebug(envProbe->IsReady());

    AssertDebug(envProbe->GetEnvProbeType() == EPT_AMBIENT);

    // temp shit
    RenderApi_AssignResourceBinding(envProbe, envProbe->GetRenderResource().GetBufferIndex());
}

void OnBindingChanged_EnvGrid(EnvGrid* envGrid, uint32 prev, uint32 next)
{
    AssertDebug(envGrid != nullptr);

    IRenderProxy* proxy = RenderApi_GetRenderProxy(envGrid->Id());
    AssertThrow(proxy != nullptr);

    RenderProxyEnvGrid* proxyCasted = static_cast<RenderProxyEnvGrid*>(proxy);

    // temp shit

    AssertDebug(envGrid->GetRenderResource().GetBufferIndex() != ~0u);
    RenderApi_AssignResourceBinding(envGrid, envGrid->GetRenderResource().GetBufferIndex());

    // if (next != ~0u)
    // {
    switch (envGrid->GetEnvGridType())
    {
    case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
    {
        AssertDebug(envGrid->GetLightFieldIrradianceTexture().IsValid());
        AssertDebug(envGrid->GetLightFieldDepthTexture().IsValid());

        // @TODO: Set based on binding index
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            g_renderGlobalState->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)
                ->SetElement(NAME("LightFieldColorTexture"), envGrid->GetLightFieldIrradianceTexture()->GetRenderResource().GetImageView());

            g_renderGlobalState->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)
                ->SetElement(NAME("LightFieldDepthTexture"), envGrid->GetLightFieldDepthTexture()->GetRenderResource().GetImageView());
        }

        return;
    }
    default:
        break;
    }

    if (envGrid->GetOptions().flags & EnvGridFlags::USE_VOXEL_GRID)
    {
        AssertDebug(envGrid->GetVoxelGridTexture().IsValid());

        // Set our voxel grid texture in the global descriptor set so we can use it in shaders
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            g_renderGlobalState->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)
                ->SetElement(NAME("VoxelGridTexture"), envGrid->GetVoxelGridTexture()->GetRenderResource().GetImageView());
        }
    }
    // }
}

void OnBindingChanged_Light(Light* light, uint32 prev, uint32 next)
{
    AssertDebug(light != nullptr);

    // temp shit
    RenderApi_AssignResourceBinding(light, light->GetRenderResource().GetBufferIndex());
}

void OnBindingChanged_LightmapVolume(LightmapVolume* lightmapVolume, uint32 prev, uint32 next)
{
    RenderApi_AssignResourceBinding(lightmapVolume, next);
}

} // namespace hyperion