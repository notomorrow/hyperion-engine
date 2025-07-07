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
struct RenderSetup;

HYP_STRUCT(ConfigName = "app", JSONPath = "rendering.debug.debug_drawer")
struct DebugDrawerConfig : public ConfigBase<DebugDrawerConfig>
{
    HYP_FIELD(Description = "Enable or disable the debug drawer.", JSONPath = "enabled")
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
    virtual void Commit() = 0;
};

class IDebugDrawShape
{
public:
    virtual ~IDebugDrawShape() = default;

    virtual DebugDrawType GetDebugDrawType() const = 0;
};

class HYP_API MeshDebugDrawShapeBase : public IDebugDrawShape
{
public:
    MeshDebugDrawShapeBase(IDebugDrawCommandList& commandList, const Handle<Mesh>& mesh);

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
    IDebugDrawCommandList& m_commandList;
    Handle<Mesh> m_mesh;
};

class HYP_API SphereDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    SphereDebugDrawShape(IDebugDrawCommandList& commandList);

    virtual ~SphereDebugDrawShape() override = default;

    void operator()(const Vec3f& position, float radius, const Color& color);
    void operator()(const Vec3f& position, float radius, const Color& color, const RenderableAttributeSet& attributes);
};

class HYP_API AmbientProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    AmbientProbeDebugDrawShape(IDebugDrawCommandList& commandList)
        : SphereDebugDrawShape(commandList)
    {
    }

    virtual ~AmbientProbeDebugDrawShape() override = default;

    void operator()(const Vec3f& position, float radius, const EnvProbe& envProbe);
};

class HYP_API ReflectionProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    ReflectionProbeDebugDrawShape(IDebugDrawCommandList& commandList)
        : SphereDebugDrawShape(commandList)
    {
    }

    virtual ~ReflectionProbeDebugDrawShape() override = default;

    void operator()(const Vec3f& position, float radius, const EnvProbe& envProbe);
};

class HYP_API BoxDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    BoxDebugDrawShape(IDebugDrawCommandList& commandList);

    virtual ~BoxDebugDrawShape() override = default;

    void operator()(const Vec3f& position, const Vec3f& size, const Color& color);
    void operator()(const Vec3f& position, const Vec3f& size, const Color& color, const RenderableAttributeSet& attributes);
};

class HYP_API PlaneDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    PlaneDebugDrawShape(IDebugDrawCommandList& commandList);

    virtual ~PlaneDebugDrawShape() override = default;

    void operator()(const FixedArray<Vec3f, 4>& points, const Color& color);
    void operator()(const FixedArray<Vec3f, 4>& points, const Color& color, const RenderableAttributeSet& attributes);
};

class DebugDrawCommandList final : public IDebugDrawCommandList
{
public:
    friend class DebugDrawer;

    DebugDrawCommandList(DebugDrawer& debugDrawer)
        : Sphere(*this),
          AmbientProbe(*this),
          ReflectionProbe(*this),
          Box(*this),
          Plane(*this),
          m_debugDrawer(debugDrawer)
    {
    }

    DebugDrawCommandList(const DebugDrawCommandList& other) = delete;
    DebugDrawCommandList& operator=(const DebugDrawCommandList& other) = delete;

    virtual ~DebugDrawCommandList() override = default;

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return m_numDrawCommands.Get(MemoryOrder::ACQUIRE) == 0;
    }

    virtual void Push(UniquePtr<DebugDrawCommand>&& command) override;
    virtual void Commit() override;

    SphereDebugDrawShape Sphere;
    AmbientProbeDebugDrawShape AmbientProbe;
    ReflectionProbeDebugDrawShape ReflectionProbe;
    BoxDebugDrawShape Box;
    PlaneDebugDrawShape Plane;

private:
    DebugDrawer& m_debugDrawer;
    Array<UniquePtr<DebugDrawCommand>> m_drawCommands;
    Mutex m_drawCommandsMutex;
    AtomicVar<uint32> m_numDrawCommands { 0 };
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

    UniquePtr<DebugDrawCommandList> CreateCommandList();
    void CommitCommands(DebugDrawCommandList& commandList);

private:
    void UpdateDrawCommands();

    DebugDrawerConfig m_config;

    DebugDrawCommandList m_defaultCommandList;
    AtomicVar<bool> m_isInitialized;

public: // Shapes
    SphereDebugDrawShape& Sphere;
    AmbientProbeDebugDrawShape& AmbientProbe;
    ReflectionProbeDebugDrawShape& ReflectionProbe;
    BoxDebugDrawShape& Box;
    PlaneDebugDrawShape& Plane;

private:
    GraphicsPipelineRef FetchGraphicsPipeline(RenderableAttributeSet attributes, uint32 drawableLayer);

    ShaderRef m_shader;
    DescriptorTableRef m_descriptorTable;
    HashMap<RenderableAttributeSet, GraphicsPipelineWeakRef> m_graphicsPipelines;

    Array<UniquePtr<DebugDrawCommand>> m_drawCommands;
    Array<UniquePtr<DebugDrawCommand>> m_drawCommandsPendingAddition;
    AtomicVar<uint32> m_numDrawCommandsPendingAddition;
    Mutex m_drawCommandsMutex;

    FixedArray<GpuBufferRef, g_framesInFlight> m_instanceBuffers;
};

} // namespace hyperion
