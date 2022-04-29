#include "renderer_raytracing_pipeline.h"
#include "../renderer_command_buffer.h"

#include <system/debug.h>
#include <math/math_util.h>
#include <math/transform.h>

#include <cstring>

#include "../renderer_features.h"

namespace hyperion {
namespace renderer {
RaytracingPipeline::RaytracingPipeline(std::unique_ptr<ShaderProgram> &&shader_program)
    : Pipeline(),
      m_shader_program(std::move(shader_program))
{
    static int x = 0;
    DebugLog(LogType::Debug, "Create Raytracing Pipeline [%d]\n", x++);
}

RaytracingPipeline::~RaytracingPipeline() = default;

Result RaytracingPipeline::Create(Device *device,
                                  DescriptorPool *descriptor_pool)
{
    if (!device->GetFeatures().SupportsRaytracing()) {
        return {Result::RENDERER_ERR, "Raytracing is not supported on this device"};
    }

    AssertThrow(m_shader_program != nullptr);
    HYPERION_BUBBLE_ERRORS(m_shader_program->Create(device));

    auto result = Result::OK;

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_info.setLayoutCount         = uint32_t(descriptor_pool->GetDescriptorSetLayouts().size());
    layout_info.pSetLayouts            = descriptor_pool->GetDescriptorSetLayouts().data();
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges    = VK_NULL_HANDLE;

    HYPERION_VK_PASS_ERRORS(
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, VK_NULL_HANDLE, &layout),
        result
    );

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    VkRayTracingPipelineCreateInfoKHR pipeline_info{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};

    const auto &stages        = m_shader_program->GetShaderStages();
    const auto &shader_groups = m_shader_program->GetShaderGroups();

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_group_create_infos;
    shader_group_create_infos.resize(shader_groups.size());

    for (size_t i = 0; i < shader_groups.size(); i++) {
        shader_group_create_infos[i] = shader_groups[i].raytracing_group_create_info;
    }

    pipeline_info.stageCount          = uint32_t(stages.size());
    pipeline_info.pStages             = stages.data();
    pipeline_info.groupCount          = uint32_t(shader_group_create_infos.size());
    pipeline_info.pGroups             = shader_group_create_infos.data();
    pipeline_info.layout              = layout;
    pipeline_info.basePipelineHandle  = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex   = -1;
    
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

    HYPERION_PASS_ERRORS(CreateShaderBindingTables(device), result);
    
    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    HYPERION_RETURN_OK;
}

Result RaytracingPipeline::Destroy(Device *device)
{
    DebugLog(LogType::Debug, "Destroying raytracing pipeline\n");

    auto result = Result::OK;

    for (auto &it : m_shader_binding_table_buffers) {
        HYPERION_PASS_ERRORS(it.second.buffer->Destroy(device), result);
    }

    if (m_shader_program != nullptr) {
        HYPERION_PASS_ERRORS(m_shader_program->Destroy(device), result);
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

void RaytracingPipeline::Bind(CommandBuffer *command_buffer)
{
    vkCmdBindPipeline(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline
    );
}

void RaytracingPipeline::TraceRays(Device *device,
    CommandBuffer *command_buffer,
    Extent2D extent) const
{
    device->GetFeatures().dyn_functions.vkCmdTraceRaysKHR(
        command_buffer->GetCommandBuffer(),
        &m_shader_binding_table_entries.ray_gen,
        &m_shader_binding_table_entries.ray_miss,
        &m_shader_binding_table_entries.closest_hit,
        &m_shader_binding_table_entries.callable,
        extent.width, extent.height, 1
    );
}

Result RaytracingPipeline::CreateShaderBindingTables(Device *device)
{
    const auto &shader_groups = m_shader_program->GetShaderGroups();

    const auto &features = device->GetFeatures();
    const auto &properties = features.GetRaytracingPipelineProperties();

    const size_t handle_size = properties.shaderGroupHandleSize;
    const size_t handle_size_aligned = device->GetFeatures().PaddedSize(handle_size, properties.shaderGroupHandleAlignment);
    const size_t table_size = shader_groups.size() * handle_size_aligned;

    std::vector<uint8_t> shader_handle_storage;
    shader_handle_storage.resize(table_size);

    HYPERION_VK_CHECK(features.dyn_functions.vkGetRayTracingShaderGroupHandlesKHR(
        device->GetDevice(),
        pipeline,
        0,
        static_cast<uint32_t>(shader_groups.size()),
        static_cast<uint32_t>(shader_handle_storage.size()),
        shader_handle_storage.data()
    ));

    auto result = Result::OK;

    size_t offset = 0;

    ShaderBindingTableMap buffers;

    for (const auto &group : shader_groups) {
        const auto &create_info = group.raytracing_group_create_info;

        ShaderBindingTableEntry entry;

#define SHADER_PRESENT_IN_GROUP(type) (create_info.type != VK_SHADER_UNUSED_KHR ? 1 : 0)

        const uint32_t shader_count = SHADER_PRESENT_IN_GROUP(generalShader)
            + SHADER_PRESENT_IN_GROUP(closestHitShader)
            + SHADER_PRESENT_IN_GROUP(anyHitShader)
            + SHADER_PRESENT_IN_GROUP(intersectionShader);

#undef SHADER_PRESENT_IN_GROUP

        if (shader_count == 0) {
            HYP_BREAKPOINT;
        }

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

    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModule::Type::RAY_GEN, ray_gen);
    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModule::Type::RAY_MISS, ray_miss);
    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModule::Type::RAY_CLOSEST_HIT, closest_hit);

#undef GET_STRIDED_DEVICE_ADDRESS_REGION

    return result;
}

Result RaytracingPipeline::CreateShaderBindingTableEntry(Device *device,
    uint32_t num_shaders,
    ShaderBindingTableEntry &out)
{
    const auto &properties = device->GetFeatures().GetRaytracingPipelineProperties();

    AssertThrow(properties.shaderGroupHandleSize != 0);

    if (num_shaders == 0) {
        return {Result::RENDERER_ERR, "Creating shader binding table entry with zero shader count"};
    }

    auto result = Result::OK;

    out.buffer.reset(new ShaderBindingTableBuffer());

    HYPERION_PASS_ERRORS(
        out.buffer->Create(
            device,
            properties.shaderGroupHandleSize * num_shaders
        ),
        result
    );

    if (result) {
        /* Get strided device address region */
        const uint32_t handle_size = device->GetFeatures().PaddedSize(properties.shaderGroupHandleSize, properties.shaderGroupHandleAlignment);

        out.strided_device_address_region = VkStridedDeviceAddressRegionKHR{
            .deviceAddress = out.buffer->GetBufferDeviceAddress(device),
            .stride        = handle_size,
            .size          = num_shaders * handle_size
        };
    } else {
        out.buffer.reset();
    }

    return result;
}

} // namespace renderer
} // namespace hyperion