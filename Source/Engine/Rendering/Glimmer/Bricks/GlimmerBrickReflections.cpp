/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Bricks/GlimmerBrickReflections.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickWorld.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickCVars.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerBrickReflections("Rendering/GPU/Glimmer/BrickReflections");

static StaticShaderPropertyId s_propReflectionsModeRequests { ShaderProperty(NAME("MODE"), NAME("REQUESTS")) };
static StaticShaderPropertyId s_propReflectionsModeResolve { ShaderProperty(NAME("MODE"), NAME("RESOLVE")) };
static StaticShaderPropertyId s_propReflectionsModeFinish { ShaderProperty(NAME("MODE"), NAME("FINISH")) };
static StaticShaderPropertyId s_propReflectionsModeRays { ShaderProperty(NAME("MODE"), NAME("RAYS")) };
static StaticShaderPropertyId s_propReflectionsModeHistory { ShaderProperty(NAME("MODE"), NAME("HISTORY")) };
static StaticShaderPropertyId s_propReflectionsModeSpread { ShaderProperty(NAME("MODE"), NAME("SPREAD")) };

static constexpr float RayBiasColumns = 0.5f;
static constexpr float LobeAlphas = 3.0f;
static constexpr float RayDistance = 500.0f;
static constexpr float MatteRoughness = 0.65f;
static constexpr float AimAlphas = 2.0f;
static constexpr float Fireflies = 5.0f;
static constexpr float DitherReachColumns = 2.0f;

static constexpr uint32 SlotFrames = 16;
static constexpr uint32 RayFrames = 32;

static constexpr uint32 NumBlocks = 1u << 17;
static constexpr uint32 BlockWords = 10;
static constexpr uint32 NumClaims = 1u << 20;
static constexpr uint32 RequestBudget = 32768;
static constexpr uint32 RequestWords = 4;
static constexpr uint32 NumCounts = 8;
static constexpr uint32 TraceRow = 256;

struct GlimmerBrickReflectionsConstants
{
    GlimmerBrickShaderData bricks;
    Vec4u dimensions; // xy = the view's, zw = the half resolution picture's
    Vec4f params;     // x = columns off its surface a ray starts, y = alphas a bin is wide, z = how far a ray looks, w = roughness from which the reflection is handed back
    Vec4f aim;        // x = alphas across a card is read over along a drawn direction, y = how many times what it has added up a sample may be, z = columns a moving dither reaches
    Vec4u frame;      // x = frame, y = 1 when pixels read slots, z = frames a pixel that reads slots adds up, w = frames one that traces for itself adds up
    Vec4u limits;     // x = crossings, y = blocks, z = requests a frame at most, w = 1 when there is no last frame to read
};

struct ReflectionsPass
{
    const StaticShaderPropertyId* modeProperty = nullptr;
    Vec3u groups;

    bool usesSlots = false;

    Texture* outColor = nullptr;
    Texture* outInfo = nullptr;
    Texture* outGuide = nullptr;

    Texture* color = nullptr;
    Texture* info = nullptr;
    Texture* guide = nullptr;
    Texture* historyColor = nullptr;
    Texture* historyInfo = nullptr;
    Texture* historyGuide = nullptr;
};

#pragma region GlimmerBrickReflections

GlimmerBrickReflections::GlimmerBrickReflections()
    : m_numCrossings(0),
      m_frame(0),
      m_parity(0),
      m_hasHistory(false),
      m_hasRequests(false),
      m_isValid(false)
{
}

GlimmerBrickReflections::~GlimmerBrickReflections()
{
    EnqueueDeletion(std::move(m_blockOfBuffer));
    EnqueueDeletion(std::move(m_blocksBuffer));
    EnqueueDeletion(std::move(m_claimsBuffer));
    EnqueueDeletion(std::move(m_requestsBuffer));
    EnqueueDeletion(std::move(m_wantedBuffer));
    EnqueueDeletion(std::move(m_listBuffer));
    EnqueueDeletion(std::move(m_countsBuffer));
}

const GpuImageViewRef& GlimmerBrickReflections::GetImageView() const
{
    return RI.textureViewCache->GetOrCreate(m_isValid ? m_fullTexture : RI.placeholderData->textureSolidBlack);
}

