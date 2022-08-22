#include "Tonemap.hpp"
#include <rendering/Shader.hpp>
#include <rendering/PostFX.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

TonemapEffect::TonemapEffect()
    : PostProcessingEffect(stage, index)
{
}

TonemapEffect::~TonemapEffect() = default;

Handle<Shader> TonemapEffect::CreateShader(Engine *engine)
{
    return Handle<Shader>(new Shader(
        std::vector<SubShader> {
            SubShader{ShaderModule::Type::VERTEX, {
                Reader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/PostEffect.vert.spv")).ReadBytes()
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                Reader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/tonemap.frag.spv")).ReadBytes()
            }}
        }
    ));
}

} // namespace hyperion::v2