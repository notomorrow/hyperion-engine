#include "resources.h"
#include "../engine.h"

namespace hyperion::v2 {

Resources::Resources(Engine *engine)
    : shaders(engine->callbacks, {engine}),
      textures(engine->callbacks, {engine}),
      materials(engine->callbacks, {engine}),
      spatials(engine->callbacks, {engine}),
      meshes(engine->callbacks, {engine}),
      skeletons(engine->callbacks, {engine})
{
}

Resources::~Resources()
{
}

void Resources::Create(Engine *engine)
{
}

void Resources::Destroy(Engine *engine)
{
    framebuffers.RemoveAll(engine);
    render_passes.RemoveAll(engine);
    compute_pipelines.RemoveAll(engine);
}

} // namespace hyperion::v2