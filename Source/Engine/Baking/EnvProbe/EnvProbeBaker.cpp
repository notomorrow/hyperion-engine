/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/Lightmaps/LightmapPathTraceGpu.hpp>

#include <Baking/EnvProbe/EnvProbeBaker.hpp>
#include <Baking/EnvProbe/EnvProbeBakeJob.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/Passes/EnvProbePass.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Scene/EnvProbe.hpp>

#include <Scene/Swatch.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {

extern ENGINE_API const FilePath& GetTempDirectory();

namespace EnvProbeHelpers {

void ConvolveEnvProbeCubemap(
    const Handle<Texture>& inTexture,
    const Handle<Texture>& outTexture,
    const EnvProbe& envProbe);

void ComputeEnvProbeSphericalHarmonics(
    const EnvProbe& envProbe,
    const Texture& inColorTexture,
    Name swatchName = Name::Invalid());

} // namespace EnvProbeHelpers

namespace Baking {

Baker<EnvProbe>::Baker(BakerConfig&& config, BakeLayer& bakeLayer, const Handle<EnvProbe>& envProbe)
    : BakerBase(std::move(config), bakeLayer, envProbe, MakeStrongRef(envProbe->GetScene()), BoundingBox::Empty()),
      m_envProbe(envProbe)
{
}

Name Baker<EnvProbe>::GetBakeLayerName() const
{
    return m_bakeLayer ? m_bakeLayer->name : g_defaultSwatchName;
}

UniquePtr<BakeJobBase> Baker<EnvProbe>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<EnvProbe>>(std::move(params), m_envProbe, &m_bakeData);
}

void Baker<EnvProbe>::CreateLightmapRenderers()
{
    m_pathTracers.Clear();

    if (!PerformsRayTracing())
    {
        return;
    }

    const uint32 shadingTypesMask = GetShadingTypesMask();
    const uint32 maxTexelsPerFrame = MaxTexelsPerFrame();
    AssertDebug(maxTexelsPerFrame > 0);

    for (uint32 i = 0; i < uint32(LightmapShadingType::MAX); i++)
    {
        if (!(shadingTypesMask & (1u << i)))
        {
            continue;
        }

        const UniquePtr<PathTracer>& pathTracer = m_pathTracers.PushBack(CreatePathTracer(LightmapShadingType(i), maxTexelsPerFrame));

        if (!pathTracer)
        {
            m_pathTracers.PopBack();
            
            continue;
        }

        pathTracer->Create();
    }
}

Result Baker<EnvProbe>::Build_Internal()
{
    Assert(m_envProbe != nullptr);

    InitObject(m_envProbe);

    m_envProbe->SetDimensions(EnvProbe::GetDefaultDimensions(m_envProbe->GetEnvProbeType()));

    m_bakeData = BakeData<EnvProbe>(m_bakeEntities, m_envProbe.Get());

    if (!PerformsRayTracing())
    {
        return {};
    }

    return m_bakeData.Build();
}

