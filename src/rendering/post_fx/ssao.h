#ifndef HYPERION_V2_SSAO_H
#define HYPERION_V2_SSAO_H

#include <rendering/post_fx.h>

namespace hyperion::v2 {

class SsaoEffect : public PostProcessingEffect {
public:
    SsaoEffect();
    virtual ~SsaoEffect();

protected:
    virtual Ref<Shader> CreateShader(Engine *engine) override;
};

} // namespace hyperion::v2

#endif