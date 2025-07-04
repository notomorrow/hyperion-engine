/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/IndirectDraw.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/backend/RenderBackend.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <scene/Mesh.hpp>

#include <core/math/MathUtil.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

static bool ResizeBuffer(
    FrameBase* frame,
    const GpuBufferRef& buffer,
    SizeType newBufferSize)
{
    if constexpr (IndirectDrawState::useNextPow2Size)
    {
        newBufferSize = MathUtil::NextPowerOf2(newBufferSize);
    }

    bool sizeChanged = false;

    HYPERION_ASSERT_RESULT(buffer->EnsureCapacity(newBufferSize, &sizeChanged));

    return sizeChanged;
}

static bool ResizeIndirectDrawCommandsBuffer(
    FrameBase* frame,
    uint32 numDrawCommands,
    const GpuBufferRef& indirectBuffer,
    const GpuBufferRef& stagingBuffer)
{
    const bool wasCreatedOrResized = ResizeBuffer(
        frame,
        indirectBuffer,
        numDrawCommands * sizeof(IndirectDrawCommand));

    if (!wasCreatedOrResized)
    {
        return false;
    }

    // upload zeros to the buffer using a staging buffer.
    if (!stagingBuffer->IsCreated())
    {
        HYPERION_ASSERT_RESULT(stagingBuffer->Create());
    }

    HYPERION_ASSERT_RESULT(stagingBuffer->EnsureCapacity(indirectBuffer->Size()));

    stagingBuffer->Memset(stagingBuffer->Size(), 0);

    frame->GetCommandList().Add<InsertBarrier>(stagingBuffer, RS_COPY_SRC);
    frame->GetCommandList().Add<InsertBarrier>(indirectBuffer, RS_COPY_DST);

    frame->GetCommandList().Add<CopyBuffer>(stagingBuffer, indirectBuffer, stagingBuffer->Size());

    frame->GetCommandList().Add<InsertBarrier>(indirectBuffer, RS_INDIRECT_ARG);

    return true;
}

static bool ResizeInstancesBuffer(
    FrameBase* frame,
    uint32 numObjectInstances,
    const GpuBufferRef& instanceBuffer,
    const GpuBufferRef& stagingBuffer)
{
    const bool wasCreatedOrResized = ResizeBuffer(
        frame,
        instanceBuffer,
        numObjectInstances * sizeof(ObjectInstance));

    if (wasCreatedOrResized)
    {
        instanceBuffer->Memset(instanceBuffer->Size(), 0);
    }

    return wasCreatedOrResized;
}

static bool ResizeIfNeeded(
    FrameBase* frame,
    const FixedArray<GpuBufferRef, maxFramesInFlight>& indirectBuffers,
    const FixedArray<GpuBufferRef, maxFramesInFlight>& instanceBuffers,
    const FixedArray<GpuBufferRef, maxFramesInFlight>& stagingBuffers,
    uint32 numDrawCommands,
    uint32 numObjectInstances,
    uint8 dirtyBits)
{
    bool resizeHappened = false;

    const GpuBufferRef& indirectBuffer = indirectBuffers[frame->GetFrameIndex()];
    const GpuBufferRef& instanceBuffer = instanceBuffers[frame->GetFrameIndex()];
    const GpuBufferRef& stagingBuffer = stagingBuffers[frame->GetFrameIndex()];

    if ((dirtyBits & (1u << frame->GetFrameIndex())) || !indirectBuffers[frame->GetFrameIndex()].IsValid())
    {
        resizeHappened |= ResizeIndirectDrawCommandsBuffer(frame, numDrawCommands, indirectBuffer, stagingBuffer);
    }

    if ((dirtyBits & (1u << frame->GetFrameIndex())) || !instanceBuffers[frame->GetFrameIndex()].IsValid())
    {
        resizeHappened |= ResizeInstancesBuffer(frame, numObjectInstances, instanceBuffer, stagingBuffer);
    }

    return resizeHappened;
}

#pragma region Render commands

