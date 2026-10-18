/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Framework/CVarManager.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Threading/Task.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class FontAtlas;

extern ENGINE_API CVar<bool> g_cvEditorGrid;

// spacing of the minor grid lines; the editor's translate snapping uses the same grid
extern ENGINE_API CVar<float> g_cvEditorGridSize;

// Y offset moves the grid plane, X/Z offset shift where the lines fall
extern ENGINE_API CVar<float> g_cvEditorGridOffsetX;
extern ENGINE_API CVar<float> g_cvEditorGridOffsetY;
extern ENGINE_API CVar<float> g_cvEditorGridOffsetZ;

class EditorGridLabelFont
{
public:
    static constexpr uint32 NumGlyphs = 15;

    struct Glyphs
    {
        Vec4f uvRects[NumGlyphs];    // xy: top left uv, zw: uv size
        Vec4f pixelRects[NumGlyphs]; // xy: offset from the pen position at the label's top, zw: size, all in pixels
        Vec4f advances[(NumGlyphs + 3) / 4];
    };

    EditorGridLabelFont();
    EditorGridLabelFont(const EditorGridLabelFont& other) = delete;
    EditorGridLabelFont& operator=(const EditorGridLabelFont& other) = delete;
    ~EditorGridLabelFont();

    bool Update();

    HYP_FORCE_INLINE const Glyphs& GetGlyphs() const
    {
        return m_glyphs;
    }

    GpuImageView* GetAtlasImageView() const;

private:
    Task<Handle<FontAtlas>> m_fontAtlasTask;
    Handle<FontAtlas> m_fontAtlas;
    Glyphs m_glyphs {};
    bool m_isReady = false;
};

class EditorGridPass final : public FullScreenPass
{
public:
    EditorGridPass();

    EditorGridPass(const EditorGridPass& other) = delete;
    EditorGridPass& operator=(const EditorGridPass& other) = delete;

    virtual ~EditorGridPass() override;

    virtual void Create() override;
    virtual void Render(Frame* frame, const RenderSetup& renderSetup) override;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderCheckerboarded() const override
    {
        return false;
    }

    EditorGridLabelFont m_labelFont;
};

} // namespace Hyperion
