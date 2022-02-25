#include "blur_compute_shader.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"

namespace hyperion {
BlurComputeShader::BlurComputeShader(const ShaderProperties &properties)
    : ComputeShader(properties)
{
    const std::string cs_path("shaders/compute/blur.comp");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_COMPUTE,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(cs_path)->GetText(),
        properties,
        cs_path
    );

    m_uniform_src_texture = m_uniforms.Acquire("srcTex").id;
    m_uniform_src_mip_level = m_uniforms.Acquire("srcMipLevel").id;
}

BlurComputeShader::~BlurComputeShader()
{
}
} // namespace hyperion