/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/GTAOPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/SamplerCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGTAO("Rendering/GPU/GTAO");

// 0 = low, 1 = medium, 2 = high, 3 = ultra
static CVar<int> s_cvGTAOQuality("Rendering.GTAO.Quality", 2);
static CVar<float> s_cvGTAORadius("Rendering.GTAO.Radius", 1.0f);
static CVar<float> s_cvGTAOPower("Rendering.GTAO.Power", 2.2f);
static CVar<float> s_cvGTAOThinOccluderCompensation("Rendering.GTAO.ThinOccluderCompensation", 0.0f);
static CVar<int> s_cvGTAODenoisePasses("Rendering.GTAO.DenoisePasses", 1);
static CVar<bool> s_cvGTAOTemporal("Rendering.GTAO.Temporal", true);
static CVar<int> s_cvGTAOTemporalFrames("Rendering.GTAO.TemporalFrames", 16);

static constexpr uint32 NumDepthMips = 5;
static constexpr uint32 DepthPrefilterTileSize = 1u << (NumDepthMips - 1);

static constexpr float FalloffRange = 0.615f;
static constexpr float SampleDistributionPower = 2.0f;
static constexpr float DepthMipSamplingOffset = 3.3f;
static constexpr float DenoiseBlurBeta = 1.2f;
static constexpr float MaxScreenRadiusFraction = 0.3f;

// slices, steps per slice side
static const Vec2f QualityPresets[] = {
    Vec2f(1.0f, 2.0f),
    Vec2f(2.0f, 2.0f),
    Vec2f(3.0f, 3.0f),
    Vec2f(9.0f, 3.0f)
};

enum GTAOUniformSlot : uint32
{
    GTAOSlot_CBuffer,
    GTAOSlot_GBufferDepthTexture,
    GTAOSlot_GBufferNormalsTexture,
    GTAOSlot_GBufferVelocityTexture,
    GTAOSlot_SamplerPoint,
    GTAOSlot_ViewDepthTexture,
    GTAOSlot_OutDepthMip0,
    GTAOSlot_OutDepthMip1,
    GTAOSlot_OutDepthMip2,
    GTAOSlot_OutDepthMip3,
    GTAOSlot_OutDepthMip4,
    GTAOSlot_InAO,
    GTAOSlot_InEdges,
    GTAOSlot_HistoryTexture,
    GTAOSlot_OutAO,
    GTAOSlot_OutEdges,
    GTAOSlot_OutHistory
};

struct GTAOConstants
{
    CameraShaderData camera;
    Mat4f previousViewProjection;
    Vec4u dimensions;
    Vec4f ndcToView;
    Vec4f depthUnpack;
    Vec4f params0;
    Vec4f params1;
    Vec4f params2;
    Vec4f viewDepthScale;
};

static Handle<Texture> CreateGTAOTexture(TextureFormat format, const Vec2u& extent, bool hasMips, Name name)
{
    Handle<Texture> texture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        format,
        Vec3u(extent, 1),
        hasMips ? TextureFilterMode::NearestMipmap : TextureFilterMode::Nearest,
        TextureFilterMode::Nearest,
        TextureWrapMode::ClampToEdge,
        1,
        ImageUsage::Storage | ImageUsage::Sampled });

    texture->SetIsTransient(true);
    texture->SetName(name);
    Check(texture->Create());

    return texture;
}

static Vec2u GetViewDepthExtent(const Vec2u& extent)
{
    return Vec2u(
        (extent.x + DepthPrefilterTileSize - 1) / DepthPrefilterTileSize * DepthPrefilterTileSize,
        (extent.y + DepthPrefilterTileSize - 1) / DepthPrefilterTileSize * DepthPrefilterTileSize);
}

GTAOPass::GTAOPass(Vec2u extent, GBuffer* gbuffer)
    : m_extent(extent),
      m_gbuffer(gbuffer),
      m_finalTexture(nullptr),
      m_previousViewProjection(Mat4f::Identity()),
      m_historyIndex(0),
      m_lastRenderedFrame(~0u),
      m_hasHistory(false)
{
}

GTAOPass::~GTAOPass()
{
}

void GTAOPass::Create()
{
}

