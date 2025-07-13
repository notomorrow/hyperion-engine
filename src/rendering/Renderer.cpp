#include <rendering/Renderer.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderBackend.hpp>

#include <scene/View.hpp>

#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/Threads.hpp>

#include <core/Defines.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

const RenderSetup& NullRenderSetup()
{
    static const RenderSetup nullRenderSetup;
    return nullRenderSetup;
}

#pragma region PassData

PassData::~PassData()
{
    HYP_LOG(Rendering, Debug, "Destroying PassData");

    if (next != nullptr)
    {
        delete next;
        next = nullptr;
    }

    for (auto& entry : renderGroupCache)
    {
        HYP_LOG(Rendering, Debug, "Destroying RenderGroupCacheEntry for RenderGroup '{}'", entry.renderGroup.Id());

        SafeRelease(std::move(entry.graphicsPipeline));
    }

    SafeRelease(std::move(descriptorSets));
}

int PassData::CullUnusedGraphicsPipelines(int maxIter)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // Ensures the iterator is valid: the Iterator type for SparsePagedArray will find the next available slot in the constructor
    // elements may have been added in the middle or removed in the meantime.
    // elements that were added will be handled after the next time this loops around; elements that were removed will be skipped over to find the next valid entry.
    renderGroupCacheIterator = typename SparsePagedArray<RenderGroupCacheEntry, 32>::Iterator(
        &renderGroupCache,
        renderGroupCacheIterator.page,
        renderGroupCacheIterator.elem);

    // Loop around to the beginning of the container when the end is reached.
    if (renderGroupCacheIterator == renderGroupCache.End())
    {
        renderGroupCacheIterator = renderGroupCache.Begin();
    }

    int numCycles = 0;

    for (; renderGroupCacheIterator != renderGroupCache.End() && numCycles < maxIter; numCycles++)
    {
        RenderGroupCacheEntry& entry = *renderGroupCacheIterator;

        if (!entry.renderGroup.Lock())
        {
            HYP_LOG(Rendering, Debug, "Removing graphics pipeline for RenderGroup '{}' as it is no longer valid.", entry.renderGroup.Id());

            SafeRelease(std::move(entry.graphicsPipeline));

            renderGroupCacheIterator = renderGroupCache.Erase(renderGroupCacheIterator);

            continue;
        }

        ++renderGroupCacheIterator;
    }

    return numCycles;
}

GraphicsPipelineRef PassData::CreateGraphicsPipeline(
    PassData* pd,
    const ShaderRef& shader,
    const RenderableAttributeSet& renderableAttributes,
    const DescriptorTableRef& descriptorTable,
    IDrawCallCollectionImpl* impl)
{
    HYP_SCOPE;

    Assert(pd != nullptr);

    HYP_LOG(Rendering, Debug, "Creating graphics pipeline for View: {}, RenderableAttributeSet hash: {}", pd->view.Id(), renderableAttributes.GetHashCode().Value());

    Handle<View> view = pd->view.Lock();
    Assert(view.IsValid());
    Assert(view->GetOutputTarget().IsValid());

    Assert(shader.IsValid());

    DescriptorTableRef table;

    if (!descriptorTable.IsValid())
    {
        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        table = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
        table->SetDebugName(NAME_FMT("DescriptorTable_{}", shader->GetCompiledShader()->GetName()));

        // Setup instancing buffers if "Instancing" descriptor set exists
        const uint32 instancingDescriptorSetIndex = table->GetDescriptorSetIndex(NAME("Instancing"));

        if (instancingDescriptorSetIndex != ~0u)
        {
            if (!impl)
            {
                impl = GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>();
            }

            AssertDebug(impl != nullptr);

            for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
            {
                const GpuBufferRef& gpuBuffer = impl->GetEntityInstanceBatchHolder()->GetBuffer(frameIndex);
                Assert(gpuBuffer.IsValid());

                const DescriptorSetRef& instancingDescriptorSet = table->GetDescriptorSet(NAME("Instancing"), frameIndex);
                Assert(instancingDescriptorSet.IsValid());

                instancingDescriptorSet->SetElement(NAME("EntityInstanceBatchesBuffer"), gpuBuffer);
            }
        }

        DeferCreate(table);
    }
    else
    {
        table = descriptorTable;
    }

    Assert(table.IsValid());

    return g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
        shader,
        table,
        view->GetOutputTarget().GetFramebuffers(),
        renderableAttributes);
}

