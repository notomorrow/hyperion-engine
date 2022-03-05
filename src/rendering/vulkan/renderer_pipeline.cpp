//
// Created by emd22 on 2022-02-20.
//

#include "renderer_pipeline.h"

#include "../../system/debug.h"
#include "../../math/math_util.h"
#include "../../math/transform.h"

#include <cstring>

namespace hyperion {

RendererPipeline::RendererPipeline(RendererDevice *_device, RendererSwapchain *_swapchain)
    : intern_vertex_buffers(nullptr),
      intern_vertex_buffers_size(0)
{
    this->primitive = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    this->swapchain = _swapchain;
    this->device = _device;
    // this->intern_vertex_buffers = nullptr;
    // this->intern_vertex_buffers_size = 0;

    auto width = (float) swapchain->extent.width;
    auto height = (float) swapchain->extent.height;
    this->SetViewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
    this->SetScissor(0, 0, _swapchain->extent.width, _swapchain->extent.height);

    std::vector<VkDynamicState> default_dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
    };
    this->SetDynamicStates(default_dynamic_states);

    static int x = 0;
    DebugLog(LogType::Debug, "Create RendererPipeline [%d]\n", x++);
}

void RendererPipeline::SetPrimitive(VkPrimitiveTopology _primitive) {
    this->primitive = _primitive;
}

VkPrimitiveTopology RendererPipeline::GetPrimitive() {
    return this->primitive;
}

void RendererPipeline::SetViewport(float x, float y, float width, float height, float min_depth, float max_depth) {
    VkViewport *vp = &this->viewport;
    vp->x = x;
    vp->y = y;
    vp->width = width;
    vp->height = height;
    vp->minDepth = min_depth;
    vp->maxDepth = max_depth;
}

void RendererPipeline::SetScissor(int x, int y, uint32_t width, uint32_t height) {
    this->scissor.offset = {x, y};
    this->scissor.extent = {width, height};
}

void RendererPipeline::SetDynamicStates(const std::vector<VkDynamicState> &_states) {
    this->dynamic_states = _states;
}

std::vector<VkDynamicState> RendererPipeline::GetDynamicStates() {
    return this->dynamic_states;
}

VkRenderPass *RendererPipeline::GetRenderPass() {
    return &this->render_pass;
}

RendererResult RendererPipeline::CreateCommandPool() {
    QueueFamilyIndices family_indices = this->device->FindQueueFamilies();

    VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.queueFamilyIndex = family_indices.graphics_family.value();
    /* TODO: look into VK_COMMAND_POOL_CREATE_TRANSIENT_BIT for constantly changing objects */
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    HYPERION_VK_CHECK_MSG(
        vkCreateCommandPool(this->device->GetDevice(), &pool_info, nullptr, &this->command_pool),
        "Could not create Vulkan command pool"
    );

    DebugLog(LogType::Debug, "Create Command pool\n");

    return RendererResult(RendererResult::RENDERER_OK);
}

RendererResult RendererPipeline::CreateCommandBuffers(uint16_t count) {
    AssertThrow(count >= 1);
    this->command_buffers.resize(this->swapchain->framebuffers.size());

    VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.commandPool = this->command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = (uint32_t) this->command_buffers.size();

    HYPERION_VK_CHECK_MSG(
        vkAllocateCommandBuffers(this->device->GetDevice(), &alloc_info, this->command_buffers.data()),
        "Could not create Vulkan command buffers"
    );

    DebugLog(LogType::Debug, "Allocate %d command buffers\n", this->command_buffers.size());

    return RendererResult(RendererResult::RENDERER_OK);
}

void RendererPipeline::UpdateDynamicStates(VkCommandBuffer cmd) {
    vkCmdSetViewport(cmd, 0, 1, &this->viewport);
    vkCmdSetScissor(cmd, 0, 1, &this->scissor);
}

void RendererPipeline::SetVertexBuffers(std::vector<RendererVertexBuffer> &vertex_buffers) {
    if (intern_vertex_buffers != nullptr) {
        delete[] intern_vertex_buffers;
    }
    /* Because C++ can have some overhead with vector implementations we'll just
     * copy the direct VkBuffer's to a memory chunk so we are guaranteed to have near
     * linear complexity. */
    uint32_t size = vertex_buffers.size();
    this->intern_vertex_buffers_size = size;
    this->intern_vertex_buffers = new VkBuffer[size];
    /* We should never run out of memory here... */
    AssertThrowMsg(this->intern_vertex_buffers != nullptr, "Could not allocate memory!\n");
    for (uint32_t i = 0; i < size; i++) {
        memcpy(&this->intern_vertex_buffers[i], &vertex_buffers[i].memory, sizeof(VkBuffer));
    }
}

