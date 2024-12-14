/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCREENSPACE_REFLECTION_RENDERER_HPP
#define HYPERION_SCREENSPACE_REFLECTION_RENDERER_HPP

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/config/Config.hpp>

#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

class Engine;

struct RenderCommand_CreateSSRImageOutputs;
struct RenderCommand_DestroySSRInstance;

enum class SSRRendererOptions : uint32
{
    NONE                    = 0x0,
    CONE_TRACING            = 0x1,
    ROUGHNESS_SCATTERING    = 0x2
};

HYP_MAKE_ENUM_FLAGS(SSRRendererOptions)

class SSRRendererConfig final : public ConfigBase<SSRRendererConfig>
{
public:
    SSRRendererConfig()
        : m_options(SSRRendererOptions::NONE)
    {
    }

    SSRRendererConfig(const SSRRendererConfig &other)                   = default;
    SSRRendererConfig &operator=(const SSRRendererConfig &other)        = delete;
    SSRRendererConfig(SSRRendererConfig &&other) noexcept               = default;
    SSRRendererConfig &operator=(SSRRendererConfig &&other) noexcept    = delete;

    virtual ~SSRRendererConfig() override                               = default;

    HYP_FORCE_INLINE Vec2u GetExtent() const
        { return m_extent; }

    HYP_FORCE_INLINE bool IsConeTracingEnabled() const
        { return m_options & SSRRendererOptions::CONE_TRACING; }

    HYP_FORCE_INLINE bool IsRoughnessScatteringEnabled() const
        { return m_options & SSRRendererOptions::ROUGHNESS_SCATTERING; }

protected:
    static HYP_API Vec2u GetSwapchainExtent();

    virtual SSRRendererConfig Default_Internal() const override
    {
        SSRRendererConfig result;
        result.m_options = SSRRendererOptions::ROUGHNESS_SCATTERING | SSRRendererOptions::CONE_TRACING;
        result.m_extent = GetSwapchainExtent();

        return result;
    }

    virtual SSRRendererConfig FromConfig_Internal() const override
    {
        SSRRendererConfig result = Default();

        const String ssr_quality = Get("rendering.ssr.quality").ToString().ToLower();

        if (ssr_quality == "low") {
            result.m_extent /= 4;
        } else if (ssr_quality == "medium") {
            result.m_extent /= 2;
        }

        const json::JSONValue roughness_scattering_option = Get("rendering.ssr.roughness_scattering");
        const json::JSONValue cone_tracing_option = Get("rendering.ssr.cone_tracing");

        if (!roughness_scattering_option.IsUndefined() || !cone_tracing_option.IsUndefined()) {
            result.m_options = SSRRendererOptions::NONE;
        }

        if (roughness_scattering_option.ToBool()) {
            result.m_options |= SSRRendererOptions::ROUGHNESS_SCATTERING;
        }

        if (cone_tracing_option.ToBool()) {
            result.m_options |= SSRRendererOptions::CONE_TRACING;
        }

        return result;
    }

    EnumFlags<SSRRendererOptions>   m_options;
    Vec2u                           m_extent;
};


class SSRRenderer
{
public:
    friend struct RenderCommand_CreateSSRImageOutputs;
    friend struct RenderCommand_DestroySSRInstance;

    SSRRenderer(SSRRendererConfig &&config);
    ~SSRRenderer();

    HYP_FORCE_INLINE bool IsRendered() const
        { return m_is_rendered; }

    void Create();
    void Destroy();

    void Render(Frame *frame);

private:
    ShaderProperties GetShaderProperties() const;

    void CreateUniformBuffers();
    void CreateBlueNoiseBuffer();
    void CreateComputePipelines();

    FixedArray<Handle<Texture>, 4>  m_image_outputs;
    
    GPUBufferRef                    m_uniform_buffer;
    
    ComputePipelineRef              m_write_uvs;
    ComputePipelineRef              m_sample_gbuffer;

    UniquePtr<TemporalBlending>     m_temporal_blending;

    SSRRendererConfig               m_config;

    bool                            m_is_rendered;
};

} // namespace hyperion

#endif