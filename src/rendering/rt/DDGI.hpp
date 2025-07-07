/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/ShaderManager.hpp>

#include <rendering/RenderStructs.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/rt/RenderRaytracingPipeline.hpp>

#include <core/math/BoundingBox.hpp>

#include <Types.hpp>

#include <random>

namespace hyperion {

struct RenderSetup;

class Engine;

enum ProbeSystemFlags : uint32
{
    PROBE_SYSTEM_FLAGS_NONE = 0x0,
    PROBE_SYSTEM_FLAGS_FIRST_RUN = 0x1
};

struct ProbeRayData
{
    Vec4f directionDepth;
    Vec4f origin;
    Vec4f normal;
    Vec4f color;
};

static_assert(sizeof(ProbeRayData) == 64);

struct DDGIUniforms
{
    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4u probeBorder;
    Vec4u probeCounts;
    Vec4u gridDimensions;
    Vec4u imageDimensions;
    Vec4u params; // x = probe distance, y = num rays per probe, z = flags, w = num bound lights
    uint32 shadowMapIndex;
    uint32 _pad0, _pad1, _pad2;
    uint32 lightIndices[16];
};

struct DDGIInfo
{
    static constexpr uint32 irradianceOctahedronSize = 8;
    static constexpr uint32 depthOctahedronSize = 16;
    static constexpr Vec3u probeBorder = Vec3u { 2, 0, 2 };

    BoundingBox aabb;
    float probeDistance = 2.5f;
    uint32 numRaysPerProbe = 16;

    HYP_FORCE_INLINE const Vec3f& GetOrigin() const
    {
        return aabb.min;
    }

    HYP_FORCE_INLINE Vec3u NumProbesPerDimension() const
    {
        const Vec3f probesPerDimension = MathUtil::Ceil((aabb.GetExtent() / probeDistance) + Vec3f(probeBorder));

        return Vec3u(probesPerDimension);
    }

    HYP_FORCE_INLINE uint32 NumProbes() const
    {
        const Vec3u perDimension = NumProbesPerDimension();

        return perDimension.x * perDimension.y * perDimension.z;
    }

    HYP_FORCE_INLINE Vec2u GetImageDimensions() const
    {
        return { uint32(MathUtil::NextPowerOf2(NumProbes())), numRaysPerProbe };
    }
};

struct RotationMatrixGenerator
{
    Matrix4 matrix;
    std::random_device randomDevice;
    std::mt19937 mt { randomDevice() };
    std::uniform_real_distribution<float> angle { 0.0f, 359.0f };
    std::uniform_real_distribution<float> axis { -1.0f, 1.0f };

    const Matrix4& Next()
    {
        return matrix = Matrix4::Rotation({ Vec3f { axis(mt), axis(mt), axis(mt) }.Normalize(),
                   MathUtil::DegToRad(angle(mt)) });
    }
};

struct Probe
{
    Vec3f position;
};

class DDGI
{
public:
    HYP_API DDGI(DDGIInfo&& gridInfo);
    DDGI(const DDGI& other) = delete;
    DDGI& operator=(const DDGI& other) = delete;
    HYP_API ~DDGI();

    const Array<Probe>& GetProbes() const
    {
        return m_probes;
    }

    HYP_FORCE_INLINE void SetTopLevelAccelerationStructures(const FixedArray<TLASRef, g_framesInFlight>& topLevelAccelerationStructures)
    {
        m_topLevelAccelerationStructures = topLevelAccelerationStructures;
    }

    HYP_API void ApplyTLASUpdates(RTUpdateStateFlags flags);

    const GpuBufferRef& GetRadianceBuffer() const
    {
        return m_radianceBuffer;
    }

    const ImageRef& GetIrradianceImage() const
    {
        return m_irradianceImage;
    }

    const ImageViewRef& GetIrradianceImageView() const
    {
        return m_irradianceImageView;
    }

    HYP_API void Create();

    HYP_API void Render(FrameBase* frame, const RenderSetup& renderSetup);

private:
    void CreatePipelines();
    void CreateUniformBuffer();
    void CreateStorageBuffers();
    void UpdateUniforms(FrameBase* frame);

    DDGIInfo m_gridInfo;
    Array<Probe> m_probes;

    FixedArray<uint32, g_framesInFlight> m_updates;

    ComputePipelineRef m_updateIrradiance;
    ComputePipelineRef m_updateDepth;
    ComputePipelineRef m_copyBorderTexelsIrradiance;
    ComputePipelineRef m_copyBorderTexelsDepth;

    ShaderRef m_shader;

    RaytracingPipelineRef m_pipeline;

    GpuBufferRef m_uniformBuffer;
    GpuBufferRef m_radianceBuffer;

    ImageRef m_irradianceImage;
    ImageViewRef m_irradianceImageView;
    ImageRef m_depthImage;
    ImageViewRef m_depthImageView;

    FixedArray<TLASRef, g_framesInFlight> m_topLevelAccelerationStructures;

    DDGIUniforms m_uniforms;

    RotationMatrixGenerator m_randomGenerator;
    uint32 m_time;
};

} // namespace hyperion
