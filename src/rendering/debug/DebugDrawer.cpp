/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/debug/DebugDrawer.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGpuBuffer.hpp>

#include <rendering/Mesh.hpp>

#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <util/MeshBuilder.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

static RenderableAttributeSet GetRenderableAttributes()
{
    return RenderableAttributeSet(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes },
        MaterialAttributes {
            .bucket = RB_TRANSLUCENT,
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction::None(),
            .flags = MAF_DEPTH_TEST,
            .stencilFunction = StencilFunction {
                .passOp = SO_KEEP,
                .failOp = SO_KEEP,
                .depthFailOp = SO_KEEP,
                .compareOp = SCO_NOT_EQUAL,
                .mask = 0x0,
                .value = 0x1 } });
}

#pragma region DebugDrawCommand_Probe

struct DebugDrawCommand_Probe : DebugDrawCommand
{
    const EnvProbe* envProbe = nullptr;
};

#pragma endregion DebugDrawCommand_Probe

#pragma region IDebugDrawShape

void IDebugDrawShape::UpdateBufferData(const DebugDrawCommand& cmd, ImmediateDrawShaderData* bufferData) const
{
    *bufferData = ImmediateDrawShaderData {
        cmd.transformMatrix,
        cmd.color.Packed()
    };
}

#pragma endregion IDebugDrawShape

#pragma region MeshDebugDrawShapeBase

MeshDebugDrawShapeBase::MeshDebugDrawShapeBase(IDebugDrawCommandList& list, const Handle<Mesh>& mesh)
    : list(list),
      m_mesh(mesh)
{
    InitObject(m_mesh);
}

#pragma endregion MeshDebugDrawShapeBase

#pragma region SphereDebugDrawShape

SphereDebugDrawShape::SphereDebugDrawShape(IDebugDrawCommandList& list)
    : MeshDebugDrawShapeBase(list, MeshBuilder::NormalizedCubeSphere(4))
{
}

void SphereDebugDrawShape::operator()(const Vec3f& position, float radius, const Color& color)
{
    (*this)(position, radius, color, GetRenderableAttributes());
}

void SphereDebugDrawShape::operator()(const Vec3f& position, float radius, const Color& color, const RenderableAttributeSet& attributes)
{
    list.Push(MakeUnique<DebugDrawCommand>(DebugDrawCommand {
        this,
        Transform(position, radius, Quaternion::Identity()).GetMatrix(),
        color,
        attributes }));
}

#pragma endregion SphereDebugDrawShape

#pragma region AmbientProbeDebugDrawShape

void AmbientProbeDebugDrawShape::UpdateBufferData(const DebugDrawCommand& cmd, ImmediateDrawShaderData* bufferData) const
{
    IDebugDrawShape::UpdateBufferData(cmd, bufferData);

    const uint32 envProbeIndex = RenderApi_RetrieveResourceBinding(static_cast<const DebugDrawCommand_Probe&>(cmd).envProbe);

    bufferData->envProbeType = EPT_AMBIENT;
    bufferData->envProbeIndex = envProbeIndex;
}

void AmbientProbeDebugDrawShape::operator()(const Vec3f& position, float radius, const EnvProbe& envProbe)
{
    Assert(envProbe.IsReady());

    UniquePtr<DebugDrawCommand_Probe> command = MakeUnique<DebugDrawCommand_Probe>();
    command->shape = this;
    command->transformMatrix = Transform(position, radius, Quaternion::Identity()).GetMatrix();
    command->color = Color::White();
    command->envProbe = &envProbe;

    list.Push(std::move(command));
}

#pragma endregion AmbientProbeDebugDrawShape

#pragma region ReflectionProbeDebugDrawShape

