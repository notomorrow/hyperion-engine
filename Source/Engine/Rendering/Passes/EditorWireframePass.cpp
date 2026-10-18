/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/EditorWireframePass.hpp>
#include <Rendering/Passes/EditorGridPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/Attachment.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/CommandRecorderAllocator.hpp>
#include <Rendering/DebugDrawer.hpp>
#include <Rendering/ThumbnailCaptureState.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/EngineDefaults.hpp>
#include <Framework/View.hpp>

#include <UI/Font/FontAtlas.hpp>

#include <Core/Threading/TaskSystem.hpp>

#include <Core/Logging/Logger.hpp>

#include <Scene/Camera/Camera.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statEditorWireframe("Rendering/GPU/EditorWireframe");
static EngineStatCounter<uint32> s_statEditorWireframeRedraws("Rendering/EditorWireframeRedraws");

static constexpr uint32 UnusedTargetFrames = 30;

static const Vec4f BackgroundColor { 0.012f, 0.012f, 0.014f, 1.0f };

// digits are drawn this tall, the shader's label layout assumes it
static constexpr float LabelCapHeightPixels = 10.0f;

static constexpr char LabelGlyphChars[EditorWireframePass::NumLabelGlyphs] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '.', 'X', 'Y', 'Z'
};

// matches EditorGridConstants in Shaders/Editor/EditorOrthoGrid.hlsl
struct EditorOrthoGridConstants
{
    Vec4f gridOffsetAndSize;
    Vec4f labelParams; // x: 1 when the glyph atlas is bound
    EditorWireframePass::LabelGlyphs glyphs;
};

EditorWireframePass::EditorWireframePass() = default;

EditorWireframePass::~EditorWireframePass()
{
    if (m_labelFontTask.IsValid() && !m_labelFontTask.IsCompleted())
    {
        m_labelFontTask.Await();
    }

    for (auto& it : m_targets)
    {
        EnqueueDeletion(std::move(it.second.framebuffer));
    }

    EnqueueDeletion(std::move(m_quadMesh));
}

EditorWireframePass::ViewTarget& EditorWireframePass::GetOrCreateTarget(View* view, const Vec2u& extent)
{
    ViewTarget& target = m_targets[view->Id().Value()];
    target.lastUsedFrame = GetFrameCounter();

    if (target.framebuffer.IsValid() && target.framebuffer->GetExtent() == extent)
    {
        return target;
    }

    EnqueueDeletion(std::move(target.framebuffer));

    FramebufferDesc framebufferDesc {};
    framebufferDesc.extent = extent;

    target.framebuffer = RI.MakeFramebuffer(framebufferDesc);

#ifdef HYP_RHI_DEBUG_NAMES
    target.framebuffer->SetDebugName(NAME("EditorWireframeFramebuffer"));
#endif

    AttachmentDesc colorAttachmentDesc {};
    colorAttachmentDesc.imageType = TextureType::Texture2D;
    colorAttachmentDesc.format = TextureFormat::RGBA16F;
    colorAttachmentDesc.loadOp = LoadOperation::Clear;
    colorAttachmentDesc.storeOp = StoreOperation::Store;

    target.framebuffer->AddAttachment(0, colorAttachmentDesc)->SetClearColor(BackgroundColor);

    AttachmentDesc depthAttachmentDesc {};
    depthAttachmentDesc.imageType = TextureType::Texture2D;
    depthAttachmentDesc.format = TextureFormat::D32F;
    depthAttachmentDesc.loadOp = LoadOperation::Clear;
    depthAttachmentDesc.storeOp = StoreOperation::None;

    target.framebuffer->AddAttachment(1, depthAttachmentDesc);

    Check(target.framebuffer->Create());

    target.hasContent = false;

    return target;
}

void EditorWireframePass::PurgeUnusedTargets()
{
    const uint32 frameCounter = GetFrameCounter();

    for (auto it = m_targets.Begin(); it != m_targets.End();)
    {
        if (frameCounter - it->second.lastUsedFrame > UnusedTargetFrames)
        {
            EnqueueDeletion(std::move(it->second.framebuffer));

            it = m_targets.Erase(it);

            continue;
        }

        ++it;
    }
}

