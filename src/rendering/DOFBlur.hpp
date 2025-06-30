/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOF_BLUR_HPP
#define HYPERION_DOF_BLUR_HPP

#include <rendering/FullScreenPass.hpp>

namespace hyperion {

class Engine;
class GBuffer;

class DOFBlur
{
public:
    DOFBlur(const Vec2u& extent, GBuffer* gbuffer);
    DOFBlur(const DOFBlur& other) = delete;
    DOFBlur& operator=(const DOFBlur& other) = delete;
    ~DOFBlur();

    const UniquePtr<FullScreenPass>& GetHorizontalBlurPass() const
    {
        return m_blurHorizontalPass;
    }

    const UniquePtr<FullScreenPass>& GetVerticalBlurPass() const
    {
        return m_blurVerticalPass;
    }

    const UniquePtr<FullScreenPass>& GetCombineBlurPass() const
    {
        return m_blurMixPass;
    }

    void Create();
    void Destroy();

    void Render(FrameBase* frame, const RenderSetup& renderSetup);

private:
    GBuffer* m_gbuffer;

    Vec2u m_extent;

    UniquePtr<FullScreenPass> m_blurHorizontalPass;
    UniquePtr<FullScreenPass> m_blurVerticalPass;
    UniquePtr<FullScreenPass> m_blurMixPass;
};

} // namespace hyperion

#endif