struct RENDER_COMMAND(CreateIndirectDrawStateBuffers)
    : RenderCommand
{
    FixedArray<GpuBufferRef, maxFramesInFlight> indirectBuffers;
    FixedArray<GpuBufferRef, maxFramesInFlight> instanceBuffers;
    FixedArray<GpuBufferRef, maxFramesInFlight> stagingBuffers;

    RENDER_COMMAND(CreateIndirectDrawStateBuffers)(
        const FixedArray<GpuBufferRef, maxFramesInFlight>& indirectBuffers,
        const FixedArray<GpuBufferRef, maxFramesInFlight>& instanceBuffers,
        const FixedArray<GpuBufferRef, maxFramesInFlight>& stagingBuffers)
        : indirectBuffers(indirectBuffers),
          instanceBuffers(instanceBuffers),
          stagingBuffers(stagingBuffers)
    {
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            Assert(this->indirectBuffers[frameIndex].IsValid());
            Assert(this->instanceBuffers[frameIndex].IsValid());
            Assert(this->stagingBuffers[frameIndex].IsValid());
        }
    }

    virtual ~RENDER_COMMAND(CreateIndirectDrawStateBuffers)() override
    {
        SafeRelease(std::move(indirectBuffers));
        SafeRelease(std::move(instanceBuffers));
        SafeRelease(std::move(stagingBuffers));
    }

    virtual RendererResult operator()() override
    {
        SingleTimeCommands commands;

        commands.Push([this](CmdList& cmd)
            {
                for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
                {
                    FrameRef frame = g_renderBackend->MakeFrame(frameIndex);

                    if (!ResizeIndirectDrawCommandsBuffer(frame, IndirectDrawState::initialCount, indirectBuffers[frameIndex], stagingBuffers[frameIndex]))
                    {
                        HYP_FAIL("Failed to create indirect draw commands buffer!");
                    }

                    if (!ResizeInstancesBuffer(frame, IndirectDrawState::initialCount, instanceBuffers[frameIndex], stagingBuffers[frameIndex]))
                    {
                        HYP_FAIL("Failed to create instances buffer!");
                    }

                    cmd.Concat(std::move(frame->GetCommandList()));

                    HYPERION_ASSERT_RESULT(frame->Destroy());
                }
            });

        return commands.Execute();
    }
};

#pragma endregion Render commands

#pragma region IndirectDrawState

IndirectDrawState::IndirectDrawState()
    : m_numDrawCommands(0),
      m_dirtyBits(0x3)
{
    // Allocate used buffers so they can be set in descriptor sets
    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        m_indirectBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::INDIRECT_ARGS_BUFFER, sizeof(IndirectDrawCommand));
        m_instanceBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(ObjectInstance));
        m_stagingBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, sizeof(IndirectDrawCommand));
    }
}

IndirectDrawState::~IndirectDrawState()
{
    SafeRelease(std::move(m_indirectBuffers));
    SafeRelease(std::move(m_instanceBuffers));
    SafeRelease(std::move(m_stagingBuffers));
}

void IndirectDrawState::Create()
{
    PUSH_RENDER_COMMAND(
        CreateIndirectDrawStateBuffers,
        m_indirectBuffers,
        m_instanceBuffers,
        m_stagingBuffers);
}

void IndirectDrawState::PushDrawCall(const DrawCall& drawCall, DrawCommandData& out)
{
    out = {};

    const uint32 drawCommandIndex = m_numDrawCommands++;

    ObjectInstance& instance = m_objectInstances.EmplaceBack();
    instance.entityId = drawCall.entityId.Value();
    instance.drawCommandIndex = drawCommandIndex;
    instance.batchIndex = ~0u;

    out.drawCommandIndex = drawCommandIndex;

    if (m_drawCommands.Size() < m_numDrawCommands)
    {
        m_drawCommands.Resize(m_numDrawCommands);
    }

    drawCall.renderMesh->PopulateIndirectDrawCommand(m_drawCommands[drawCommandIndex]);

    m_dirtyBits |= 0x3;
}

void IndirectDrawState::PushInstancedDrawCall(const InstancedDrawCall& drawCall, DrawCommandData& out)
{
    out = {};

    const uint32 drawCommandIndex = m_numDrawCommands++;

    for (uint32 index = 0; index < drawCall.count; index++)
    {
        ObjectInstance& instance = m_objectInstances.EmplaceBack();
        instance.entityId = drawCall.entityIds[index].Value();
        instance.drawCommandIndex = drawCommandIndex;
        instance.batchIndex = drawCall.batch->batchIndex;
    }

    out.drawCommandIndex = drawCommandIndex;

    if (m_drawCommands.Size() < m_numDrawCommands)
    {
        m_drawCommands.Resize(m_numDrawCommands);
    }

    drawCall.renderMesh->PopulateIndirectDrawCommand(m_drawCommands[drawCommandIndex]);

    m_dirtyBits |= 0x3;
}