std::vector<VkVertexInputAttributeDescription> RendererPipeline::SetVertexAttribs() {
    VkVertexInputBindingDescription binding_desc{};
    binding_desc.binding = 0;
    binding_desc.stride = 64/* Jesus christ */;
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    this->vertex_binding_descriptions.push_back(binding_desc);

    const VkFormat fmt_vec3 = VK_FORMAT_R32G32B32_SFLOAT;
    const VkFormat fmt_vec2 = VK_FORMAT_R32G32_SFLOAT;

    uint32_t prev_size = 0;

    std::vector<VkVertexInputAttributeDescription> attrs;
    //attrs.resize(6);
    attrs.resize(6);

    /* Position */
    attrs[0] = { 0, 0, fmt_vec3, prev_size*(uint32_t)sizeof(float) };
    prev_size += 3;
    /* Normal */
    attrs[1] = { 1, 0, fmt_vec3, prev_size*(uint32_t)sizeof(float) };
    prev_size += 3;
    /* Texcoord0 */
    attrs[2] = { 2, 0, fmt_vec2, prev_size*(uint32_t)sizeof(float) };
    prev_size += 2;
    /* Texcoord1 */
    attrs[3] = { 3, 0, fmt_vec2, prev_size*(uint32_t)sizeof(float) };
    prev_size += 2;
    /* Tangent */
    attrs[4] = { 4, 0, fmt_vec3, prev_size*(uint32_t)sizeof(float) };
    prev_size += 3;
    /* Bitangent */
    attrs[5] = { 5, 0, fmt_vec3, prev_size*(uint32_t)sizeof(float) };
    prev_size += 3;
    DebugLog(LogType::Info, "Total size: %d\n", prev_size);

    this->vertex_attributes = attrs;

    return attrs;
}

void RendererPipeline::StartRenderPass(VkCommandBuffer cmd, uint32_t image_index) {
    //VkCommandBuffer *cmd = &this->command_buffers[frame_index];
    VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    //begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;
    /* Begin recording our command buffer */
    auto result = vkBeginCommandBuffer(cmd, &begin_info);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to start recording command buffer!\n");

    VkRenderPassBeginInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render_pass_info.renderPass = this->render_pass;
    render_pass_info.framebuffer = this->swapchain->framebuffers[image_index];
    render_pass_info.renderArea.offset = { 0, 0 };
    render_pass_info.renderArea.extent = this->swapchain->extent;

    const std::array clear_values {
        VkClearValue{.color = {0.0f, 0.0f, 0.0f, 1.0f}},
        VkClearValue{.depthStencil = {1.0f, 0}}
    };
    render_pass_info.clearValueCount = clear_values.size();
    render_pass_info.pClearValues = clear_values.data();

    /* Start adding draw commands  */
    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    /* Bind the graphics pipeline */
    //vkCmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline);
    this->UpdateDynamicStates(cmd);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants), &push_constants);
}

void RendererPipeline::EndRenderPass(VkCommandBuffer cmd) {
    //VkCommandBuffer *cmd = &this->command_buffers[frame_index];
    vkCmdEndRenderPass(cmd);

    auto result = vkEndCommandBuffer(cmd);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to record command buffer!\n");
}