bool EditorWireframePass::UpdateLabelFont()
{
    if (!m_labelFontAtlas.IsValid())
    {
        // only fetched once - a missing atlas just leaves the grid unlabeled
        if (!m_labelFontTask.IsValid())
        {
            m_labelFontTask = TaskSystem::GetInstance().Enqueue([]() -> Handle<FontAtlas>
                {
                    return EngineDefaults::LoadAsset<FontAtlas>(EngineDefaults::Get().font);
                });

            return false;
        }

        if (!m_labelFontTask.IsCompleted())
        {
            return false;
        }

        const Handle<FontAtlas> fontAtlas = m_labelFontTask.Await();

        if (!fontAtlas.IsValid() || !fontAtlas->GetAtlasTextures().GetMainAtlas().IsValid())
        {
            HYP_LOG_ONCE(Rendering, Warning, "No font atlas for editor grid labels");

            return false;
        }

        const Vec2f atlasPixelSize = Vec2f::One() / Vec2f(MathUtil::Max(fontAtlas->GetAtlasTextures().GetMainAtlas()->GetExtent().GetXY(), Vec2u::One()));

        Optional<const GlyphMetrics&> zeroMetrics = fontAtlas->GetGlyphMetricsForChar(uint32('0'));
        const float capHeight = zeroMetrics.HasValue() ? MathUtil::Max(float(zeroMetrics->bearingY), 1.0f) : 46.0f;
        const float scale = LabelCapHeightPixels / capHeight;

        m_labelGlyphs = {};

        for (uint32 glyphIndex = 0; glyphIndex < NumLabelGlyphs; glyphIndex++)
        {
            Optional<const GlyphMetrics&> metrics = fontAtlas->GetGlyphMetricsForChar(uint32(LabelGlyphChars[glyphIndex]));

            if (!metrics.HasValue())
            {
                continue;
            }

            const Vec2f glyphSize = Vec2f(float(metrics->width), float(metrics->height));

            m_labelGlyphs.uvRects[glyphIndex] = Vec4f(Vec2f(metrics->imagePosition) * atlasPixelSize, glyphSize * atlasPixelSize);

            m_labelGlyphs.pixelRects[glyphIndex] = Vec4f(
                float(metrics->bearingX) * scale,
                (capHeight - float(metrics->bearingY)) * scale,
                glyphSize.x * scale,
                glyphSize.y * scale);

            // advance is 26.6 fixed point
            m_labelGlyphs.advances[glyphIndex / 4][glyphIndex % 4] = (float(metrics->advance) / 64.0f) * scale;
        }

        m_labelFontAtlas = fontAtlas;
    }

    Texture* atlasTexture = m_labelFontAtlas->GetAtlasTextures().GetMainAtlas().Get();

    // uploaded lazily when first bound, same as ResourceBindings
    if (!atlasTexture->IsCreated())
    {
        Check(atlasTexture->Create());
    }

    return atlasTexture->IsCreated();
}

void EditorWireframePass::RenderGrid(Frame* frame, const RenderSetup& rs, bool drawLabels)
{
    if (!g_cvEditorGrid.Get())
    {
        return;
    }

    if (!m_quadMesh)
    {
        m_quadMesh = MeshBuilder::Quad();
        m_quadMesh->SetName(NAME("EditorWireframeGridQuad"));
        m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
        m_quadMesh->SetIsTransient(true);
        m_quadMesh->UploadGpuData();
    }

    EditorOrthoGridConstants constants {};
    constants.gridOffsetAndSize = Vec4f(
        g_cvEditorGridOffsetX.Get(),
        g_cvEditorGridOffsetY.Get(),
        g_cvEditorGridOffsetZ.Get(),
        MathUtil::Max(g_cvEditorGridSize.Get(), MathUtil::epsilonF));
    constants.labelParams = Vec4f(drawLabels ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    constants.glyphs = m_labelGlyphs;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    cr << SetFillMode(FillMode::Fill);
    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetStencilTest(false);
    cr << SetFaceCullMode(FaceCullMode::None);
    cr << SetTopology(Topology::Triangles);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    cr << SetCurrentShader(ShaderDesc(NAME("EditorOrthoGrid")));

    cr << SetShaderUniform(0, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(rs.view->GetCamera()));
    cr << SetShaderUniform(1, "EditorGridConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(2, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(3, "GlyphAtlas"_sh, drawLabels
            ? RI.textureViewCache->GetOrCreate(m_labelFontAtlas->GetAtlasTextures().GetMainAtlas().Get())
            : RI.placeholderData->GetImageView2D1x1R8());

    cr << CommitDrawState();

    cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer(0));
    cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer(0));

    cr << DrawIndexed(6);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
}

