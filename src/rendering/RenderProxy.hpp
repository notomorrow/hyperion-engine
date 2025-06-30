/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_PROXY_HPP
#define HYPERION_RENDER_PROXY_HPP

#include <core/object/ObjId.hpp>

#include <core/utilities/UserData.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/containers/Bitset.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/math/Transform.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Matrix4.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/MeshInstanceData.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <util/ResourceTracker.hpp>

namespace hyperion {

class Entity;
class Mesh;
class Material;
class Skeleton;
class EnvProbe;
class EnvGrid;

HYP_STRUCT()
struct MeshRaytracingData
{
    HYP_FIELD()
    FixedArray<BLASRef, maxFramesInFlight> bottomLevelAccelerationStructures;
};

class IRenderProxy
{
public:
    virtual ~IRenderProxy() = default;

    virtual void SafeRelease()
    {
    }

#ifdef HYP_DEBUG_MODE
    uint16 state = 0x1;
#endif
};

/*! \brief Proxy for a renderable Entity with a valid Mesh and Material assigned */
class RenderProxy : public IRenderProxy
{
public:
    WeakHandle<Entity> entity;
    Handle<Mesh> mesh;
    Handle<Material> material;
    Handle<Skeleton> skeleton;
    Matrix4 modelMatrix;
    Matrix4 previousModelMatrix;
    BoundingBox aabb;
    UserData<32, 16> userData;
    MeshInstanceData instanceData;
    int version = 0;

    ~RenderProxy() override = default;

    HYP_API virtual void SafeRelease() override;

    void IncRefs() const;
    void DecRefs() const;

    bool operator==(const RenderProxy& other) const
    {
        // Check version first for faster comparison
        return version == other.version
            && entity == other.entity
            && mesh == other.mesh
            && material == other.material
            && skeleton == other.skeleton
            && modelMatrix == other.modelMatrix
            && previousModelMatrix == other.previousModelMatrix
            && aabb == other.aabb
            && userData == other.userData
            && instanceData == other.instanceData;
    }

    bool operator!=(const RenderProxy& other) const
    {
        // Check version first for faster comparison
        return version != other.version
            || entity != other.entity
            || mesh != other.mesh
            || material != other.material
            || skeleton != other.skeleton
            || modelMatrix != other.modelMatrix
            || previousModelMatrix != other.previousModelMatrix
            || aabb != other.aabb
            || userData != other.userData
            || instanceData != other.instanceData;
    }
};

struct EnvProbeSphericalHarmonics
{
    Vec4f values[9];
};

struct EnvProbeShaderData
{
    Matrix4 faceViewMatrices[6];

    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4f worldPosition;

    uint32 textureIndex = ~0u;
    uint32 flags = 0;
    float cameraNear = 0.01f;
    float cameraFar = 100.0f;

    Vec2u dimensions;
    uint64 visibilityBits = 0;
    Vec4i positionInGrid;

    EnvProbeSphericalHarmonics sh;
};

class RenderProxyEnvProbe : public IRenderProxy
{
public:
    ~RenderProxyEnvProbe() override = default;

    WeakHandle<EnvProbe> envProbe;
    EnvProbeShaderData bufferData {};
};

struct EnvGridShaderData
{
    uint32 probeIndices[maxBoundAmbientProbes];

    Vec4f center;
    Vec4f extent;
    Vec4f aabbMax;
    Vec4f aabbMin;

    Vec4u density;

    Vec4f voxelGridAabbMax;
    Vec4f voxelGridAabbMin;

    Vec2i lightFieldImageDimensions;
    Vec2i irradianceOctahedronSize;
};

class RenderProxyEnvGrid : public IRenderProxy
{
public:
    ~RenderProxyEnvGrid() override = default;

    WeakHandle<EnvGrid> envGrid;
    EnvGridShaderData bufferData {};
};

struct LightShaderData
{
    uint32 lightId;
    uint32 lightType;
    uint32 colorPacked;
    float radius;
    // 16

    float falloff;
    uint32 shadowMapIndex;
    Vec2f areaSize;
    // 32

    Vec4f positionIntensity;
    Vec4f normal;
    // 64

    Vec2f spotAngles;
    uint32 materialIndex;
    uint32 _pad2;

    Vec4f aabbMin;
    Vec4f aabbMax;

    Vec4u pad5;
};

static_assert(sizeof(LightShaderData) == 128);

class RenderProxyLight : public IRenderProxy
{
public:
    ~RenderProxyLight() override = default;

    WeakHandle<Light> light;
    LightShaderData bufferData {};
};

struct LightmapVolumeShaderData
{
    Vec4f aabbMax;
    Vec4f aabbMin;

    uint32 textureIndex;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;
};

class RenderProxyLightmapVolume : public IRenderProxy
{
public:
    ~RenderProxyLightmapVolume() override = default;

    WeakHandle<LightmapVolume> lightmapVolume;
    LightmapVolumeShaderData bufferData {};
};

} // namespace hyperion

#endif
