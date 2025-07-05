/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Defines.hpp>

#include <rendering/Deferred.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/SSRRenderer.hpp>
#include <rendering/SSGI.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/rt/RaytracingReflections.hpp>
#include <rendering/rt/DDGI.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/World.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/Texture.hpp>
#include <scene/View.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <system/AppContext.hpp>

#include <util/Float16.hpp>

#include <util/MeshBuilder.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

static constexpr TextureFormat envGridRadianceFormat = TF_RGBA8;
static constexpr TextureFormat envGridIrradianceFormat = TF_R11G11B10F;

static constexpr TextureFormat envGridPassFormats[EGPM_MAX] = {
    envGridRadianceFormat,  // EGPM_RADIANCE
    envGridIrradianceFormat // EGPM_IRRADIANCE
};

static const Float16 s_ltcMatrix[] = {
#include <rendering/inl/LTCMatrix.inl>
};

static_assert(sizeof(s_ltcMatrix) == 64 * 64 * 4 * 2, "Invalid LTC matrix size");

static const Float16 s_ltcBrdf[] = {
#include <rendering/inl/LTCBRDF.inl>
};

static_assert(sizeof(s_ltcBrdf) == 64 * 64 * 4 * 2, "Invalid LTC BRDF size");

void GetDeferredShaderProperties(ShaderProperties& outShaderProperties)
{
    ShaderProperties properties;

    properties.Set("RT_REFLECTIONS_ENABLED", g_renderBackend->GetRenderConfig().IsRaytracingSupported() && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool());
    properties.Set("RT_GI_ENABLED", g_renderBackend->GetRenderConfig().IsRaytracingSupported() && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.gi.enabled").ToBool());
    properties.Set("ENV_GRID_ENABLED", g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.enabled").ToBool());
    properties.Set("HBIL_ENABLED", g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbil.enabled").ToBool());
    properties.Set("HBAO_ENABLED", g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbao.enabled").ToBool());

    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.debug.reflections").ToBool())
    {
        properties.Set("DEBUG_REFLECTIONS");
    }
    else if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.debug.irradiance").ToBool())
    {
        properties.Set("DEBUG_IRRADIANCE");
    }

    outShaderProperties = std::move(properties);
}

static constexpr TypeId g_envProbeTypeToTypeId[EPT_MAX] = {
    TypeId::ForType<SkyProbe>(),        // EPT_SKY
    TypeId::ForType<ReflectionProbe>(), // EPT_REFLECTION
    TypeId::ForType<EnvProbe>(),        // EPT_SHADOW (fixme when derived class)
    TypeId::ForType<EnvProbe>()         // EPT_AMBIENT (fixme when derived class)
};

#pragma region Deferred pass

DeferredPass::DeferredPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TF_RGBA16F, extent, gbuffer),
      m_mode(mode)
{
    SetBlendFunction(BlendFunction::Additive());
}

DeferredPass::~DeferredPass()
{
    SafeRelease(std::move(m_ltcSampler));
}

void DeferredPass::Create()
{
    CreateShader();

    FullScreenPass::Create();
}

void DeferredPass::CreateShader()
{
    static const FixedArray<ShaderProperties, LT_MAX> lightTypeProperties {
        ShaderProperties { { "LIGHT_TYPE_DIRECTIONAL" } },
        ShaderProperties { { "LIGHT_TYPE_POINT" } },
        ShaderProperties { { "LIGHT_TYPE_SPOT" } },
        ShaderProperties { { "LIGHT_TYPE_AREA_RECT" } }
    };

    switch (m_mode)
    {
    case DeferredPassMode::INDIRECT_LIGHTING:
    {
        ShaderProperties shaderProperties;
        GetDeferredShaderProperties(shaderProperties);

        m_shader = g_shaderManager->GetOrCreate(NAME("DeferredIndirect"), shaderProperties);
        Assert(m_shader.IsValid());

        break;
    }
    case DeferredPassMode::DIRECT_LIGHTING:
        for (uint32 i = 0; i < LT_MAX; i++)
        {
            ShaderProperties shaderProperties;
            GetDeferredShaderProperties(shaderProperties);

            shaderProperties.Merge(lightTypeProperties[i]);

            m_directLightShaders[i] = g_shaderManager->GetOrCreate(NAME("DeferredDirect"), shaderProperties);

            Assert(m_directLightShaders[i].IsValid());
        }

        break;
    default:
        HYP_FAIL("Invalid deferred pass mode");
    }
}

void DeferredPass::CreatePipeline(const RenderableAttributeSet& renderableAttributes)
{
    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING)
    {
        FullScreenPass::CreatePipeline(renderableAttributes);
        return;
    }

    { // linear transform cosines texture data
        m_ltcSampler = g_renderBackend->MakeSampler(
            TFM_NEAREST,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE);

        DeferCreate(m_ltcSampler);

        ByteBuffer ltcMatrixData(sizeof(s_ltcMatrix), s_ltcMatrix);

        m_ltcMatrixTexture = CreateObject<Texture>(TextureData {
            TextureDesc {
                TT_TEX2D,
                TF_RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE },
            std::move(ltcMatrixData) });

        InitObject(m_ltcMatrixTexture);

        m_ltcMatrixTexture->SetPersistentRenderResourceEnabled(true);

        ByteBuffer ltcBrdfData(sizeof(s_ltcBrdf), s_ltcBrdf);

        m_ltcBrdfTexture = CreateObject<Texture>(TextureData {
            TextureDesc {
                TT_TEX2D,
                TF_RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE },
            std::move(ltcBrdfData) });

        InitObject(m_ltcBrdfTexture);

        m_ltcBrdfTexture->SetPersistentRenderResourceEnabled(true);
    }

    for (uint32 i = 0; i < LT_MAX; i++)
    {
        ShaderRef& shader = m_directLightShaders[i];
        Assert(shader.IsValid());

        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frameIndex);
            Assert(descriptorSet.IsValid());

            descriptorSet->SetElement(NAME("MaterialsBuffer"), g_renderGlobalState->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));

            descriptorSet->SetElement(NAME("LTCSampler"), m_ltcSampler);
            descriptorSet->SetElement(NAME("LTCMatrixTexture"), m_ltcMatrixTexture->GetRenderResource().GetImageView());
            descriptorSet->SetElement(NAME("LTCBRDFTexture"), m_ltcBrdfTexture->GetRenderResource().GetImageView());
        }

        DeferCreate(descriptorTable);

        GraphicsPipelineRef graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
            shader,
            descriptorTable,
            { &m_framebuffer, 1 },
            renderableAttributes);

        m_directLightGraphicsPipelines[i] = graphicsPipeline;

        if (i == 0)
        {
            m_graphicsPipeline = graphicsPipeline;
        }
    }
}

void DeferredPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void DeferredPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
    AssertDebug(rs.passData != nullptr);

    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING)
    {
        RenderToFramebuffer(frame, rs, nullptr);

        return;
    }

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(rs.view->GetView());

    // no lights bound, do not render direct shading at all
    if (rpl.lights.NumCurrent() == 0)
    {
        return;
    }

    static const bool useBindlessTextures = g_renderBackend->GetRenderConfig().IsBindlessSupported();

    // render with each light
    for (uint32 lightTypeIndex = 0; lightTypeIndex < LT_MAX; lightTypeIndex++)
    {
        const LightType lightType = LightType(lightTypeIndex);

        bool isFirstLight = true;

        for (Light* light : rpl.lights)
        {
            if (light->GetLightType() != lightTypeIndex)
            {
                continue;
            }

            const GraphicsPipelineRef& pipeline = m_directLightGraphicsPipelines[lightTypeIndex];

            const uint32 globalDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
            const uint32 viewDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));
            const uint32 materialDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
            const uint32 deferredDirectDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("DeferredDirectDescriptorSet"));

            if (isFirstLight)
            {
                pipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

                frame->GetCommandList().Add<BindGraphicsPipeline>(pipeline);

                // Bind textures globally (bindless)
                if (materialDescriptorSetIndex != ~0u && useBindlessTextures)
                {
                    frame->GetCommandList().Add<BindDescriptorSet>(
                        pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame->GetFrameIndex()),
                        pipeline,
                        ArrayMap<Name, uint32> {},
                        materialDescriptorSetIndex);
                }

                if (deferredDirectDescriptorSetIndex != ~0u)
                {
                    frame->GetCommandList().Add<BindDescriptorSet>(
                        pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frame->GetFrameIndex()),
                        pipeline,
                        ArrayMap<Name, uint32> {},
                        deferredDirectDescriptorSetIndex);
                }

                isFirstLight = false;
            }

            frame->GetCommandList().Add<BindDescriptorSet>(
                pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame->GetFrameIndex()),
                pipeline,
                ArrayMap<Name, uint32> {
                    { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*rs.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*rs.view->GetCamera()) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(light, 0) } },
                globalDescriptorSetIndex);

            frame->GetCommandList().Add<BindDescriptorSet>(
                rs.passData->descriptorSets[frame->GetFrameIndex()],
                pipeline,
                ArrayMap<Name, uint32> {},
                viewDescriptorSetIndex);

            // Bind material descriptor set (for area lights)
            if (materialDescriptorSetIndex != ~0u && !useBindlessTextures)
            {
                const DescriptorSetRef& materialDescriptorSet = g_renderGlobalState->materialDescriptorSetManager->ForBoundMaterial(light->GetMaterial(), frame->GetFrameIndex());

                frame->GetCommandList().Add<BindDescriptorSet>(
                    materialDescriptorSet,
                    pipeline,
                    ArrayMap<Name, uint32> {},
                    materialDescriptorSetIndex);
            }

            m_fullScreenQuad->GetRenderResource().Render(frame->GetCommandList());
        }
    }
}

#pragma endregion Deferred pass

#pragma region TonemapPass

TonemapPass::TonemapPass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TF_R11G11B10F, extent, gbuffer)
{
}

TonemapPass::~TonemapPass()
{
}

void TonemapPass::Create()
{
    Threads::AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void TonemapPass::CreatePipeline()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    RenderableAttributeSet renderableAttributes(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes },
        MaterialAttributes {
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction::None(),
            .flags = MAF_NONE });

    m_shader = g_shaderManager->GetOrCreate(NAME("Tonemap"));

    FullScreenPass::CreatePipeline(renderableAttributes);
}

void TonemapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void TonemapPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    FullScreenPass::Render(frame, rs);
}

#pragma endregion TonemapPass

#pragma region TonemapPass

LightmapPass::LightmapPass(const FramebufferRef& framebuffer, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(
          ShaderRef::Null(),
          DescriptorTableRef::Null(),
          framebuffer,
          TF_RGBA8,
          extent,
          gbuffer)
{
}

LightmapPass::~LightmapPass()
{
}

void LightmapPass::Create()
{
    Threads::AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void LightmapPass::CreatePipeline()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    RenderableAttributeSet renderableAttributes(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes },
        MaterialAttributes {
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction(
                BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
                BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA),
            .flags = MAF_NONE });

    m_shader = g_shaderManager->GetOrCreate(NAME("ApplyLightmap"));

    FullScreenPass::CreatePipeline(renderableAttributes);
}

void LightmapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void LightmapPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    FullScreenPass::Render(frame, rs);
}

#pragma endregion LightmapPass

#pragma region Env grid pass

static EnvGridApplyMode EnvGridTypeToApplyEnvGridMode(EnvGridType type)
{
    switch (type)
    {
    case EnvGridType::ENV_GRID_TYPE_SH:
        return EGAM_SH;
    case EnvGridType::ENV_GRID_TYPE_VOXEL:
        return EGAM_VOXEL;
    case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
        return EGAM_LIGHT_FIELD;
    default:
        HYP_FAIL("Invalid EnvGridType");
    }
}

EnvGridPass::EnvGridPass(EnvGridPassMode mode, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(envGridPassFormats[mode], extent, gbuffer),
      m_mode(mode),
      m_isFirstFrame(true)
{
    if (mode == EGPM_RADIANCE)
    {
        SetBlendFunction(BlendFunction(
            BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
            BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));
    }
}

EnvGridPass::~EnvGridPass()
{
}

void EnvGridPass::Create()
{
    Threads::AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void EnvGridPass::CreatePipeline()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    RenderableAttributeSet renderableAttributes(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes },
        MaterialAttributes {
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
                BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA),
            .flags = MAF_NONE });

    if (m_mode == EGPM_RADIANCE)
    {
        m_shader = g_shaderManager->GetOrCreate(NAME("ApplyEnvGrid"), ShaderProperties { { "MODE_RADIANCE" } });

        FullScreenPass::CreatePipeline(renderableAttributes);

        return;
    }

    static const FixedArray<Pair<EnvGridApplyMode, ShaderProperties>, EGAM_MAX> applyEnvGridPasses = {
        Pair<EnvGridApplyMode, ShaderProperties> { EGAM_SH, ShaderProperties { { "MODE_IRRADIANCE", "IRRADIANCE_MODE_SH" } } },
        Pair<EnvGridApplyMode, ShaderProperties> { EGAM_VOXEL, ShaderProperties { { "MODE_IRRADIANCE", "IRRADIANCE_MODE_VOXEL" } } },
        Pair<EnvGridApplyMode, ShaderProperties> { EGAM_LIGHT_FIELD, ShaderProperties { { "MODE_IRRADIANCE", "IRRADIANCE_MODE_LIGHT_FIELD" } } }
    };

    for (const auto& it : applyEnvGridPasses)
    {
        ShaderRef shader = g_shaderManager->GetOrCreate(NAME("ApplyEnvGrid"), it.second);
        Assert(shader.IsValid());

        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
        DeferCreate(descriptorTable);

        GraphicsPipelineRef graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
            shader,
            descriptorTable,
            { &m_framebuffer, 1 },
            renderableAttributes);

        m_graphicsPipelines[uint32(it.first)] = std::move(graphicsPipeline);
    }

    m_graphicsPipeline = m_graphicsPipelines[EGAM_SH];
}

void EnvGridPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

