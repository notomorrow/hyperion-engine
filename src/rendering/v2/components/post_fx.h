#ifndef HYPERION_V2_POST_FX_H
#define HYPERION_V2_POST_FX_H

#include "render_pass.h"
#include "framebuffer.h"
#include "shader.h"
#include "graphics.h"

#include <rendering/mesh.h>
#include <rendering/backend/renderer_frame.h>
#include <rendering/backend/renderer_structs.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <memory>
#include <utility>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::CommandBuffer;
using renderer::PerFrameData;
using renderer::MeshInputAttributeSet;

class Engine;

class PostEffect {
public:
    static std::shared_ptr<hyperion::Mesh> full_screen_quad;
    
    PostEffect();
    PostEffect(Ref<Shader> &&shader);
    PostEffect(const PostEffect &) = delete;
    PostEffect &operator=(const PostEffect &) = delete;
    ~PostEffect();

    inline auto &GetFrameData()
        { return m_frame_data; }
    inline const auto &GetFrameData() const
        { return m_frame_data; }

    inline Framebuffer *GetFramebuffer() const
        { return m_framebuffer.ptr; }

    inline Shader *GetShader() const
        { return m_shader.ptr; }

    inline RenderPass *GetRenderPass() const
        { return m_render_pass.ptr; }

    inline GraphicsPipeline::ID GetGraphicsPipelineId() const
        { return m_pipeline_id; }

    void CreateRenderPass(Engine *engine);
    void Create(Engine *engine);
    void CreateDescriptors(Engine *engine, uint32_t &binding_offset);
    void CreatePipeline(Engine *engine);

    void Destroy(Engine *engine);
    void DestroyPipeline(Engine *engine);

    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index);
    void Record(Engine *engine, uint32_t frame_index);

protected:
    void CreatePerFrameData(Engine *engine);

    std::unique_ptr<PerFrameData<CommandBuffer>> m_frame_data;
    Ref<Framebuffer> m_framebuffer;
    Ref<Shader> m_shader;
    Ref<RenderPass> m_render_pass;
    GraphicsPipeline::ID m_pipeline_id;

    std::vector<std::unique_ptr<renderer::Attachment>> m_attachments;
};

class PostProcessing {
public:
    PostProcessing();
    PostProcessing(const PostProcessing &) = delete;
    PostProcessing &operator=(const PostProcessing &) = delete;
    ~PostProcessing();

    void AddFilter(std::unique_ptr<PostEffect> &&filter);
    inline PostEffect *GetFilter(size_t index) const { return m_filters[index].get(); }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index) const;

private:
    std::vector<std::unique_ptr<PostEffect>> m_filters;
};

} // namespace hyperion::v2

#endif // HYPERION_V2_POST_FX_H

