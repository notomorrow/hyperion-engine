#include "./shadertoy_filter.h"
#include "../../../asset/asset_manager.h"
#include "../../shader_manager.h"
#include "../../environment.h"
#include "../../shaders/post/shadertoy.h"

namespace hyperion {

ShadertoyFilter::ShadertoyFilter()
    : PostFilter(ShaderManager::GetInstance()->GetShader<ShadertoyShader>(ShaderProperties()))
{
    
}

void ShadertoyFilter::SetUniforms(Camera *cam)
{
}

} // namespace hyperion