void ReflectionProbeDebugDrawShape::UpdateBufferData(const DebugDrawCommand& cmd, ImmediateDrawShaderData* bufferData) const
{
    IDebugDrawShape::UpdateBufferData(cmd, bufferData);

    const uint32 envProbeIndex = RenderApi_RetrieveResourceBinding(static_cast<const DebugDrawCommand_Probe&>(cmd).envProbe);

    bufferData->envProbeType = EPT_REFLECTION;
    bufferData->envProbeIndex = envProbeIndex;
}

void ReflectionProbeDebugDrawShape::operator()(const Vec3f& position, float radius, const EnvProbe& envProbe)
{
    Assert(envProbe.IsReady());

    UniquePtr<DebugDrawCommand_Probe> command = MakeUnique<DebugDrawCommand_Probe>();
    command->shape = this;
    command->transformMatrix = Transform(position, radius, Quaternion::Identity()).GetMatrix();
    command->color = Color::White();
    command->envProbe = &envProbe;

    list.Push(std::move(command));
}

#pragma endregion ReflectionProbeDebugDrawShape

#pragma region BoxDebugDrawShape

BoxDebugDrawShape::BoxDebugDrawShape(IDebugDrawCommandList& list)
    : MeshDebugDrawShapeBase(list, MeshBuilder::Cube())
{
}

void BoxDebugDrawShape::operator()(const Vec3f& position, const Vec3f& size, const Color& color)
{
    (*this)(position, size, color, GetRenderableAttributes());
}

void BoxDebugDrawShape::operator()(const Vec3f& position, const Vec3f& size, const Color& color, const RenderableAttributeSet& attributes)
{
    list.Push(MakeUnique<DebugDrawCommand>(DebugDrawCommand {
        this,
        Transform(position, size, Quaternion::Identity()).GetMatrix(),
        color,
        attributes }));
}

#pragma endregion BoxDebugDrawShape

#pragma region PlaneDebugDrawShape

PlaneDebugDrawShape::PlaneDebugDrawShape(IDebugDrawCommandList& list)
    : MeshDebugDrawShapeBase(list, MeshBuilder::Quad())
{
}

void PlaneDebugDrawShape::operator()(const FixedArray<Vec3f, 4>& points, const Color& color)
{
    (*this)(points, color, GetRenderableAttributes());
}

void PlaneDebugDrawShape::operator()(const FixedArray<Vec3f, 4>& points, const Color& color, const RenderableAttributeSet& attributes)
{
    Vec3f x = (points[1] - points[0]).Normalize();
    Vec3f y = (points[2] - points[0]).Normalize();
    Vec3f z = x.Cross(y).Normalize();

    const Vec3f center = points.Avg();

    Matrix4 transformMatrix;
    transformMatrix.rows[0] = Vec4f(x, 0.0f);
    transformMatrix.rows[1] = Vec4f(y, 0.0f);
    transformMatrix.rows[2] = Vec4f(z, 0.0f);
    transformMatrix.rows[3] = Vec4f(center, 1.0f);

    list.Push(MakeUnique<DebugDrawCommand>(DebugDrawCommand {
        this,
        transformMatrix,
        color,
        attributes }));
}

#pragma endregion PlaneDebugDrawShape

#pragma region DebugDrawer

DebugDrawer::DebugDrawer()
    : m_config(DebugDrawerConfig::FromConfig()),
      m_isInitialized(false)
{
}

DebugDrawer::~DebugDrawer()
{
    m_shader.Reset();

    SafeRelease(std::move(m_instanceBuffers));
    SafeRelease(std::move(m_descriptorTable));
}

