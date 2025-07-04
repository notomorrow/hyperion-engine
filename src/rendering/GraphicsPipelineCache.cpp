/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/backend/RenderBackend.hpp>
#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/algorithm/Map.hpp>

// For CompiledShader
#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

#pragma region CachedPipelinesMap

struct CachedPipelinesMap : HashMap<RenderableAttributeSet, Array<GraphicsPipelineRef>>
{
};

#pragma endregion CachedPipelinesMap

#pragma region GraphicsPipelineCache

GraphicsPipelineCache::GraphicsPipelineCache()
    : m_cachedPipelines(new CachedPipelinesMap())
{
}

GraphicsPipelineCache::~GraphicsPipelineCache()
{
    for (auto& it : *m_cachedPipelines)
    {
        for (GraphicsPipelineRef& pipeline : it.second)
        {
            SafeRelease(std::move(pipeline));
        }
    }

    m_cachedPipelines->Clear();

    Assert(m_cachedPipelines->Empty(), "Graphics pipeline cache not empty!");
    delete m_cachedPipelines;
}

GraphicsPipelineRef GraphicsPipelineCache::GetOrCreate(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptorTable,
    Span<const FramebufferRef> framebuffers,
    const RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    if (!shader.IsValid())
    {
        HYP_LOG(Rendering, Error, "Shader is null or invalid!");

        return nullptr;
    }

    const DescriptorTableDeclaration* descriptorTableDecl = nullptr;

    DescriptorTableRef table;

    if (descriptorTable)
    {
        table = descriptorTable;
        descriptorTableDecl = table->GetDeclaration();

        Assert(descriptorTableDecl != nullptr);
    }
    else
    {
        descriptorTableDecl = &shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    }

    GraphicsPipelineRef graphicsPipeline = FindGraphicsPipeline(
        shader,
        *descriptorTableDecl,
        framebuffers,
        attributes);

    if (graphicsPipeline.IsValid())
    {
        return graphicsPipeline;
    }

    if (!table)
    {
        table = g_renderBackend->MakeDescriptorTable(descriptorTableDecl);
        if (!table.IsValid())
        {
            HYP_LOG(Rendering, Error, "Failed to create descriptor table for shader: {}", shader->GetDebugName());

            return nullptr;
        }

        DeferCreate(table);
    }

    Proc<void(const GraphicsPipelineRef&)> newCallback = [this, attributes](const GraphicsPipelineRef& graphicsPipeline)
    {
        Mutex::Guard guard(m_mutex);

        HYP_LOG(Rendering, Info, "Adding graphics pipeline to cache ({})", attributes.GetHashCode().Value());

        (*m_cachedPipelines)[attributes].PushBack(graphicsPipeline);
    };

    graphicsPipeline = g_renderBackend->MakeGraphicsPipeline(
        shader,
        table,
        framebuffers,
        attributes);

    struct RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)
        : RenderCommand
    {
        GraphicsPipelineRef graphicsPipeline;
        Proc<void(const GraphicsPipelineRef&)> callback;

        RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)(
            const GraphicsPipelineRef& graphicsPipeline,
            Proc<void(const GraphicsPipelineRef&)>&& callback)
            : graphicsPipeline(graphicsPipeline),
              callback(std::move(callback))
        {
            Assert(graphicsPipeline.IsValid());
        }

        virtual ~RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)() override
        {
            SafeRelease(std::move(graphicsPipeline));
        }

        virtual RendererResult operator()() override
        {
            HYPERION_BUBBLE_ERRORS(graphicsPipeline->Create());

            if (callback.IsValid())
            {
                callback(graphicsPipeline);
            }

            return RendererResult {};
        }
    };

    PUSH_RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache, graphicsPipeline, std::move(newCallback));

    return graphicsPipeline;
}

GraphicsPipelineRef GraphicsPipelineCache::FindGraphicsPipeline(
    const ShaderRef& shader,
    const DescriptorTableDeclaration& descriptorTableDecl,
    Span<const FramebufferRef> framebuffers,
    const RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    PerformanceClock clock;
    clock.Start();

    Mutex::Guard guard(m_mutex);

    const RenderableAttributeSet key = attributes;

    auto it = m_cachedPipelines->Find(key);

    if (it == m_cachedPipelines->End())
    {
        HYP_LOG(Rendering, Warning, "GraphicsPipelineCache cache miss ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());

        return nullptr;
    }

    const HashCode shaderHashCode = shader->GetCompiledShader()->GetHashCode();

    for (const GraphicsPipelineRef& pipeline : it->second)
    {
        if (pipeline->MatchesSignature(
                shader,
                descriptorTableDecl,
                Map(framebuffers, [](const FramebufferRef& framebuffer)
                    {
                        return static_cast<const FramebufferBase*>(framebuffer.Get());
                    }),
                attributes))
        {
            HYP_LOG(Rendering, Info, "GraphicsPipelineCache cache hit ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());

            return pipeline;
        }
    }

    HYP_LOG(Rendering, Warning, "GraphicsPipelineCache cache miss ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());

    return nullptr;
}

#pragma endregion GraphicsPipelineCache

} // namespace hyperion
