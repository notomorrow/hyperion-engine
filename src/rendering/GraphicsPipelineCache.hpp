/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Task.hpp>

#include <core/utilities/Span.hpp>

#include <rendering/RenderObject.hpp>

#include <core/Constants.hpp>

namespace hyperion {

class RenderableAttributeSet;
class CachedPipelinesMap;
struct DescriptorTableDeclaration;

class GraphicsPipelineCache
{
public:
    GraphicsPipelineCache();
    GraphicsPipelineCache(const GraphicsPipelineCache&) = delete;
    GraphicsPipelineCache& operator=(const GraphicsPipelineCache&) = delete;
    GraphicsPipelineCache(GraphicsPipelineCache&&) = delete;
    GraphicsPipelineCache& operator=(GraphicsPipelineCache&&) = delete;
    ~GraphicsPipelineCache();

    /*! \brief Gets or creates a graphics pipeline based on the provided shader, descriptor table, framebuffers, and attributes.
     *  Returns a pointer to the reference, do not store a strong reference to it as it will be discarded after a certain number of frames if not used.
     *  Instead, use the returned pointer to access the graphics pipeline. It's guaranteed to be valid for at least 10 frames after this method returns.
     */
    GraphicsPipelineRef* GetOrCreate(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable,
        Span<const FramebufferRef> framebuffers,
        const RenderableAttributeSet& attributes);

    int RunCleanupCycle(int maxIter = 10);

private:
    GraphicsPipelineRef* FindGraphicsPipeline(
        const ShaderRef& shader,
        const DescriptorTableDeclaration& descriptorTableDecl,
        Span<const FramebufferRef> framebuffers,
        const RenderableAttributeSet& attributes);

    CachedPipelinesMap* m_cachedPipelines;
    Mutex m_mutex;
};

} // namespace hyperion