void DebugDrawer::Initialize()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

    Assert(!m_isInitialized.Get(MemoryOrder::ACQUIRE));

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        m_instanceBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(ImmediateDrawShaderData));
        DeferCreate(m_instanceBuffers[frameIndex]);
    }

    m_shader = g_shaderManager->GetOrCreate(
        NAME("DebugAABB"),
        ShaderProperties(staticMeshVertexAttributes, { { NAME("IMMEDIATE_MODE") } }));

    Assert(m_shader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    m_descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
    Assert(m_descriptorTable != nullptr);

    const uint32 debugDrawerDescriptorSetIndex = m_descriptorTable->GetDescriptorSetIndex(NAME("DebugDrawerDescriptorSet"));
    Assert(debugDrawerDescriptorSetIndex != ~0u);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& debugDrawerDescriptorSet = m_descriptorTable->GetDescriptorSet(debugDrawerDescriptorSetIndex, frameIndex);
        Assert(debugDrawerDescriptorSet != nullptr);

        debugDrawerDescriptorSet->SetElement(NAME("ImmediateDrawsBuffer"), m_instanceBuffers[frameIndex]);
    }

    DeferCreate(m_descriptorTable);

    m_isInitialized.Set(true, MemoryOrder::RELEASE);
}

void DebugDrawer::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    const uint32 idx = RenderApi_GetFrameIndex();

    if (m_commandLists[idx].Empty())
    {
        return;
    }

    for (DebugDrawCommandList& it : m_commandLists[idx])
    {
        m_drawCommands[idx].Concat(std::move(it.m_drawCommands));
    }

    m_commandLists[idx].Clear();
}

void DebugDrawer::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // wait for initialization on the game thread
    if (!m_isInitialized.Get(MemoryOrder::ACQUIRE))
    {
        return;
    }

    const uint32 idx = RenderApi_GetFrameIndex();

    if (!IsEnabled())
    {
        // clear, otherwise we'll start to leak a huge amount of memory
        m_drawCommands[idx].Clear();

        return;
    }

    if (m_drawCommands[idx].Empty())
    {
        return;
    }

    const uint32 frameIndex = frame->GetFrameIndex();

    GpuBufferRef& instanceBuffer = m_instanceBuffers[frameIndex];
    bool wasInstanceBufferRebuilt = false;

    if (m_drawCommands[idx].Size() * sizeof(ImmediateDrawShaderData) > instanceBuffer->Size())
    {
        HYPERION_ASSERT_RESULT(instanceBuffer->EnsureCapacity(
            m_drawCommands[idx].Size() * sizeof(ImmediateDrawShaderData),
            &wasInstanceBufferRebuilt));
    }

    const uint32 debugDrawerDescriptorSetIndex = m_descriptorTable->GetDescriptorSetIndex(NAME("DebugDrawerDescriptorSet"));
    Assert(debugDrawerDescriptorSetIndex != ~0u);

    const DescriptorSetRef& debugDrawerDescriptorSet = m_descriptorTable->GetDescriptorSet(debugDrawerDescriptorSetIndex, frameIndex);
    Assert(debugDrawerDescriptorSet != nullptr);

    // Update descriptor set if instance buffer was rebuilt
    if (wasInstanceBufferRebuilt)
    {
        debugDrawerDescriptorSet->SetElement(NAME("ImmediateDrawsBuffer"), instanceBuffer);

        debugDrawerDescriptorSet->Update();
    }

    Array<ImmediateDrawShaderData> shaderData;
    shaderData.Resize(m_drawCommands[idx].Size());

    for (SizeType drawCommandIdx = 0; drawCommandIdx < m_drawCommands[idx].Size(); drawCommandIdx++)
    {
        const DebugDrawCommand& drawCommand = *m_drawCommands[idx][drawCommandIdx];

        uint32 envProbeType = uint32(EPT_INVALID);
        uint32 envProbeIndex = ~0u;

        drawCommand.shape->UpdateBufferData(drawCommand, &shaderData[drawCommandIdx]);
    }

    instanceBuffer->Copy(shaderData.ByteSize(), shaderData.Data());

    struct
    {
        HashCode attributesHashCode;
        GraphicsPipelineRef graphicsPipeline;
        uint32 drawableLayer = ~0u;
    } previousState;

    for (SizeType drawCommandIdx = 0; drawCommandIdx < m_drawCommands[idx].Size(); drawCommandIdx++)
    {
        const DebugDrawCommand& drawCommand = *m_drawCommands[idx][drawCommandIdx];

        bool isNewGraphicsPipeline = false;

        GraphicsPipelineRef& graphicsPipeline = previousState.graphicsPipeline;

        if (!graphicsPipeline.IsValid() || previousState.attributesHashCode != drawCommand.attributes.GetHashCode())
        {
            graphicsPipeline = FetchGraphicsPipeline(drawCommand.attributes, ++previousState.drawableLayer, renderSetup.passData);
            previousState.attributesHashCode = drawCommand.attributes.GetHashCode();

            isNewGraphicsPipeline = true;
        }

        if (isNewGraphicsPipeline)
        {
            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline);

            frame->renderQueue << BindDescriptorTable(
                m_descriptorTable,
                graphicsPipeline,
                ArrayMap<Name, ArrayMap<Name, uint32>> {
                    { NAME("DebugDrawerDescriptorSet"),
                        { { NAME("ImmediateDrawsBuffer"), ShaderDataOffset<ImmediateDrawShaderData>(0u) } } },
                    { NAME("Global"),
                        { { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                            { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) } } },
                    { NAME("Object"), {} },
                    { NAME("Instancing"), { { NAME("EntityInstanceBatchesBuffer"), 0 } } } },
                frameIndex);
        }

        frame->renderQueue << BindDescriptorSet(
            debugDrawerDescriptorSet,
            graphicsPipeline,
            ArrayMap<Name, uint32> {
                { NAME("ImmediateDrawsBuffer"), ShaderDataOffset<ImmediateDrawShaderData>(uint32(drawCommandIdx)) } },
            debugDrawerDescriptorSetIndex);

        switch (drawCommand.shape->GetDebugDrawType())
        {
        case DebugDrawType::MESH:
        {
            MeshDebugDrawShapeBase* meshShape = static_cast<MeshDebugDrawShapeBase*>(drawCommand.shape);

            frame->renderQueue << BindVertexBuffer(meshShape->GetMesh()->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(meshShape->GetMesh()->GetIndexBuffer());
            frame->renderQueue << DrawIndexed(meshShape->GetMesh()->NumIndices());

            break;
        }
        default:
            HYP_UNREACHABLE();
        }
    }

    m_drawCommands[idx].Clear();
}