EditorWireframePass::RenderResult EditorWireframePass::Render(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);
    AssertDebug(rs.view != nullptr);

    PurgeUnusedTargets();

    View* view = rs.view;

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera()));

    if (!cameraProxy || rs.viewport.extent.Volume() == 0)
    {
        return {};
    }

    ViewTarget& target = GetOrCreateTarget(view, rs.viewport.extent);
    const FramebufferRef& framebuffer = target.framebuffer;

    Attachment* colorAttachment = framebuffer->GetAttachment(0);

    const Vec4f gridParams = Vec4f(
        g_cvEditorGridOffsetX.Get(),
        g_cvEditorGridOffsetY.Get(),
        g_cvEditorGridOffsetZ.Get(),
        g_cvEditorGrid.Get() ? g_cvEditorGridSize.Get() : 0.0f);

    const HashCode debugDrawHash = DebugDrawer::GetInstance().GetCommandsHash();
    const bool hasLabelFont = UpdateLabelFont();
    const bool meshesChanged = GetConsumerProxyList(view).GetMeshEntities().GetDiff().NeedsUpdate();
    const bool captureRequested = view->thumbnailCaptureState != nullptr && view->thumbnailCaptureState->IsRequested();

    // consume the request even when something else already forces a redraw
    const bool redrawRequested = view->ConsumeRedrawRequest();

    const bool needsRedraw = redrawRequested
        || !target.hasContent
        || meshesChanged
        || captureRequested
        || target.viewProjMat != cameraProxy->bufferData.viewProjMat
        || target.gridParams != gridParams
        || target.debugDrawHash != debugDrawHash
        || target.hasLabelFont != hasLabelFont;

    if (!needsRedraw)
    {
        return { colorAttachment->GetImageView(), false };
    }

    target.hasContent = true;
    target.viewProjMat = cameraProxy->bufferData.viewProjMat;
    target.gridParams = gridParams;
    target.debugDrawHash = debugDrawHash;
    target.hasLabelFont = hasLabelFont;

    ++s_statEditorWireframeRedraws;

    RenderSetup viewRS = rs.Fork();
    viewRS.framebuffer = framebuffer;
    viewRS.passData = nullptr;
    viewRS.viewport = Viewport { framebuffer->GetExtent() };

    CommandRecorder& cr = frame->cr;

    ENGINE_STAT_GPU_SCOPE(&s_statEditorWireframe, &cr);

    cr << SetCurrentFramebuffer(framebuffer);
    cr << SetCurrentViewport(viewRS.viewport);

    RenderGrid(frame, viewRS, hasLabelFont);

    RenderCollector& renderCollector = GetRenderCollector(view);

    renderCollector.ExecuteDrawCalls(
        frame, viewRS, nullptr,
        RenderBucketMask<RenderBucket::Opaque, RenderBucket::Lightmapped, RenderBucket::Translucent>,
        /* isDepthPrepass */ false);

    DebugDrawer::GetInstance().Render(frame, viewRS);

    renderCollector.ExecuteDrawCalls(frame, viewRS, nullptr, RenderBucketMask<RenderBucket::Debug>, /* isDepthPrepass */ false);

    cr << SetCurrentFramebuffer(nullptr);

    cr << InsertBarrier(colorAttachment->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);

    if (captureRequested)
    {
        view->thumbnailCaptureState->CaptureFrom(frame, colorAttachment->GetImageView(), colorAttachment->GetImageView());
    }

    return { colorAttachment->GetImageView(), true };
}

} // namespace Hyperion
