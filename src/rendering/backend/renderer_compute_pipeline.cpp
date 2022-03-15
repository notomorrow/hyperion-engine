#include "renderer_compute_pipeline.h"

#include "../../system/debug.h"
#include "../../math/math_util.h"
#include "../../math/transform.h"

#include <cstring>

namespace hyperion {
namespace renderer {
ComputePipeline::ComputePipeline()
    : pipeline{},
      layout{}
{
    static int x = 0;
    DebugLog(LogType::Debug, "Create Compute Pipeline [%d]\n", x++);
}

ComputePipeline::~ComputePipeline()
{
    AssertThrowMsg(this->pipeline == nullptr, "Compute pipeline should have been destroyed");
    AssertThrowMsg(this->layout == nullptr, "Compute pipeline should have been destroyed");
}

void ComputePipeline::Bind(VkCommandBuffer cmd) const
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, this->pipeline);
}

void ComputePipeline::Dispatch(VkCommandBuffer cmd, size_t num_groups_x, size_t num_groups_y, size_t num_groups_z) const
{
    vkCmdDispatch(cmd, num_groups_x, num_groups_y, num_groups_z);
}

Result ComputePipeline::Create(Device *device, ShaderProgram *shader, DescriptorPool *descriptor_pool)
{
    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_info.setLayoutCount = descriptor_pool->m_descriptor_set_layouts.size();
    layout_info.pSetLayouts = descriptor_pool->m_descriptor_set_layouts.data();

    HYPERION_VK_CHECK_MSG(
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, nullptr, &this->layout),
        "Failed to create compute pipeline layout"
    );

    VkComputePipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};

    const auto &stages = shader->shader_stages;
    AssertThrowMsg(stages.size() == 1, "Compute pipelines must have only one shader stage");

    pipeline_info.stage = stages.front();
    pipeline_info.layout = layout;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    HYPERION_VK_CHECK_MSG(
        vkCreateComputePipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &this->pipeline),
        "Failed to create compute pipeline"
    );

    HYPERION_RETURN_OK;
}

Result ComputePipeline::Destroy(Device *device)
{
    DebugLog(LogType::Debug, "Destroying compute pipeline\n");

    vkDestroyPipeline(device->GetDevice(), this->pipeline, nullptr);
    this->pipeline = nullptr;

    vkDestroyPipelineLayout(device->GetDevice(), this->layout, nullptr);
    this->layout = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion