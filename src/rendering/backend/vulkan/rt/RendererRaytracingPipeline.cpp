#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <system/Debug.hpp>
#include <math/MathUtil.hpp>
#include <math/Transform.hpp>

#include <cstring>

namespace hyperion {
namespace renderer {
namespace platform {

static constexpr VkShaderStageFlags push_constant_stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
    | VK_SHADER_STAGE_MISS_BIT_KHR
    | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

RaytracingPipeline<Platform::VULKAN>::RaytracingPipeline()
    : Pipeline<Platform::VULKAN>()
{
}

RaytracingPipeline<Platform::VULKAN>::RaytracingPipeline(ShaderProgramRef<Platform::VULKAN> shader_program)
    : Pipeline<Platform::VULKAN>(std::move(shader_program))
{
}

RaytracingPipeline<Platform::VULKAN>::RaytracingPipeline(ShaderProgramRef<Platform::VULKAN> shader_program, const Array<DescriptorSetRef> &used_descriptor_sets)
    : Pipeline<Platform::VULKAN>(std::move(shader_program), used_descriptor_sets)
{
}

RaytracingPipeline<Platform::VULKAN>::RaytracingPipeline(const Array<DescriptorSetRef> &used_descriptor_sets)
    : Pipeline<Platform::VULKAN>(used_descriptor_sets)
{
}

RaytracingPipeline<Platform::VULKAN>::~RaytracingPipeline() = default;

Result RaytracingPipeline<Platform::VULKAN>::Create(
    Device<Platform::VULKAN> *device,
    DescriptorPool *descriptor_pool
)
{
    if (!device->GetFeatures().IsRaytracingSupported()) {
        return {Result::RENDERER_ERR, "Raytracing is not supported on this device"};
    }

    AssertThrow(m_shader_program != nullptr);

    auto result = Result::OK;

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

    const auto used_layouts = GetDescriptorSetLayouts(device, descriptor_pool);
    const auto max_set_layouts = device->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    DebugLog(
        LogType::Debug,
        "Using %llu descriptor set layouts in pipeline\n",
        used_layouts.size()
    );
    
    if (used_layouts.size() > max_set_layouts) {
        DebugLog(
            LogType::Debug,
            "Device max bound descriptor sets exceeded (%llu > %u)\n",
            used_layouts.size(),
            max_set_layouts
        );

        return Result{Result::RENDERER_ERR, "Device max bound descriptor sets exceeded"};
    }

    layout_info.setLayoutCount = uint32(used_layouts.size());
    layout_info.pSetLayouts = used_layouts.data();
    
    /* Push constants */
    const VkPushConstantRange push_constant_ranges[] = {
        {
            .stageFlags = push_constant_stage_flags,
            .offset     = 0,
            .size       = uint32(device->GetFeatures().PaddedSize<PushConstantData>())
        }
    };

    layout_info.pushConstantRangeCount = uint32(std::size(push_constant_ranges));
    layout_info.pPushConstantRanges = push_constant_ranges;

    HYPERION_VK_PASS_ERRORS(
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, VK_NULL_HANDLE, &layout),
        result
    );

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    VkRayTracingPipelineCreateInfoKHR pipeline_info{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};

    const auto &stages = m_shader_program->GetShaderStages();
    const auto &shader_groups = m_shader_program->GetShaderGroups();

    Array<VkRayTracingShaderGroupCreateInfoKHR> shader_group_create_infos;
    shader_group_create_infos.Resize(shader_groups.Size());

    for (SizeType i = 0; i < shader_groups.Size(); i++) {
        shader_group_create_infos[i] = shader_groups[i].raytracing_group_create_info;
    }

    pipeline_info.stageCount            = uint32(stages.Size());
    pipeline_info.pStages               = stages.Data();
    pipeline_info.groupCount            = uint32(shader_group_create_infos.Size());
    pipeline_info.pGroups               = shader_group_create_infos.Data();
    pipeline_info.layout                = layout;
    pipeline_info.basePipelineHandle    = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex     = -1;
    
    HYPERION_VK_PASS_ERRORS(
        device->GetFeatures().dyn_functions.vkCreateRayTracingPipelinesKHR(
            device->GetDevice(),
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            VK_NULL_HANDLE,
            &pipeline
        ),
        result
    );

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    HYPERION_PASS_ERRORS(CreateShaderBindingTables(device, m_shader_program), result);
    
    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    HYPERION_RETURN_OK;
}

Result RaytracingPipeline<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    DebugLog(LogType::Debug, "Destroying raytracing pipeline\n");

    auto result = Result::OK;

    for (auto &it : m_shader_binding_table_buffers) {
        HYPERION_PASS_ERRORS(it.second.buffer->Destroy(device), result);
    }

    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device->GetDevice(), pipeline, VK_NULL_HANDLE);
        pipeline = VK_NULL_HANDLE;
    }
    
    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device->GetDevice(), layout, VK_NULL_HANDLE);
        layout = VK_NULL_HANDLE;
    }

    return result;
}

void RaytracingPipeline<Platform::VULKAN>::Bind(CommandBuffer<Platform::VULKAN> *command_buffer)
{
    vkCmdBindPipeline(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline
    );
}