void IndirectDrawState::ResetDrawState()
{
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    m_numDrawCommands = 0;

    m_objectInstances.Clear();
    m_drawCommands.Clear();

    m_dirtyBits |= 0x3;
}

void IndirectDrawState::UpdateBufferData(FrameBase* frame, bool* outWasResized)
{
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    const uint32 frameIndex = frame->GetFrameIndex();

    if ((*outWasResized = ResizeIfNeeded(
             frame,
             m_indirectBuffers,
             m_instanceBuffers,
             m_stagingBuffers,
             m_numDrawCommands,
             m_objectInstances.Size(),
             m_dirtyBits)))
    {
        m_dirtyBits |= (1u << frameIndex);
    }

    if (!(m_dirtyBits & (1u << frameIndex)))
    {
        return;
    }

    // fill instances buffer with data of the meshes
    {
        Assert(m_stagingBuffers[frameIndex].IsValid());
        Assert(m_stagingBuffers[frameIndex]->Size() >= sizeof(IndirectDrawCommand) * m_drawCommands.Size());

        m_stagingBuffers[frameIndex]->Copy(
            m_drawCommands.Size() * sizeof(IndirectDrawCommand),
            m_drawCommands.Data());

        frame->GetCommandList().Add<InsertBarrier>(
            m_stagingBuffers[frameIndex],
            RS_COPY_SRC);

        frame->GetCommandList().Add<InsertBarrier>(
            m_indirectBuffers[frameIndex],
            RS_COPY_DST);

        frame->GetCommandList().Add<CopyBuffer>(
            m_stagingBuffers[frameIndex],
            m_indirectBuffers[frameIndex],
            m_stagingBuffers[frameIndex]->Size());

        frame->GetCommandList().Add<InsertBarrier>(
            m_indirectBuffers[frameIndex],
            RS_INDIRECT_ARG);
    }

    Assert(m_instanceBuffers[frameIndex]->Size() >= m_objectInstances.Size() * sizeof(ObjectInstance));

    // update data for object instances (cpu - gpu)
    m_instanceBuffers[frameIndex]->Copy(
        m_objectInstances.Size() * sizeof(ObjectInstance),
        m_objectInstances.Data());

    m_dirtyBits &= ~(1u << frameIndex);
}

#pragma endregion IndirectDrawState

#pragma region IndirectRenderer

IndirectRenderer::IndirectRenderer()
    : m_cachedCullDataUpdatedBits(0x0)
{
}

IndirectRenderer::~IndirectRenderer()
{
    SafeRelease(std::move(m_objectVisibility));
}

void IndirectRenderer::Create(IDrawCallCollectionImpl* impl)
{
    m_indirectDrawState.Create();

    ShaderRef objectVisibilityShader = g_shaderManager->GetOrCreate(NAME("ObjectVisibility"));
    Assert(objectVisibilityShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = objectVisibilityShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    Assert(impl != nullptr);

    GpuBufferHolderBase* entityInstanceBatches = impl->GetEntityInstanceBatchHolder();
    const SizeType batchSizeof = impl->GetBatchSizeOf();

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("ObjectVisibilityDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        auto* entityInstanceBatchesBufferElement = descriptorSet->GetLayout().GetElement(NAME("EntityInstanceBatchesBuffer"));
        Assert(entityInstanceBatchesBufferElement != nullptr);

        if (entityInstanceBatchesBufferElement->size != ~0u)
        {
            const SizeType entityInstanceBatchesBufferSize = entityInstanceBatchesBufferElement->size;
            const SizeType sizeMod = entityInstanceBatchesBufferSize % batchSizeof;

            Assert(sizeMod == 0,
                "EntityInstanceBatchesBuffer descriptor has size %llu but DrawCallCollection has batch struct size of %llu",
                entityInstanceBatchesBufferSize,
                batchSizeof);
        }

        descriptorSet->SetElement(NAME("ObjectInstancesBuffer"), m_indirectDrawState.GetInstanceBuffer(frameIndex));
        descriptorSet->SetElement(NAME("IndirectDrawCommandsBuffer"), m_indirectDrawState.GetIndirectBuffer(frameIndex));
        descriptorSet->SetElement(NAME("EntityInstanceBatchesBuffer"), entityInstanceBatches->GetBuffer(frameIndex));
    }

    DeferCreate(descriptorTable);

    m_objectVisibility = g_renderBackend->MakeComputePipeline(
        objectVisibilityShader,
        descriptorTable);

    // use DeferCreate because our Shader might not be ready yet
    DeferCreate(m_objectVisibility);
}

void IndirectRenderer::PushDrawCallsToIndirectState(DrawCallCollection& drawCallCollection)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    for (DrawCall& drawCall : drawCallCollection.drawCalls)
    {
        DrawCommandData drawCommandData;
        m_indirectDrawState.PushDrawCall(drawCall, drawCommandData);

        drawCall.drawCommandIndex = drawCommandData.drawCommandIndex;
    }

    for (InstancedDrawCall& drawCall : drawCallCollection.instancedDrawCalls)
    {
        DrawCommandData drawCommandData;
        m_indirectDrawState.PushInstancedDrawCall(drawCall, drawCommandData);

        drawCall.drawCommandIndex = drawCommandData.drawCommandIndex;
    }
}