void Baker<EnvProbe>::OnCompleted_Internal()
{
    HYP_SCOPE;

    // This is all PT only stuff (raster does its own stuff, see EnvProbePass.cpp)
    if (!PerformsRayTracing())
    {
        return;
    }

    AssertDebug(m_bakeData.IsBuilt());
    if (!m_bakeData.IsBuilt())
    {
        HYP_LOG(Lightmap, Warning, "Lightmap data for PT EnvProbe {} is not built! shouldn't get here", m_envProbe->Id());
        return;
    }

    // prevent writing on other threads
    auto envProbeWriteScope = TUniqueResLock<EnvProbe>(*m_envProbe);

    const Vec2u dimensions = Vec2u(uint32(m_envProbe->GetDimensions()));
    AssertDebug(dimensions.Volume() > 0);

    // Convert lightmap data to bitmaps (6 faces stacked vertically)
    BakeData<EnvProbe>::BitmapType bitmap = m_bakeData.ToBitmap();

    TextureDesc desc {
        TextureType::Cubemap,
        bitmap.GetFormat(),
        Vec3u { dimensions, 1 },
        TextureFilterMode::LinearMipmap,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge
    };

    ByteBuffer buffer = ByteBuffer(bitmap.ToByteView());

    //// temp
    FileByteWriter tempWriter(EngineGlobals::GetTempDirectory() / "TempEnvProbe.bmp");
    bitmap.Write(&tempWriter);
    tempWriter.Close();

    bitmap = {};

    Texture::GenerateMipmaps(desc, buffer);

    Handle<Texture> bakedTexture = MakeHandle<Texture>(desc, buffer.ToByteView());

    buffer.Clear();

    const Name bakeLayerName = GetBakeLayerName();

    // Ambient probes don't save their texture; it is transient,
    // only used for calc'ing SH
    if (m_envProbe->IsAmbientProbe())
    {
        bakedTexture->SetIsTransient(true);
    }
    else if (IsDefaultSwatch(bakeLayerName))
    {
        // Other swatches get their name (and asset registration) from SetBakedTextureForSwatch.
        bakedTexture->SetName(NAME_FMT("{}_ColorMap", m_envProbe->GetName()));

        GetCurrentAssetRegistry()->PutAssetUnique(bakedTexture);
    }

    Check(bakedTexture->Create());

    m_envProbe->SetBakedTextureForSwatch(bakedTexture, bakeLayerName);

    // Bake visibility texture
    if (m_envProbe->GetEnvProbeFlags() & EPF_VISIBILITY)
    {
        BakeData<EnvProbe>::VisibilityBitmapType visBitmap = m_bakeData.ToVisibilityBitmap();

        TextureDesc visDesc {
            TextureType::Cubemap,
            visBitmap.GetFormat(),
            Vec3u {
                EnvProbe::VisibilityTextureDimensions,
                EnvProbe::VisibilityTextureDimensions,
                1 },
            TextureFilterMode::Linear,
            TextureFilterMode::Linear,
            TextureWrapMode::ClampToEdge,
            1,
            ImageUsage::Sampled | ImageUsage::Storage
        };

        ByteBuffer visBuffer = ByteBuffer(visBitmap.ToByteView());

        visBitmap = {};

        Handle<Texture> visibilityTexture = MakeHandle<Texture>(visDesc, visBuffer.ToByteView());

        visBuffer.Clear();

        if (IsDefaultSwatch(bakeLayerName))
        {
            visibilityTexture->SetName(NAME_FMT("{}_VisibilityMap", m_envProbe->GetName()));
        }
        else
        {
            visibilityTexture->SetName(NAME_FMT("{}_{}_VisibilityMap", m_envProbe->GetName(), bakeLayerName));
        }

        // SetVisibilityTexture handles Create() and asset registration; SetVisibilityTextureForSwatch
        // handles asset registration for non-Default swatches.
        m_envProbe->SetVisibilityTextureForSwatch(visibilityTexture, bakeLayerName);
    }

    // Convolves the env probe cubemap and computes SH coefficients on the GPU
    class PostProcessEnvProbe : public CmdBase
    {
    public:
        struct Payload
        {
            HYP_DEF_POOL_NEW_DELETE(g_renderPool);

            Handle<EnvProbe> envProbe;
            Name swatchName;
            BakeData<EnvProbe>::HitMaskBitmapType hitMaskBitmap;
        };

        Payload* payload;

        explicit PostProcessEnvProbe(Payload* payload)
            : payload(payload)
        {
        }

        static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
        {
            PostProcessEnvProbe* cmdCasted = static_cast<PostProcessEnvProbe*>(cmd);
            HYP_DEFER({ delete cmdCasted->payload; });

            const Handle<EnvProbe>& envProbe = cmdCasted->payload->envProbe;

            auto envProbeWriteScope = TUniqueResLock<EnvProbe>(*envProbe);

            const Handle<Texture>& texture = envProbe->GetBakedTextureForSwatch(cmdCasted->payload->swatchName);
            Assert(texture.IsValid() && texture->IsCreated());

            if (!texture->IsCreated())
            {
                // Must be in created state, or we'll crash (gpu image can't be null)
                if (!Check(texture->Create()))
                {
                    HYP_LOG(Lightmap, Error, "Texture creation failed, cannot bake probe");
                    return;
                }
            }

            if (envProbe->ShouldComputePrefilteredEnvMap())
            {
                // Path traced bakes convolve in place into the just-created baked texture (there
                // is no raster capture state on this path).
                EnvProbeHelpers::ConvolveEnvProbeCubemap(texture, texture, *envProbe);
            }

            if (envProbe->ShouldComputeSphericalHarmonics())
            {
                EnvProbeHelpers::ComputeEnvProbeSphericalHarmonics(*envProbe, *texture, cmdCasted->payload->swatchName);
            }
            
            if (envProbe->ShouldCreateHitMask())
            {
                SphericalHarmonicsData hitMaskSH = ComputeSphericalHarmonicsCubemap<TextureFormat::R8, false>(cmdCasted->payload->hitMaskBitmap);

                Vec4f hitMaskData;
                hitMaskData[0] = hitMaskSH.GetOrder0().x;

                const FixedArray<Vec3f, 3> order1 = hitMaskSH.GetOrder1();
                hitMaskData[1] = order1[0][0];
                hitMaskData[2] = order1[1][0];
                hitMaskData[3] = order1[2][0];

                envProbe->SetHitMaskData(hitMaskData);
            }

            HYP_LOG(Lightmap, Verbose, "EnvProbe {} lightmap baking complete", envProbe->GetName());
        }
    };

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();
    cr << PostProcessEnvProbe(new PostProcessEnvProbe::Payload { m_envProbe, bakeLayerName, m_bakeData.ToHitMaskBitmap() });
    cr.Done();

    m_envProbe->Invalidate(true);
}

} // namespace Baking
} // namespace Hyperion