RendererResult RendererPipeline::CreateRenderPass(VkSampleCountFlagBits sample_count) {
    std::array color_attachment_references{
        VkAttachmentReference{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        }
    };

    std::array depth_stencil_attachment_references{
        VkAttachmentReference{
            .attachment = 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    };

    static_assert(
        depth_stencil_attachment_references.size() == 1,
        "May only have 1 depth/stencil attachment"
    );

    AssertExit(this->swapchain->depth_buffer.image != nullptr);

    std::array attachment_descriptions{
        /* Color attachment */
        VkAttachmentDescription{
            .format = this->swapchain->image_format,
            .samples = sample_count,
            /* Options to change what happens before and after our render pass */
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            /* Images are presented to the swapchain */
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        /* Depth attachment */
        VkAttachmentDescription {
            .format = this->swapchain->depth_buffer.image->GetImageFormat(),
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    };

    static_assert(
        attachment_descriptions.size() == color_attachment_references.size() + depth_stencil_attachment_references.size(),
        "Number of attachment descriptions must match number of references"
    );

    /* Subpasses for post processing, etc. */
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = color_attachment_references.size();
    subpass.pColorAttachments = color_attachment_references.data();
    subpass.pDepthStencilAttachment = depth_stencil_attachment_references.data();

    //const VkAttachmentDescription attachments[] = {attachment};
    const VkSubpassDescription subpasses[] = {subpass};

    /* Create render pass */
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    render_pass_info.attachmentCount = attachment_descriptions.size();
    render_pass_info.pAttachments = attachment_descriptions.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = subpasses;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    HYPERION_VK_CHECK_MSG(
        vkCreateRenderPass(this->device->GetDevice(), &render_pass_info, nullptr, &this->render_pass),
        "Could not create render pass!"
    );

    DebugLog(LogType::Debug, "Renderpass created!\n");

    return RendererResult(RendererResult::RENDERER_OK);
}

void RendererPipeline::SetVertexInputMode(std::vector<VkVertexInputBindingDescription> &binding_descs,
                                          std::vector<VkVertexInputAttributeDescription> &attribs) {
    this->vertex_binding_descriptions = binding_descs;
    this->vertex_attributes = attribs;
}

void RendererPipeline::Rebuild(RendererShader *shader) {
    this->SetVertexAttribs();

    VkPipelineVertexInputStateCreateInfo vertex_input_info{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input_info.vertexBindingDescriptionCount   = (uint32_t)(this->vertex_binding_descriptions.size());
    vertex_input_info.pVertexBindingDescriptions      = this->vertex_binding_descriptions.data();
    vertex_input_info.vertexAttributeDescriptionCount = (uint32_t)(this->vertex_attributes.size());
    vertex_input_info.pVertexAttributeDescriptions    = this->vertex_attributes.data();

    VkPipelineInputAssemblyStateCreateInfo input_asm_info{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_asm_info.topology = this->GetPrimitive();
    input_asm_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkViewport viewports[] = {this->viewport};
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = viewports;

    VkRect2D scissors[] = {this->scissor};
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = scissors;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    /* TODO: Revisit this for shadow maps! */
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    /* Backface culling */
    /*rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;*/
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    /* Also visit for shadow mapping! Along with other optional parameters such as
     * depthBiasClamp, slopeFactor etc. */
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;

    /* TODO: enable multisampling and the GPU feature required for it.  */

    VkPipelineColorBlendAttachmentState color_blend_attachment{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blend_attachment.colorWriteMask = (
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

    /* TODO: with multiple framebuffers and post processing, we will need to enable colour blending */
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blending.logicOpEnable = false;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    /* Push constants */
    //setup push constants
    VkPushConstantRange push_constant{};
    //this push constant range starts at the beginning
    push_constant.offset = 0;
    //this push constant range takes up the size of a MeshPushConstants struct
    push_constant.size = sizeof(PushConstants);
    //this push constant range is accessible only in the vertex shader
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    /* Dynamic states; these are the values that can be changed without
     * rebuilding the rendering pipeline. */
    VkPipelineDynamicStateCreateInfo dynamic_state{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    const auto states = this->GetDynamicStates();

    dynamic_state.dynamicStateCount = states.size();
    dynamic_state.pDynamicStates = states.data();
    DebugLog(LogType::Info, "Enabling [%d] dynamic states\n", dynamic_state.dynamicStateCount);

    auto descriptor_pool_result = descriptor_pool.Create(device, 0);
    AssertThrowMsg(descriptor_pool_result, "Error creating descriptor pool! Message was: %s\n", descriptor_pool_result.message);

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_info.setLayoutCount = descriptor_pool.m_descriptor_set_layouts.size(); // TODO: refactor
    layout_info.pSetLayouts = descriptor_pool.m_descriptor_set_layouts.data();
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;

    if (vkCreatePipelineLayout(this->device->GetDevice(), &layout_info, nullptr, &this->layout) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Error creating pipeline layout!\n");
        throw std::runtime_error("Error creating pipeline layout");
    }

    /* Depth / stencil */
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f; // Optional
    depth_stencil.maxDepthBounds = 1.0f; // Optional
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {}; // Optional
    depth_stencil.back = {}; // Optional

    VkGraphicsPipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    auto stages = shader->shader_stages;
    pipeline_info.stageCount = stages.size();
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
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0; /* Index of the subpass */
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(this->device->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                  &this->pipeline) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Could not create graphics pipeline!\n");
        throw std::runtime_error("Could not create pipeline");
    }
    DebugLog(LogType::Info, "Created graphics pipeline!\n");
}

void RendererPipeline::Destroy() {
    if (intern_vertex_buffers != nullptr) {
        delete[] intern_vertex_buffers;
    }

    VkDevice render_device = this->device->GetDevice();

    vkFreeCommandBuffers(render_device, this->command_pool, this->command_buffers.size(), this->command_buffers.data());
    vkDestroyCommandPool(render_device, this->command_pool, nullptr);

    DebugLog(LogType::Info, "Destroying pipeline!\n");

    descriptor_pool.Destroy(this->device);

    vkDestroyPipeline(render_device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(render_device, this->layout, nullptr);
    vkDestroyRenderPass(render_device, this->render_pass, nullptr);
}

helpers::SingleTimeCommands RendererPipeline::GetSingleTimeCommands()
{
    QueueFamilyIndices family_indices = this->device->FindQueueFamilies(); // TODO: this result should be cached

    helpers::SingleTimeCommands single_time_commands{};
    single_time_commands.cmd = nullptr;
    single_time_commands.pool = this->command_pool;
    single_time_commands.family_indices = family_indices;

    return single_time_commands;
}

} /* namespace hyperion */