void RaytracingPipeline<Platform::VULKAN>::SubmitPushConstants(CommandBuffer<Platform::VULKAN> *cmd) const
{
    vkCmdPushConstants(
        cmd->GetCommandBuffer(),
        layout,
        push_constant_stage_flags,
        0,
        static_cast<uint32>(sizeof(push_constants)),
        &push_constants
    );
}

void RaytracingPipeline<Platform::VULKAN>::TraceRays(
    Device<Platform::VULKAN> *device,
    CommandBuffer<Platform::VULKAN> *command_buffer,
    Extent3D extent
) const
{
    device->GetFeatures().dyn_functions.vkCmdTraceRaysKHR(
        command_buffer->GetCommandBuffer(),
        &m_shader_binding_table_entries.ray_gen,
        &m_shader_binding_table_entries.ray_miss,
        &m_shader_binding_table_entries.closest_hit,
        &m_shader_binding_table_entries.callable,
        extent.width, extent.height, extent.depth
    );
}

Result RaytracingPipeline<Platform::VULKAN>::CreateShaderBindingTables(Device<Platform::VULKAN> *device, ShaderProgram<Platform::VULKAN> *shader_program)
{
    const auto &shader_groups = shader_program->GetShaderGroups();

    const auto &features = device->GetFeatures();
    const auto &properties = features.GetRaytracingPipelineProperties();

    const uint32 handle_size = properties.shaderGroupHandleSize;
    const uint32 handle_size_aligned = device->GetFeatures().PaddedSize(handle_size, properties.shaderGroupHandleAlignment);
    const uint32 table_size = static_cast<uint32>(shader_groups.Size()) * handle_size_aligned;

    ByteBuffer shader_handle_storage(table_size);

    HYPERION_VK_CHECK(features.dyn_functions.vkGetRayTracingShaderGroupHandlesKHR(
        device->GetDevice(),
        pipeline,
        0,
        static_cast<uint32>(shader_groups.Size()),
        static_cast<uint32>(shader_handle_storage.Size()),
        shader_handle_storage.Data()
    ));

    auto result = Result::OK;

    uint32 offset = 0;

    ShaderBindingTableMap buffers;

    for (const auto &group : shader_groups) {
        const auto &create_info = group.raytracing_group_create_info;

        ShaderBindingTableEntry entry;

#define SHADER_PRESENT_IN_GROUP(type) (create_info.type != VK_SHADER_UNUSED_KHR ? 1 : 0)

        const uint32 shader_count = SHADER_PRESENT_IN_GROUP(generalShader)
            + SHADER_PRESENT_IN_GROUP(closestHitShader)
            + SHADER_PRESENT_IN_GROUP(anyHitShader)
            + SHADER_PRESENT_IN_GROUP(intersectionShader);

#undef SHADER_PRESENT_IN_GROUP

        AssertThrow(shader_count != 0);

        HYPERION_PASS_ERRORS(
            CreateShaderBindingTableEntry(
                device,
                shader_count,
                entry
            ),
            result
        );

        if (result) {
            entry.buffer->Copy(
                device,
                handle_size,
                &shader_handle_storage[offset]
            );

            offset += handle_size;
        } else {
            for (auto &it : buffers) {
                HYPERION_IGNORE_ERRORS(it.second.buffer->Destroy(device));
            }

            return result;
        }

        buffers[group.type] = std::move(entry);
    }

    m_shader_binding_table_buffers = std::move(buffers);

#define GET_STRIDED_DEVICE_ADDRESS_REGION(type, out) \
    do { \
        auto it = m_shader_binding_table_buffers.find(type); \
        if (it != m_shader_binding_table_buffers.end()) { \
            m_shader_binding_table_entries.out = it->second.strided_device_address_region; \
        } \
    } while (0)

    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModuleType::RAY_GEN, ray_gen);
    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModuleType::RAY_MISS, ray_miss);
    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModuleType::RAY_CLOSEST_HIT, closest_hit);

#undef GET_STRIDED_DEVICE_ADDRESS_REGION

    return result;
}

Result RaytracingPipeline<Platform::VULKAN>::CreateShaderBindingTableEntry(
    Device<Platform::VULKAN> *device,
    uint32 num_shaders,
    ShaderBindingTableEntry &out
)
{
    const auto &properties = device->GetFeatures().GetRaytracingPipelineProperties();

    AssertThrow(properties.shaderGroupHandleSize != 0);

    if (num_shaders == 0) {
        return { Result::RENDERER_ERR, "Creating shader binding table entry with zero shader count" };
    }

    auto result = Result::OK;

    out.buffer.reset(new ShaderBindingTableBuffer<Platform::VULKAN>());

    HYPERION_PASS_ERRORS(
        out.buffer->Create(
            device,
            properties.shaderGroupHandleSize * num_shaders
        ),
        result
    );

    if (result) {
        /* Get strided device address region */
        const uint32 handle_size = device->GetFeatures().PaddedSize(properties.shaderGroupHandleSize, properties.shaderGroupHandleAlignment);

        out.strided_device_address_region = VkStridedDeviceAddressRegionKHR {
            .deviceAddress  = out.buffer->GetBufferDeviceAddress(device),
            .stride         = handle_size,
            .size           = num_shaders * handle_size
        };
    } else {
        out.buffer.reset();
    }

    return result;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
