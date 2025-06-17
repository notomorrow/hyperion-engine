/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GRAPHICS_PIPELINE_CACHE_HPP
#define HYPERION_GRAPHICS_PIPELINE_CACHE_HPP

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Task.hpp>

#include <core/utilities/Span.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Constants.hpp>

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

    GraphicsPipelineRef GetOrCreate(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable,
        Span<const FramebufferRef> framebuffers,
        const RenderableAttributeSet& attributes);

private:
    GraphicsPipelineRef FindGraphicsPipeline(
        const ShaderRef& shader,
        const DescriptorTableDeclaration& descriptorTableDecl,
        Span<const FramebufferRef> framebuffers,
        const RenderableAttributeSet& attributes);

    CachedPipelinesMap* m_cachedPipelines;
    Mutex m_mutex;
};

} // namespace hyperion

#endif
