#ifndef HYPERION_RENDERER_RAYTRACING_PIPELINE_H
#define HYPERION_RENDERER_RAYTRACING_PIPELINE_H

#include <core/lib/DynArray.hpp>

#include <Types.hpp>

#include "../RendererPipeline.hpp"
#include "../RendererDevice.hpp"
#include "../RendererBuffer.hpp"
#include "../RendererShader.hpp"
#include "../RendererDescriptorSet.hpp"

#include <memory>

namespace hyperion {
namespace renderer {

class RaytracingPipeline : public Pipeline
{
public:
    RaytracingPipeline();
    RaytracingPipeline(
        const DynArray<const DescriptorSet *> &used_descriptor_sets
    );
    RaytracingPipeline(const RaytracingPipeline &other) = delete;
    RaytracingPipeline &operator=(const RaytracingPipeline &other) = delete;
    ~RaytracingPipeline();

    Result Create(
        Device *device,
        ShaderProgram *shader_program,
        DescriptorPool *pool
    );
    Result Destroy(Device *device);

    void Bind(CommandBuffer *command_buffer);
    void SubmitPushConstants(CommandBuffer *cmd) const;
    void TraceRays(
        Device *device,
        CommandBuffer *command_buffer,
        Extent3D extent
    ) const;

private:
    struct ShaderBindingTableEntry {
        std::unique_ptr<ShaderBindingTableBuffer> buffer;
        VkStridedDeviceAddressRegionKHR           strided_device_address_region;
    };

    struct {
        VkStridedDeviceAddressRegionKHR ray_gen{};
        VkStridedDeviceAddressRegionKHR ray_miss{};
        VkStridedDeviceAddressRegionKHR closest_hit{};
        VkStridedDeviceAddressRegionKHR callable{};
    } m_shader_binding_table_entries;

    using ShaderBindingTableMap = std::unordered_map<ShaderModule::Type, ShaderBindingTableEntry>;

    Result CreateShaderBindingTables(Device *device, ShaderProgram *shader_program);

    Result CreateShaderBindingTableEntry(
        Device *device,
        UInt32 num_shaders,
        ShaderBindingTableEntry &out
    );

    ShaderBindingTableMap m_shader_binding_table_buffers;
};

} // namespace renderer
}; // namespace hyperion

#endif //HYPERION_RENDERER_RAYTRACING_PIPELINE_H
