#include "./gamma_correction_filter.h"
#include "../../shader_manager.h"
#include "../../shaders/post/gamma_correct.h"

namespace hyperion {

GammaCorrectionFilter::GammaCorrectionFilter()
    : PostFilter(ShaderManager::GetInstance()->GetShader<GammaCorrectShader>(ShaderProperties()))
{
}

void GammaCorrectionFilter::SetUniforms(Camera *cam)
{
}

} // namespace hyperion
