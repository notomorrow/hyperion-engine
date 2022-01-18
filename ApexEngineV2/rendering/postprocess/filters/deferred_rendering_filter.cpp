#include "./deferred_rendering_filter.h"
#include "../../shader_manager.h"
#include "../../shaders/post/deferred_rendering.h"

namespace apex {

DeferredRenderingFilter::DeferredRenderingFilter()
    : PostFilter(ShaderManager::GetInstance()->GetShader<DeferredRenderingShader>(ShaderProperties()))
{
}

void DeferredRenderingFilter::SetUniforms(Camera *cam)
{
}

} // namespace apex
