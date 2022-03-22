//
// Created by emd22 on 2022-02-20.
//

#include "renderer_graphics_pipeline.h"
#include "renderer_render_pass.h"
#include "renderer_fbo.h"

#include "../../system/debug.h"
#include "../../math/math_util.h"
#include "../../math/transform.h"

#include <cstring>

namespace hyperion {
namespace renderer {
GraphicsPipeline::GraphicsPipeline()
    : pipeline{},
      layout{},
      push_constants{},
      viewport{},
      scissor{}
{
}
GraphicsPipeline::GraphicsPipeline(ConstructionInfo &&construction_info)
    : m_construction_info(std::move(construction_info)),
      pipeline{},
      layout{},
      push_constants{},
      viewport{},
      scissor{}
{
}

GraphicsPipeline::~GraphicsPipeline()
{
    AssertThrowMsg(this->pipeline == nullptr, "Expected pipeline to have been destroyed");
    AssertThrowMsg(this->layout == nullptr, "Expected layout to have been destroyed");
}

void GraphicsPipeline::SetViewport(float x, float y, float width, float height, float min_depth, float max_depth)
{
    VkViewport *vp = &this->viewport;
    vp->x = x;
    vp->y = y;
    vp->width = width;
    vp->height = height;
    vp->minDepth = min_depth;
    vp->maxDepth = max_depth;
}

void GraphicsPipeline::SetScissor(int x, int y, uint32_t width, uint32_t height)
{
    this->scissor.offset = { x, y };
    this->scissor.extent = { width, height };
}

void GraphicsPipeline::SetDynamicStates(const std::vector<VkDynamicState> &_states)
{
    this->dynamic_states = _states;
}

std::vector<VkDynamicState> GraphicsPipeline::GetDynamicStates()
{
    return this->dynamic_states;
}

void GraphicsPipeline::UpdateDynamicStates(VkCommandBuffer cmd)
{
    vkCmdSetViewport(cmd, 0, 1, &this->viewport);
    vkCmdSetScissor(cmd, 0, 1, &this->scissor);
}

std::vector<VkVertexInputAttributeDescription> GraphicsPipeline::BuildVertexAttributes(const MeshInputAttributeSet &attribute_set)
{
    std::unordered_map<uint32_t, size_t> binding_sizes{};

    this->vertex_attributes = std::vector<VkVertexInputAttributeDescription>(attribute_set.attributes.size());

    for (size_t i = 0; i < attribute_set.attributes.size(); i++) {
        const auto &attribute = attribute_set.attributes[i];

        this->vertex_attributes[i] = VkVertexInputAttributeDescription{
            .location = attribute.location,
            .binding = attribute.binding,
            .format = attribute.GetFormat(),
            .offset = uint32_t(binding_sizes[attribute.binding])
        };

        binding_sizes[attribute.binding] += attribute.size;
    }

    this->vertex_binding_descriptions.clear();
    this->vertex_binding_descriptions.reserve(binding_sizes.size());

    for (auto &it : binding_sizes) {
        VkVertexInputBindingDescription binding_desc{};
        binding_desc.binding = it.first;
        binding_desc.stride = it.second;
        binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        this->vertex_binding_descriptions.push_back(binding_desc);
    }

    return this->vertex_attributes;
}

void GraphicsPipeline::BeginRenderPass(CommandBuffer *cmd, size_t index, VkSubpassContents contents)
{

    m_construction_info.render_pass->Begin(
        cmd,
        m_construction_info.fbos[index]->GetFramebuffer(),
        VkExtent2D{ uint32_t(m_construction_info.fbos[index]->GetWidth()), uint32_t(m_construction_info.fbos[index]->GetHeight()) },
        contents
    );
}

void GraphicsPipeline::EndRenderPass(CommandBuffer *cmd, size_t index)
{
    m_construction_info.render_pass->End(cmd);
}

void GraphicsPipeline::SubmitPushConstants(CommandBuffer *cmd) const
{
    vkCmdPushConstants(
        cmd->GetCommandBuffer(),
        layout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(push_constants),
        &push_constants
    );
}

void GraphicsPipeline::Bind(CommandBuffer *cmd)
{
    this->UpdateDynamicStates(cmd->GetCommandBuffer());

    vkCmdBindPipeline(cmd->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline);

    this->SubmitPushConstants(cmd);
}

void GraphicsPipeline::SetVertexInputMode(std::vector<VkVertexInputBindingDescription> &binding_descs,
    std::vector<VkVertexInputAttributeDescription> &attribs)
{
    this->vertex_binding_descriptions = binding_descs;
    this->vertex_attributes = attribs;
}

Result GraphicsPipeline::Create(Device *device, DescriptorPool *descriptor_pool)
{
    auto construction_info = std::move(m_construction_info);

    return Create(device, std::move(construction_info), descriptor_pool);
}

Result GraphicsPipeline::Create(Device *device, ConstructionInfo &&construction_info, DescriptorPool *descriptor_pool)
{
    m_construction_info = std::move(construction_info);

    AssertExit(m_construction_info.shader != nullptr);
    AssertExit(m_construction_info.fbos.size() != 0);

    size_t width = m_construction_info.fbos[0]->GetWidth();
    size_t height = m_construction_info.fbos[0]->GetHeight();
    this->SetViewport(0.0f, float(height), float(width), -float(height), 0.0f, 1.0f);
    this->SetScissor(0, 0, width, height);

    std::vector<VkDynamicState> default_dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    this->SetDynamicStates(default_dynamic_states);

    static int x = 0;
    DebugLog(LogType::Debug, "Create Pipeline [%d]\n", x++);

    return Rebuild(device, descriptor_pool);
}

Result GraphicsPipeline::Rebuild(Device *device, DescriptorPool *descriptor_pool)
{
    this->BuildVertexAttributes(m_construction_info.vertex_attributes);

    VkPipelineVertexInputStateCreateInfo vertex_input_info{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertex_input_info.vertexBindingDescriptionCount = uint32_t(this->vertex_binding_descriptions.size());
    vertex_input_info.pVertexBindingDescriptions = this->vertex_binding_descriptions.data();
    vertex_input_info.vertexAttributeDescriptionCount = uint32_t(this->vertex_attributes.size());
    vertex_input_info.pVertexAttributeDescriptions = this->vertex_attributes.data();

    VkPipelineInputAssemblyStateCreateInfo input_asm_info{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    input_asm_info.topology = m_construction_info.topology;
    input_asm_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    VkViewport viewports[] = { this->viewport };
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = viewports;

    VkRect2D scissors[] = { this->scissor };
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = scissors;

    VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    /* TODO: Revisit this for shadow maps! */
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    /* Backface culling */
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    switch (m_construction_info.cull_mode) {
    case ConstructionInfo::CullMode::BACK:
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        break;
    case ConstructionInfo::CullMode::FRONT:
        rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        break;
    default:
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        break;
    }

    /* Also visit for shadow mapping! Along with other optional parameters such as
     * depthBiasClamp, slopeFactor etc. */
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;

    /* TODO: enable multisampling and the GPU feature required for it.  */
    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments;
    color_blend_attachments.resize(this->m_construction_info.render_pass->GetColorAttachments().size());

    for (size_t i = 0; i < color_blend_attachments.size(); i++) {
        color_blend_attachments[i] = VkPipelineColorBlendAttachmentState{
            .blendEnable = VK_FALSE,
            .colorWriteMask = (
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
        };
    }

    VkPipelineColorBlendStateCreateInfo color_blending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    color_blending.logicOpEnable = false;
    color_blending.attachmentCount = uint32_t(color_blend_attachments.size());
    color_blending.pAttachments = color_blend_attachments.data();

    /* Push constants */
    VkPushConstantRange push_constant{};
    push_constant.offset = 0;
    push_constant.size = sizeof(push_constants);
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    /* Dynamic states; these are the values that can be changed without
     * rebuilding the rendering pipeline. */
    VkPipelineDynamicStateCreateInfo dynamic_state{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    const auto states = this->GetDynamicStates();

    dynamic_state.dynamicStateCount = uint32_t(states.size());
    dynamic_state.pDynamicStates = states.data();
    DebugLog(LogType::Info, "Enabling [%d] dynamic states\n", dynamic_state.dynamicStateCount);

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layout_info.setLayoutCount = uint32_t(descriptor_pool->m_descriptor_set_layouts.size());
    layout_info.pSetLayouts    = descriptor_pool->m_descriptor_set_layouts.data();
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;

    HYPERION_VK_CHECK_MSG(
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, nullptr, &this->layout),
        "Failed to create graphics pipeline layout"
    );

    /* Depth / stencil */
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = m_construction_info.depth_test;
    depth_stencil.depthWriteEnable = m_construction_info.depth_write;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f; // Optional
    depth_stencil.maxDepthBounds = 1.0f; // Optional
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {}; // Optional
    depth_stencil.back = {}; // Optional

    VkGraphicsPipelineCreateInfo pipeline_info{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    auto stages = m_construction_info.shader->shader_stages;
    pipeline_info.stageCount = uint32_t(stages.size());
    pipeline_info.pStages = stages.data();

    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_asm_info;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    /* TODO: reimplement dynamic states! */
    pipeline_info.pDynamicState = &dynamic_state;

    pipeline_info.layout = layout;
    pipeline_info.renderPass = m_construction_info.render_pass->GetRenderPass();

    pipeline_info.subpass = 0; /* Index of the subpass */
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    HYPERION_VK_CHECK_MSG(
        vkCreateGraphicsPipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &this->pipeline),
        "Failed to create graphics pipeline"
    );

    HYPERION_RETURN_OK;
}

Result GraphicsPipeline::Destroy(Device *device)
{
    VkDevice render_device = device->GetDevice();

    m_construction_info.fbos.clear();

    DebugLog(LogType::Info, "Destroying pipeline!\n");

    vkDestroyPipeline(render_device, this->pipeline, nullptr);
    this->pipeline = nullptr;

    vkDestroyPipelineLayout(render_device, this->layout, nullptr);
    this->layout = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion