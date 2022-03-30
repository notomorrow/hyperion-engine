#include "renderer.h"

#include "deferred.h"

namespace hyperion::v2 {

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
}

void Renderer::RenderOpaqueObjects(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    m_render_list.Get(GraphicsPipeline::Bucket::BUCKET_OPAQUE).BeginRenderPass(engine, primary, 0);

    for (const auto &pipeline : m_render_list.Get(GraphicsPipeline::Bucket::BUCKET_SKYBOX).pipelines.objects) {
        pipeline->Render(engine, primary, frame_index);
    }


    for (const auto &pipeline : m_render_list.Get(GraphicsPipeline::Bucket::BUCKET_OPAQUE).pipelines.objects) {
        pipeline->Render(engine, primary, frame_index);
    }

    m_render_list.Get(GraphicsPipeline::Bucket::BUCKET_OPAQUE).EndRenderPass(engine, primary);
}

void Renderer::RenderTransparentObjects(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    for (const auto &pipeline : m_render_list[GraphicsPipeline::Bucket::BUCKET_TRANSLUCENT].pipelines.objects) {
        pipeline->Render(engine, primary, frame_index);
    }
}

} // namespace hyperion::v2