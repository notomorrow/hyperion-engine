/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Rendering/RenderMemory.hpp>
#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class Light;
class View;
class Camera;
class Texture;

struct ENGINE_API ShadowMapCaptureState
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    ShadowMapCaptureState(Light* light, Name swatchName);
    ~ShadowMapCaptureState();

    ShadowMapCaptureState(const ShadowMapCaptureState& other) = delete;
    ShadowMapCaptureState& operator=(const ShadowMapCaptureState& other) = delete;

    void Begin();
    void End(bool commitResult);

    HYP_FORCE_INLINE uint32 GetNumFaces() const
    {
        return m_numFaces;
    }

    HYP_FORCE_INLINE View* GetView(uint32 faceIndex) const
    {
        return faceIndex < m_numFaces ? m_views[faceIndex].Get() : nullptr;
    }

    HYP_FORCE_INLINE const FramebufferRef& GetFramebuffer(uint32 faceIndex) const
    {
        return m_framebuffers[faceIndex];
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetTexture() const
    {
        return m_texture;
    }

    HYP_FORCE_INLINE bool IsRenderComplete() const
    {
        return m_isRenderComplete.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE void SetRenderComplete()
    {
        m_isRenderComplete.Set(true, MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE uint32 GetRenderedFacesMask() const
    {
        return m_renderedFacesMask.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE void SetRenderedFacesMask(uint32 mask)
    {
        m_renderedFacesMask.Set(mask, MemoryOrder::RELEASE);
    }

    Name swatchName; //!< swatch being baked for

private:
    friend class Light;

    Light* m_light;

    Handle<Texture> m_texture; //!< capture target
    Handle<Camera> m_camera;

    FixedArray<Handle<View>, 6> m_views;
    FixedArray<FramebufferRef, 6> m_framebuffers;

    uint32 m_numFaces;

    AtomicVar<bool> m_isRenderComplete { false };
    AtomicVar<uint32> m_renderedFacesMask { 0 };
};

} // namespace Hyperion
