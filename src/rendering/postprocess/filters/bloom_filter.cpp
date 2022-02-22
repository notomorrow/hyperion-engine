#include "./bloom_filter.h"
#include "../../../asset/asset_manager.h"
#include "../../shader_manager.h"
#include "../../shaders/post/bloom.h"

namespace hyperion {

BloomFilter::BloomFilter()
    : PostFilter(ShaderManager::GetInstance()->GetShader<BloomShader>(
        ShaderProperties()
            .Define("BLOOM_INTENSITY", 1.4f)
            .Define("BLOOM_SPREAD", 0.7f)
    ))
{
}

void BloomFilter::SetUniforms(Camera *cam)
{
}

} // namespace hyperion