HYP_DISABLE_OPTIMIZATION;
void EnvGridPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
    AssertDebug(rs.passData != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(rs.view->GetView());

    // shouldn't be called if no env grids are present
    AssertDebug(rpl.envGrids.NumCurrent() != 0);

    const uint32 frameIndex = frame->GetFrameIndex();

    frame->GetCommandList().Add<BeginFramebuffer>(m_framebuffer, frameIndex);

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_renderTextureToScreenPass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, rs);
    }

    for (EnvGrid* envGrid : rpl.envGrids)
    {
        const GraphicsPipelineRef& graphicsPipeline = m_mode == EGPM_RADIANCE
            ? m_graphicsPipeline
            : m_graphicsPipelines[EnvGridTypeToApplyEnvGridMode(envGrid->GetEnvGridType())];

        Assert(graphicsPipeline.IsValid());

        const uint32 globalDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        graphicsPipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

        if (ShouldRenderHalfRes())
        {
            const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (rs.world->GetBufferData().frameCounter & 1);
            const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

            frame->GetCommandList().Add<BindGraphicsPipeline>(graphicsPipeline, viewportOffset, viewportExtent);
        }
        else
        {
            frame->GetCommandList().Add<BindGraphicsPipeline>(graphicsPipeline);
        }

        frame->GetCommandList().Add<BindDescriptorSet>(
            graphicsPipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frameIndex),
            graphicsPipeline,
            ArrayMap<Name, uint32> {
                { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*rs.world) },
                { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*rs.view->GetCamera()) },
                { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid, 0) } },
            globalDescriptorSetIndex);

        frame->GetCommandList().Add<BindDescriptorSet>(
            rs.passData->descriptorSets[frameIndex],
            graphicsPipeline,
            ArrayMap<Name, uint32> {},
            viewDescriptorSetIndex);

        m_fullScreenQuad->GetRenderResource().Render(frame->GetCommandList());
    }

    frame->GetCommandList().Add<EndFramebuffer>(m_framebuffer, frameIndex);

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, rs);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, rs);
        }

        m_temporalBlending->Render(frame, rs);
    }

    m_isFirstFrame = false;
}
HYP_ENABLE_OPTIMIZATION;

#pragma endregion Env grid pass

#pragma region ReflectionsPass

ReflectionsPass::ReflectionsPass(Vec2u extent, GBuffer* gbuffer, const ImageViewRef& mipChainImageView, const ImageViewRef& deferredResultImageView)
    : FullScreenPass(TF_R10G10B10A2, extent, gbuffer),
      m_mipChainImageView(mipChainImageView),
      m_deferredResultImageView(deferredResultImageView),
      m_isFirstFrame(true)
{
    SetBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));
}

ReflectionsPass::~ReflectionsPass()
{
    m_ssrRenderer.Reset();
}

void ReflectionsPass::Create()
{
    HYP_SCOPE;

    FullScreenPass::Create();

    CreateSSRRenderer();
}

void ReflectionsPass::CreatePipeline()
{
    HYP_SCOPE;

    CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes },
        MaterialAttributes {
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
                BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA),
            .flags = MAF_NONE }));
}

void ReflectionsPass::CreatePipeline(const RenderableAttributeSet& renderableAttributes)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // Default pass type (non parallax corrected)

    static const FixedArray<Pair<ApplyReflectionProbeMode, ShaderProperties>, ApplyReflectionProbeMode::MAX> applyReflectionProbePasses = {
        Pair<ApplyReflectionProbeMode, ShaderProperties> { ApplyReflectionProbeMode::DEFAULT, ShaderProperties {} },
        Pair<ApplyReflectionProbeMode, ShaderProperties> { ApplyReflectionProbeMode::PARALLAX_CORRECTED, ShaderProperties { { "ENV_PROBE_PARALLAX_CORRECTED" } } }
    };

    for (const auto& it : applyReflectionProbePasses)
    {
        ShaderRef shader = g_shaderManager->GetOrCreate(
            NAME("ApplyReflectionProbe"),
            it.second);

        Assert(shader.IsValid());

        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
        DeferCreate(descriptorTable);

        GraphicsPipelineRef graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
            shader,
            descriptorTable,
            { &m_framebuffer, 1 },
            renderableAttributes);

        m_graphicsPipelines[it.first] = std::move(graphicsPipeline);
    }

    m_graphicsPipeline = m_graphicsPipelines[ApplyReflectionProbeMode::DEFAULT];
}

bool ReflectionsPass::ShouldRenderSSR() const
{
    return g_engine->GetAppContext()->GetConfiguration().Get("rendering.ssr.enabled").ToBool(true)
        && !g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool(false);
}

void ReflectionsPass::CreateSSRRenderer()
{
    m_ssrRenderer = MakeUnique<SSRRenderer>(SSRRendererConfig::FromConfig(), m_gbuffer, m_mipChainImageView, m_deferredResultImageView);
    m_ssrRenderer->Create();

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen"));
    Assert(renderTextureToScreenShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("InTexture"), m_ssrRenderer->GetFinalResultTexture()->GetRenderResource().GetImageView());
    }

    DeferCreate(descriptorTable);

    m_renderSsrToScreenPass = MakeUnique<FullScreenPass>(
        renderTextureToScreenShader,
        std::move(descriptorTable),
        m_imageFormat,
        m_extent,
        m_gbuffer);

    // Use alpha blending to blend SSR into the reflection probes
    m_renderSsrToScreenPass->SetBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

    m_renderSsrToScreenPass->Create();
}

void ReflectionsPass::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;

    FullScreenPass::Resize_Internal(newSize);

    if (ShouldRenderSSR())
    {
        CreateSSRRenderer();
    }
}

void ReflectionsPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
    AssertDebug(rs.passData != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(rs.view->GetView());

    if (ShouldRenderSSR())
    {
        m_ssrRenderer->Render(frame, rs);
    }

    // Sky renders first
    static const FixedArray<EnvProbeType, ApplyReflectionProbeMode::MAX> reflectionProbeTypes {
        EPT_SKY,
        EPT_REFLECTION
    };

    static const FixedArray<ApplyReflectionProbeMode, ApplyReflectionProbeMode::MAX> reflectionProbeModes {
        ApplyReflectionProbeMode::DEFAULT,           // ENV_PROBE_TYPE_SKY
        ApplyReflectionProbeMode::PARALLAX_CORRECTED // ENV_PROBE_TYPE_REFLECTION
    };

    FixedArray<Pair<GraphicsPipelineRef*, Array<EnvProbe*>>, ApplyReflectionProbeMode::MAX> passPtrs;

    for (uint32 modeIndex = ApplyReflectionProbeMode::DEFAULT; modeIndex < ApplyReflectionProbeMode::MAX; modeIndex++)
    {
        passPtrs[modeIndex] = { &m_graphicsPipelines[modeIndex], {} };

        const EnvProbeType envProbeType = reflectionProbeTypes[modeIndex];

        for (EnvProbe* envProbe : rpl.envProbes.GetElements(g_envProbeTypeToTypeId[envProbeType]))
        {
            passPtrs[modeIndex].second.PushBack(envProbe);
        }
    }

    frame->GetCommandList().Add<BeginFramebuffer>(GetFramebuffer(), frameIndex);

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_renderTextureToScreenPass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, rs);
    }

    uint32 numRenderedEnvProbes = 0;

    for (uint32 reflectionProbeTypeIndex = 0; reflectionProbeTypeIndex < ArraySize(reflectionProbeTypes); reflectionProbeTypeIndex++)
    {
        const EnvProbeType envProbeType = reflectionProbeTypes[reflectionProbeTypeIndex];
        const ApplyReflectionProbeMode mode = reflectionProbeModes[reflectionProbeTypeIndex];

        const Pair<GraphicsPipelineRef*, Array<EnvProbe*>>& it = passPtrs[mode];

        if (it.second.Empty())
        {
            continue;
        }

        const GraphicsPipelineRef& graphicsPipeline = *it.first;
        const Array<EnvProbe*>& envProbes = it.second;

        graphicsPipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

        if (ShouldRenderHalfRes())
        {
            const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (rs.world->GetBufferData().frameCounter & 1);
            const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

            frame->GetCommandList().Add<BindGraphicsPipeline>(graphicsPipeline, viewportOffset, viewportExtent);
        }
        else
        {
            frame->GetCommandList().Add<BindGraphicsPipeline>(graphicsPipeline);
        }

        const uint32 globalDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        for (EnvProbe* envProbe : envProbes)
        {
            if (numRenderedEnvProbes >= maxBoundReflectionProbes)
            {
                HYP_LOG(Rendering, Warning, "Attempting to render too many reflection probes.");

                break;
            }

            // RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi_GetRenderProxy(envProbe->Id()));
            // Assert(envProbeProxy != nullptr);
            // AssertDebug(envProbeProxy->bufferData.textureIndex != ~0u, "EnvProbe texture index not set: not bound for proxy %p at frame idx %u!", (void*)envProbeProxy,
            //     RenderApi_GetFrameIndex_RenderThread());

            frame->GetCommandList().Add<BindDescriptorSet>(
                graphicsPipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frameIndex),
                graphicsPipeline,
                ArrayMap<Name, uint32> {
                    { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*rs.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*rs.view->GetCamera()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(envProbe) } },
                globalDescriptorSetIndex);

            frame->GetCommandList().Add<BindDescriptorSet>(
                rs.passData->descriptorSets[frameIndex],
                graphicsPipeline,
                ArrayMap<Name, uint32> {},
                viewDescriptorSetIndex);

            m_fullScreenQuad->GetRenderResource().Render(frame->GetCommandList());

            ++numRenderedEnvProbes;
        }
    }

    if (ShouldRenderSSR())
    {
        m_renderSsrToScreenPass->RenderToFramebuffer(frame, rs, GetFramebuffer());
    }

    frame->GetCommandList().Add<EndFramebuffer>(GetFramebuffer(), frameIndex);

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, rs);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, rs);
        }

        m_temporalBlending->Render(frame, rs);
    }

    m_isFirstFrame = false;
}

#pragma endregion ReflectionsPass

#pragma region DeferredPassData

DeferredPassData::~DeferredPassData()
{
    depthPyramidRenderer.Reset();

    hbao.Reset();

    temporalAa.Reset();

    // m_dofBlur->Destroy();

    ssgi.Reset();

    postProcessing->Destroy();
    postProcessing.Reset();

    combinePass.Reset();

    envGridRadiancePass.Reset();
    envGridIrradiancePass.Reset();

    reflectionsPass.Reset();

    lightmapPass.Reset();
    tonemapPass.Reset();
    mipChain.Reset();
    indirectPass.Reset();
    directPass.Reset();

    raytracingReflections.Reset();
    ddgi.Reset();

    SafeRelease(std::move(finalPassDescriptorSet));
    SafeRelease(std::move(topLevelAccelerationStructures));
}

#pragma endregion DeferredPassData

#pragma region DeferredRenderer

constexpr Vec2u mipChainExtent { 512, 512 };
constexpr TextureFormat mipChainFormat = TF_R10G10B10A2;

DeferredRenderer::DeferredRenderer()
    : m_rendererConfig(RendererConfig::FromConfig())
{
}

DeferredRenderer::~DeferredRenderer()
{
}

void DeferredRenderer::Initialize()
{
}

void DeferredRenderer::Shutdown()
{
}

PassData* DeferredRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    HYP_SCOPE;

    Assert(view != nullptr);
    Assert(view->GetFlags() & ViewFlags::GBUFFER);

    HYP_LOG(Rendering, Debug, "Creating View pass data for View {}", view->Id());

    DeferredPassData* pd = new DeferredPassData;

    pd->view = view->WeakHandleFromThis();
    pd->viewport = view->GetRenderResource().GetViewport();

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    AssertDebug(gbuffer != nullptr);

    gbuffer->Create();

    HYP_LOG(Rendering, Info, "Creating renderer for view '{}' with GBuffer '{}'", view->Id(), gbuffer->GetExtent());

    const FramebufferRef& opaqueFbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmapFbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    pd->envGridRadiancePass = MakeUnique<EnvGridPass>(EGPM_RADIANCE, pd->viewport.extent, gbuffer);
    pd->envGridRadiancePass->Create();

    pd->envGridIrradiancePass = MakeUnique<EnvGridPass>(EGPM_IRRADIANCE, pd->viewport.extent, gbuffer);
    pd->envGridIrradiancePass->Create();

    pd->ssgi = MakeUnique<SSGI>(SSGIConfig::FromConfig(), gbuffer);
    pd->ssgi->Create();

    pd->postProcessing = MakeUnique<PostProcessing>();
    pd->postProcessing->Create();

    pd->indirectPass = MakeUnique<DeferredPass>(DeferredPassMode::INDIRECT_LIGHTING, pd->viewport.extent, gbuffer);
    pd->indirectPass->Create();

    pd->directPass = MakeUnique<DeferredPass>(DeferredPassMode::DIRECT_LIGHTING, pd->viewport.extent, gbuffer);
    pd->directPass->Create();

    pd->depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
    pd->depthPyramidRenderer->Create();

    pd->cullData.depthPyramidImageView = pd->depthPyramidRenderer->GetResultImageView();
    pd->cullData.depthPyramidDimensions = pd->depthPyramidRenderer->GetExtent();

    pd->mipChain = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        mipChainFormat,
        Vec3u { mipChainExtent.x, mipChainExtent.y, 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR_MIPMAP,
        TWM_CLAMP_TO_EDGE });

    InitObject(pd->mipChain);

    pd->mipChain->SetPersistentRenderResourceEnabled(true);

    pd->hbao = MakeUnique<HBAO>(HBAOConfig::FromConfig(), pd->viewport.extent, gbuffer);
    pd->hbao->Create();

    // m_dofBlur = MakeUnique<DOFBlur>(gbuffer->GetResolution(), gbuffer);
    // m_dofBlur->Create();

    CreateViewCombinePass(view, *pd);

    pd->reflectionsPass = MakeUnique<ReflectionsPass>(pd->viewport.extent, gbuffer, pd->mipChain->GetRenderResource().GetImageView(), pd->combinePass->GetFinalImageView());
    pd->reflectionsPass->Create();

    pd->tonemapPass = MakeUnique<TonemapPass>(pd->viewport.extent, gbuffer);
    pd->tonemapPass->Create();

    // We'll render the lightmap pass into the translucent framebuffer after deferred shading has been applied to OPAQUE objects.
    pd->lightmapPass = MakeUnique<LightmapPass>(translucentFbo, pd->viewport.extent, gbuffer);
    pd->lightmapPass->Create();

    pd->temporalAa = MakeUnique<TemporalAA>(pd->tonemapPass->GetFinalImageView(), pd->viewport.extent, gbuffer);
    pd->temporalAa->Create();


    CreateViewDescriptorSets(view, *pd);
    CreateViewFinalPassDescriptorSet(view, *pd);
    CreateViewTopLevelAccelerationStructures(view, *pd);

    pd->raytracingReflections = MakeUnique<RaytracingReflections>(RaytracingReflectionsConfig::FromConfig(), gbuffer);
    pd->raytracingReflections->SetTopLevelAccelerationStructures(pd->topLevelAccelerationStructures);
    pd->raytracingReflections->Create();

    pd->ddgi = MakeUnique<DDGI>(DDGIInfo { .aabb = { { -45.0f, -5.0f, -45.0f }, { 45.0f, 60.0f, 45.0f } } });
    pd->ddgi->SetTopLevelAccelerationStructures(pd->topLevelAccelerationStructures);
    pd->ddgi->Create();

    return pd;
}