DebugDrawCommandList& DebugDrawer::CreateCommandList()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | g_renderThread);

    const uint32 idx = RenderApi_GetFrameIndex();

    return m_commandLists[idx].EmplaceBack();
}

GraphicsPipelineRef DebugDrawer::FetchGraphicsPipeline(RenderableAttributeSet attributes, uint32 drawableLayer, PassData* passData)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(passData != nullptr);

    attributes.SetDrawableLayer(drawableLayer);

    GraphicsPipelineRef graphicsPipeline;

    auto it = m_graphicsPipelines.Find(attributes);

    if (it != m_graphicsPipelines.End())
    {
        graphicsPipeline = it->second.Lock();
    }

    if (!graphicsPipeline)
    {
        Handle<View> view = passData->view.Lock();
        Assert(view.IsValid());

        graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
            m_shader,
            m_descriptorTable,
            view->GetOutputTarget().GetFramebuffers(),
            attributes);

        if (it != m_graphicsPipelines.End())
        {
            it->second = graphicsPipeline;
        }
        else
        {
            m_graphicsPipelines.Insert(attributes, graphicsPipeline);
        }
    }

    return graphicsPipeline;
}

#pragma endregion DebugDrawer

#pragma region DebugDrawCommandList

void DebugDrawCommandList::Push(UniquePtr<DebugDrawCommand>&& command)
{
    HYP_SCOPE;

    m_drawCommands.PushBack(std::move(command));
}

#pragma endregion DebugDrawCommandList

} // namespace hyperion
