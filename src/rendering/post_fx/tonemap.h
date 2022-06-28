#ifndef HYPERION_V2_TONEMAP_H
#define HYPERION_V2_TONEMAP_H

#include <rendering/post_fx.h>
#include <types.h>

namespace hyperion::v2 {

class TonemapEffect : public PostProcessingEffect {
public:
    static constexpr Stage stage = Stage::POST_SHADING;
    static constexpr UInt  index = ~0;

    TonemapEffect();
    virtual ~TonemapEffect();

protected:
    virtual Ref<Shader> CreateShader(Engine *engine) override;
};

} // namespace hyperion::v2

#endif