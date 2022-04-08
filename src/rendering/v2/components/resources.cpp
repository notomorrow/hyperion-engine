#include "resources.h"
#include "../engine.h"

namespace hyperion::v2 {

Resources::Resources(EngineCallbacks &callbacks)
    : shaders(callbacks),
      spatials(callbacks),
      meshes(callbacks)
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
    //shaders.RemoveAll(engine);
    textures.RemoveAll(engine);
    materials.RemoveAll(engine);
    compute_pipelines.RemoveAll(engine);
    //spatials.RemoveAll(engine);
}

} // namespace hyperion::v2