void GTAOPass::CreateTextures(const Vec2u& extent)
{
    const auto createTexture = [](Handle<Texture>& texture, TextureFormat format, const Vec2u& textureExtent, bool hasMips, Name name)
    {
        if (texture.IsValid())
        {
            EnqueueDeletion(std::move(texture));
        }

        texture = CreateGTAOTexture(format, textureExtent, hasMips, name);
    };

    createTexture(m_viewDepthTexture, TextureFormat::R32F, GetViewDepthExtent(extent), true, NAME("GTAOViewDepth"));

    createTexture(m_aoTextures[0], TextureFormat::R16F, extent, false, NAME("GTAOWorking0"));
    createTexture(m_aoTextures[1], TextureFormat::R16F, extent, false, NAME("GTAOWorking1"));
    createTexture(m_edgesTexture, TextureFormat::RGBA8, extent, false, NAME("GTAOEdges"));

    createTexture(m_historyTextures[0], TextureFormat::RGBA16F, extent, false, NAME("GTAOHistory0"));
    createTexture(m_historyTextures[1], TextureFormat::RGBA16F, extent, false, NAME("GTAOHistory1"));

    m_extent = extent;
    m_finalTexture = nullptr;
    m_hasHistory = false;
}

const GpuImageViewRef& GTAOPass::GetFinalImageView() const
{
    return RI.textureViewCache->GetOrCreate(m_finalTexture ? m_finalTexture : RI.placeholderData->textureSolidWhite.Get());
}

void GTAOPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.view != nullptr);

    m_finalTexture = nullptr;

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));

    if (!cameraProxy)
    {
        m_hasHistory = false;

        return;
    }

    const CameraShaderData& cameraData = cameraProxy->bufferData;
    const Mat4f& projection = cameraData.projMat;

    // the horizon search needs a perspective projection
    if (projection[3][3] != 0.0f)
    {
        m_hasHistory = false;

        return;
    }

    const GBufferTarget& opaquePass = m_gbuffer->GetPass(GBufferPass::Opaque);

    GpuImage* depthImage = opaquePass.GetAttachment(GBufferTarget::Depth)->GetGpuImage();
    GpuImage* normalsImage = opaquePass.GetAttachment(GBufferTarget::Normals)->GetGpuImage();
    GpuImage* velocityImage = opaquePass.GetAttachment(GBufferTarget::Velocity)->GetGpuImage();

    const Vec2u extent = Vec2u(depthImage->GetExtent().x, depthImage->GetExtent().y);

    if (extent.x == 0 || extent.y == 0)
    {
        m_hasHistory = false;

        return;
    }

    if (!m_viewDepthTexture.IsValid() || m_extent != extent)
    {
        CreateTextures(extent);
    }

    if (m_viewDepthTexture->GetTextureDesc().NumMips() < NumDepthMips)
    {
        m_hasHistory = false;

        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGTAO);

    const uint32 frameCounter = GetFrameCounter();
    const bool useTemporal = s_cvGTAOTemporal.Get();
    const bool isHistoryValid = useTemporal && m_hasHistory && m_lastRenderedFrame + 1 == frameCounter;

    const Vec2f qualityPreset = QualityPresets[MathUtil::Clamp(s_cvGTAOQuality.Get(), 0, int(std::size(QualityPresets)) - 1)];
    const uint32 numDenoisePasses = uint32(MathUtil::Clamp(s_cvGTAODenoisePasses.Get(), 0, 4));

    // view xy = (ndc xy - jitter - projection offset) / projection scale * view z, with ndc y flipped from uv
    const float p00 = projection[0][0];
    const float p11 = projection[1][1];
    const float p02 = projection[0][2];
    const float p12 = projection[1][2];

    GTAOConstants constants {};
    constants.camera = cameraData;
    constants.previousViewProjection = isHistoryValid ? m_previousViewProjection : cameraData.viewProjMat;
    constants.dimensions = Vec4u(extent.x, extent.y, frameCounter, isHistoryValid ? 1u : 0u);
    constants.ndcToView = Vec4f(
        2.0f / p00,
        -2.0f / p11,
        (-1.0f - p02 - cameraData.jitter.x) / p00,
        (1.0f - p12 - cameraData.jitter.y) / p11);
    constants.depthUnpack = Vec4f(projection[2][3], projection[2][2], 1.0f / float(extent.x), 1.0f / float(extent.y));
    constants.params0 = Vec4f(
        MathUtil::Max(s_cvGTAORadius.Get(), 0.01f),
        FalloffRange,
        SampleDistributionPower,
        MathUtil::Max(s_cvGTAOThinOccluderCompensation.Get(), 0.0f));
    constants.params1 = Vec4f(MathUtil::Max(s_cvGTAOPower.Get(), 0.01f), DepthMipSamplingOffset, qualityPreset.x, qualityPreset.y);
    constants.params2 = Vec4f(
        DenoiseBlurBeta,
        float(extent.y) * MaxScreenRadiusFraction,
        float(MathUtil::Clamp(s_cvGTAOTemporalFrames.Get(), 1, 64)),
        0.0f);

    const Vec2u viewDepthExtent = GetViewDepthExtent(extent);
    constants.viewDepthScale = Vec4f(float(extent.x) / float(viewDepthExtent.x), float(extent.y) / float(viewDepthExtent.y), 0.0f, 0.0f);

    const auto commitConstants = [&constants](GpuBuffer*& outCbuffer, size_t& outOffset, size_t& outSize)
    {
        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(outCbuffer, outOffset, outSize);
    };

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    commitConstants(cbuffer, cbufferOffset, cbufferSize);

    Sampler* pointSampler = RI.samplerCache->GetOrCreate(SamplerDesc {
        TextureFilterMode::NearestMipmap,
        TextureFilterMode::Nearest,
        TextureWrapMode::ClampToEdge });

    CommandRecorder& cr = frame->cr;

    // drops uniforms bound by earlier passes, which would otherwise be matched by name against these shaders
    cr << SetCurrentFramebuffer(nullptr);

    for (GpuImage* gbufferImage : { depthImage, normalsImage, velocityImage })
    {
        cr << InsertBarrier(gbufferImage, ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    { // view space depth and its mips
        cr << InsertBarrier(m_viewDepthTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GTAOPrefilterDepth")));

        cr << SetShaderUniform(GTAOSlot_CBuffer, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(GTAOSlot_GBufferDepthTexture, "GBufferDepthTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Depth)->GetImageView());
        cr << SetShaderUniform(GTAOSlot_OutDepthMip0, "OutDepthMip0"_sh, RI.textureViewCache->GetOrCreate(m_viewDepthTexture, 0, 1));
        cr << SetShaderUniform(GTAOSlot_OutDepthMip1, "OutDepthMip1"_sh, RI.textureViewCache->GetOrCreate(m_viewDepthTexture, 1, 1));
        cr << SetShaderUniform(GTAOSlot_OutDepthMip2, "OutDepthMip2"_sh, RI.textureViewCache->GetOrCreate(m_viewDepthTexture, 2, 1));
        cr << SetShaderUniform(GTAOSlot_OutDepthMip3, "OutDepthMip3"_sh, RI.textureViewCache->GetOrCreate(m_viewDepthTexture, 3, 1));
        cr << SetShaderUniform(GTAOSlot_OutDepthMip4, "OutDepthMip4"_sh, RI.textureViewCache->GetOrCreate(m_viewDepthTexture, 4, 1));

        cr << DispatchCompute(Vec3u(viewDepthExtent.x / DepthPrefilterTileSize, viewDepthExtent.y / DepthPrefilterTileSize, 1));

        cr << InsertBarrier(m_viewDepthTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    const Vec3u groups = Vec3u((extent.x + 7) / 8, (extent.y + 7) / 8, 1);

    uint32 aoIndex = 0;

    { // horizon search
        cr << InsertBarrier(m_aoTextures[aoIndex]->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_edgesTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GTAOMain")));

        cr << SetShaderUniform(GTAOSlot_CBuffer, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(GTAOSlot_ViewDepthTexture, "ViewDepthTexture"_sh, RI.textureViewCache->GetOrCreate(m_viewDepthTexture));
        cr << SetShaderUniform(GTAOSlot_GBufferNormalsTexture, "GBufferNormalsTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Normals)->GetImageView());
        cr << SetShaderUniform(GTAOSlot_SamplerPoint, "SamplerPoint"_sh, pointSampler);
        cr << SetShaderUniform(GTAOSlot_OutAO, "OutAO"_sh, RI.textureViewCache->GetOrCreate(m_aoTextures[aoIndex]));
        cr << SetShaderUniform(GTAOSlot_OutEdges, "OutEdges"_sh, RI.textureViewCache->GetOrCreate(m_edgesTexture));

        cr << DispatchCompute(groups);

        cr << InsertBarrier(m_aoTextures[aoIndex]->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
        cr << InsertBarrier(m_edgesTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    for (uint32 passIndex = 0; passIndex < numDenoisePasses; passIndex++)
    {
        const uint32 outIndex = aoIndex ^ 1u;

        constants.params2.w = passIndex + 1 == numDenoisePasses ? 1.0f : 0.0f;

        GpuBuffer* denoiseCbuffer = nullptr;
        size_t denoiseCbufferOffset = 0;
        size_t denoiseCbufferSize = 0;

        commitConstants(denoiseCbuffer, denoiseCbufferOffset, denoiseCbufferSize);

        cr << InsertBarrier(m_aoTextures[outIndex]->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GTAODenoise")));

        cr << SetShaderUniform(GTAOSlot_CBuffer, "CBuffer"_sh, denoiseCbuffer, ShaderDataOffset(denoiseCbufferOffset, denoiseCbufferSize));
        cr << SetShaderUniform(GTAOSlot_InAO, "InAO"_sh, RI.textureViewCache->GetOrCreate(m_aoTextures[aoIndex]));
        cr << SetShaderUniform(GTAOSlot_InEdges, "InEdges"_sh, RI.textureViewCache->GetOrCreate(m_edgesTexture));
        cr << SetShaderUniform(GTAOSlot_OutAO, "OutAO"_sh, RI.textureViewCache->GetOrCreate(m_aoTextures[outIndex]));

        cr << DispatchCompute(groups);

        cr << InsertBarrier(m_aoTextures[outIndex]->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);

        aoIndex = outIndex;
    }

    if (useTemporal)
    {
        const uint32 previousIndex = m_historyIndex;
        const uint32 nextIndex = m_historyIndex ^ 1u;

        Texture* outHistoryTexture = m_historyTextures[nextIndex].Get();

        cr << InsertBarrier(m_historyTextures[previousIndex]->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
        cr << InsertBarrier(outHistoryTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GTAOTemporal")));

        cr << SetShaderUniform(GTAOSlot_CBuffer, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(GTAOSlot_InAO, "InAO"_sh, RI.textureViewCache->GetOrCreate(m_aoTextures[aoIndex]));
        cr << SetShaderUniform(GTAOSlot_ViewDepthTexture, "ViewDepthTexture"_sh, RI.textureViewCache->GetOrCreate(m_viewDepthTexture));
        cr << SetShaderUniform(GTAOSlot_GBufferVelocityTexture, "GBufferVelocityTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Velocity)->GetImageView());
        cr << SetShaderUniform(GTAOSlot_HistoryTexture, "HistoryTexture"_sh, RI.textureViewCache->GetOrCreate(m_historyTextures[previousIndex]));
        cr << SetShaderUniform(GTAOSlot_OutHistory, "OutHistory"_sh, RI.textureViewCache->GetOrCreate(outHistoryTexture));

        cr << DispatchCompute(groups);

        m_historyIndex = nextIndex;
        m_finalTexture = outHistoryTexture;
    }
    else
    {
        m_finalTexture = m_aoTextures[aoIndex].Get();
    }

    cr << InsertBarrier(m_finalTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);

    for (GpuImage* gbufferImage : { depthImage, normalsImage, velocityImage })
    {
        cr << InsertBarrier(gbufferImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);
    }

    cr << SetCurrentFramebuffer(nullptr);

    m_previousViewProjection = cameraData.viewProjMat;
    m_lastRenderedFrame = frameCounter;
    m_hasHistory = useTemporal;
}

} // namespace Hyperion