void IndirectRenderer::ExecuteCullShaderInBatches(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());
    AssertDebug(renderSetup.passData != nullptr);

    Assert(renderSetup.passData->cullData.depthPyramidImageView != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    Assert(m_indirectDrawState.GetIndirectBuffer(frameIndex).IsValid());
    Assert(m_indirectDrawState.GetIndirectBuffer(frameIndex)->Size() != 0);

    const SizeType numInstances = m_indirectDrawState.GetInstances().Size();
    const uint32 numBatches = (uint32(numInstances) / IndirectDrawState::batchSize) + 1;

    if (numInstances == 0)
    {
        return;
    }

    {
        bool wasBufferResized = false;
        m_indirectDrawState.UpdateBufferData(frame, &wasBufferResized);

        if (wasBufferResized)
        {
            RebuildDescriptors(frame);
        }
    }

    if (m_cachedCullData != renderSetup.passData->cullData)
    {
        m_cachedCullData = renderSetup.passData->cullData;
        m_cachedCullDataUpdatedBits = 0xFF;
    }

    // if (m_cachedCullDataUpdatedBits & (1u << frameIndex)) {
    //     m_descriptorSets[frameIndex]->GetDescriptor(6)
    //         ->SetElementSRV(0, m_cachedCullData.depthPyramidImageView);

    //     m_descriptorSets[frameIndex]->ApplyUpdates();

    //     m_cachedCullDataUpdatedBits &= ~(1u << frameIndex);
    // }

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_objectVisibility->GetDescriptorTable(),
        m_objectVisibility,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*renderSetup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*renderSetup.view->GetCamera()) } } } },
        frameIndex);

    const uint32 viewDescriptorSetIndex = m_objectVisibility->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

    if (viewDescriptorSetIndex != ~0u)
    {
        frame->GetCommandList().Add<BindDescriptorSet>(
            renderSetup.passData->descriptorSets[frameIndex],
            m_objectVisibility,
            ArrayMap<Name, uint32> {},
            viewDescriptorSetIndex);
    }

    frame->GetCommandList().Add<InsertBarrier>(
        m_indirectDrawState.GetIndirectBuffer(frameIndex),
        RS_INDIRECT_ARG);

    struct
    {
        uint32 batchOffset;
        uint32 numInstances;
        Vec2u depthPyramidDimensions;
    } pushConstants;

    pushConstants.batchOffset = 0;
    pushConstants.numInstances = numInstances;
    pushConstants.depthPyramidDimensions = static_cast<DeferredPassData*>(renderSetup.passData)->depthPyramidRenderer->GetExtent();

    m_objectVisibility->SetPushConstants(&pushConstants, sizeof(pushConstants));

    frame->GetCommandList().Add<BindComputePipeline>(m_objectVisibility);

    frame->GetCommandList().Add<DispatchCompute>(
        m_objectVisibility,
        Vec3u { numBatches, 1, 1 });

    frame->GetCommandList().Add<InsertBarrier>(
        m_indirectDrawState.GetIndirectBuffer(frameIndex),
        RS_INDIRECT_ARG);
}

void IndirectRenderer::RebuildDescriptors(FrameBase* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    const DescriptorTableRef& descriptorTable = m_objectVisibility->GetDescriptorTable();

    const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("ObjectVisibilityDescriptorSet"), frameIndex);
    Assert(descriptorSet != nullptr);

    descriptorSet->SetElement(NAME("ObjectInstancesBuffer"), m_indirectDrawState.GetInstanceBuffer(frameIndex));
    descriptorSet->SetElement(NAME("IndirectDrawCommandsBuffer"), m_indirectDrawState.GetIndirectBuffer(frameIndex));

    descriptorSet->Update();
}

#pragma endregion IndirectRenderer

} // namespace hyperion