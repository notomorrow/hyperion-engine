/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RAYTRACING_REFLECTIONS_RENDERER_HPP
#define HYPERION_RAYTRACING_REFLECTIONS_RENDERER_HPP

#include <Constants.hpp>

#include <rendering/ShaderManager.hpp>
#include <rendering/TemporalBlending.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <core/config/Config.hpp>

namespace hyperion {

class Engine;
class GBuffer;
class RenderView;

struct RenderCommand_DestroyRaytracingReflections;
struct RenderCommand_CreateRTRadianceImageOutputs;

HYP_STRUCT(ConfigName = "app", JSONPath = "rendering.rt.reflections")
struct RaytracingReflectionsConfig : public ConfigBase<RaytracingReflectionsConfig>
{
    HYP_FIELD()
    Vec2u extent = { 1024, 1024 };

    HYP_FIELD()
    bool pathTracing = false;

    virtual ~RaytracingReflectionsConfig() override = default;

    bool Validate() const
    {
        return extent.x * extent.y != 0;
    }
};

class RaytracingReflections
{
public:
    friend struct RenderCommand_DestroyRaytracingReflections;
    friend struct RenderCommand_CreateRTRadianceImageOutputs;

    HYP_API RaytracingReflections(
        RaytracingReflectionsConfig&& config,
        GBuffer* gbuffer);

    HYP_API ~RaytracingReflections();

    HYP_FORCE_INLINE bool IsPathTracer() const
    {
        return m_config.pathTracing;
    }

    HYP_FORCE_INLINE void SetTopLevelAccelerationStructures(const FixedArray<TLASRef, maxFramesInFlight>& tlas)
    {
        m_topLevelAccelerationStructures = tlas;
    }

    HYP_API void ApplyTLASUpdates(RTUpdateStateFlags flags);

    HYP_API void Create();

    HYP_API void Render(FrameBase* frame, const RenderSetup& renderSetup);

private:
    void CreateImages();
    void CreateUniformBuffer();
    void CreateRaytracingPipeline();
    void CreateTemporalBlending();
    void UpdateUniforms(FrameBase* frame, const RenderSetup& renderSetup);

    RaytracingReflectionsConfig m_config;

    GBuffer* m_gbuffer;

    FixedArray<TLASRef, maxFramesInFlight> m_topLevelAccelerationStructures;

    FixedArray<uint32, maxFramesInFlight> m_updates;

    ShaderRef m_shader;

    Handle<Texture> m_texture;
    UniquePtr<TemporalBlending> m_temporalBlending;

    RaytracingPipelineRef m_raytracingPipeline;
    FixedArray<GpuBufferRef, maxFramesInFlight> m_uniformBuffers;

    Matrix4 m_previousViewMatrix;
};

} // namespace hyperion

#endif