//
// Created by emd22 on 2022-02-20.
//

#include "renderer_pipeline.h"
#include "renderer_render_pass.h"
#include "renderer_fbo.h"

#include "../../system/debug.h"
#include "../../math/math_util.h"
#include "../../math/transform.h"

#include <cstring>

namespace hyperion {

RendererPipeline::RendererPipeline(RendererDevice *_device,
    ConstructionInfo &&construction_info)
    : intern_vertex_buffers(nullptr),
      intern_vertex_buffers_size(0),
      m_construction_info(std::move(construction_info))
{
    AssertExit(construction_info.shader != nullptr);
    AssertExit(construction_info.fbos.size() != 0);

    this->primitive = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    this->device = _device;
    // this->intern_vertex_buffers = nullptr;
    // this->intern_vertex_buffers_size = 0;

    size_t width  = construction_info.fbos[0]->GetWidth();
    size_t height = construction_info.fbos[0]->GetHeight();
    this->SetViewport(0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f);
    this->SetScissor(0, 0, width, height);

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

    HYPERION_RETURN_OK;
}

RendererResult RendererPipeline::CreateCommandBuffers(uint16_t count) {
    AssertThrow(count >= 1);
    //this->command_buffers.resize(this->swapchain->framebuffers.size());
    this->command_buffers.resize(this->m_construction_info.fbos.size());

    VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.commandPool = this->command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = uint32_t(this->command_buffers.size());

    HYPERION_VK_CHECK_MSG(
        vkAllocateCommandBuffers(this->device->GetDevice(), &alloc_info, this->command_buffers.data()),
        "Could not create Vulkan command buffers"
    );

    DebugLog(LogType::Debug, "Allocate %d command buffers\n", this->command_buffers.size());

    HYPERION_RETURN_OK;
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

std::vector<VkVertexInputAttributeDescription> RendererPipeline::BuildVertexAttributes(const RendererMeshInputAttributeSet &attribute_set)
{
    std::unordered_map<uint32_t, size_t> binding_sizes{};

    this->vertex_attributes = std::vector<VkVertexInputAttributeDescription>(attribute_set.attributes.size());

    for (size_t i = 0; i < attribute_set.attributes.size(); i++) {
        const auto &attribute = attribute_set.attributes[i];

        this->vertex_attributes[i] = VkVertexInputAttributeDescription{
            .location = attribute.location,
            .binding = attribute.binding,
            .format = attribute.format,
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

void RendererPipeline::StartRenderPass(VkCommandBuffer cmd, size_t index) {
    //VkCommandBuffer *cmd = &this->command_buffers[frame_index];
    VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    //begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;
    /* Begin recording our command buffer */
    auto result = vkBeginCommandBuffer(cmd, &begin_info);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to start recording command buffer!\n");

    m_construction_info.render_pass->Begin(
        cmd,
        m_construction_info.fbos[index]->GetFramebuffer(),
        VkExtent2D{ uint32_t(m_construction_info.fbos[index]->GetWidth()), uint32_t(m_construction_info.fbos[index]->GetHeight())}
    );

    this->UpdateDynamicStates(cmd);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline);

    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants), &push_constants);
}

void RendererPipeline::EndRenderPass(VkCommandBuffer cmd, size_t index) {
    m_construction_info.render_pass->End(cmd);

    auto result = vkEndCommandBuffer(cmd);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to record command buffer!\n");
}

/*RendererResult RendererPipeline::CreateRenderPass(VkSampleCountFlagBits sample_count) {
    AssertExit(this->swapchain->depth_buffer.image != nullptr);

    AssertExit(render_pass == nullptr);

    render_pass = new RendererRenderPass();

    render_pass->AddAttachment(RendererRenderPass::AttachmentInfo{
        .attachment = std::make_unique<RendererAttachment>(
            this->swapchain->image_format,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            0, 
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        ),
        .is_depth_attachment = false
    });

    render_pass->AddAttachment(RendererRenderPass::AttachmentInfo{
        .attachment = std::make_unique<RendererAttachment>(
            this->swapchain->depth_buffer.image->GetImageFormat(),
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            1,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ),
        .is_depth_attachment = true
    });

    render_pass->AddDependency(VkSubpassDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    });

    HYPERION_BUBBLE_ERRORS(render_pass->Create(this->device));

    DebugLog(LogType::Debug, "Renderpass created!\n");

    HYPERION_RETURN_OK;
}*/

void RendererPipeline::SetVertexInputMode(std::vector<VkVertexInputBindingDescription> &binding_descs,
                                          std::vector<VkVertexInputAttributeDescription> &attribs) {
    this->vertex_binding_descriptions = binding_descs;
    this->vertex_attributes = attribs;
}

void RendererPipeline::Rebuild(ConstructionInfo &&construction_info, RendererDescriptorPool *descriptor_pool) {
    AssertExit(construction_info.shader != nullptr);

    m_construction_info = std::move(construction_info);

    this->BuildVertexAttributes(m_construction_info.vertex_attributes);

    VkPipelineVertexInputStateCreateInfo vertex_input_info{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input_info.vertexBindingDescriptionCount   = uint32_t(this->vertex_binding_descriptions.size());
    vertex_input_info.pVertexBindingDescriptions      = this->vertex_binding_descriptions.data();
    vertex_input_info.vertexAttributeDescriptionCount = uint32_t(this->vertex_attributes.size());
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
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

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

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_info.setLayoutCount = descriptor_pool->m_descriptor_set_layouts.size();
    layout_info.pSetLayouts    = descriptor_pool->m_descriptor_set_layouts.data();
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;

    if (vkCreatePipelineLayout(this->device->GetDevice(), &layout_info, nullptr, &this->layout) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Error creating pipeline layout!\n");
        throw std::runtime_error("Error creating pipeline layout");
    }

    /* Depth / stencil */
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = construction_info.depth_test;
    depth_stencil.depthWriteEnable = construction_info.depth_write;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f; // Optional
    depth_stencil.maxDepthBounds = 1.0f; // Optional
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {}; // Optional
    depth_stencil.back = {}; // Optional

    VkGraphicsPipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    auto stages = construction_info.shader->shader_stages;
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
    pipeline_info.renderPass = m_construction_info.render_pass->GetRenderPass();

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

    vkDestroyPipeline(render_device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(render_device, this->layout, nullptr);

    /*render_pass->Destroy(this->device);
    delete render_pass;
    render_pass = nullptr;*/

    //vkDestroyRenderPass(render_device, this->render_pass, nullptr);
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