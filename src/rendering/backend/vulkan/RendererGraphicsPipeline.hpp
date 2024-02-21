#ifndef HYPERION_RENDERER_BACKEND_VULKAN_GRAPHICS_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_GRAPHICS_PIPELINE_HPP

#include <vulkan/vulkan.h>

#include <rendering/backend/RendererPipeline.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/lib/DynArray.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class FramebufferObject;

template <PlatformType PLATFORM>
class RenderPass;

template <>
class GraphicsPipeline<Platform::VULKAN> : public Pipeline<Platform::VULKAN>
{
public:
    struct ConstructionInfo
    {
        VertexAttributeSet                              vertex_attributes;

        Topology                                        topology = Topology::TRIANGLES;
        FaceCullMode                                    cull_mode = FaceCullMode::BACK;
        FillMode                                        fill_mode = FillMode::FILL;
        BlendMode                                       blend_mode = BlendMode::NONE;

        bool                                            depth_test = true;
        bool                                            depth_write = true;

        ShaderProgram<Platform::VULKAN>                 *shader = nullptr;
        RenderPassRef<Platform::VULKAN>                 render_pass;
        Array<FramebufferObjectRef<Platform::VULKAN>>   fbos;

        // stencil
        StencilState                        stencil_state { };
    };

    GraphicsPipeline();
    GraphicsPipeline(ShaderProgramRef<Platform::VULKAN> shader);
    /*! \brief Construct a pipeline using the given \ref used_descriptor_set as the descriptor sets to be
        used with this pipeline.  */
    GraphicsPipeline(const Array<DescriptorSetRef> &used_descriptor_sets);
    GraphicsPipeline(ShaderProgramRef<Platform::VULKAN> shader, const Array<DescriptorSetRef> &used_descriptor_sets);
    GraphicsPipeline(ShaderProgramRef<Platform::VULKAN> shader, DescriptorTableRef<Platform::VULKAN> descriptor_table);
    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    void SetViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f);
    void SetScissor(int x, int y, uint32 width, uint32 height);

    // V2, uses the new descriptor set system
    Result Create(Device<Platform::VULKAN> *device, ConstructionInfo &&construction_info);

    // v1
    Result Create(Device<Platform::VULKAN> *device, ConstructionInfo &&construction_info, DescriptorPool *descriptor_pool);
    Result Destroy(Device<Platform::VULKAN> *device);
    
    void Bind(CommandBuffer<Platform::VULKAN> *cmd);
    void SubmitPushConstants(CommandBuffer<Platform::VULKAN> *cmd) const;

    const ConstructionInfo &GetConstructionInfo() const
        { return m_construction_info; }

private:
    Result Rebuild(Device<Platform::VULKAN> *device, DescriptorPool *descriptor_pool);
    void UpdateDynamicStates(VkCommandBuffer cmd);
    Array<VkVertexInputAttributeDescription> BuildVertexAttributes(const VertexAttributeSet &attribute_set);

    Array<VkDynamicState>                       m_dynamic_states;

    VkViewport                                  viewport;
    VkRect2D                                    scissor;

    Array<VkVertexInputBindingDescription>      vertex_binding_descriptions { };
    Array<VkVertexInputAttributeDescription>    vertex_attributes{};

    ConstructionInfo                            m_construction_info;

};

} // namsepace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_GRAPHICS_PIPELINE_HPP
