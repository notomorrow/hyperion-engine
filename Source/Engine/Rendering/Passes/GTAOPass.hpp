/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class GBuffer;
class Texture;
struct RenderSetup;

class GTAOPass final
{
public:
    GTAOPass(Vec2u extent, GBuffer* gbuffer);
    GTAOPass(const GTAOPass& other) = delete;
    GTAOPass& operator=(const GTAOPass& other) = delete;
    ~GTAOPass();

    void Create();
    void Render(Frame* frame, const RenderSetup& renderSetup);

    const GpuImageViewRef& GetFinalImageView() const;

private:
    void CreateTextures(const Vec2u& extent);

    Vec2u m_extent;
    GBuffer* m_gbuffer;

    Handle<Texture> m_viewDepthTexture;

    Handle<Texture> m_aoTextures[2];
    Handle<Texture> m_edgesTexture;
    Handle<Texture> m_historyTextures[2];

    Texture* m_finalTexture;

    Mat4f m_previousViewProjection;
    uint32 m_historyIndex;
    uint32 m_lastRenderedFrame;
    bool m_hasHistory;
};

} // namespace Hyperion
