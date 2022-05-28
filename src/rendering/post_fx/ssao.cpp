#include "ssao.h"
#include <rendering/shader.h>
#include <engine.h>

namespace hyperion::v2 {

SsaoEffect::SsaoEffect()
    : PostProcessingEffect()
{
}

SsaoEffect::~SsaoEffect() = default;

Ref<Shader> SsaoEffect::CreateShader(Engine *engine)
{
    return engine->resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {
                Reader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/filter_pass_vert.spv")).ReadBytes()
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                Reader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/filter_pass_frag.spv")).ReadBytes()
            }}
        }
    ));
}

} // namespace hyperion::v2