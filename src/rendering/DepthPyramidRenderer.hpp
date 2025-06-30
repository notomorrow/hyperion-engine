/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEPTH_PYRAMID_RENDERER_HPP
#define HYPERION_DEPTH_PYRAMID_RENDERER_HPP

#include <core/containers/Array.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/Extent.hpp>

namespace hyperion {

class GBuffer;

class DepthPyramidRenderer
{
public:
    DepthPyramidRenderer(GBuffer* gbuffer);
    ~DepthPyramidRenderer();

    HYP_FORCE_INLINE const ImageViewRef& GetResultImageView() const
    {
        return m_depthPyramidView;
    }

    HYP_FORCE_INLINE bool IsRendered() const
    {
        return m_isRendered;
    }

    Vec2u GetExtent() const;

    void Create();

    void Render(FrameBase* frame);

private:
    GBuffer* m_gbuffer;

    ImageViewRef m_depthImageView;
    ImageRef m_depthPyramid;
    ImageViewRef m_depthPyramidView;
    Array<ImageViewRef> m_mipImageViews;
    Array<GpuBufferRef> m_mipUniformBuffers;
    Array<DescriptorTableRef> m_mipDescriptorTables;
    SamplerRef m_depthPyramidSampler;

    ComputePipelineRef m_generateDepthPyramid;

    bool m_isRendered;
};

} // namespace hyperion

#endif