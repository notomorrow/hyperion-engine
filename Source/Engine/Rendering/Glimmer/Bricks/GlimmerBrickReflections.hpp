/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector2.hpp>

namespace Hyperion {

class Texture;
class View;
class GBuffer;
class GlimmerBrickWorld;

class GlimmerBrickReflections final
{
public:
    GlimmerBrickReflections();

    GlimmerBrickReflections(const GlimmerBrickReflections& other) = delete;
    GlimmerBrickReflections& operator=(const GlimmerBrickReflections& other) = delete;

    ~GlimmerBrickReflections();

    void Render(Frame* frame, View* view, GBuffer* gbuffer, const Vec2u& extent, const GlimmerBrickWorld* brickWorld);

    bool IsDebugView() const;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_isValid;
    }

    // rgb = radiance times a, a = how much of the pixel's reflection is the bricks'
    const GpuImageViewRef& GetImageView() const;

private:
    void CreateTextures(const Vec2u& extent);
    void CreateSlotBuffers(uint32 numCrossings);
    void CopyCounts(Frame* frame);
    void ReadCounts(Frame* frame);

    // half resolution
    Handle<Texture> m_rawColorTexture;
    Handle<Texture> m_rawInfoTexture;
    FixedArray<Handle<Texture>, 2> m_guideTextures;
    FixedArray<Handle<Texture>, 2> m_keptColorTextures;
    FixedArray<Handle<Texture>, 2> m_keptInfoTextures;

    Handle<Texture> m_fullTexture;
    Handle<Texture> m_unusedOutputTexture;

    GpuBufferRef m_blockOfBuffer;
    GpuBufferRef m_blocksBuffer;
    GpuBufferRef m_claimsBuffer;
    GpuBufferRef m_requestsBuffer;
    GpuBufferRef m_wantedBuffer;
    GpuBufferRef m_listBuffer;
    GpuBufferRef m_wholeBuffer;
    GpuBufferRef m_countsBuffer;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_readbackBuffers;
    FixedArray<bool, NumFramesInFlight> m_isReadbackPending;

    Vec2u m_extent;
    uint32 m_numCrossings;
    uint32 m_frame;
    uint32 m_parity;
    bool m_hasHistory;
    bool m_hasRequests;
    bool m_isValid;
};

} // namespace Hyperion
