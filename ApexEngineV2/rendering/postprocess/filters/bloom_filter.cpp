#include "./bloom_filter.h"
#include "../../../asset/asset_manager.h"
#include "../../shader_manager.h"
#include "../../shaders/post/bloom.h"

namespace apex {

BloomFilter::BloomFilter()
    : PostFilter(ShaderManager::GetInstance()->GetShader<BloomShader>(ShaderProperties()))
{
}

void BloomFilter::SetUniforms(Camera *cam)
{
}

} // namespace apex
