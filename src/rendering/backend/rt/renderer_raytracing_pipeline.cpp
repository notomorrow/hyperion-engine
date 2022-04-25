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

void RaytracingPipeline::Bind(CommandBuffer *command_buffer)
{
    vkCmdBindPipeline(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline
    );
}

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


    pipeline_info.stageCount          = uint32_t(stages.size());
    pipeline_info.pStages             = stages.data();
    pipeline_info.groupCount          = uint32_t(shader_groups.size());
    pipeline_info.pGroups             = shader_groups.data();
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

    for (auto &table : m_shader_binding_table_buffers) {
        HYPERION_PASS_ERRORS(table->Destroy(device), result);
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

    std::vector<std::unique_ptr<ShaderBindingTableBuffer>> buffers;
    buffers.reserve(shader_groups.size());

    size_t offset = 0;

    for (size_t i = 0; i < shader_groups.size(); i++) {
        const auto &group = shader_groups[i];

        std::unique_ptr<ShaderBindingTableBuffer> buffer;

        HYPERION_PASS_ERRORS(
            CreateShaderBindingTable(
                device,
                (group.generalShader        ? 1 : 0)
                + (group.closestHitShader   ? 1 : 0)
                + (group.anyHitShader       ? 1 : 0)
                + (group.intersectionShader ? 1 : 0),
                buffer
            ),
            result
        );

        if (result) {
            buffer->Copy(
                device,
                handle_size,
                &shader_handle_storage[offset]
            );

            offset += handle_size;
        } else {
            for (auto j = static_cast<int64_t>(i - 1); j >= 0; j--) {
                HYPERION_IGNORE_ERRORS(buffers[j]->Destroy(device));
            }

            return result;
        }

        buffers.push_back(std::move(buffer));
    }

    m_shader_binding_table_buffers = std::move(buffers);

    return result;
}

Result RaytracingPipeline::CreateShaderBindingTable(Device *device,
    uint32_t num_shaders,
    std::unique_ptr<ShaderBindingTableBuffer> &out_buffer)
{
    auto result = Result::OK;

    out_buffer.reset(new ShaderBindingTableBuffer());

    HYPERION_PASS_ERRORS(
        out_buffer->Create(
            device,
            device->GetFeatures().GetRaytracingPipelineProperties().shaderGroupHandleSize * num_shaders
        ),
        result
    );

    if (!result) {
        out_buffer.reset();
    }

    return result;
}

} // namespace renderer
} // namespace hyperion