#ifndef HYPERION_V2_VOXELIZER_H
#define HYPERION_V2_VOXELIZER_H

#include "base.h"
#include "scene.h"
#include "render_pass.h"
#include "framebuffer.h"
#include "shader.h"
#include "graphics.h"
#include "atomics.h"

#include <rendering/backend/renderer_command_buffer.h>
#include <rendering/backend/renderer_fence.h>
#include <rendering/backend/renderer_buffer.h>

namespace hyperion::v2 {

class Engine;

class Voxelizer : public EngineComponentBase<STUB_CLASS(Voxelizer)> {
    struct Fragment {
        uint32_t x, y;
    };

public:
    static constexpr size_t octree_depth = 10;
    static constexpr size_t voxel_map_size = 1 << octree_depth;
    static constexpr size_t default_fragment_list_buffer_size = 20000 * sizeof(Fragment);

    Voxelizer();
    Voxelizer(const Voxelizer &other) = delete;
    Voxelizer &operator=(const Voxelizer &other) = delete;
    ~Voxelizer();

    Scene *GetScene() const { return m_scene.ptr; }
    GraphicsPipeline::ID GetGraphicsPipelineId() const { return m_pipeline_id; }

    uint32_t NumFragments() const { return m_num_fragments; }

    void Init(Engine *engine);
    void Render(Engine *engine);

private:
    void AddSpatialsToPipeline(Engine *engine);
    void CreatePipeline(Engine *engine);
    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateFramebuffer(Engine *engine);
    void CreateDescriptors(Engine *engine);

    void ResizeFragmentListBuffer(Engine *engine);

    void RenderFragmentList(Engine *engine, bool count_mode);

    Ref<Scene> m_scene;

    std::unique_ptr<AtomicCounter> m_counter;
    std::unique_ptr<StorageBuffer> m_fragment_list_buffer;

    Ref<Framebuffer> m_framebuffer;
    Ref<Shader> m_shader;
    Ref<RenderPass> m_render_pass;
    GraphicsPipeline::ID m_pipeline_id;

    std::vector<std::unique_ptr<renderer::Attachment>> m_attachments;

    uint32_t m_num_fragments;
};

} // namespace hyperion::v2

#endif