void DeferredRenderer::CreateViewFinalPassDescriptorSet(View* view, DeferredPassData& passData)
{
    HYP_SCOPE;

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    Assert(renderTextureToScreenShader.IsValid());

    const ImageViewRef& inputImageView = m_rendererConfig.taaEnabled
        ? passData.temporalAa->GetResultTexture()->GetRenderResource().GetImageView()
        : passData.combinePass->GetFinalImageView();

    Assert(inputImageView.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorSetDeclaration* decl = descriptorTableDecl.FindDescriptorSetDeclaration(NAME("RenderTextureToScreenDescriptorSet"));
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);
    descriptorSet->SetDebugName(NAME("FinalPassDescriptorSet"));
    descriptorSet->SetElement(NAME("InTexture"), inputImageView);

    DeferCreate(descriptorSet);

    passData.finalPassDescriptorSet = std::move(descriptorSet);
}

void DeferredRenderer::CreateViewDescriptorSets(View* view, DeferredPassData& passData)
{
    HYP_SCOPE;

    const DescriptorSetDeclaration* decl = g_renderGlobalState->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("View"));
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, maxFramesInFlight> descriptorSets;

    static const FixedArray<Name, GTN_MAX> gbufferTextureNames {
        NAME("GBufferAlbedoTexture"),
        NAME("GBufferNormalsTexture"),
        NAME("GBufferMaterialTexture"),
        NAME("GBufferLightmapTexture"),
        NAME("GBufferVelocityTexture"),
        NAME("GBufferMaskTexture"),
        NAME("GBufferWSNormalsTexture"),
        NAME("GBufferTranslucentTexture")
    };

    const FramebufferRef& opaqueFbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmapFbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    // depth attachment goes into separate slot
    AttachmentBase* depthAttachment = opaqueFbo->GetAttachment(GTN_MAX - 1);
    Assert(depthAttachment != nullptr);

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);
        descriptorSet->SetDebugName(NAME_FMT("SceneViewDescriptorSet_{}", frameIndex));

        if (g_renderBackend->GetRenderConfig().IsDynamicDescriptorIndexingSupported())
        {
            uint32 gbufferElementIndex = 0;

            // not including depth texture here (hence the - 1)
            for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX - 1; attachmentIndex++)
            {
                descriptorSet->SetElement(NAME("GBufferTextures"), gbufferElementIndex++, opaqueFbo->GetAttachment(attachmentIndex)->GetImageView());
            }

            // add translucent bucket's albedo
            descriptorSet->SetElement(NAME("GBufferTextures"), gbufferElementIndex++, translucentFbo->GetAttachment(0)->GetImageView());
        }
        else
        {
            for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX - 1; attachmentIndex++)
            {
                descriptorSet->SetElement(gbufferTextureNames[attachmentIndex], opaqueFbo->GetAttachment(attachmentIndex)->GetImageView());
            }

            // add translucent bucket's albedo
            descriptorSet->SetElement(NAME("GBufferTranslucentTexture"), translucentFbo->GetAttachment(0)->GetImageView());
        }

        descriptorSet->SetElement(NAME("GBufferDepthTexture"), depthAttachment->GetImageView());

        descriptorSet->SetElement(NAME("GBufferMipChain"), passData.mipChain->GetRenderResource().GetImageView());

        descriptorSet->SetElement(NAME("PostProcessingUniforms"), passData.postProcessing->GetUniformBuffer());

        descriptorSet->SetElement(NAME("DepthPyramidResult"), passData.depthPyramidRenderer->GetResultImageView());

        descriptorSet->SetElement(NAME("TAAResultTexture"), passData.temporalAa->GetResultTexture()->GetRenderResource().GetImageView());

        if (passData.reflectionsPass->ShouldRenderSSR())
        {
            descriptorSet->SetElement(NAME("SSRResultTexture"), passData.reflectionsPass->GetSSRRenderer()->GetFinalResultTexture()->GetRenderResource().GetImageView());
        }
        else
        {
            descriptorSet->SetElement(NAME("SSRResultTexture"), g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        if (passData.ssgi)
        {
            descriptorSet->SetElement(NAME("SSGIResultTexture"), passData.ssgi->GetFinalResultTexture()->GetRenderResource().GetImageView());
        }
        else
        {
            descriptorSet->SetElement(NAME("SSGIResultTexture"), g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        if (passData.hbao)
        {
            descriptorSet->SetElement(NAME("SSAOResultTexture"), passData.hbao->GetFinalImageView());
        }
        else
        {
            descriptorSet->SetElement(NAME("SSAOResultTexture"), g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        descriptorSet->SetElement(NAME("DeferredResult"), passData.combinePass->GetFinalImageView());

        descriptorSet->SetElement(NAME("DeferredIndirectResultTexture"), passData.indirectPass->GetFinalImageView());

        descriptorSet->SetElement(NAME("ReflectionProbeResultTexture"), passData.reflectionsPass->GetFinalImageView());

        descriptorSet->SetElement(NAME("EnvGridRadianceResultTexture"), passData.envGridRadiancePass->GetFinalImageView());
        descriptorSet->SetElement(NAME("EnvGridIrradianceResultTexture"), passData.envGridIrradiancePass->GetFinalImageView());

        HYPERION_ASSERT_RESULT(descriptorSet->Create());

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    passData.descriptorSets = std::move(descriptorSets);
}

void DeferredRenderer::CreateViewCombinePass(View* view, DeferredPassData& passData)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // The combine pass will render into the translucent bucket's framebuffer with the shaded result.
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);
    Assert(translucentFbo != nullptr);

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    Assert(renderTextureToScreenShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("InTexture"), passData.indirectPass->GetFinalImageView());
    }

    DeferCreate(descriptorTable);

    passData.combinePass = MakeUnique<FullScreenPass>(
        renderTextureToScreenShader,
        std::move(descriptorTable),
        translucentFbo,
        translucentFbo->GetAttachment(0)->GetFormat(),
        translucentFbo->GetExtent(),
        nullptr);

    passData.combinePass->Create();
}

void DeferredRenderer::CreateViewTopLevelAccelerationStructures(View* view, DeferredPassData& passData)
{
    SafeRelease(std::move(passData.topLevelAccelerationStructures));

    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        HYP_LOG(Rendering, Debug, "Ray tracing is disabled, skipping creation of TLAS for View '{}'", view->Id());

        return;
    }

    // @FIXME: Hack solution since TLAS can only be created if it has a non-zero number of BLASes.
    // This whole thing should be reworked
    Handle<Mesh> defaultMesh = MeshBuilder::Cube();
    InitObject(defaultMesh);

    Handle<Material> defaultMaterial = CreateObject<Material>();
    InitObject(defaultMaterial);

    BLASRef blas;

    {
        TResourceHandle<RenderMesh> meshResourceHandle(defaultMesh->GetRenderResource());

        blas = meshResourceHandle->BuildBLAS(defaultMaterial);
    }

    DeferCreate(blas);

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        TLASRef& tlas = passData.topLevelAccelerationStructures[frameIndex];

        tlas = g_renderBackend->MakeTLAS();
        tlas->AddBLAS(blas);

        DeferCreate(tlas);
    }
}