#pragma endregion PassData

#pragma region RendererBase

struct NullPassDataExt final : PassDataExt
{
    PassDataExt* Clone() override
    {
        HYP_FAIL("Should not Clone() NullPassDataExt!");
    }
};

RendererBase::RendererBase()
    : m_viewPassDataCleanupIterator(m_viewPassData.End())
{
}

RendererBase::~RendererBase()
{
    for (PassData* pd : m_viewPassData)
    {
        delete pd;
    }
}

int RendererBase::RunCleanupCycle(int maxIter)
{
    // Ensures the iterator is valid: the Iterator type for SparsePagedArray will find the next available slot in the constructor
    // elements may have been added in the middle or removed in the meantime.
    // elements that were added will be handled after the next time this loops around; elements that were removed will be skipped over to find the next valid entry.
    m_viewPassDataCleanupIterator = typename SparsePagedArray<PassData*, 16>::Iterator(
        &m_viewPassData,
        m_viewPassDataCleanupIterator.page,
        m_viewPassDataCleanupIterator.elem);

    // Loop around to the beginning of the container when the end is reached.
    if (m_viewPassDataCleanupIterator == m_viewPassData.End())
    {
        m_viewPassDataCleanupIterator = m_viewPassData.Begin();
    }

    int numCycles = 0;

    while (m_viewPassDataCleanupIterator != m_viewPassData.End() && numCycles < maxIter)
    {
        PassData* pd = *m_viewPassDataCleanupIterator;

        int numLocalCycles = 1;

        if (!pd->view.Lock())
        {
            HYP_LOG(Rendering, Debug, "Removing PassData for View {} as it is no longer valid.", pd->view.Id());

            delete pd;

            m_viewPassDataCleanupIterator = m_viewPassData.Erase(m_viewPassDataCleanupIterator);
        }
        else
        {
            numLocalCycles += pd->CullUnusedGraphicsPipelines(maxIter - numCycles - 1);

            ++m_viewPassDataCleanupIterator;
        }

        numCycles += numLocalCycles;
    }

    return numCycles;
}

PassData* RendererBase::TryGetViewPassData(View* view)
{
    if (!view)
    {
        return nullptr;
    }

    if (PassData** pdPtr = m_viewPassData.TryGet(view->Id().ToIndex()))
    {
        return *pdPtr;
    }

    return nullptr;
}

PassData* RendererBase::FetchViewPassData(View* view, PassDataExt* ext)
{
    if (!view)
    {
        return nullptr;
    }

    PassData* pd = nullptr;

    if (PassData** pdPtr = m_viewPassData.TryGet(view->Id().ToIndex()))
    {
        pd = *pdPtr;
    }

    if (!pd)
    {
        NullPassDataExt nullPassDataExt {};

        // call virtual function to alloc / create

        pd = CreateViewPassData(view, ext ? *ext : nullPassDataExt);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        m_viewPassData.Set(view->Id().ToIndex(), pd);
    }
    else if (pd->view.GetUnsafe() != view)
    {
        delete pd;

        NullPassDataExt nullPassDataExt {};

        pd = CreateViewPassData(view, ext ? *ext : nullPassDataExt);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        m_viewPassData.Set(view->Id().ToIndex(), pd);
    }

    AssertDebug(pd->view.GetUnsafe() == view);

    return pd;
}

#pragma region RendererBase

} // namespace hyperion
