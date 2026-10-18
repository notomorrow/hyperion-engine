/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/EditorGridPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>
#include <Framework/EngineDefaults.hpp>

#include <UI/Font/FontAtlas.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Logging/Logger.hpp>

#include <Scene/Camera/Camera.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statDrawEditorGrid("Rendering/GPU/EditorGrid");

CVar<bool> g_cvEditorGrid { "Editor.ShowGrid", true };
CVar<float> g_cvEditorGridSize { "Editor.Grid.Size", 1.0f };
CVar<float> g_cvEditorGridOffsetX { "Editor.Grid.OffsetX", 0.0f };
CVar<float> g_cvEditorGridOffsetY { "Editor.Grid.OffsetY", 0.0f };
CVar<float> g_cvEditorGridOffsetZ { "Editor.Grid.OffsetZ", 0.0f };

/// digits are drawn as this tall
static constexpr float LabelCapHeightPixels = 10.0f;
static constexpr char LabelGlyphChars[EditorGridLabelFont::NumGlyphs] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    '-', '.',
    'X', 'Y', 'Z'
};

struct EditorGridConstants
{
    Vec4f gridOffsetAndSize;

    /// x == 1 when the glyph atlas is bound
    Vec4f labelParams;

    EditorGridLabelFont::Glyphs glyphs;
};

#pragma region EditorGridLabelFont

EditorGridLabelFont::EditorGridLabelFont() = default;

EditorGridLabelFont::~EditorGridLabelFont()
{
    if (m_fontAtlasTask.IsValid() && !m_fontAtlasTask.IsCompleted())
    {
        m_fontAtlasTask.Await();
    }
}

bool EditorGridLabelFont::Update()
{
    AssertOnThread(g_renderThread);

    m_isReady = false;

    if (!m_fontAtlas.IsValid())
    {
        // only fetched once - a missing atlas just leaves the grid unlabeled
        if (!m_fontAtlasTask.IsValid())
        {
            m_fontAtlasTask = TaskSystem::GetInstance().Enqueue([]() -> Handle<FontAtlas>
                {
                    return EngineDefaults::LoadAsset<FontAtlas>(EngineDefaults::Get().font);
                });

            return false;
        }

        if (!m_fontAtlasTask.IsCompleted())
        {
            return false;
        }

        const Handle<FontAtlas> fontAtlas = m_fontAtlasTask.Await();

        if (!fontAtlas.IsValid() || !fontAtlas->GetAtlasTextures().GetMainAtlas().IsValid())
        {
            HYP_LOG_ONCE(Rendering, Warning, "No font atlas for editor grid labels");

            return false;
        }

        const Vec2f atlasPixelSize = Vec2f::One() / Vec2f(MathUtil::Max(fontAtlas->GetAtlasTextures().GetMainAtlas()->GetExtent().GetXY(), Vec2u::One()));

        Optional<const GlyphMetrics&> zeroMetrics = fontAtlas->GetGlyphMetricsForChar(uint32('0'));
        const float capHeight = zeroMetrics.HasValue() ? MathUtil::Max(float(zeroMetrics->bearingY), 1.0f) : 46.0f;
        const float scale = LabelCapHeightPixels / capHeight;

        m_glyphs = {};

        for (uint32 glyphIndex = 0; glyphIndex < NumGlyphs; glyphIndex++)
        {
            Optional<const GlyphMetrics&> metrics = fontAtlas->GetGlyphMetricsForChar(uint32(LabelGlyphChars[glyphIndex]));

            if (!metrics.HasValue())
            {
                continue;
            }

            const Vec2f glyphSize = Vec2f(float(metrics->width), float(metrics->height));

            m_glyphs.uvRects[glyphIndex] = Vec4f(Vec2f(metrics->imagePosition) * atlasPixelSize, glyphSize * atlasPixelSize);

            m_glyphs.pixelRects[glyphIndex] = Vec4f(
                float(metrics->bearingX) * scale,
                (capHeight - float(metrics->bearingY)) * scale,
                glyphSize.x * scale,
                glyphSize.y * scale);

            // advance is 26.6 fixed point
            m_glyphs.advances[glyphIndex / 4][glyphIndex % 4] = (float(metrics->advance) / 64.0f) * scale;
        }

        m_fontAtlas = fontAtlas;
    }

    Texture* atlasTexture = m_fontAtlas->GetAtlasTextures().GetMainAtlas().Get();

    // uploaded lazily when first bound, same as ResourceBindings
    if (!atlasTexture->IsCreated())
    {
        Check(atlasTexture->Create());
    }

    m_isReady = atlasTexture->IsCreated();

    return m_isReady;
}

GpuImageView* EditorGridLabelFont::GetAtlasImageView() const
{
    if (!m_isReady)
    {
        return RI.placeholderData->GetImageView2D1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_fontAtlas->GetAtlasTextures().GetMainAtlas().Get());
}

#pragma endregion EditorGridLabelFont

#pragma region EditorGridPass

EditorGridPass::EditorGridPass()
    : FullScreenPass(FSP_EXTERNAL_RENDERTARGET)
{
    SetPassName(NAME("EditorGrid"));

    SetBlendFunction(BlendFunction::AlphaBlending());
}

EditorGridPass::~EditorGridPass()
{
}

void EditorGridPass::Create()
{
    AssertOnThread(g_renderThread);

    m_shaderDesc = ShaderDesc(NAME("EditorGrid"));

    FullScreenPass::Create();
}

void EditorGridPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.world && renderSetup.view && renderSetup.framebuffer);

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));

    if (!cameraProxy)
    {
        return;
    }

    const bool drawLabels = m_labelFont.Update();

    EditorGridConstants constants {};

    constants.gridOffsetAndSize = Vec4f(
        g_cvEditorGridOffsetX.Get(),
        g_cvEditorGridOffsetY.Get(),
        g_cvEditorGridOffsetZ.Get(),
        MathUtil::Max(g_cvEditorGridSize.Get(), MathUtil::epsilonF));

    if (drawLabels)
    {
        constants.labelParams.x = 1.0f;
    }

    constants.glyphs = m_labelFont.GetGlyphs();

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    ENGINE_STAT_GPU_SCOPE(&s_statDrawEditorGrid, &cr);

    cr << SetFillMode(FillMode::Fill);
    cr << SetDepthWrite(false);
    cr << SetDepthTest(true);
    cr << SetStencilTest(false);
    cr << SetFaceCullMode(FaceCullMode::None);
    cr << SetTopology(Topology::Triangles);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetCurrentBlendFunction(GetBlendFunction());

    cr << SetCurrentViewport(renderSetup.viewport);
    cr << SetCurrentFramebuffer(renderSetup.framebuffer);

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetShaderUniform(0, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(1, "EditorGridConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(2, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(3, "GlyphAtlas"_sh, m_labelFont.GetAtlasImageView());

    RenderFullScreenQuad(frame, renderSetup);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    m_isFirstFrame = false;
}

#pragma endregion EditorGridPass

} // namespace Hyperion
