#ifndef HYPERION_V2_PROBE_SYSTEM_H
#define HYPERION_V2_PROBE_SYSTEM_H

#include "shader.h"
#include <rendering/backend/rt/renderer_raytracing_pipeline.h>
#include <rendering/backend/renderer_image.h>

#include <math/bounding_box.h>

namespace hyperion::v2 {

using renderer::RaytracingPipeline;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::UniformBuffer;

class Engine;

struct alignas(256) ProbeSystemUniforms {
    Vector4  aabb_max;
    Vector4  aabb_min;
    uint32_t probe_border_x;
    uint32_t probe_border_y;
    uint32_t probe_border_z;
    float    probe_distance;
};

struct ProbeSystemSetup {
    static constexpr uint8_t num_rays_per_probe = 128;

    BoundingBox aabb;
    Vector3     probe_border{2, 0, 2};
    float       probe_distance;

    const Vector3 &GetOrigin() const
        { return aabb.min; }

    Extent3D NumProbesPerDimension() const
    {
        const auto probes_per_dimension = MathUtil::Ceil((aabb.GetDimensions() / probe_distance) + probe_border);

        return {
            static_cast<decltype(Extent3D::width)>(probes_per_dimension.x),
            static_cast<decltype(Extent3D::height)>(probes_per_dimension.y),
            static_cast<decltype(Extent3D::depth)>(probes_per_dimension.z)
        };
    }

    uint32_t NumProbes() const
    {
        const Extent3D per_dimension = NumProbesPerDimension();

        return per_dimension.width * per_dimension.height * per_dimension.depth;
    }
};

struct Probe {
    Vector3 position;

    void Render(Engine *engine);
};

class ProbeSystem {
public:
    ProbeSystem(ProbeSystemSetup &&setup);
    ProbeSystem(const ProbeSystem &other) = delete;
    ProbeSystem &operator=(const ProbeSystem &other) = delete;
    ~ProbeSystem();

    void Init(Engine *engine);
    void RenderProbes(Engine *engine);

private:
    void CreatePipeline(Engine *engine);
    void CreateUniformBuffer(Engine *engine);
    void CreateStorageImages(Engine *engine);
    void AddDescriptors(Engine *engine);

    ProbeSystemSetup   m_setup;
    std::vector<Probe> m_probes;

    std::unique_ptr<RaytracingPipeline> m_pipeline;
    std::unique_ptr<UniformBuffer>      m_probe_system_uniforms;
    std::unique_ptr<StorageImage>       m_radiance_image;
    std::unique_ptr<ImageView>          m_radiance_image_view;
};

} // namespace hyperion::v2

#endif