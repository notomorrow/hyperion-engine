#ifndef HYPERION_RENDERER_BACKEND_VULKAN_RAYTRACING_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_RAYTRACING_PIPELINE_HPP

#include <core/lib/DynArray.hpp>

#include <Types.hpp>

#include <rendering/backend/RendererPipeline.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererShader.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
class RaytracingPipeline<Platform::VULKAN> : public Pipeline<Platform::VULKAN>
{
public:
    RaytracingPipeline();
    RaytracingPipeline(ShaderProgramRef<Platform::VULKAN> shader, DescriptorTableRef<Platform::VULKAN> descriptor_table);
    RaytracingPipeline(const RaytracingPipeline &other)             = delete;
    RaytracingPipeline &operator=(const RaytracingPipeline &other)  = delete;
    ~RaytracingPipeline();

    Result Create(Device<Platform::VULKAN> *device);
    Result Destroy(Device<Platform::VULKAN> *device);

    void Bind(CommandBuffer<Platform::VULKAN> *command_buffer);
    void SubmitPushConstants(CommandBuffer<Platform::VULKAN> *cmd) const;
    void TraceRays(
        Device<Platform::VULKAN> *device,
        CommandBuffer<Platform::VULKAN> *command_buffer,
        Extent3D extent
    ) const;

private:
    struct ShaderBindingTableEntry
    {
        std::unique_ptr<ShaderBindingTableBuffer<Platform::VULKAN>> buffer;
        VkStridedDeviceAddressRegionKHR                             strided_device_address_region;
    };

    struct
    {
        VkStridedDeviceAddressRegionKHR ray_gen { };
        VkStridedDeviceAddressRegionKHR ray_miss { };
        VkStridedDeviceAddressRegionKHR closest_hit { };
        VkStridedDeviceAddressRegionKHR callable { };
    } m_shader_binding_table_entries;

    using ShaderBindingTableMap = std::unordered_map<ShaderModuleType, ShaderBindingTableEntry>;

    Result CreateShaderBindingTables(Device<Platform::VULKAN> *device, ShaderProgram<Platform::VULKAN> *shader_program);

    Result CreateShaderBindingTableEntry(
        Device<Platform::VULKAN> *device,
        uint32 num_shaders,
        ShaderBindingTableEntry &out
    );

    ShaderBindingTableMap m_shader_binding_table_buffers;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_RAYTRACING_PIPELINE_HPP