void DeferredRenderer::ResizeView(Viewport viewport, View* view, DeferredPassData& passData)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    HYP_LOG(Rendering, Debug, "Resizing View '{}' to {}x{}", view->Id(), viewport.extent.x, viewport.extent.y);

    Assert(viewport.extent.Volume() > 0);

    const Vec2u newSize = Vec2u(viewport.extent);

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    AssertDebug(gbuffer != nullptr);

    gbuffer->Resize(newSize);

    const FramebufferRef& opaqueFbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmapFbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    passData.hbao->Resize(newSize);

    passData.directPass->Resize(newSize);
    passData.indirectPass->Resize(newSize);

    passData.combinePass.Reset();
    CreateViewCombinePass(view, passData);

    passData.envGridRadiancePass->Resize(newSize);
    passData.envGridIrradiancePass->Resize(newSize);

    passData.reflectionsPass->Resize(newSize);

    passData.tonemapPass->Resize(newSize);

    passData.lightmapPass = MakeUnique<LightmapPass>(translucentFbo, newSize, gbuffer);
    passData.lightmapPass->Create();

    passData.temporalAa = MakeUnique<TemporalAA>(passData.tonemapPass->GetFinalImageView(), newSize, gbuffer);
    passData.temporalAa->Create();

    passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
    passData.depthPyramidRenderer->Create();

    SafeRelease(std::move(passData.descriptorSets));
    CreateViewDescriptorSets(view, passData);

    SafeRelease(std::move(passData.finalPassDescriptorSet));
    CreateViewFinalPassDescriptorSet(view, passData);

    passData.view = view->WeakHandleFromThis();
    passData.viewport = viewport;
}

void DeferredRenderer::RenderFrame(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    Assert(rs.IsValid());

    EngineRenderStatsCounts counts {};

    Array<RenderProxyList*> renderProxyLists;

    // Collect view-independent renderable types from all views, binned
    /// @TODO: We could use the existing binning by subclass that ResourceTracker now provides.
    FixedArray<Array<EnvProbe*>, EPT_MAX> envProbes;
    FixedArray<Array<Light*>, LT_MAX> lights;
    Array<EnvGrid*> envGrids;

    // For rendering EnvGrids and EnvProbes, we use a directional light from one of the Views that references it (if found)
    /// TODO: This could be a little bit more robust.
    HashMap<EnvGrid*, Light*> envGridLights;
    HashMap<EnvProbe*, Light*> envProbeLights;

    // Render UI to render targets
    for (RendererBase* renderer : g_renderGlobalState->globalRenderers[GRT_UI])
    {
        renderer->RenderFrame(frame, rs);
    }

    // init view pass data and collect global rendering resources
    // (env probes, env grids)
    for (const TResourceHandle<RenderView>& renderView : rs.world->GetViews())
    {
        Assert(renderView);

        View* view = renderView->GetView();

        if (!view)
        {
            continue;
        }

        RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
        renderProxyLists.PushBack(&rpl);

        DeferredPassData* pd = static_cast<DeferredPassData*>(FetchViewPassData(view));
        if (pd->viewport != view->GetRenderResource().GetViewport())
        {
            ResizeView(view->GetRenderResource().GetViewport(), view, *pd);
        }

        // @TODO Call ApplyTLASUpdates on DDGI, RT reflections if need be

        pd->priority = view->GetRenderResource().GetPriority();

#if 1 // temp
        for (Light* light : rpl.lights)
        {
            AssertDebug(light != nullptr);

            lights[light->GetLightType()].PushBack(light);
        }

        for (EnvProbe* envProbe : rpl.envProbes)
        {
            if (envProbe->IsControlledByEnvGrid())
            {
                // skip it if it is controlled by an EnvGrid, we don't handle them here
                continue;
            }

            if (envProbes[envProbe->GetEnvProbeType()].Contains(envProbe))
            {
                continue;
            }

            if (!envProbeLights.Contains(envProbe))
            {
                for (Light* light : rpl.lights)
                {
                    AssertDebug(light != nullptr);

                    if (light->GetLightType() == LT_DIRECTIONAL)
                    {
                        envProbeLights[envProbe] = light;

                        break;
                    }
                }
            }

            envProbes[envProbe->GetEnvProbeType()].PushBack(envProbe);
        }

        for (EnvGrid* envGrid : rpl.envGrids)
        {
            if (envGrids.Contains(envGrid))
            {
                continue;
            }

            if (!envGridLights.Contains(envGrid))
            {
                for (Light* light : rpl.lights)
                {
                    if (light->GetLightType() == LT_DIRECTIONAL)
                    {
                        envGridLights[envGrid] = light;

                        break;
                    }
                }
            }

            envGrids.PushBack(envGrid);
        }
#endif
    }

    // Render global environment probes and grids and set fallbacks
    RenderSetup newRs = rs;

#if 1
    {
        // Set sky as fallback probe
        if (envProbes[EPT_SKY].Any())
        {
            newRs.envProbe = envProbes[EPT_SKY][0];
        }

        if (lights[LT_DIRECTIONAL].Any())
        {
            newRs.light = lights[LT_DIRECTIONAL][0];
        }

        if (envProbes.Any())
        {
            for (uint32 envProbeType = 0; envProbeType <= EPT_REFLECTION; envProbeType++)
            {
                for (EnvProbe* envProbe : envProbes[envProbeType])
                {
                    if (envProbe->NeedsRender())
                    {
                        if (RendererBase* renderer = g_renderGlobalState->globalRenderers[GRT_ENV_PROBE][envProbeType])
                        {
                            newRs.envProbe = envProbe;
                            renderer->RenderFrame(frame, newRs);
                            newRs.envProbe = nullptr; // reset for next probe

                            counts[ERS_ENV_PROBES]++;
                        }
                        else
                        {

                            HYP_LOG(Rendering, Warning, "No EnvProbeRenderer found for EnvProbeType {}! Skipping rendering of env probes of this type.", EPT_REFLECTION);
                        }

                        envProbe->SetNeedsRender(false);
                    }
                }
            }
        }

        if (envGrids.Any())
        {
            for (EnvGrid* envGrid : envGrids)
            {
                // Set global directional light as fallback
                if (envGridLights.Contains(envGrid))
                {
                    newRs.light = envGridLights[envGrid];
                }

                newRs.envGrid = envGrid;

                g_renderGlobalState->globalRenderers[GRT_ENV_GRID][0]->RenderFrame(frame, newRs);

                newRs.light = nullptr;
                newRs.envGrid = nullptr;

                counts[ERS_ENV_GRIDS]++;
            }
        }
    }
#endif

    // reset renderer state back to what it was before
    newRs = rs;

#if 1
    for (const TResourceHandle<RenderView>& renderView : rs.world->GetViews())
    {
        Assert(renderView);

        View* view = renderView->GetView();

        if (!view)
        {
            continue;
        }

        DeferredPassData* pd = static_cast<DeferredPassData*>(FetchViewPassData(view));
        AssertDebug(pd != nullptr);

        newRs.view = renderView.Get();
        newRs.passData = pd;

        RenderFrameForView(frame, newRs);

        newRs.view = nullptr;
        newRs.passData = nullptr;

        pd->CullUnusedGraphicsPipelines();

#ifdef HYP_ENABLE_RENDER_STATS
        RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);

        counts[ERS_VIEWS]++;
        counts[ERS_LIGHTMAP_VOLUMES] += rpl.lightmapVolumes.NumCurrent();
        counts[ERS_LIGHTS] += rpl.lights.NumCurrent();
        counts[ERS_ENV_GRIDS] += rpl.envGrids.NumCurrent();
        counts[ERS_ENV_PROBES] += rpl.envProbes.NumCurrent();
#endif
    }

    g_engine->GetRenderStatsCalculator().AddCounts(counts);
