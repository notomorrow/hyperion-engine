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
    static const MeshInputAttributeSet vertex_attributes;
    static std::shared_ptr<Mesh> full_screen_quad;

    PostEffect(Shader::ID shader_id);
    PostEffect(const PostEffect &) = delete;
    PostEffect &operator=(const PostEffect &) = delete;
    ~PostEffect();

    inline bool IsRecorded() const
        { return m_recorded; }

    inline auto &GetFrameData()
        { return m_frame_data; }
    inline const auto &GetFrameData() const
        { return m_frame_data; }

    void CreateRenderPass(Engine *engine);
    void CreateFrameData(Engine *engine);
    void CreateDescriptors(Engine *engine, uint32_t &binding_offset);
    void CreatePipeline(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary_command_buffer, uint32_t frame_index);
    void Record(Engine *engine, uint32_t frame_index);

private:
    std::unique_ptr<PerFrameData<CommandBuffer>> m_frame_data;
    Framebuffer::ID m_framebuffer_id;
    Shader::ID m_shader_id;
    RenderPass::ID m_render_pass_id;
    GraphicsPipeline::ID m_pipeline_id;
    bool m_recorded;
};

class PostProcessing {
public:
    PostProcessing();
    PostProcessing(const PostProcessing &) = delete;
    PostProcessing &operator=(const PostProcessing &) = delete;
    ~PostProcessing();

    void AddFilter(std::unique_ptr<PostEffect> &&filter);

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void BuildPipelines(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index) const;

private:
    std::vector<std::unique_ptr<PostEffect>> m_filters;
};

} // namespace hyperion::v2

#endif // HYPERION_V2_POST_FX_H

