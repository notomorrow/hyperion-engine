#include "Resources.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

Resources::Resources(Engine *engine)
    : m_engine(engine),
      shaders(engine),
      textures(engine),
      materials(engine),
      lights(engine),
      entities(engine),
      meshes(engine),
      skeletons(engine),
      scenes(engine),
      render_passes(engine),
      framebuffers(engine),
      renderer_instances(engine),
      compute_pipelines(engine),
      blas(engine),
      cameras(engine),
      env_probes(engine)
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
}

} // namespace hyperion::v2