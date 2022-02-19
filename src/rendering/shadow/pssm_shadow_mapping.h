#ifndef PSSM_SHADOW_MAPPING_H
#define PSSM_SHADOW_MAPPING_H

#include "shadow_mapping.h"
#include "../renderer.h"
#include "../renderable.h"
#include "../camera/camera.h"

#include <vector>
#include <memory>

namespace hyperion {

class Shader;

class PssmShadowMapping : public Renderable {
public:
    PssmShadowMapping(int num_splits, double max_dist);
    ~PssmShadowMapping() = default;

    int NumSplits() const;
    void SetLightDirection(const Vector3 &dir);

    inline const Vector3 &GetOrigin() const { return m_origin; }
    void SetOrigin(const Vector3 &origin);

    inline bool IsVarianceShadowMapping() const { return m_is_variance_shadow_mapping; }
    void SetVarianceShadowMapping(bool value);

    virtual void Render(Renderer *renderer, Camera *cam) override;

private:
    const int m_num_splits;
    const double m_max_dist;
    bool m_is_variance_shadow_mapping;
    std::vector<std::shared_ptr<ShadowMapping>> m_shadow_renderers;
    Vector3 m_origin;

    virtual std::shared_ptr<Renderable> CloneImpl() override;
};

} // namespace hyperion

#endif