#endif
}

#define CHECK_FRAMEBUFFER_SIZE(fb)                                                                    \
    AssertDebug(fb->GetExtent() == pd->viewport.extent,                                               \
        "Deferred pass framebuffer extent does not match viewport extent! Expected %ux%u, got %ux%u", \
        pd->viewport.extent.x, pd->viewport.extent.y,                                                 \
        fb->GetExtent().x, fb->GetExtent().y)

void DeferredRenderer::RenderFrameForView(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    Assert(rs.IsValid());
    Assert(rs.HasView());

    uint32 globalFrameIndex = RenderApi_GetFrameIndex_RenderThread();
    if (m_lastFrameData.frameId != globalFrameIndex)
    {
        m_lastFrameData.frameId = globalFrameIndex;
        m_lastFrameData.passData.Clear();
    }

    View* view = rs.view->GetView();

    if (!(view->GetFlags() & ViewFlags::GBUFFER))
    {
        return;
    }

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);

    DeferredPassData* pd = static_cast<DeferredPassData*>(rs.passData);
    AssertDebug(pd != nullptr);

    if (TLASRef& tlas = pd->topLevelAccelerationStructures[frame->GetFrameIndex()])
    {
        RTUpdateStateFlags updateStateFlags = RTUpdateStateFlagBits::RT_UPDATE_STATE_FLAGS_NONE;
        tlas->UpdateStructure(updateStateFlags);
        pd->raytracingReflections->ApplyTLASUpdates(updateStateFlags);
    }

    // HACK TEST HACK TEST HACK TEST HACK TEST
    /*if (TLASRef& tlas = pd->topLevelAccelerationStructures[frame->GetFrameIndex()])
    {
        for (RenderProxyMesh& proxyMesh : rpl.meshes.GetElements<Entity>())
        {
            BLASRef& blas = proxyMesh.raytracingData.bottomLevelAccelerationStructures[frame->GetFrameIndex()];

            if (!blas)
            {
                blas = proxyMesh.mesh->GetRenderResource().BuildBLAS(proxyMesh.material);
                blas->SetTransform(proxyMesh.bufferData.modelMatrix);
                HYPERION_ASSERT_RESULT(blas->Create());
            }

            if (!tlas->HasBLAS(blas))
            {
                tlas->AddBLAS(blas);
            }
        }
        RTUpdateStateFlags updateStateFlags = RTUpdateStateFlagBits::RT_UPDATE_STATE_FLAGS_NONE;
        tlas->UpdateStructure(updateStateFlags);
        pd->raytracingReflections->ApplyTLASUpdates(updateStateFlags);
    }*/

    const uint32 frameIndex = frame->GetFrameIndex();

    // @TODO: Refactor to put this in the RenderWorld
    RenderEnvironment* environment = rs.world->GetEnvironment();

    const FramebufferRef& opaqueFbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmapFbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    CHECK_FRAMEBUFFER_SIZE(opaqueFbo);
    CHECK_FRAMEBUFFER_SIZE(lightmapFbo);
    CHECK_FRAMEBUFFER_SIZE(translucentFbo);

    const bool doParticles = true;
    const bool doGaussianSplatting = false; // environment && environment->IsReady();

    const bool useRtRadiance = (m_rendererConfig.pathTracerEnabled || m_rendererConfig.rtReflectionsEnabled) && pd->raytracingReflections != nullptr;
    const bool useDdgi = m_rendererConfig.rtGiEnabled && pd->ddgi != nullptr;
    const bool useHbao = m_rendererConfig.hbaoEnabled;
    const bool useHbil = m_rendererConfig.hbilEnabled;
    const bool useSsgi = m_rendererConfig.ssgiEnabled;

    const bool useEnvGridIrradiance = rpl.envGrids.NumCurrent() && m_rendererConfig.envGridGiEnabled;
    const bool useEnvGridRadiance = rpl.envGrids.NumCurrent() && m_rendererConfig.envGridRadianceEnabled;

    const bool useTemporalAa = pd->temporalAa != nullptr && m_rendererConfig.taaEnabled;

    // const bool useReflectionProbes = rpl.trackedEnvProbes.NumCurrent(TypeId::ForType<SkyProbe>()) != 0
    //     || rpl.trackedEnvProbes.NumCurrent(TypeId::ForType<ReflectionProbe>()) != 0;

    const bool useReflectionProbes = rpl.envProbes.GetElements<SkyProbe>().Any()
        || rpl.envProbes.GetElements<ReflectionProbe>().Any();

    if (useTemporalAa)
    {
        view->GetRenderResource().m_renderCamera->ApplyJitter(rs);
    }

    struct
    {
        uint32 flags;
        uint32 screenWidth;
        uint32 screenHeight;
    } deferredData;

    Memory::MemSet(&deferredData, 0, sizeof(deferredData));

    deferredData.flags |= useHbao ? DEFERRED_FLAGS_HBAO_ENABLED : 0;
    deferredData.flags |= useHbil ? DEFERRED_FLAGS_HBIL_ENABLED : 0;
    deferredData.flags |= useRtRadiance ? DEFERRED_FLAGS_RT_RADIANCE_ENABLED : 0;
    deferredData.flags |= useDdgi ? DEFERRED_FLAGS_DDGI_ENABLED : 0;

    deferredData.screenWidth = view->GetRenderResource().GetViewport().extent.x;  // rpl.viewport.extent.x;
    deferredData.screenHeight = view->GetRenderResource().GetViewport().extent.y; // rpl.viewport.extent.y;

    PerformOcclusionCulling(frame, rs);

    if (doParticles)
    {
        environment->GetParticleSystem()->UpdateParticles(frame, rs);
    }

    if (doGaussianSplatting)
    {
        environment->GetGaussianSplatting()->UpdateSplats(frame, rs);
    }

    pd->indirectPass->SetPushConstants(&deferredData, sizeof(deferredData));
    pd->directPass->SetPushConstants(&deferredData, sizeof(deferredData));

    { // render opaque objects into separate framebuffer
        frame->GetCommandList().Add<BeginFramebuffer>(opaqueFbo, frameIndex);

        ExecuteDrawCalls(frame, rs, (1u << RB_OPAQUE));

        frame->GetCommandList().Add<EndFramebuffer>(opaqueFbo, frameIndex);
    }

    if (useEnvGridIrradiance || useEnvGridRadiance)
    {
        if (useEnvGridIrradiance)
        {
            pd->envGridIrradiancePass->SetPushConstants(&deferredData, sizeof(deferredData));
            pd->envGridIrradiancePass->Render(frame, rs);
        }

        if (useEnvGridRadiance)
        {
            pd->envGridRadiancePass->SetPushConstants(&deferredData, sizeof(deferredData));
            pd->envGridRadiancePass->Render(frame, rs);
        }
    }

    if (useReflectionProbes)
    {
        pd->reflectionsPass->SetPushConstants(&deferredData, sizeof(deferredData));
        pd->reflectionsPass->Render(frame, rs);
    }

    if (useRtRadiance)
    {
        pd->raytracingReflections->Render(frame, rs);
    }

    if (useDdgi)
    {
        pd->ddgi->Render(frame, rs);
    }

    if (useHbao || useHbil)
    {
        pd->hbao->Render(frame, rs);
    }

    if (useSsgi)
    {
        RenderSetup newRenderSetup = rs;

        // if (const auto& skyProbes = rpl.trackedEnvProbes.GetElements<SkyProbe>(); skyProbes.Any())
        if (const auto& skyProbes = rpl.envProbes.GetElements<SkyProbe>(); skyProbes.Any())
        {
            newRenderSetup.envProbe = skyProbes.Front();
        }

        pd->ssgi->Render(frame, rs);
    }

    pd->postProcessing->RenderPre(frame, rs);

    // render indirect and direct lighting into the same framebuffer
    const FramebufferRef& deferredPassFramebuffer = pd->indirectPass->GetFramebuffer();
    CHECK_FRAMEBUFFER_SIZE(deferredPassFramebuffer);

    { // deferred lighting on opaque objects
        frame->GetCommandList().Add<BeginFramebuffer>(deferredPassFramebuffer, frameIndex);

        pd->indirectPass->Render(frame, rs);
        pd->directPass->Render(frame, rs);

        frame->GetCommandList().Add<EndFramebuffer>(deferredPassFramebuffer, frameIndex);
    }

    if (rpl.lightmapVolumes.NumCurrent())
    { // render objects to be lightmapped, separate from the opaque objects.
        // The lightmap bucket's framebuffer has a color attachment that will write into the opaque framebuffer's color attachment.
        frame->GetCommandList().Add<BeginFramebuffer>(lightmapFbo, frameIndex);

        ExecuteDrawCalls(frame, rs, (1u << RB_LIGHTMAP));

        frame->GetCommandList().Add<EndFramebuffer>(lightmapFbo, frameIndex);
    }

    { // generate mipchain after rendering opaque objects' lighting, now we can use it for transmission
        const ImageRef& srcImage = deferredPassFramebuffer->GetAttachment(0)->GetImage();
        GenerateMipChain(frame, rs, srcImage);
    }

    { // translucent objects
        frame->GetCommandList().Add<BeginFramebuffer>(translucentFbo, frameIndex);

        { // Render the deferred lighting into the translucent pass framebuffer with a full screen quad.
            frame->GetCommandList().Add<BindGraphicsPipeline>(pd->combinePass->GetGraphicsPipeline());

            frame->GetCommandList().Add<BindDescriptorTable>(
                pd->combinePass->GetGraphicsPipeline()->GetDescriptorTable(),
                pd->combinePass->GetGraphicsPipeline(),
                ArrayMap<Name, ArrayMap<Name, uint32>> {},
                frameIndex);

            pd->combinePass->GetQuadMesh()->GetRenderResource().Render(frame->GetCommandList());
        }

        // Render the objects to have lightmaps applied into the translucent pass framebuffer with a full screen quad.
        // Apply lightmaps over the now shaded opaque objects.
        pd->lightmapPass->RenderToFramebuffer(frame, rs, translucentFbo);

        // begin translucent with forward rendering
        ExecuteDrawCalls(frame, rs, (1u << RB_TRANSLUCENT));
        ExecuteDrawCalls(frame, rs, (1u << RB_DEBUG));

        // if (doParticles)
        // {
        //     environment->GetParticleSystem()->Render(frame, rs);
        // }

        // if (doGaussianSplatting)
        // {
        //     environment->GetGaussianSplatting()->Render(frame, rs);
        // }

        ExecuteDrawCalls(frame, rs, (1u << RB_SKYBOX));

        // // render debug draw
        // g_engine->GetDebugDrawer()->Render(frame, rs);

        frame->GetCommandList().Add<EndFramebuffer>(translucentFbo, frameIndex);
    }

    { // render depth pyramid
        pd->depthPyramidRenderer->Render(frame);
        // update culling info now that depth pyramid has been rendered
        pd->cullData.depthPyramidImageView = pd->depthPyramidRenderer->GetResultImageView();
        pd->cullData.depthPyramidDimensions = pd->depthPyramidRenderer->GetExtent();
    }

    pd->postProcessing->RenderPost(frame, rs);

    pd->tonemapPass->Render(frame, rs);

    if (useTemporalAa)
    {
        pd->temporalAa->Render(frame, rs);
    }

    // depth of field
    // m_dofBlur->Render(frame);

    // Ordered by View priority
    auto lastFrameDataIt = std::lower_bound(
        m_lastFrameData.passData.Begin(),
        m_lastFrameData.passData.End(),
        Pair<View*, DeferredPassData*> { view, pd },
        [view](const Pair<View*, DeferredPassData*>& a, const Pair<View*, DeferredPassData*>& b)
        {
            return a.second->priority < b.second->priority;
        });

    m_lastFrameData.passData.Insert(lastFrameDataIt, Pair<View*, DeferredPassData*> { view, pd });
}

