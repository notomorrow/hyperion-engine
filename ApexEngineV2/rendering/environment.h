#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "../math/matrix4.h"
#include "../rendering/texture.h"

#include <memory>
#include <array>

namespace apex {
class Environment {
public:
    static Environment *GetInstance();

    Environment();

    bool ShadowsEnabled() const;
    void SetShadowsEnabled(bool);
    int NumCascades() const;
    void SetNumCascades(int);
    std::shared_ptr<Texture> GetShadowMap(int i) const;
    void SetShadowMap(int i, std::shared_ptr<Texture>);
    const Matrix4 &GetShadowMatrix(int i) const;
    void SetShadowMatrix(int i, const Matrix4 &);

private:
    static Environment *instance;

    bool shadows_enabled;
    int num_cascades;
    std::array<std::shared_ptr<Texture>, 4> shadow_maps;
    std::array<Matrix4, 4> shadow_matrices;
};
}

#endif