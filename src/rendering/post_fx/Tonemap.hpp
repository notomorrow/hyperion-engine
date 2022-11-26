#ifndef HYPERION_V2_TONEMAP_H
#define HYPERION_V2_TONEMAP_H

#include <rendering/PostFX.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class TonemapEffect : public PostProcessingEffect {
public:
    static constexpr Stage stage = Stage::POST_SHADING;
    static constexpr UInt index = ~0;

    TonemapEffect();
    virtual ~TonemapEffect();

protected:
    virtual Handle<Shader> CreateShader() override;
};

} // namespace hyperion::v2

#endif