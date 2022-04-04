#include "renderer.h"
#include "../engine.h"

namespace hyperion::v2 {

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
}

void Renderer::RenderOpaqueObjects(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    auto &render_list = engine->GetRenderList();

    for (const auto &pipeline : render_list.Get(GraphicsPipeline::Bucket::BUCKET_SKYBOX).pipelines.objects) {
        pipeline->Render(engine, primary, frame_index);
    }


    for (const auto &pipeline : render_list.Get(GraphicsPipeline::Bucket::BUCKET_OPAQUE).pipelines.objects) {
        pipeline->Render(engine, primary, frame_index);
    }
}

void Renderer::RenderTransparentObjects(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    const auto &render_list = engine->GetRenderList();

    for (const auto &pipeline : render_list[GraphicsPipeline::Bucket::BUCKET_TRANSLUCENT].pipelines.objects) {
        pipeline->Render(engine, primary, frame_index);
    }
}

} // namespace hyperion::v2