#undef CHECK_FRAMEBUFFER_SIZE

void DeferredRenderer::PerformOcclusionCulling(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    constexpr uint32 bucketMask = (1 << RB_OPAQUE)
        | (1 << RB_LIGHTMAP)
        | (1 << RB_SKYBOX)
        | (1 << RB_TRANSLUCENT)
        | (1 << RB_DEBUG);

    RenderCollector::PerformOcclusionCulling(frame, rs, RenderApi_GetConsumerProxyList(rs.view->GetView()), bucketMask);
}

void DeferredRenderer::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& rs, uint32 bucketMask)
{
    HYP_SCOPE;

    RenderCollector::ExecuteDrawCalls(frame, rs, RenderApi_GetConsumerProxyList(rs.view->GetView()), bucketMask);
}

void DeferredRenderer::GenerateMipChain(FrameBase* frame, const RenderSetup& rs, const ImageRef& srcImage)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    DeferredPassData* pd = static_cast<DeferredPassData*>(rs.passData);

    const ImageRef& mipmappedResult = pd->mipChain->GetRenderResource().GetImage();
    Assert(mipmappedResult.IsValid());

    frame->GetCommandList().Add<InsertBarrier>(srcImage, RS_COPY_SRC);
    frame->GetCommandList().Add<InsertBarrier>(mipmappedResult, RS_COPY_DST);

    // Blit into the mipmap chain img
    frame->GetCommandList().Add<BlitRect>(
        srcImage,
        mipmappedResult,
        Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
        Rect<uint32> { 0, 0, mipmappedResult->GetExtent().x, mipmappedResult->GetExtent().y });

    frame->GetCommandList().Add<GenerateMipmaps>(mipmappedResult);

    frame->GetCommandList().Add<InsertBarrier>(srcImage, RS_SHADER_RESOURCE);
}

#pragma endregion DeferredRenderer

} // namespace hyperion
