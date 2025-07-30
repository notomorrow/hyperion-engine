/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Constants.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/Handle.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/config/Config.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Color.hpp>
#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderStructs.hpp>

#include <GameCounter.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class RenderGroup;
class Mesh;
class DebugDrawer;
class DebugDrawCommandList;
class IDebugDrawShape;
class UIObject;
class UIStage;
class PassData;
struct RenderSetup;
struct ImmediateDrawShaderData;

HYP_STRUCT(ConfigName = "app", JsonPath = "rendering.debug.debug_drawer")
struct DebugDrawerConfig : public ConfigBase<DebugDrawerConfig>
{
    HYP_FIELD(Description = "Enable or disable the debug drawer.", JsonPath = "enabled")
    bool enabled = false;

    virtual ~DebugDrawerConfig() override = default;
};

enum class DebugDrawType : int
{
    MESH = 0
};

struct DebugDrawCommand
{
    IDebugDrawShape* shape;
    Matrix4 transformMatrix;
    Color color;
    RenderableAttributeSet attributes;
};

class IDebugDrawCommandList
{
public:
    virtual ~IDebugDrawCommandList() = default;

    virtual void Push(UniquePtr<DebugDrawCommand>&& command) = 0;
};

class IDebugDrawShape
{
public:
    virtual ~IDebugDrawShape() = default;

    virtual DebugDrawType GetDebugDrawType() const = 0;
    virtual void UpdateBufferData(const DebugDrawCommand& cmd, ImmediateDrawShaderData* bufferData) const;
};

class HYP_API MeshDebugDrawShapeBase : public IDebugDrawShape
{
public:
    MeshDebugDrawShapeBase(IDebugDrawCommandList& list, const Handle<Mesh>& mesh);

    virtual ~MeshDebugDrawShapeBase() override = default;

    virtual DebugDrawType GetDebugDrawType() const override
    {
        return DebugDrawType::MESH;
    }

    HYP_FORCE_INLINE const Handle<Mesh>& GetMesh() const
    {
        return m_mesh;
    }

protected:
    IDebugDrawCommandList& list;
    Handle<Mesh> m_mesh;
};

class HYP_API SphereDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    SphereDebugDrawShape(IDebugDrawCommandList& list);

    virtual ~SphereDebugDrawShape() override = default;

    void operator()(const Vec3f& position, float radius, const Color& color);
    void operator()(const Vec3f& position, float radius, const Color& color, const RenderableAttributeSet& attributes);
};

class HYP_API AmbientProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    AmbientProbeDebugDrawShape(IDebugDrawCommandList& list)
        : SphereDebugDrawShape(list)
    {
    }

    virtual ~AmbientProbeDebugDrawShape() override = default;

    virtual void UpdateBufferData(const DebugDrawCommand& cmd, ImmediateDrawShaderData* bufferData) const override;

    void operator()(const Vec3f& position, float radius, const EnvProbe& envProbe);
};

class HYP_API ReflectionProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    ReflectionProbeDebugDrawShape(IDebugDrawCommandList& list)
        : SphereDebugDrawShape(list)
    {
    }

    virtual ~ReflectionProbeDebugDrawShape() override = default;

    virtual void UpdateBufferData(const DebugDrawCommand& cmd, ImmediateDrawShaderData* bufferData) const override;

    void operator()(const Vec3f& position, float radius, const EnvProbe& envProbe);
};

class HYP_API BoxDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    BoxDebugDrawShape(IDebugDrawCommandList& list);

    virtual ~BoxDebugDrawShape() override = default;

    void operator()(const Vec3f& position, const Vec3f& size, const Color& color);
    void operator()(const Vec3f& position, const Vec3f& size, const Color& color, const RenderableAttributeSet& attributes);
};

class HYP_API PlaneDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    PlaneDebugDrawShape(IDebugDrawCommandList& list);

    virtual ~PlaneDebugDrawShape() override = default;

    void operator()(const FixedArray<Vec3f, 4>& points, const Color& color);
    void operator()(const FixedArray<Vec3f, 4>& points, const Color& color, const RenderableAttributeSet& attributes);
};

class DebugDrawCommandList final : public IDebugDrawCommandList
{
public:
    friend class DebugDrawer;

    DebugDrawCommandList()
        : sphere(*this),
          ambientProbe(*this),
          reflectionProbe(*this),
          box(*this),
          plane(*this)
    {
    }

    DebugDrawCommandList(const DebugDrawCommandList& other) = delete;
    DebugDrawCommandList& operator=(const DebugDrawCommandList& other) = delete;

    DebugDrawCommandList(DebugDrawCommandList&& other) noexcept = delete;
    DebugDrawCommandList& operator=(DebugDrawCommandList&& other) noexcept = delete;

    virtual ~DebugDrawCommandList() override = default;

    virtual void Push(UniquePtr<DebugDrawCommand>&& command) override;

    SphereDebugDrawShape sphere;
    AmbientProbeDebugDrawShape ambientProbe;
    ReflectionProbeDebugDrawShape reflectionProbe;
    BoxDebugDrawShape box;
    PlaneDebugDrawShape plane;

private:
    Array<UniquePtr<DebugDrawCommand>> m_drawCommands;
};

class IDebugDrawer
{
public:
    virtual ~IDebugDrawer() = default;

    virtual bool IsEnabled() const = 0;

    virtual void Initialize() = 0;
    virtual void Update(float delta) = 0;
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup) = 0;
};

class HYP_API DebugDrawer final : public IDebugDrawer
{
public:
    DebugDrawer();
    virtual ~DebugDrawer() override;

    virtual bool IsEnabled() const override
    {
        return m_config.enabled;
    }

    virtual void Initialize() override;
    virtual void Update(float delta) override;
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup) override;

    DebugDrawCommandList& CreateCommandList();

private:
    DebugDrawerConfig m_config;

    AtomicVar<bool> m_isInitialized;

private:
    GraphicsPipelineRef FetchGraphicsPipeline(RenderableAttributeSet attributes, uint32 drawableLayer, PassData* passData);

    ShaderRef m_shader;
    DescriptorTableRef m_descriptorTable;
    HashMap<RenderableAttributeSet, GraphicsPipelineWeakRef> m_graphicsPipelines;

    FixedArray<Array<UniquePtr<DebugDrawCommand>>, g_tripleBuffer ? 3 : 2> m_drawCommands;
    FixedArray<LinkedList<DebugDrawCommandList>, g_tripleBuffer ? 3 : 2> m_commandLists;

    FixedArray<GpuBufferRef, g_framesInFlight> m_instanceBuffers;
};

} // namespace hyperion
