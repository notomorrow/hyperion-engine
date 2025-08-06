/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/FullScreenPass.hpp>

namespace hyperion {

class GBuffer;

class DOFBlur
{
public:
    DOFBlur(const Vec2u& extent, GBuffer* gbuffer);
    DOFBlur(const DOFBlur& other) = delete;
    DOFBlur& operator=(const DOFBlur& other) = delete;
    ~DOFBlur();

    const Handle<FullScreenPass>& GetHorizontalBlurPass() const
    {
        return m_blurHorizontalPass;
    }

    const Handle<FullScreenPass>& GetVerticalBlurPass() const
    {
        return m_blurVerticalPass;
    }

    const Handle<FullScreenPass>& GetCombineBlurPass() const
    {
        return m_blurMixPass;
    }

    void Create();
    void Destroy();

    void Render(FrameBase* frame, const RenderSetup& renderSetup);

private:
    GBuffer* m_gbuffer;

    Vec2u m_extent;

    Handle<FullScreenPass> m_blurHorizontalPass;
    Handle<FullScreenPass> m_blurVerticalPass;
    Handle<FullScreenPass> m_blurMixPass;
};

} // namespace hyperion