static GpuBufferRef CreateWordBuffer(size_t numWords, Name name)
{
    GpuBufferRef buffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, MathUtil::Max(numWords, size_t(4)) * sizeof(uint32), alignof(uint32));
    Check(buffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
    buffer->SetDebugName(name);
#endif

    return buffer;
}

void GlimmerBrickReflections::CreateTextures(const Vec2u& extent)
{
    const Vec2u halfExtent = Vec2u((extent.x + 1) / 2, (extent.y + 1) / 2);

    const auto createTexture = [](const Vec2u& textureExtent, Name name)
    {
        return CreateGlimmerStorageTexture(TextureType::Texture2D, TextureFormat::RGBA16F, Vec3u(textureExtent, 1), 1, name, TextureWrapMode::ClampToEdge);
    };

    m_rawColorTexture = createTexture(halfExtent, NAME("GlimmerBrickReflectionsRawColor"));
    m_rawInfoTexture = createTexture(halfExtent, NAME("GlimmerBrickReflectionsRawInfo"));

    for (uint32 index = 0; index < 2; index++)
    {
        m_guideTextures[index] = createTexture(halfExtent, NAME("GlimmerBrickReflectionsGuide"));
        m_keptColorTextures[index] = createTexture(halfExtent, NAME("GlimmerBrickReflectionsKeptColor"));
        m_keptInfoTextures[index] = createTexture(halfExtent, NAME("GlimmerBrickReflectionsKeptInfo"));
    }

    m_fullTexture = createTexture(extent, NAME("GlimmerBrickReflections"));
    m_unusedOutputTexture = createTexture(Vec2u(1, 1), NAME("GlimmerBrickReflectionsUnused"));

    EnqueueDeletion(std::move(m_wantedBuffer));
    EnqueueDeletion(std::move(m_listBuffer));

    const size_t numHalfPixels = size_t(halfExtent.x) * halfExtent.y;

    m_wantedBuffer = CreateWordBuffer(numHalfPixels * RequestWords, NAME("GlimmerBrickSlotWanted"));
    m_listBuffer = CreateWordBuffer(numHalfPixels, NAME("GlimmerBrickSlotList"));

    m_extent = extent;
    m_hasHistory = false;
}

void GlimmerBrickReflections::CreateSlotBuffers(uint32 numCrossings)
{
    EnqueueDeletion(std::move(m_blockOfBuffer));
    EnqueueDeletion(std::move(m_blocksBuffer));
    EnqueueDeletion(std::move(m_claimsBuffer));
    EnqueueDeletion(std::move(m_requestsBuffer));
    EnqueueDeletion(std::move(m_countsBuffer));

    m_blockOfBuffer = CreateWordBuffer(numCrossings, NAME("GlimmerBrickSlotBlockOf"));
    m_blocksBuffer = CreateWordBuffer(size_t(NumBlocks) * BlockWords, NAME("GlimmerBrickSlotBlocks"));
    m_claimsBuffer = CreateWordBuffer(NumClaims, NAME("GlimmerBrickSlotClaims"));
    m_requestsBuffer = CreateWordBuffer(size_t(RequestBudget) * RequestWords, NAME("GlimmerBrickSlotRequests"));
    m_countsBuffer = CreateWordBuffer(NumCounts, NAME("GlimmerBrickSlotCounts"));

    m_numCrossings = numCrossings;
    m_hasRequests = false;
}

