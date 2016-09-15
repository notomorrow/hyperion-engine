#ifndef RENDERABLE_H
#define RENDERABLE_H

#include "shader.h"
#include "material.h"

#include <memory>

namespace apex {
class CoreEngine;

class Renderable {
    friend class Renderer;
public:
    enum RenderBucket {
        RB_OPAQUE,
        RB_TRANSPARENT,
        RB_SKY,
        RB_PARTICLE,
        RB_SCREEN,
    };

    virtual ~Renderable()
    {
    }

    RenderBucket GetRenderBucket() const;
    void SetRenderBucket(RenderBucket rb);
    std::shared_ptr<Shader> GetShader();
    void SetShader(std::shared_ptr<Shader> ptr);
    Material &GetMaterial();
    void SetMaterial(const Material &mat);

    virtual void Render() = 0;

private:
    RenderBucket bucket = RB_OPAQUE;
    std::shared_ptr<Shader> shader;
    Material material;
};
} // namespace apex

#endif