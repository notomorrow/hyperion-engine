#include "./fxaa_filter.h"
#include "../../../asset/asset_manager.h"
#include "../../shader_manager.h"
#include "../../shaders/post/fxaa.h"

namespace hyperion {

FXAAFilter::FXAAFilter()
    : PostFilter(ShaderManager::GetInstance()->GetShader<FXAAShader>(ShaderProperties()))
{
}

void FXAAFilter::SetUniforms(Camera *cam)
{
    PostFilter::SetUniforms(cam);
}

} // namespace hyperion
