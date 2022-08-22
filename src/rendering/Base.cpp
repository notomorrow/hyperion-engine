#include "Base.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

using renderer::Device;

class Engine;

Device *GetEngineDevice(Engine *engine)
{
    return engine->GetInstance()->GetDevice();
}

#if 0
void AttachmentMap::InitializeAll(Engine *engine)
{
    InitializeObjects<Shader>(engine);
    InitializeObjects<Mesh>(engine);
    InitializeObjects<Framebuffer>(engine);
    InitializeObjects<Skeleton>(engine);
    InitializeObjects<Entity>(engine);
    InitializeObjects<Texture>(engine);
    InitializeObjects<Light>(engine);
    InitializeObjects<EnvProbe>(engine);
    InitializeObjects<RendererInstance>(engine);
    InitializeObjects<Material>(engine);
    InitializeObjects<Camera>(engine);
    InitializeObjects<PostProcessingEffect>(engine);
    InitializeObjects<Scene>(engine);
    InitializeObjects<RenderPass>(engine);
    InitializeObjects<ShadowRenderer>(engine);
    InitializeObjects<World>(engine);
    InitializeObjects<ComputePipeline>(engine);


    // InitializeObjects<CubemapRenderer>(engine);
    // InitializeObjects<RenderEnvironment>(engine);
    // InitializeObjects<SparseVoxelOctree>(engine);
    // InitializeObjects<Voxelizer>(engine);
    // .....
}
#endif

} // namespace hyperion::v2