void GlimmerBrickReflections::Render(Frame* frame, View* view, GBuffer* gbuffer, const Vec2u& extent, const GlimmerBrickWorld* brickWorld)
{
    HYP_SCOPE;

    m_isValid = false;

    if (!brickWorld || !brickWorld->IsReady() || !g_cvGlimmerBricksReflections.Get() || !view || !view->GetCamera() || extent.x == 0 || extent.y == 0)
    {
        m_hasHistory = false;
        m_hasRequests = false;

        return;
    }

    if (!m_fullTexture.IsValid() || m_extent != extent)
    {
        CreateTextures(extent);
    }

    if (!m_blockOfBuffer.IsValid() || m_numCrossings != brickWorld->GetNumCrossings())
    {
        CreateSlotBuffers(brickWorld->GetNumCrossings());
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickReflections);

    const Vec2u halfExtent = Vec2u((extent.x + 1) / 2, (extent.y + 1) / 2);
    const bool addsUp = g_cvGlimmerBricksReflectionsTemporal.Get();

    GlimmerBrickReflectionsConstants constants {};
    constants.bricks = brickWorld->GetShaderData();
    constants.dimensions = Vec4u(extent.x, extent.y, halfExtent.x, halfExtent.y);
    constants.params = Vec4f(RayBiasColumns, LobeAlphas, RayDistance, MatteRoughness);
    constants.aim = Vec4f(AimAlphas, Fireflies, DitherReachColumns, 0.0f);
    constants.frame = Vec4u(m_frame, g_cvGlimmerBricksReflectionsSlots.Get() ? 1u : 0u, addsUp ? SlotFrames : 0u, addsUp ? RayFrames : 0u);
    constants.limits = Vec4u(m_numCrossings, NumBlocks, RequestBudget, m_hasHistory ? 0u : 1u);

    CommandRecorder& cr = frame->cr;

    const GBufferTarget& opaquePass = gbuffer->GetPass(GBufferPass::Opaque);

    GpuImage* gbufferImages[] = {
        opaquePass.GetAttachment(GBufferTarget::Normals)->GetGpuImage(),
        opaquePass.GetAttachment(GBufferTarget::Depth)->GetGpuImage(),
        opaquePass.GetAttachment(GBufferTarget::MatData)->GetGpuImage()
    };

    for (GpuImage* gbufferImage : gbufferImages)
    {
        cr << InsertBarrier(gbufferImage, ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    GpuBuffer* slotBuffers[] = {
        m_blockOfBuffer.Get(),
        m_blocksBuffer.Get(),
        m_claimsBuffer.Get(),
        m_requestsBuffer.Get(),
        m_wantedBuffer.Get(),
        m_listBuffer.Get(),
        m_countsBuffer.Get()
    };

    const auto dispatchPass = [&](const ReflectionsPass& pass)
    {
        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(*pass.modeProperty);

        for (Texture* output : { pass.outColor, pass.outInfo, pass.outGuide })
        {
            if (output)
            {
                cr << InsertBarrier(output->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
            }
        }

        for (Texture* input : { pass.color, pass.info, pass.guide, pass.historyColor, pass.historyInfo, pass.historyGuide })
        {
            if (input)
            {
                cr << InsertBarrier(input->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
            }
        }

        if (pass.usesSlots)
        {
            for (GpuBuffer* slotBuffer : slotBuffers)
            {
                cr << InsertBarrier(slotBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
            }
        }

        cr << InsertBarrier(m_unusedOutputTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickReflections"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        {
            cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Normals)->GetImageView());
            cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Depth)->GetImageView());
            cr << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, opaquePass.GetAttachment(GBufferTarget::MatData)->GetImageView());
            cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(view->GetCamera()));
        }

        {
            cr << SetShaderUniform(uniformIndex++, "GlimmerBrickTableBuffer"_sh, brickWorld->GetTableBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBrickIndexBuffer"_sh, brickWorld->GetIndexBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingsBuffer"_sh, brickWorld->GetCrossingsBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBrickMaterialBuffer"_sh, brickWorld->GetMaterialBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBrickRadianceBuffer"_sh, brickWorld->GetRadianceBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingLightBuffer"_sh, brickWorld->GetCrossingLightBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        }

        {
            cr << SetShaderUniform(uniformIndex++, "SlotBlockOfBuffer"_sh, m_blockOfBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "SlotBlocksBuffer"_sh, m_blocksBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "SlotClaimsBuffer"_sh, m_claimsBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "SlotRequestsBuffer"_sh, m_requestsBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "SlotWantedBuffer"_sh, m_wantedBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "SlotListBuffer"_sh, m_listBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "SlotCountsBuffer"_sh, m_countsBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        }

        const auto setOutput = [&](StringHash name, Texture* texture)
        {
            cr << SetShaderUniform(uniformIndex++, name, RI.textureViewCache->GetOrCreate(texture ? texture : m_unusedOutputTexture.Get()));
        };

        const auto setInput = [&](StringHash name, Texture* texture)
        {
            cr << SetShaderUniform(uniformIndex++, name, RI.textureViewCache->GetOrCreate(texture ? texture : RI.placeholderData->textureSolidBlack.Get()));
        };

        setOutput("OutColor"_sh, pass.outColor);
        setOutput("OutInfo"_sh, pass.outInfo);
        setOutput("OutGuide"_sh, pass.outGuide);
        setInput("ColorTexture"_sh, pass.color);
        setInput("InfoTexture"_sh, pass.info);
        setInput("GuideTexture"_sh, pass.guide);
        setInput("HistoryColorTexture"_sh, pass.historyColor);
        setInput("HistoryInfoTexture"_sh, pass.historyInfo);
        setInput("HistoryGuideTexture"_sh, pass.historyGuide);

        cr << DispatchCompute(pass.groups);
    };

    const uint32 current = m_parity;
    const uint32 previous = 1 - m_parity;

    const Vec3u halfGroups = Vec3u((halfExtent.x + 7) / 8, (halfExtent.y + 7) / 8, 1);
    const uint32 numHalfPixels = halfExtent.x * halfExtent.y;

    const auto rowGroups = [](uint32 numThreads)
    {
        return Vec3u(TraceRow / 8, MathUtil::Max((numThreads + TraceRow - 1) / TraceRow + 7, 8u) / 8, 1);
    };

    if (m_hasRequests)
    {
        ReflectionsPass requests;
        requests.modeProperty = &s_propReflectionsModeRequests;
        requests.groups = rowGroups(RequestBudget);
        requests.usesSlots = true;

        dispatchPass(requests);
    }

    ReflectionsPass resolve;
    resolve.modeProperty = &s_propReflectionsModeResolve;
    resolve.groups = halfGroups;
    resolve.usesSlots = true;
    resolve.outColor = m_rawColorTexture.Get();
    resolve.outInfo = m_rawInfoTexture.Get();
    resolve.outGuide = m_guideTextures[current].Get();

    dispatchPass(resolve);

    ReflectionsPass finish;
    finish.modeProperty = &s_propReflectionsModeFinish;
    finish.groups = halfGroups;
    finish.usesSlots = true;
    finish.outInfo = m_rawInfoTexture.Get();
    finish.outGuide = m_guideTextures[current].Get();

    dispatchPass(finish);

    ReflectionsPass rays;
    rays.modeProperty = &s_propReflectionsModeRays;
    rays.groups = rowGroups(numHalfPixels);
    rays.usesSlots = true;
    rays.outColor = m_rawColorTexture.Get();
    rays.outInfo = m_rawInfoTexture.Get();

    dispatchPass(rays);

    ReflectionsPass history;
    history.modeProperty = &s_propReflectionsModeHistory;
    history.groups = halfGroups;
    history.outColor = m_keptColorTextures[current].Get();
    history.outInfo = m_keptInfoTextures[current].Get();
    history.color = m_rawColorTexture.Get();
    history.info = m_rawInfoTexture.Get();
    history.guide = m_guideTextures[current].Get();
    history.historyColor = m_keptColorTextures[previous].Get();
    history.historyInfo = m_keptInfoTextures[previous].Get();
    history.historyGuide = m_guideTextures[previous].Get();

    dispatchPass(history);

    ReflectionsPass spread;
    spread.modeProperty = &s_propReflectionsModeSpread;
    spread.groups = Vec3u((extent.x + 7) / 8, (extent.y + 7) / 8, 1);
    spread.outColor = m_fullTexture.Get();
    spread.color = m_keptColorTextures[current].Get();

    dispatchPass(spread);

    cr << InsertBarrier(m_fullTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);

    for (GpuImage* gbufferImage : gbufferImages)
    {
        cr << InsertBarrier(gbufferImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);
    }

    m_frame++;
    m_parity = previous;
    m_hasHistory = true;
    m_hasRequests = true;
    m_isValid = true;
}

#pragma endregion GlimmerBrickReflections

} // namespace Hyperion
