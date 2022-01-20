#ifndef GI_MAPPING_H
#define GI_MAPPING_H

#include "../shadow/shadow_mapping.h"

#include <memory>

namespace apex {
class Texture;
class GIMapping : public ShadowMapping {
public:
    GIMapping(Camera *view_cam, double max_dist);
    virtual ~GIMapping() = default;

    inline unsigned int GetTextureId() const { return m_texture_id; }

    inline const Vector3 &GetProbePosition() const { return m_center_pos; }

    virtual void Begin() override;
    virtual void End() override;

private:
    unsigned int m_texture_id;
};
} // namespace apex

#endif
